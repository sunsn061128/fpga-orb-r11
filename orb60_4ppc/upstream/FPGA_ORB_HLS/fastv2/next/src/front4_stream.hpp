#pragma once
struct CandidateQuad { uint64_t lane[4]; uint8_t mask; };

// Row-end invalid lanes are zero; input bytes remain tightly packed across rows.
void unpack_front4_words(hls::stream<uint64_t>& words,hls::stream<uint32_t>& output,int width,int height) {
#pragma HLS INLINE off
    uint64_t cached=0;int position=8;
    for(int y=0;y<height;++y) {
    UnpackQuads:
        for(int x=0;x<width;x+=4) {
#pragma HLS PIPELINE II=1
            const int n=width-x<4 ? width-x : 4;
            uint64_t next=0;
            if(position+n>8) next=words.read();
            uint32_t value=0;
            for(int lane=0;lane<4;++lane) {
#pragma HLS UNROLL
                if(lane<n) {
                    const int byte=position+lane;
                    const uint8_t pixel=byte<8 ? uint8_t(cached>>(8*byte)) : uint8_t(next>>(8*(byte-8)));
                    value|=uint32_t(pixel)<<(8*lane);
                }
            }
            if(position+n>8) {cached=next;position=position+n-8;} else position+=n;
            output.write(value);
        }
    }
}

template<int Lane> uint8_t front4_score(const uint8_t window[7][10]) {
#pragma HLS INLINE off
#pragma HLS PIPELINE II=1
#pragma HLS ARRAY_PARTITION variable=window complete dim=0
    uint8_t ring[16];
#pragma HLS ARRAY_PARTITION variable=ring complete
    ring[0]=window[6][3+Lane];ring[1]=window[6][4+Lane];
    ring[2]=window[5][5+Lane];ring[3]=window[4][6+Lane];
    ring[4]=window[3][6+Lane];ring[5]=window[2][6+Lane];
    ring[6]=window[1][5+Lane];ring[7]=window[0][4+Lane];
    ring[8]=window[0][3+Lane];ring[9]=window[0][2+Lane];
    ring[10]=window[1][1+Lane];ring[11]=window[2][Lane];
    ring[12]=window[3][Lane];ring[13]=window[4][Lane];
    ring[14]=window[5][1+Lane];ring[15]=window[6][2+Lane];
    return fast_score(window[3][3+Lane],ring);
}

