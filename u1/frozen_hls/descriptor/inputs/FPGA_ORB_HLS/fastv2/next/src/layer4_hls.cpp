#define ORB60_LAYER_TOP 1
#define ORB60_FRONT2_TOP 1
#include "common2018.hpp"
#include "layer2.hpp"
#include "layer2_axis_rom.hpp"
#include "quad_rom.hpp"
namespace {
#include "image2018_helpers.hpp"
#include "front4_stream.hpp"
#include "layer4_images.hpp"

void fork_quads(hls::stream<uint32_t>& input,hls::stream<uint32_t>& fast,
    hls::stream<uint32_t>& gaussian,hls::stream<uint32_t>& resize,int width,int height) {
#pragma HLS INLINE off
    for(int y=0;y<height;++y) {
    ForkLayerQuads:
        for(int x=0;x<width;x+=4) {
#pragma HLS PIPELINE II=1
            const uint32_t value=input.read();fast.write(value);gaussian.write(value);resize.write(value);
        }
    }
}
void count4_cycles(hls::stream<uint8_t>& fast_done,hls::stream<uint8_t>& blur_done,
    hls::stream<uint8_t>& resize_done,hls::stream<uint64_t>& result) {
#pragma HLS INLINE off
    bool fast=false,blur=false,resize=false;uint8_t status=0,token=0;uint32_t cycles=0;
CountLayer4Cycles:
    for(;;) {
#pragma HLS PIPELINE II=1
        if(fast && blur && resize)break;
        if(!fast && fast_done.read_nb(status))fast=true;
        if(!blur && blur_done.read_nb(token))blur=true;
        if(!resize && resize_done.read_nb(token))resize=true;
        ++cycles;
    }
    result.write((uint64_t(cycles)<<32)|status);
}
void layer4_dataflow(const uint64_t* image,uint64_t* blurred,uint64_t* resized,uint64_t* selected,
    uint64_t* result,const fastv2_profile::Profile& p,int level,int high,int low,int dual,int capacity,
    int dw,int dh,hls::stream<uint64_t>& completion) {
#pragma HLS INLINE off
    const int width=p.width,height=p.height,pixels=width*height;
    const int quad_offset=orb4_rom::kOffsets[level];
    static hls::stream<uint64_t> words,candidates,horizontal,blur_words,resize_words;
    static hls::stream<uint32_t> quads,fast_raw,gaussian_raw,resize_raw,blur_quads,resize_quads;
    static hls::stream<CandidateQuad> candidate_quads;
    static hls::stream<uint8_t> fast_done,blur_done,resize_done;
#pragma HLS STREAM variable=words depth=128
#pragma HLS STREAM variable=quads depth=64
#pragma HLS STREAM variable=fast_raw depth=64
#pragma HLS STREAM variable=gaussian_raw depth=64
#pragma HLS STREAM variable=resize_raw depth=64
#pragma HLS STREAM variable=horizontal depth=64
#pragma HLS STREAM variable=blur_quads depth=64
#pragma HLS STREAM variable=resize_quads depth=64
#pragma HLS STREAM variable=candidate_quads depth=64
#pragma HLS STREAM variable=candidates depth=128
#pragma HLS STREAM variable=blur_words depth=64
#pragma HLS STREAM variable=resize_words depth=64
#pragma HLS STREAM variable=fast_done depth=2
#pragma HLS STREAM variable=blur_done depth=2
#pragma HLS STREAM variable=resize_done depth=2
#pragma HLS DATA_PACK variable=candidate_quads
#pragma HLS RESOURCE variable=words core=FIFO_BRAM
#pragma HLS RESOURCE variable=candidate_quads core=FIFO_SRL
#pragma HLS DATAFLOW
    read_front2_words(image,words,pixels);
    unpack_front4_words(words,quads,width,height);
    fork_quads(quads,fast_raw,gaussian_raw,resize_raw,width,height);
    scan_front4_quads(fast_raw,candidate_quads,p,quad_offset,high,low,dual);
    serialize_front4_quads(candidate_quads,candidates);
    consume_and_select(candidates,selected,result,p,high,dual,capacity,fast_done);
    gaussian4_horizontal(gaussian_raw,horizontal,width,height);
    gaussian4_vertical(horizontal,blur_quads,width,height);
    layer_pack_quads<0>(blur_quads,blur_words,width,height);
    layer_write_words<0>(blur_words,blurred,pixels,blur_done);
    resize4_quads(resize_raw,resize_quads,level,width,height,dw,dh);
    layer_pack_quads<1>(resize_quads,resize_words,dw,dh);
    layer_write_words<1>(resize_words,resized,dw*dh,resize_done);
    count4_cycles(fast_done,blur_done,resize_done,completion);
#ifndef __SYNTHESIS__
    assert(words.empty() && quads.empty() && fast_raw.empty() && gaussian_raw.empty() && resize_raw.empty());
    assert(horizontal.empty() && blur_quads.empty() && resize_quads.empty() && candidate_quads.empty());
    assert(candidates.empty() && blur_words.empty() && resize_words.empty());
    assert(fast_done.empty() && blur_done.empty() && resize_done.empty());
#endif
}
void write4_stats(uint64_t* destination,const orb60::LayerStats& s) {
#pragma HLS INLINE
    uint64_t words[8];
#pragma HLS ARRAY_PARTITION variable=words complete
    words[0]=field_pair(s.version,s.status);words[1]=field_pair(s.level,s.width);
    words[2]=field_pair(s.height,s.input_read_bytes);words[3]=field_pair(s.blur_write_bytes,s.resize_write_bytes);
    words[4]=field_pair(s.image_pairs,s.blur_pairs);words[5]=field_pair(s.resize_pairs,s.dataflow_cycles);
    words[6]=field_pair(s.flags,s.reserved[0]);words[7]=field_pair(s.reserved[1],s.reserved[2]);
    std::memcpy(destination,words,64);
}
}
// Preserve the accepted register/record ABI; flags distinguishes four-lane
// implementation. Legacy pair counters still count logical pairs for PS ABI.
extern "C" int fastv2_layer4_hls(const uint64_t* image,uint64_t* blurred,uint64_t* resized,
    uint64_t* selected,uint64_t* result,uint64_t* stats,int level,int budget,int high_threshold,
    int low_threshold,int dual_threshold,int selected_capacity) {
#pragma HLS INTERFACE m_axi port=image offset=slave bundle=gmem0 depth=86400 max_read_burst_length=64 num_read_outstanding=2
#pragma HLS INTERFACE m_axi port=selected offset=slave bundle=gmem1 depth=2000 max_write_burst_length=16 num_write_outstanding=2
#pragma HLS INTERFACE m_axi port=result offset=slave bundle=gmem1 depth=8 max_write_burst_length=16 num_write_outstanding=2
#pragma HLS INTERFACE m_axi port=stats offset=slave bundle=gmem1 depth=8 max_write_burst_length=16 num_write_outstanding=2
#pragma HLS INTERFACE m_axi port=blurred offset=slave bundle=gmem2 depth=86400 max_write_burst_length=64 num_write_outstanding=2
#pragma HLS INTERFACE m_axi port=resized offset=slave bundle=gmem3 depth=86400 max_write_burst_length=64 num_write_outstanding=2
#pragma HLS INTERFACE s_axilite port=image bundle=control
#pragma HLS INTERFACE s_axilite port=blurred bundle=control
#pragma HLS INTERFACE s_axilite port=resized bundle=control
#pragma HLS INTERFACE s_axilite port=selected bundle=control
#pragma HLS INTERFACE s_axilite port=result bundle=control
#pragma HLS INTERFACE s_axilite port=stats bundle=control
#pragma HLS INTERFACE s_axilite port=level bundle=control
#pragma HLS INTERFACE s_axilite port=budget bundle=control
#pragma HLS INTERFACE s_axilite port=high_threshold bundle=control
#pragma HLS INTERFACE s_axilite port=low_threshold bundle=control
#pragma HLS INTERFACE s_axilite port=dual_threshold bundle=control
#pragma HLS INTERFACE s_axilite port=selected_capacity bundle=control
#pragma HLS INTERFACE s_axilite port=return bundle=control
    orb60::LayerStats s={};s.version=orb60::kLayerVersion;s.level=uint32_t(level);
    const bool invalid=level<0 || level>7 || high_threshold<1 || high_threshold>255 ||
        low_threshold<1 || low_threshold>high_threshold || (dual_threshold!=0 && dual_threshold!=1) || selected_capacity<0;
    if(invalid || budget!=fastv2_profile::kProfiles[level].budget) {
        Result summary;clear_result(summary);summary.status=fastv2::kInvalidArgument;write_summary(result,summary);
        s.status=fastv2::kInvalidArgument;write4_stats(stats,s);return fastv2::kInvalidArgument;
    }
    const auto p=fastv2_profile::kProfiles[level];
    const int dw=level<7 ? fastv2_profile::kProfiles[level+1].width : 0,dh=level<7 ? fastv2_profile::kProfiles[level+1].height : 0;
    hls::stream<uint64_t> completion;
#pragma HLS STREAM variable=completion depth=2
    layer4_dataflow(image,blurred,resized,selected,result,p,level,high_threshold,low_threshold,dual_threshold,selected_capacity,dw,dh,completion);
    const uint64_t outcome=completion.read();const int status=uint8_t(outcome);
    s.status=uint32_t(status);s.width=uint32_t(p.width);s.height=uint32_t(p.height);
    s.input_read_bytes=uint32_t((p.width*p.height+7)/8*8);s.blur_write_bytes=s.input_read_bytes;
    s.resize_write_bytes=uint32_t((dw*dh+7)/8*8);s.image_pairs=uint32_t((p.width+1)/2*p.height);
    s.blur_pairs=s.image_pairs;s.resize_pairs=uint32_t((dw+1)/2*dh);s.dataflow_cycles=uint32_t(outcome>>32);
    s.flags=uint32_t(4|(level<7 ? 1 : 0));write4_stats(stats,s);return status;
}

