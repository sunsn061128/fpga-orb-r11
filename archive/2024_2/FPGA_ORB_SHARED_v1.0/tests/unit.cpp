#include "../golden.hpp"
#include <iostream>
#include <stdexcept>
using namespace orb_golden;
static void checked(bool v,int line) { if(!v)throw std::runtime_error("Unit mismatch line "+std::to_string(line)); }
#define check(value) checked(value,__LINE__)
int main() try {
    Config defaults;
    check(defaults.levels==ORB_DEFAULT_LEVELS&&defaults.scale_num==6&&defaults.scale_den==5);
    check(defaults.dual_threshold&&defaults.threshold==20&&defaults.low_threshold==7);
    check(defaults.grid&&defaults.grid_top_k==1&&defaults.angle_bins==32&&defaults.blur);
    check(round_even(1,2)==0&&round_even(3,2)==2&&round_even(-3,2)==-2&&round_even(-5,2)==-2);
    for(uint64_t i=0;i<10000;++i){auto s=isqrt(i);check(s*s<=i&&(s+1)*(s+1)>i);}
    check(binary32_bits(6,5)==0x3f99999au&&binary32_bits(45,4)==0x41340000u&&binary32_bits(-1,1)==0xbf800000u);
    check(angle_q16(0,0)==0&&angle_q16(1,0)==0&&angle_q16(0,1)==90u*65536u);
    check(angle_q16(-1,0)==180u*65536u&&angle_q16(0,-1)==270u*65536u);
    for(int k=0;k<32;++k){auto center=uint32_t(k*737280);check(quantize(center,32)==k);check(quantize((center+368640)%(360u*65536u),32)==(k+1)%32);}
    for(int n:{1000,5000})for(int levels:{1,2,4,8}) {
        Config c;c.features=n;c.levels=levels;int sum=0;for(auto b:budgets(c))sum+=b;check(sum==n);
    }
    Config c;c.levels=2;c.scale_num=2;c.scale_den=1;check(budgets(c)==std::vector<int>({667,333}));
    for(int fill:{0,255}) {
        Image im(128,96);std::fill(im.pixels.begin(),im.pixels.end(),uint8_t(fill));
        check(gaussian7(im).pixels==im.pixels);check(tiled_candidates(im,c).empty());
        check(extract(im,c).ordered.empty());
    }
    Layer l;std::vector<Point> points={{1,1,20,0},{2,1,20,0},{1,2,20,0},{22,18,30,1}};
    auto chosen=select_grid(points,23,19,17,l);check(chosen.size()==2&&chosen[0].x==1&&chosen[0].y==1);
    // T2 is per selection grid cell, not per FAST scan tile. Equal scores retain raster order.
    Config t;t.dual_threshold=true;Layer weak;
    std::vector<Point> candidates={{1,1,7,0},{2,1,19,0},{1,2,20,0},{12,1,19,1},{13,1,19,1},{22,1,6,2}};
    auto fallback=select_grid(candidates,30,10,3,weak,&t);
    check(fallback.size()==2&&fallback[0].score==20&&fallback[1].x==12);
    check(weak.fallback_cells==1&&weak.empty_cells==1);
    t.grid_top_k=2;Layer two;
    auto top2=select_grid(candidates,30,10,3,two,&t);
    check(top2.size()==3&&top2[0].score==20&&top2[1].x==12&&top2[2].x==13);
    Layer capped;auto cap=select_grid(candidates,30,10,1,capped,&t);check(cap.size()==1&&cap[0].score==20);
    Image ramp(96,96);for(int y=0;y<96;++y)for(int x=0;x<96;++x)ramp.at(x,y)=uint8_t(x);
    auto m=moments(ramp,48,48);check(m.first>0&&m.second==0);
    std::cout<<"INTEGER_UNIT_PASS\n";return 0;
} catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}
