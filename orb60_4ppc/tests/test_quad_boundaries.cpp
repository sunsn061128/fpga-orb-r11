#include <cstdint>
#include <iostream>
#include <vector>
#include <stdexcept>
extern "C" int next_test_quad_roundtrip(const uint64_t*,uint64_t*,int,int);
extern "C" int next_test_front4_merge(const uint64_t*,const uint8_t*,int,uint64_t*);
int main() {
    try {
        int cases=0;
        for(int width=1;width<=65;++width) for(int height=1;height<=7;++height) {
            const int n=width*height;std::vector<uint64_t> input((n+7)/8,UINT64_C(0xa5a5a5a5a5a5a5a5));
            std::vector<uint64_t> actual((n+7)/8+4,UINT64_C(0xdeadbeefdeadbeef)),wanted=actual;
            for(int i=0;i<(n+7)/8;++i)wanted[i]=0;
            for(int i=0;i<n;++i) {
                const uint64_t v=uint8_t(i*71+width*29+height*13);const int shift=(i&7)*8;
                input[i/8]=(input[i/8]&~(UINT64_C(255)<<shift))|(v<<shift);wanted[i/8]|=v<<shift;
            }
            if(next_test_quad_roundtrip(input.data(),actual.data(),width,height) || actual!=wanted)
                throw std::runtime_error("quad flat packing width="+std::to_string(width)+" height="+std::to_string(height));
            ++cases;
        }
        for(int pattern=0;pattern<17;++pattern) {
            const int n=4096;std::vector<uint64_t> lanes(4*n),actual(4*n),wanted;
            std::vector<uint8_t> masks(n);
            for(int i=0;i<n;++i) {
                masks[i]=pattern<16 ? pattern : (i*13+i/7)&15;
                for(int lane=0;lane<4;++lane) {lanes[4*i+lane]=4*i+lane+1;if(masks[i]&(1<<lane))wanted.push_back(lanes[4*i+lane]);}
            }
            actual.resize(next_test_front4_merge(lanes.data(),masks.data(),n,actual.data()));
            if(actual!=wanted)throw std::runtime_error("quad candidate raster merge mask="+std::to_string(pattern));
        }
        std::cout<<"QUAD_BOUNDARIES_PASS packing="<<cases<<" merge=17x4096 masks=0..15,mixed\n";return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
