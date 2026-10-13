#pragma once
// Per-patch DDR producers own complete PIPO banks; no word-FIFO round trip.
bool describe_patch(const RawBlock&,const BlurBlock&,int,int,int,FeaturePacket&);

void load_raw_patch(const uint64_t* image,hls::stream<PointToken>& points,
    RawBlock& patch,int width,int height,int index,int count,hls::stream<uint32_t>& byte_counts) {
#pragma HLS INLINE off
    if(index>=count)return;
    const PointToken point=points.read();const bool valid=valid_point(point,width,height);
    const int x=signed32(uint32_t(point.xy))+1,y=signed32(uint32_t(point.xy>>32))+1;
    patch[155]=point.xy;patch[156]=point.score_tile;patch[157]=valid?0:2;
    uint32_t bytes=0;
    if(valid) {
    RawPatchRows:
        for(int row=0;row<31;++row) {
            const int address=(y+row)*width+x;
            const unsigned words=unsigned(((address&7)+31+7)>>3)&7u;
#ifndef __SYNTHESIS__
            assert(words>=4 && words<=5);
#endif
            std::memcpy(patch+row*5,image+(address>>3),words*8);bytes+=words*8;
        }
    }
    // Unwritten trailing words are outside every raw_pixel access. Invalid
    // patches are rejected by the initialized status word before any pixel read.
    byte_counts.write(bytes);
}
void load_blur_patch(const uint64_t* image,hls::stream<PointToken>& points,
    BlurBlock& patch,int width,int height,int index,int count,hls::stream<uint32_t>& byte_counts) {
#pragma HLS INLINE off
    if(index>=count)return;
    const PointToken point=points.read();const bool valid=valid_point(point,width,height);
    const int cx=signed32(uint32_t(point.xy))+16,cy=signed32(uint32_t(point.xy>>32))+16;
    const int x=cx<18?0:cx-18,last=cx+18>=width?width-1:cx+18;
    uint32_t bytes=0;
    if(valid) {
    BlurPatchRows:
        for(int row=0;row<37;++row) {
            const int y=cy-18+row;
            if(y>=0 && y<height) {
                const int address=y*width+x;
                const unsigned words=unsigned(((address&7)+(last-x+1)+7)>>3)&7u;
#ifndef __SYNTHESIS__
                assert(words>=1 && words<=6);
#endif
                std::memcpy(patch+row*6,image+(address>>3),words*8);bytes+=words*8;
            }
        }
    }
    // describe_patch checks both BRIEF coordinates before blur_pixel. Clipped
    // rows and unused trailing words are never sampled, including error cases.
    byte_counts.write(bytes);
}
void compute_patch(const RawBlock& raw_patch,const BlurBlock& blur_patch,
    int width,int height,int level,hls::stream<FeaturePacket>& records,int index,int count) {
#pragma HLS INLINE off
    if(index>=count)return;
    FeaturePacket packet={};
    if(!raw_patch[157] && describe_patch(raw_patch,blur_patch,width,height,level,packet))packet.valid=true;
    records.write(packet);
}
void patch_pipeline(const uint64_t* raw,const uint64_t* blur,
    hls::stream<PointToken>& raw_points,hls::stream<PointToken>& blur_points,
    int count,int width,int height,int level,hls::stream<FeaturePacket>& records,
    hls::stream<uint32_t>& raw_bytes,hls::stream<uint32_t>& blur_bytes,hls::stream<uint32_t>& pipeline_done) {
#pragma HLS INLINE off
PatchPipeline:
    for(int index=0;index<256;++index) {
#pragma HLS DATAFLOW
        RawBlock raw_patch;BlurBlock blur_patch;
#pragma HLS STREAM variable=raw_patch off
#pragma HLS STREAM variable=blur_patch off
#pragma HLS RESOURCE variable=raw_patch core=RAM_T2P_BRAM
#pragma HLS RESOURCE variable=blur_patch core=RAM_T2P_BRAM
        load_raw_patch(raw,raw_points,raw_patch,width,height,index,count,raw_bytes);
        load_blur_patch(blur,blur_points,blur_patch,width,height,index,count,blur_bytes);
        compute_patch(raw_patch,blur_patch,width,height,level,records,index,count);
    }
    pipeline_done.write(1);
}
void sum_patch_bytes(hls::stream<uint32_t>& input,int count,hls::stream<uint32_t>& done) {
#pragma HLS INLINE off
    uint32_t total=0;
SumPatchBytes:
    for(int index=0;index<count;++index) {
#pragma HLS PIPELINE II=1
        total+=input.read();
    }
    done.write(total);
}
