#include "layer4.hpp"
#include "descriptor.hpp"
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
static void require(bool ok,const char* tag,int level) {
    if(!ok)throw std::runtime_error(std::string(tag)+" level="+std::to_string(level));
}
static void image_exact(const std::vector<uint64_t>& actual,const uint8_t* expected,int pixels,const char* tag,int level) {
    const size_t words=(pixels+7)/8;
    require(std::memcmp(actual.data(),expected,words*8)==0,tag,level);
    for(size_t i=words;i<actual.size();++i)require(actual[i]==UINT64_C(0xdeadbeef6a5a0ff0),"image unwritten tail",level);
}
int main(int argc,char** argv) {
    try {
        if(argc!=2)return 2;std::ifstream f(argv[1],std::ios::binary|std::ios::ate);
        const auto size=f.tellg();if(size<448)throw std::runtime_error("Bad golden");f.seekg(0);
        std::vector<uint64_t> golden((size_t(size)+7)/8);
        if(!f.read(reinterpret_cast<char*>(golden.data()),size))throw std::runtime_error("Golden read failed");
        const auto g=reinterpret_cast<const uint32_t*>(golden.data());
        const auto bytes=reinterpret_cast<const uint8_t*>(golden.data());
        if(g[0]!=0x60b30001 || g[1]!=1 || g[2]!=size || g[6]!=8)throw std::runtime_error("Golden schema");
        const uint64_t poison=UINT64_C(0xdeadbeef6a5a0ff0),guard=UINT64_C(0xc3c3c3c3c3c3c3c3);
        std::vector<uint64_t> raw(86400),next(86400,poison),blur(86400,poison),points(2000),features(8002,guard);
        std::memcpy(raw.data(),bytes+g[16+4],691200);int offset=0;
        for(int l=0;l<8;++l) {
            const auto row=g+16+12*l;std::fill(next.begin(),next.end(),poison);std::fill(blur.begin(),blur.end(),poison);
            std::memset(points.data(),0xa5,16000);uint64_t summary[8]={},stats[8]={},ds[8]={};
            require(fastv2_layer4_hls(raw.data(),blur.data(),next.data(),points.data(),summary,stats,l,row[11],20,7,1,1000)==0,"image status",l);
            image_exact(blur,bytes+row[5],row[0]*row[1],"blur/padding",l);
            require(std::memcmp(points.data(),bytes+row[6],16000)==0,"points/order/unused",l);
            require(std::memcmp(summary,bytes+row[7],64)==0,"full summary",l);
            if(l<7) {const auto nr=g+16+12*(l+1);image_exact(next,bytes+nr[4],nr[0]*nr[1],"actual next raw",l);}
            const int count=int(reinterpret_cast<uint32_t*>(summary)[3]);
            require(fastv2_descriptor_hls(raw.data(),blur.data(),points.data(),features.data()+1,ds,row[0],row[1],count,l,offset,1000)==0,"descriptor status",l);
            const auto d=reinterpret_cast<uint32_t*>(ds);
            require(d[4]==unsigned(count) && d[5]==unsigned(offset) && d[6]==row[8] && d[7]==row[9],"descriptor stats",l);
            offset+=count;require(std::memcmp(features.data()+1,bytes+g[4],offset*64)==0,"frame appended features",l);
            for(size_t word=1+offset*8;word<features.size();++word)require(features[word]==guard,"capacity/tail guard",l);
            require(features[0]==guard,"leading guard",l);raw.swap(next);
        }
        require(offset==int(g[3]),"frame count",7);
        std::cout<<"FRAME4_EXACT_PASS input="<<argv[1]<<" features="<<offset<<" layers=8 upstream_actual=1 guards=1\n";return 0;
    }catch(const std::exception& e){std::cerr<<"FRAME4_FAIL "<<e.what()<<'\n';return 1;}
}