#if !defined(__SYNTHESIS__) && !defined(FASTV2_HLS_CSIM)
extern "C" int next_test_layer4_image(const uint64_t* image,uint64_t* destination,int level,int resize) {
    if(level<0 || level>(resize ? 6 : 7)) return fastv2::kInvalidArgument;
    const auto p=fastv2_profile::kProfiles[level];hls::stream<uint64_t> words,packed,horizontal;
    hls::stream<uint32_t> raw,result;hls::stream<uint8_t> done;
    read_front2_words(image,words,p.width*p.height);unpack_front4_words(words,raw,p.width,p.height);
    int w=p.width,h=p.height;
    if(resize) {w=fastv2_profile::kProfiles[level+1].width;h=fastv2_profile::kProfiles[level+1].height;
        resize4_quads(raw,result,level,p.width,p.height,w,h);
    } else {gaussian4_horizontal(raw,horizontal,w,h);gaussian4_vertical(horizontal,result,w,h);}
    layer_pack_quads<0>(result,packed,w,h);layer_write_words<0>(packed,destination,w*h,done);
    assert(words.empty() && raw.empty() && horizontal.empty() && result.empty() && packed.empty());
    assert(done.read()==1);return fastv2::kOk;
}
extern "C" int next_test_quad_roundtrip(const uint64_t* image,uint64_t* destination,int width,int height) {
    hls::stream<uint64_t> words,packed;hls::stream<uint32_t> quads;hls::stream<uint8_t> done;
    read_front2_words(image,words,width*height);unpack_front4_words(words,quads,width,height);
    layer_pack_quads<2>(quads,packed,width,height);layer_write_words<2>(packed,destination,width*height,done);
    assert(words.empty() && quads.empty() && packed.empty() && done.read()==1);return 0;
}
extern "C" int next_test_front4_merge(const uint64_t* lanes,const uint8_t* masks,int count,uint64_t* output) {
    hls::stream<CandidateQuad> quads;hls::stream<uint64_t> words;
    for(int i=0;i<count;++i) {CandidateQuad q={};q.mask=masks[i];for(int lane=0;lane<4;++lane)q.lane[lane]=lanes[4*i+lane];quads.write(q);}
    CandidateQuad end={};end.lane[0]=UINT64_C(1)<<63;quads.write(end);serialize_front4_quads(quads,words);
    int n=0;for(;;){const auto w=words.read();if(w>>63)break;output[n++]=w;}
    assert(quads.empty() && words.empty());return n;
}
#endif
