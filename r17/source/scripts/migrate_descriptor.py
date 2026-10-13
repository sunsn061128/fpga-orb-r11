"""Generate 2018.3 FIFO ingress with overlapped per-patch PIPO computation."""
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
NEXT=ROOT/'upstream/FPGA_ORB_HLS/fastv2/next'
def main():
    text=(NEXT/'src/descriptor_hls.cpp').read_text()
    # A signed arithmetic shift is floor(value / 2**amount), exactly the
    # portable branch below. Avoid two 64-bit negate/add chains on the HLS
    # CORDIC feedback path; ap_int defines the signed shift in C/RTL alike.
    portable_shift='    return value>=0 ? int64_t(uint64_t(value)>>amount) : -INT64_C(1)-int64_t(uint64_t(-(value+1))>>amount);'
    assert text.count(portable_shift)==1
    text=text.replace(portable_shift,'''#if defined(__SYNTHESIS__) || defined(FASTV2_HLS_CSIM)
    const ap_int<64> bits=value;
    return (bits>>amount).to_int64();
#else
'''+portable_shift+'''
#endif''')
    cordic_loop='CordicIterations:\n    for(int k=0;k<32 && y!=0;++k) {\n#pragma HLS PIPELINE II=1'
    assert text.count(cordic_loop)==1
    text=text.replace(cordic_loop,cordic_loop.replace('II=1','II=2'))
    text=text.replace('#include "next_block_stream.hpp"','// 2018.3: fixed-size word FIFOs replace stream_of_blocks locks.')
    a=text.index('void load_raw(');b=text.index('uint8_t raw_pixel(',a)
    text=text[:a]+'''void load_raw(const uint64_t* image,hls::stream<PointToken>& points,
    hls::stream<uint64_t>& blocks,int count,int width,int height,hls::stream<uint32_t>& done) {
#pragma HLS INLINE off
    uint32_t bytes=0;
    for(int i=0;i<count;++i) {
        const PointToken point=points.read();const bool valid=valid_point(point,width,height);
        const int x=signed32(uint32_t(point.xy))+1,y=signed32(uint32_t(point.xy>>32))+1;
        for(int row=0;row<31;++row) {
            uint64_t buffer[5];unsigned words=0;
            if(valid) {
                const int address=(y+row)*width+x;
                words=unsigned(((address&7)+31+7)>>3)&7u;
#ifndef __SYNTHESIS__
                assert(words>=4 && words<=5);
#endif
                std::memcpy(buffer,image+(address>>3),words*8);bytes+=words*8;
            }
        EmitRawPatchRow:
            for(int word=0;word<5;++word) {
#pragma HLS PIPELINE II=1
                blocks.write(unsigned(word)<words ? buffer[word] : uint64_t(0));
            }
        }
        blocks.write(point.xy);blocks.write(point.score_tile);blocks.write(valid ? 0 : 2);
    }
    done.write(bytes);
}
void load_blur(const uint64_t* image,hls::stream<PointToken>& points,
    hls::stream<uint64_t>& blocks,int count,int width,int height,hls::stream<uint32_t>& done) {
#pragma HLS INLINE off
    uint32_t bytes=0;
    for(int i=0;i<count;++i) {
        const PointToken point=points.read();const bool valid=valid_point(point,width,height);
        const int cx=signed32(uint32_t(point.xy))+16,cy=signed32(uint32_t(point.xy>>32))+16;
        const int x=cx<18 ? 0 : cx-18,last=cx+18>=width ? width-1 : cx+18;
        for(int row=0;row<37;++row) {
            uint64_t buffer[6];unsigned words=0;const int y=cy-18+row;
            if(valid && y>=0 && y<height) {
                const int address=y*width+x;
                words=unsigned(((address&7)+(last-x+1)+7)>>3)&7u;
#ifndef __SYNTHESIS__
                assert(words>=1 && words<=6);
#endif
                std::memcpy(buffer,image+(address>>3),words*8);bytes+=words*8;
            }
        EmitBlurPatchRow:
            for(int word=0;word<6;++word) {
#pragma HLS PIPELINE II=1
                blocks.write(unsigned(word)<words ? buffer[word] : uint64_t(0));
            }
        }
    }
    done.write(bytes);
}
''' +text[b:]
    text=text.replace('hls::stream_of_blocks<RawBlock>','hls::stream<uint64_t>').replace('hls::stream_of_blocks<BlurBlock>','hls::stream<uint64_t>')
    text=text.replace('hls::read_lock<RawBlock> raw_lock(raw);RawBlock& raw_patch=raw_lock;\n        hls::read_lock<BlurBlock> blur_lock(blur);BlurBlock& blur_patch=blur_lock;', '''RawBlock raw_patch;BlurBlock blur_patch;
#pragma HLS RESOURCE variable=raw_patch core=RAM_T2P_BRAM
#pragma HLS RESOURCE variable=blur_patch core=RAM_T2P_BRAM
    CollectRawPatch:
        for(int word=0;word<158;++word) {
#pragma HLS PIPELINE II=1
            raw_patch[word]=raw.read();
        }
    CollectBlurPatch:
        for(int word=0;word<222;++word) {
#pragma HLS PIPELINE II=1
            blur_patch[word]=blur.read();
        }''')
    text=text.replace('#pragma HLS BIND_STORAGE variable=raw_blocks type=ram_t2p impl=bram', '#pragma HLS STREAM variable=raw_blocks depth=316\n#pragma HLS RESOURCE variable=raw_blocks core=FIFO_BRAM')
    text=text.replace('#pragma HLS BIND_STORAGE variable=blur_blocks type=ram_t2p impl=bram', '#pragma HLS STREAM variable=blur_blocks depth=444\n#pragma HLS RESOURCE variable=blur_blocks core=FIFO_BRAM')
    import re
    text=re.sub(r'(?m)^(    )hls::stream<([^>]+)> ([^&\n]+);$',r'\1static hls::stream<\2> \3;',text)
    text=re.sub(r'#pragma HLS AGGREGATE variable=(\w+) compact=bit',r'#pragma HLS DATA_PACK variable=\1',text)
    text=re.sub(r' max_widen_bitwidth=64','',text)
    # DATA_PACK in 2018.3 does not support taking a packed array field address
    # for memcpy. Eight scalar fields preserve exactly the same wire bits.
    text=text.replace('struct FeaturePacket {uint64_t words[8];bool valid;};',
        'struct FeaturePacket {uint64_t w0,w1,w2,w3,w4,w5,w6,w7;bool valid;};')
    for i in range(4):text=text.replace(f'packet.words[{i}]',f'packet.w{i}')
    start=text.index('    for(int k=0;k<4;++k) {',text.index('bool describe_patch'))
    end=text.index('    return valid;',start)
    text=text[:start]+''.join(f'    packet.w{i+4}=fields(descriptor[{2*i}],descriptor[{2*i+1}]);\n' for i in range(4))+text[end:]
    text=text.replace('std::memcpy(features+(offset+completed)*8,packet.words,64);++completed;',
        'uint64_t words[8]={packet.w0,packet.w1,packet.w2,packet.w3,packet.w4,packet.w5,packet.w6,packet.w7};\n#pragma HLS ARRAY_PARTITION variable=words complete\n            std::memcpy(features+(offset+completed)*8,words,64);++completed;')
    text=text.replace('uint64_t words[8];','uint64_t words[8];\n#pragma HLS ARRAY_PARTITION variable=words complete')
    text=text.replace('#pragma HLS STREAM variable=records depth=2',
        '#pragma HLS STREAM variable=records depth=2\n#pragma HLS RESOURCE variable=records core=FIFO_SRL')
    # A loop DATAFLOW region gives each collected patch a two-bank PIPO.
    # Collection of patch n+1 can overlap describe_patch(n). Keep both word
    # FIFOs for finite ingress backpressure; the PIPO owns a bank until all
    # random BRIEF/moment reads complete. No manually shared RAM/feedback.
    start=text.index('void compute_descriptors(')
    end=text.index('void write_features(',start)
    text=text[:start]+'''void collect_raw_patch(hls::stream<uint64_t>& input,RawBlock& patch,int index,int count) {
#pragma HLS INLINE off
    if(index>=count)return;
CollectRawPatch:
    for(int word=0;word<158;++word) {
#pragma HLS PIPELINE II=1
        patch[word]=input.read();
    }
}
void collect_blur_patch(hls::stream<uint64_t>& input,BlurBlock& patch,int index,int count) {
#pragma HLS INLINE off
    if(index>=count)return;
CollectBlurPatch:
    for(int word=0;word<222;++word) {
#pragma HLS PIPELINE II=1
        patch[word]=input.read();
    }
}
void compute_patch(const RawBlock& raw_patch,const BlurBlock& blur_patch,
    int width,int height,int level,hls::stream<FeaturePacket>& records,int index,int count) {
#pragma HLS INLINE off
    if(index>=count)return;
    FeaturePacket packet={};
    if(!raw_patch[157] && describe_patch(raw_patch,blur_patch,width,height,level,packet))
        packet.valid=true;
    records.write(packet);
}
void compute_descriptors(hls::stream<uint64_t>& raw,hls::stream<uint64_t>& blur,
    int count,int width,int height,int level,hls::stream<FeaturePacket>& records,
    hls::stream<uint32_t>& pipeline_done) {
#pragma HLS INLINE off
PatchPipeline:
    for(int i=0;i<256;++i) {
#pragma HLS DATAFLOW
        RawBlock raw_patch;BlurBlock blur_patch;
#pragma HLS STREAM variable=raw_patch off
#pragma HLS STREAM variable=blur_patch off
#pragma HLS RESOURCE variable=raw_patch core=RAM_T2P_BRAM
#pragma HLS RESOURCE variable=blur_patch core=RAM_T2P_BRAM
        collect_raw_patch(raw,raw_patch,i,count);
        collect_blur_patch(blur,blur_patch,i,count);
        compute_patch(raw_patch,blur_patch,width,height,level,records,i,count);
    }
    pipeline_done.write(1);
}
''' +text[end:]
    # The writer owns the sticky error and valid prefix. This removes a status
    # feedback dependency from PatchPipeline while preserving invalid-point
    # completion, byte counts, prefix writes and draining every input token.
    text=text.replace('int count,int offset,hls::stream<uint32_t>& done) {',
        'int count,int offset,hls::stream<uint32_t>& done,hls::stream<int>& status_done) {')
    text=text.replace('    uint32_t completed=0;\nWriteDescriptorRecords:',
        '    uint32_t completed=0;int status=0;\nWriteDescriptorRecords:')
    text=text.replace('        if(packet.valid) {',
        '        if(!packet.valid) status=-2;\n        if(!status) {')
    text=text.replace('    done.write(completed);',
        '    done.write(completed);status_done.write(status);')
    text=text.replace('level,records,compute_done);','level,records);')
    text=text.replace('count,offset,write_done);','count,offset,write_done,compute_done);')
    # 2018.3 loop DATAFLOW requires a constant iteration bound. Inactive slots
    # transfer ownership only and neither read ingress nor emit records. Include
    # the end of this loop in the clock counter, even for count==0.
    text=text.replace('level,records);','level,records,pipeline_done);')
    text=text.replace('    static hls::stream<int> compute_done;',
        '    static hls::stream<int> compute_done;\n    static hls::stream<uint32_t> pipeline_done;')
    text=text.replace('#pragma HLS STREAM variable=compute_done depth=2',
        '#pragma HLS STREAM variable=compute_done depth=2\n#pragma HLS STREAM variable=pipeline_done depth=2')
    text=text.replace('hls::stream<int>& compute_done,hls::stream<Completion>& result)',
        'hls::stream<int>& compute_done,hls::stream<uint32_t>& pipeline_done,hls::stream<Completion>& result)')
    text=text.replace('bool raw=false,blur=false,write=false,compute=false;',
        'bool raw=false,blur=false,write=false,compute=false,pipeline=false;uint32_t pipeline_flag=0;')
    text=text.replace('        ++c.cycles;',
        '        if(!pipeline && pipeline_done.read_nb(pipeline_flag))pipeline=true;\n        ++c.cycles;')
    text=text.replace('compute_done,result);','compute_done,pipeline_done,result);')
    text=text.replace('write_done.empty() && compute_done.empty()',
        'write_done.empty() && compute_done.empty() && pipeline_done.empty()')
    thresholds=','.join(str(i*737280)+'u' for i in range(33))
    text=text.replace('    int lo=0,hi=33;',
        '    static const uint32_t thresholds[33]={'+thresholds+'};\n#pragma HLS ARRAY_PARTITION variable=thresholds complete\n    uint8_t lo=0,hi=33;')
    text=text.replace('const int mid=(lo+hi)>>1;','const uint8_t mid=uint8_t((unsigned(lo)+unsigned(hi))>>1);')
    text=text.replace('uint32_t(mid)*737280u','thresholds[mid]')
    # Check only previously latched completion flags. The post-read exit test
    # makes 2018.3 schedule this timer at II=2 and halve the clock count.
    text=text.replace('        if(raw && blur && write && compute) break;','')
    marker='        if(!raw && raw_done.read_nb(c.raw_bytes)) raw=true;'
    text=text.replace(marker,'        if(raw && blur && write && compute && pipeline)break;\n'+marker)
    # Final transport: DDR producers fill the PIPO banks directly. Preserve the
    # arithmetic/writer/timer transformations above; remove ingress word FIFOs
    # and their serial row emission/collection. The authored header is frozen
    # with every native/HLS job and covered by the existing source manifest.
    start=text.index('void load_raw(');end=text.index('uint8_t raw_pixel(',start)
    text=text[:start]+'#include "descriptor_patch2018.hpp"\n\n'+text[end:]
    start=text.index('void collect_raw_patch(');end=text.index('void write_features(',start)
    text=text[:start]+text[end:]
    start=text.index('void descriptor_dataflow(');end=text.index('void write_stats(',start)
    text=text[:start]+'''void descriptor_dataflow(const uint64_t* raw,const uint64_t* blur,const uint64_t* points,
    uint64_t* features,int width,int height,int count,int level,int offset,hls::stream<Completion>& result) {
#pragma HLS INLINE off
    static hls::stream<PointToken> raw_points,blur_points;
    static hls::stream<FeaturePacket> records;
    static hls::stream<uint32_t> raw_bytes,blur_bytes,raw_done,blur_done,write_done,pipeline_done;
    static hls::stream<int> compute_done;
#pragma HLS STREAM variable=raw_points depth=4
#pragma HLS STREAM variable=blur_points depth=4
#pragma HLS STREAM variable=raw_bytes depth=4
#pragma HLS STREAM variable=blur_bytes depth=4
#pragma HLS STREAM variable=records depth=2
#pragma HLS STREAM variable=raw_done depth=2
#pragma HLS STREAM variable=blur_done depth=2
#pragma HLS STREAM variable=write_done depth=2
#pragma HLS STREAM variable=compute_done depth=2
#pragma HLS STREAM variable=pipeline_done depth=2
#pragma HLS DATA_PACK variable=raw_points
#pragma HLS DATA_PACK variable=blur_points
#pragma HLS DATA_PACK variable=records
#pragma HLS RESOURCE variable=records core=FIFO_SRL
#pragma HLS DATAFLOW
    load_points(points,count,raw_points,blur_points);
    patch_pipeline(raw,blur,raw_points,blur_points,count,width,height,level,records,raw_bytes,blur_bytes,pipeline_done);
    sum_patch_bytes(raw_bytes,count,raw_done);
    sum_patch_bytes(blur_bytes,count,blur_done);
    write_features(records,features,count,offset,write_done,compute_done);
    count_cycles(raw_done,blur_done,write_done,compute_done,pipeline_done,result);
#ifndef __SYNTHESIS__
    assert(raw_points.empty() && blur_points.empty() && raw_bytes.empty() && blur_bytes.empty() && records.empty());
    assert(raw_done.empty() && blur_done.empty() && write_done.empty() && compute_done.empty() && pipeline_done.empty());
#endif
}
''' +text[end:]
    text='// Generated by scripts/migrate_descriptor.py. Original arithmetic unchanged.\n'+text
    (NEXT/'src/descriptor2018_hls.cpp').write_text(text,encoding='utf-8',newline='\n')
    print('DESCRIPTOR_2018_DDR_PIPO_GENERATED direct DDR-to-bank, fixed 256 slots, sticky writer status')
if __name__=='__main__':main()
