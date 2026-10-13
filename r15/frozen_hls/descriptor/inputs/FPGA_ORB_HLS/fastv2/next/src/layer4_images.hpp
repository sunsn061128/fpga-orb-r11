#pragma once
// Keep the original coefficients, staged fixed-point rounding and reflect-101.
void gaussian4_horizontal(hls::stream<uint32_t>& input,hls::stream<uint64_t>& output,int width,int height) {
#pragma HLS INLINE off
    for(int y=0;y<height;++y) {
        uint8_t head[8],recent[12]={},window[10]={};
#pragma HLS ARRAY_PARTITION variable=head complete
#pragma HLS ARRAY_PARTITION variable=recent complete
#pragma HLS ARRAY_PARTITION variable=window complete
        for(int q=0;q<2;++q) {
#pragma HLS PIPELINE II=1
            const uint32_t raw=input.read();
            for(int lane=0;lane<4;++lane) {
#pragma HLS UNROLL
                head[4*q+lane]=uint8_t(raw>>(8*lane));recent[4+4*q+lane]=head[4*q+lane];
            }
        }
        int loaded=8;
    HorizontalGaussianQuads:
        for(int scan=-5;scan<width+3;scan+=4) {
#pragma HLS PIPELINE II=1
#pragma HLS LOOP_FLATTEN off
            if(scan+3>=loaded && loaded<width) {
                const uint32_t raw=input.read();
                for(int i=0;i<8;++i) {
#pragma HLS UNROLL
                    recent[i]=recent[i+4];
                }
                for(int lane=0;lane<4;++lane) {
#pragma HLS UNROLL
                    recent[8+lane]=uint8_t(raw>>(8*lane));
                }
                loaded+=4;
            }
            for(int i=0;i<6;++i) {
#pragma HLS UNROLL
                window[i]=window[i+4];
            }
            for(int lane=0;lane<4;++lane) {
#pragma HLS UNROLL
                const int coordinate=layer_reflect(scan+lane,width);
                const int position=coordinate-(loaded-12);
#ifndef __SYNTHESIS__
                assert(coordinate<8 || (position>=0 && position<12));
#endif
                window[6+lane]=coordinate<8 ? head[coordinate] : recent[position];
            }
            if(scan>=3) {
                uint64_t sums=0;
                for(int lane=0;lane<4;++lane) {
#pragma HLS UNROLL
                    uint8_t taps[7];
#pragma HLS ARRAY_PARTITION variable=taps complete
                    for(int tap=0;tap<7;++tap) {
#pragma HLS UNROLL
                        taps[tap]=window[tap+lane];
                    }
                    const uint16_t sum=scan-3+lane<width ? uint16_t(layer_gaussian_sum(taps)) : uint16_t(0);
                    sums|=uint64_t(sum)<<(16*lane);
                }
                output.write(sums);
            }
        }
#ifndef __SYNTHESIS__
        assert(loaded==(width+3)/4*4);
#endif
    }
}

void gaussian4_vertical(hls::stream<uint64_t>& input,hls::stream<uint32_t>& output,int width,int height) {
#pragma HLS INLINE off
    uint64_t lines[7][240];
#pragma HLS ARRAY_PARTITION variable=lines complete dim=1
#pragma HLS RESOURCE variable=lines core=RAM_S2P_BRAM
    int slot=0;
    for(int sy=0;sy<height+3;++sy) {
        uint8_t bank_of_tap[7];
#pragma HLS ARRAY_PARTITION variable=bank_of_tap complete
        for(int tap=0;tap<7;++tap) {
#pragma HLS UNROLL
            const int delta=sy>=3 ? layer_reflect(sy+tap-6,height)-sy : 0;
            const int bank=slot+delta;bank_of_tap[tap]=uint8_t(bank<0 ? bank+7 : bank);
        }
    VerticalGaussianQuads:
        for(int x=0;x<width;x+=4) {
#pragma HLS PIPELINE II=1
#pragma HLS LOOP_FLATTEN off
            const uint64_t incoming=sy<height ? input.read() : uint64_t(0);
            uint64_t column[7];
#pragma HLS ARRAY_PARTITION variable=column complete
            for(int bank=0;bank<7;++bank) {
#pragma HLS UNROLL
                if(sy<height && bank==slot) {column[bank]=incoming;lines[bank][x/4]=incoming;}
                else column[bank]=sy>=6 || bank<=sy ? lines[bank][x/4] : uint64_t(0);
            }
            if(sy>=3) {
                uint32_t result=0;
                for(int lane=0;lane<4;++lane) {
#pragma HLS UNROLL
                    uint16_t taps[7];
#pragma HLS ARRAY_PARTITION variable=taps complete
                    for(int tap=0;tap<7;++tap) {
#pragma HLS UNROLL
                        taps[tap]=uint16_t(column[bank_of_tap[tap]]>>(16*lane));
                    }
                    const uint8_t value=x+lane<width ? uint8_t((layer_gaussian_sum(taps)+32768u)>>16) : uint8_t(0);
                    result|=uint32_t(value)<<(8*lane);
                }
                output.write(result);
            }
        }
        slot=slot==6 ? 0 : slot+1;
    }
}