void scan_front4_quads(hls::stream<uint32_t>& input,hls::stream<CandidateQuad>& output,
    const fastv2_profile::Profile& p,int offset,int high,int low,int dual) {
#pragma HLS INLINE off
    // Rotate complete rows: each bank is read-only or write-only for one row.
    // This removes the read/shift/write recurrence without dependency waivers.
    uint32_t pixel_lines[7][240],score_lines[3][240];
#pragma HLS ARRAY_PARTITION variable=pixel_lines complete dim=1
#pragma HLS ARRAY_PARTITION variable=score_lines complete dim=1
#pragma HLS RESOURCE variable=pixel_lines core=RAM_S2P_BRAM
#pragma HLS RESOURCE variable=score_lines core=RAM_S2P_BRAM
    int count=0,pixel_slot=0,score_slot=0;
    for(int y=0;y<p.height;++y) {
        uint8_t pixel_bank[6];
#pragma HLS ARRAY_PARTITION variable=pixel_bank complete
        for(int r=0;r<6;++r) {
#pragma HLS UNROLL
            const int bank=pixel_slot+r+1;pixel_bank[r]=uint8_t(bank>=7 ? bank-7 : bank);
        }
        const int older_bank=score_slot==2 ? 0 : score_slot+1;
        const int recent_bank=score_slot==0 ? 2 : score_slot-1;
        const uint32_t yi=fastv2_profile::kAxisYInfo[p.y_info_offset+y];
        const int ys=int(yi&1023u),ye=int((yi>>10)&1023u);
        uint8_t window[7][10],nms[3][6];
#pragma HLS ARRAY_PARTITION variable=window complete dim=0
#pragma HLS ARRAY_PARTITION variable=nms complete dim=0
        for(int r=0;r<7;++r) {
#pragma HLS UNROLL
            for(int c=0;c<10;++c) {
#pragma HLS UNROLL
                window[r][c]=0;
            }
        }
        for(int r=0;r<3;++r) {
#pragma HLS UNROLL
            for(int c=0;c<6;++c) {
#pragma HLS UNROLL
                nms[r][c]=0;
            }
        }
    RasterQuads:
        for(int x=0;x<p.width;x+=4) {
#pragma HLS PIPELINE II=1
#pragma HLS LOOP_FLATTEN off
            const uint32_t raw=input.read();uint32_t column[7],banks[7];
#pragma HLS ARRAY_PARTITION variable=column complete
#pragma HLS ARRAY_PARTITION variable=banks complete
            for(int bank=0;bank<7;++bank) {
#pragma HLS UNROLL
                if(bank==pixel_slot) {pixel_lines[bank][x/4]=raw;banks[bank]=raw;}
                else banks[bank]=pixel_lines[bank][x/4];
            }
            for(int r=0;r<6;++r) {
#pragma HLS UNROLL
                column[r]=y>=6-r ? banks[pixel_bank[r]] : uint32_t(0);
            }
            column[6]=raw;
            for(int r=0;r<7;++r) {
#pragma HLS UNROLL
                for(int c=0;c<6;++c) {
#pragma HLS UNROLL
                    window[r][c]=window[r][c+4];
                }
                for(int lane=0;lane<4;++lane) {
#pragma HLS UNROLL
                    window[r][6+lane]=uint8_t(column[r]>>(8*lane));
                }
            }
            uint8_t score[4]={};
#pragma HLS ARRAY_PARTITION variable=score complete
            // x=4 needs only lanes 2/3. Masking the whole group would lose the
            // first two legal FAST centers and their NMS neighbours.
            const uint8_t computed0=front4_score<0>(window),computed1=front4_score<1>(window);
            const uint8_t computed2=front4_score<2>(window),computed3=front4_score<3>(window);
            score[0]=y>=6 && x>=6 && x<p.width ? computed0 : uint8_t(0);
            score[1]=y>=6 && x+1>=6 && x+1<p.width ? computed1 : uint8_t(0);
            score[2]=y>=6 && x+2>=6 && x+2<p.width ? computed2 : uint8_t(0);
            score[3]=y>=6 && x+3>=6 && x+3<p.width ? computed3 : uint8_t(0);
            const uint32_t packed=uint32_t(score[0])|(uint32_t(score[1])<<8)|(uint32_t(score[2])<<16)|(uint32_t(score[3])<<24);
            const int sy=y-6;
            uint32_t score_banks[3];
#pragma HLS ARRAY_PARTITION variable=score_banks complete
            for(int bank=0;bank<3;++bank) {
#pragma HLS UNROLL
                if(bank==score_slot) {score_lines[bank][x/4]=packed;score_banks[bank]=packed;}
                else score_banks[bank]=score_lines[bank][x/4];
            }
            const uint32_t old2=sy>=2 ? score_banks[older_bank] : 0;
            const uint32_t old1=sy>=1 ? score_banks[recent_bank] : 0;
            uint32_t values[3]={old2,old1,packed};
#pragma HLS ARRAY_PARTITION variable=values complete
            for(int r=0;r<3;++r) {
#pragma HLS UNROLL
                nms[r][0]=nms[r][4];nms[r][1]=nms[r][5];
                for(int lane=0;lane<4;++lane) {
#pragma HLS UNROLL
                    nms[r][2+lane]=uint8_t(values[r]>>(8*lane));
                }
            }
            if(x>=8 && sy>=2) {
            const int index=offset+x/4;
            const uint32_t info[4]={orb4_rom::kGeometry0[index],orb4_rom::kGeometry1[index],
                orb4_rom::kGeometry2[index],orb4_rom::kGeometry3[index]};
#pragma HLS ARRAY_PARTITION variable=info complete
            CandidateQuad quad={};int found=0;
#pragma HLS ARRAY_PARTITION variable=quad.lane complete
            for(int lane=0;lane<4;++lane) {
#pragma HLS UNROLL
                const int ex=x+lane-20,ey=y-20;
                const uint32_t xi=info[lane];
                const int xs=int(xi&1023u),xe=int((xi>>10)&1023u),response=nms[1][1+lane];
                bool keep=x+lane<p.width && ex>=3 && ex<p.effective_width-3 &&
                    ey>=3 && ey<p.effective_height-3 && response>=(dual ? low : high);
                for(int dy=-1;dy<=1;++dy) {
#pragma HLS UNROLL
                    for(int dx=-1;dx<=1;++dx) {
#pragma HLS UNROLL
                        if(!dx && !dy) continue;
                        const int neighbour=ex+dx>=xs && ex+dx<xe && ey+dy>=ys && ey+dy<ye ? nms[dy+1][dx+1+lane] : 0;
                        if(neighbour>=response) keep=false;
                    }
                }
                const int cell=int((yi>>25)&31u)*p.cell_cols+int((xi>>25)&31u);
                const Point point={ex,ey,response,int((yi>>20)&31u)*p.tile_cols+int((xi>>20)&31u)};
                quad.lane[lane]=pack_point(point)|(uint64_t(cell)<<41);
                quad.mask|=uint8_t(keep)<<lane;found+=int(keep);
            }
            if(quad.mask) output.write(quad);
            count+=found;
            }
        }
        pixel_slot=pixel_slot==6 ? 0 : pixel_slot+1;
        score_slot=score_slot==2 ? 0 : score_slot+1;
    }
    CandidateQuad end={};end.lane[0]=(UINT64_C(1)<<63)|uint64_t(unsigned(count));output.write(end);
}

// One downstream candidate per cycle; never assume NMS forbids multi-valid
// groups across tile boundaries. Backpressure retains the complete pending mask.
void serialize_front4_quads(hls::stream<CandidateQuad>& input,hls::stream<uint64_t>& output) {
#pragma HLS INLINE off
    CandidateQuad pending={};
#pragma HLS ARRAY_PARTITION variable=pending.lane complete
MergeQuads:
    for(;;) {
#pragma HLS PIPELINE II=1
        if(!pending.mask) {
            pending=input.read();
            if(pending.lane[0]>>63) {output.write(pending.lane[0]);break;}
        }
        int lane=(pending.mask&1) ? 0 : (pending.mask&2) ? 1 : (pending.mask&4) ? 2 : 3;
        if(pending.mask) {output.write(pending.lane[lane]);pending.mask&=uint8_t(~(1u<<lane));}
    }
}
