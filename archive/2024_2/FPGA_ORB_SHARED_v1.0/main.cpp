#include "golden.hpp"
#include <filesystem>
#include <iostream>
#include <map>
#include <fstream>
#include <stdexcept>
#include <sstream>
#include <algorithm>
using namespace orb_golden;
int main(int argc,char** argv) try {
    std::map<std::string,std::string> a;
    for(int i=1;i<argc;i+=2) {
        if(i+1==argc || !a.emplace(argv[i],argv[i+1]).second) throw std::runtime_error("Expected unique --option value pairs");
    }
    for(auto& p:a) {
        const std::string allowed=" --input --output --trace --levels --features --scale-num --scale-den --threshold --low-threshold --fixed-fast --grid --blur --angle-bins --box2 --box-half-up --timestamp --role --overlap-left --overlap-right --bundle --sequence --profiles --stats --dual-threshold --grid-top-k --counts-only --packet-only ";
        if(allowed.find(" "+p.first+" ")==std::string::npos) throw std::runtime_error("Unknown option "+p.first);
    }
    if(!a.count("--input")||!a.count("--output")) throw std::runtime_error("Required: --input image.pgm --output packet.orb");
    Config c;
    auto arg=[&](const char* n,int d) { return a.count(n)?std::stoi(a[n]):d; };
    c.levels=arg("--levels",c.levels);c.features=arg("--features",c.features);
    c.scale_num=uint32_t(arg("--scale-num",int(c.scale_num)));c.scale_den=uint32_t(arg("--scale-den",int(c.scale_den)));
    c.threshold=arg("--threshold",c.threshold);c.low_threshold=arg("--low-threshold",c.low_threshold);
    c.fixed_fast=arg("--fixed-fast",1)!=0;c.grid=arg("--grid",1)!=0;c.blur=arg("--blur",1)!=0;
    c.angle_bins=arg("--angle-bins",32);c.box2=arg("--box2",0)!=0;c.box_half_up=arg("--box-half-up",0)!=0;
    c.overlap_left=arg("--overlap-left",0);c.overlap_right=arg("--overlap-right",1000);
    c.dual_threshold=arg("--dual-threshold",c.dual_threshold?1:0)!=0;c.grid_top_k=arg("--grid-top-k",1);c.counts_only=arg("--counts-only",0)!=0;
    int role=arg("--role",0);uint64_t ns=a.count("--timestamp")?std::stoull(a["--timestamp"]):0;
    if(role<0||role>1) throw std::runtime_error("Invalid role");
    if(a.count("--sequence")) {
        std::ifstream stamps(a["--sequence"]);if(!stamps)throw std::runtime_error("Cannot read timestamp list");
        int normal=c.features;uint64_t stamp=0,previous_stamp=0;size_t frames=0;
        std::vector<std::string> profiles;std::stringstream names(a.count("--profiles")?a["--profiles"]:"L1,L2,L4,L8,V0");std::string name;
        while(std::getline(names,name,',')) {
            if(name!="L1"&&name!="L2"&&name!="L4"&&name!="L8"&&name!="V0"&&name!="L8_T2"&&name!="L4_T2")throw std::runtime_error("Unknown profile");
            if(std::find(profiles.begin(),profiles.end(),name)!=profiles.end())throw std::runtime_error("Duplicate profile");profiles.push_back(name);
        }
        if(profiles.empty())throw std::runtime_error("Empty profiles");
        std::ofstream stats;if(a.count("--stats")){stats.open(a["--stats"]);if(!stats)throw std::runtime_error("Cannot open stats");stats<<"timestamp_ns,variant,role,level,width,height,budget,selected,candidates,selected_high,selected_low,cell_side,cell_cols,cell_rows,empty_cells,fallback_cells\n";}
        while(stamps>>stamp) {
            if(frames&&stamp<=previous_stamp)throw std::runtime_error("Non-increasing timestamps");
            Image frame=read_pgm(a["--input"]+"/"+std::to_string(stamp)+".pgm");Result prepared;
            for(const std::string& variant:profiles)for(int sequence_role:{0,1}) {
                Config profile=c;profile.features=normal*(sequence_role?5:1);
                profile.levels=variant=="V0"?8:std::stoi(variant.substr(1));profile.dual_threshold=variant.find("_T2")!=std::string::npos;
                if(variant=="V0"){profile.fixed_fast=false;profile.grid=false;profile.angle_bins=0;}
                Result next=extract(frame,profile,prepared.layers.empty()?nullptr:&prepared);
                std::string directory=a["--output"]+"/"+variant+(sequence_role?"/initialization":"/normal");
                std::filesystem::create_directories(directory);
                if(!profile.counts_only)write_packet(directory+"/"+std::to_string(stamp)+".orb",next,profile,frame.width,frame.height,stamp,sequence_role);
                if(stats.is_open())for(size_t level=0;level<next.layers.size();++level) {
                    const auto& l=next.layers[level];const auto strong=std::count_if(l.selected.begin(),l.selected.end(),[&](const Point& p){return p.score>=profile.threshold;});
                    stats<<stamp<<','<<variant<<','<<sequence_role<<','<<level<<','<<l.image.width<<','<<l.image.height<<','<<l.budget<<','<<l.selected.size()<<','<<l.candidates.size()<<','<<strong<<','<<(l.selected.size()-size_t(strong))<<','<<l.cell_side<<','<<l.cell_cols<<','<<l.cell_rows<<','<<l.empty_cells<<','<<l.fallback_cells<<'\n';
                }
                prepared=std::move(next);
            }
            previous_stamp=stamp;++frames;
            if(stats.is_open()){stats.flush();if(!stats)throw std::runtime_error("Stats write failed");}
            std::cout<<"GOLDEN_SEQUENCE_PROGRESS frames="<<frames<<" timestamp_ns="<<stamp<<std::endl;
        }
        if(!stamps.eof()||frames==0)throw std::runtime_error("Invalid timestamp list");
        std::cout<<"GOLDEN_SEQUENCE_PASS frames="<<frames<<std::endl;return 0;
    }
    Image im=read_pgm(a["--input"]);
    if(arg("--bundle",0)) {
        int normal=c.features;Result previous;
        for(int levels:{1,2,4,8})for(int bundle_role:{0,1}) {
            c.levels=levels;c.features=normal*(bundle_role?5:1);
            Result next=extract(im,c,previous.layers.empty()?nullptr:&previous);
            std::string path=a["--output"]+"/L"+std::to_string(levels)+(bundle_role?"_initialization":"_normal");
            std::filesystem::create_directories(path);
            write_packet(path+"/packet.orb",next,c,im.width,im.height,ns,bundle_role);
            if(!arg("--packet-only",0))write_trace(path+"/trace",next,c);previous=std::move(next);
        }
        std::cout<<"GOLDEN_BUNDLE_PASS\n";return 0;
    }
    Result r=extract(im,c);
    write_packet(a["--output"],r,c,im.width,im.height,ns,role);
    if(a.count("--trace"))write_trace(a["--trace"],r,c);
    std::cout<<"GOLDEN_PASS points="<<r.ordered.size()<<" levels="<<c.levels<<" role="<<role<<'\n';
    return 0;
} catch(const std::exception& e) { std::cerr<<"GOLDEN_FAILED "<<e.what()<<'\n';return 2; }