uint8_t layer4_resize_pixel(const uint8_t top[12],const uint8_t bottom[12],uint32_t axis,int source_x,uint32_t wy) {
#pragma HLS INLINE
#pragma HLS ARRAY_PARTITION variable=top complete
#pragma HLS ARRAY_PARTITION variable=bottom complete
    const int position=int(axis&1023u)-(source_x-8);const uint32_t wx=(axis>>10)&2047u;
#ifndef __SYNTHESIS__
    assert(position>=0 && position+1<12);
#endif
    const uint32_t a=uint32_t(top[position])*(2048-wx)+uint32_t(top[position+1])*wx;
    const uint32_t b=uint32_t(bottom[position])*(2048-wx)+uint32_t(bottom[position+1])*wx;
    const uint32_t value=((a>>4)*(2048-wy)>>16)+((b>>4)*wy>>16);
    return uint8_t((value+2)>>2);
}

void resize4_quads(hls::stream<uint32_t>& input,hls::stream<uint32_t>& output,
    int level,int width,int height,int destination_width,int destination_height) {
#pragma HLS INLINE off
    uint32_t previous[2][240];
#pragma HLS ARRAY_PARTITION variable=previous complete dim=1
#pragma HLS RESOURCE variable=previous core=RAM_S2P_BRAM
    int destination_y=0;
    for(int y=0;y<height;++y) {
        const int slot=y&1;
        const uint32_t vertical=level<7 && destination_y<destination_height ?
            uint32_t(fastv2_profile::kResizeYAxis[fastv2_profile::kResizeYOffsets[level]+destination_y]) : 0;
        const bool active=level<7 && destination_y<destination_height && y==int(vertical&1023u)+1;
        const uint32_t wy=(vertical>>10)&2047u;
        uint8_t top[12]={},bottom[12]={};
#pragma HLS ARRAY_PARTITION variable=top complete
#pragma HLS ARRAY_PARTITION variable=bottom complete
    ResizeSourceQuads:
        for(int x=0;x<width;x+=4) {
#pragma HLS PIPELINE II=1
#pragma HLS LOOP_FLATTEN off
            const uint32_t raw=input.read();uint32_t banks[2];
#pragma HLS ARRAY_PARTITION variable=banks complete
            for(int bank=0;bank<2;++bank) {
#pragma HLS UNROLL
                if(bank==slot) {previous[bank][x/4]=raw;banks[bank]=raw;}
                else banks[bank]=previous[bank][x/4];
            }
            const uint32_t above=y>0 ? banks[slot^1] : uint32_t(0);
            for(int i=0;i<8;++i) {
#pragma HLS UNROLL
                top[i]=top[i+4];bottom[i]=bottom[i+4];
            }
            for(int lane=0;lane<4;++lane) {
#pragma HLS UNROLL
                top[8+lane]=uint8_t(above>>(8*lane));bottom[8+lane]=uint8_t(raw>>(8*lane));
            }
            if(level<7) {
                const int index=orb4_rom::kEmitOffsets[level]+x/4;
                if(active && orb4_rom::kEmitValid[index]) {
                    const uint32_t axes[4]={uint32_t(orb4_rom::kEmitAxis0[index]),uint32_t(orb4_rom::kEmitAxis1[index]),
                        uint32_t(orb4_rom::kEmitAxis2[index]),uint32_t(orb4_rom::kEmitAxis3[index])};
#pragma HLS ARRAY_PARTITION variable=axes complete
                    uint32_t result=0;
                    for(int lane=0;lane<4;++lane) {
#pragma HLS UNROLL
                        result|=uint32_t(layer4_resize_pixel(top,bottom,axes[lane],x,wy))<<(8*lane);
                    }
                    output.write(result);
                }
            }
        }
        if(active) {
            if(destination_width&3) {
                const uint32_t axes[4]={uint32_t(orb4_rom::kTailAxis0[level]),uint32_t(orb4_rom::kTailAxis1[level]),
                    uint32_t(orb4_rom::kTailAxis2[level]),uint32_t(orb4_rom::kTailAxis3[level])};
#pragma HLS ARRAY_PARTITION variable=axes complete
                uint32_t result=0;
                for(int lane=0;lane<4;++lane) {
#pragma HLS UNROLL
                    if(lane<(destination_width&3)) result|=uint32_t(layer4_resize_pixel(top,bottom,axes[lane],(width-1)&~3,wy))<<(8*lane);
                }
                output.write(result);
            }
            ++destination_y;
        }
    }
#ifndef __SYNTHESIS__
    assert(destination_y==destination_height);
#endif
}

template<int Tag> void layer_pack_quads(hls::stream<uint32_t>& input,hls::stream<uint64_t>& output,int width,int height) {
#pragma HLS INLINE off
    uint64_t carry=0;int position=0;
    for(int y=0;y<height;++y) {
    PackLayerQuads:
        for(int x=0;x<width;x+=4) {
#pragma HLS PIPELINE II=1
            const int n=width-x<4 ? width-x : 4;
            const uint32_t raw=input.read(),valid=n==4 ? raw : raw&((UINT32_C(1)<<(8*n))-1);
            carry|=uint64_t(valid)<<(8*position);
            if(position+n>=8) {
                output.write(carry);
                carry=position+n>8 ? uint64_t(valid)>>(8*(8-position)) : uint64_t(0);
                position=position+n-8;
            } else position+=n;
        }
    }
    if(position) output.write(carry);
}
