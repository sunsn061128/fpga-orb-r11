#include "descriptor.hpp"
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
static void require(bool ok,const char* tag,int l) {if(!ok)throw std::runtime_error(std::string(tag)+" level="+std::to_string(l));}
int main(int argc,char** argv) {
    try {
        if(argc!=2)return 2;const char* labels[]={"normal","noise","zero"};int transactions=0,total=0;
        for(int frame=0;frame<3;++frame) {
            const std::string name=std::string(argv[1])+"/"+labels[frame]+"/golden.bin";
            std::ifstream f(name.c_str(),std::ios::binary|std::ios::ate);const auto size=f.tellg();
            if(size<448)throw std::runtime_error("golden input missing");f.seekg(0);
            std::vector<uint64_t> golden((size_t(size)+7)/8);f.read(reinterpret_cast<char*>(golden.data()),size);
            const auto g=reinterpret_cast<const uint32_t*>(golden.data());const auto bytes=reinterpret_cast<const uint8_t*>(golden.data());
            const uint64_t guard=UINT64_C(0xc3c3c3c3c3c3c3c3);
            std::vector<uint64_t> raw(86400),blur(86400),points(512),features(8000,guard);
            int offset=0;
            for(int l=0;l<8;++l) {
                const auto row=g+16+12*l;const size_t n=(row[0]*row[1]+7)/8*8;
                std::memcpy(raw.data(),bytes+row[4],n);std::memcpy(blur.data(),bytes+row[5],n);
                std::memcpy(points.data(),bytes+row[6],4096);uint64_t stats[8]={};
                require(fastv2_descriptor_hls(raw.data(),blur.data(),points.data(),features.data(),stats,row[0],row[1],row[2],l,offset,1000)==0,"status",l);
                offset+=row[2];const auto s=reinterpret_cast<const uint32_t*>(stats);
                require(s[0]==orb60::kDescriptorVersion && !s[1] && s[2]==unsigned(l) && s[3]==row[2] && s[4]==row[2] &&
                    s[6]==row[8] && s[7]==row[9] && s[8]==row[2]*16 && s[9]==row[2]*64 && s[10]>0,"stats",l);
                require(std::memcmp(features.data(),bytes+g[4],offset*64)==0,"appended exact",l);
                for(int word=offset*8;word<8000;++word)require(features[word]==guard,"tail guard",l);
                ++transactions;
                std::cout<<"DESCRIPTOR2018_CASE_PASS frame="<<frame<<" level="<<l<<" count="<<row[2]<<" cycles="<<s[10]<<'\n';
            }
            require(offset==int(g[3]),"frame count",7);total+=offset;
            // Rejection must leave the entire accumulated frame unchanged.
            const auto before=features;uint64_t stats[8]={};
            require(fastv2_descriptor_hls(raw.data(),blur.data(),points.data(),features.data(),stats,960,720,1,0,1000,1000)==-1 && features==before,"capacity rejection",0);
            require(fastv2_descriptor_hls(raw.data(),blur.data(),points.data(),features.data(),stats,960,720,257,0,0,1000)==-1 && features==before,"count rejection",0);
            transactions+=2;
        }
        std::cout<<"DESCRIPTOR2018_STREAM_PASS transactions="<<transactions<<" features="<<total<<'\n';return 0;
    } catch(const std::exception& e) {std::cerr<<"DESCRIPTOR2018_STREAM_FAIL "<<e.what()<<'\n';return 1;}
}
