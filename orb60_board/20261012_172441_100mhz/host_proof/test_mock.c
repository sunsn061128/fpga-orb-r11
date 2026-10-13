/* Controller tests only: a mock never proves DUT math, cache hardware or FPS. */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#define ORB60_HOST 1
#define main orb_entry
#include "board.c"
#undef main
static uint8_t ram[0x700000],mail_ram[512],gold_ram[0x800000];
static uint32_t regs[2][32],pending[2],delay[2],fault;
static uint64_t timer;
static unsigned launches[2],simultaneous;
/** @brief Resolve only the documented DDR/mailbox ranges; reject wild writes. */
uint8_t *orb_memory(uint32_t a) {
    if(a>=MAIL && a<MAIL+512)return mail_ram+a-MAIL;
    if(a>=GOLD && a<GOLD+sizeof(gold_ram))return gold_ram+a-GOLD;
    assert(a>=0x03000000 && a<0x03700000);return ram+a-0x03000000;
}
/** @brief Supply deterministic clock ticks, including an injected watchdog failure. */
void XTime_GetTime(XTime *t){timer+=fault==4?100000000ull:100;*t=timer;}
/** @brief Record generated MMIO fields; reject reentry and verify adjacent-level ownership. */
void Xil_Out32(uint32_t a,uint32_t v) {
    unsigned c=a>=DBASE,off=(a-(c?DBASE:LBASE))/4;assert(off<32);regs[c][off]=v;
    if(off==0 && v==1){assert(!pending[c]);pending[c]=1;delay[c]=3;++launches[c];
        if(pending[0]&&pending[1]){unsigned l=regs[1][DREG(LEVEL_DATA)/4];
            assert(regs[0][LREG(LEVEL_DATA)/4]==l+1);assert(regs[1][DREG(RAW_DATA)/4]!=regs[0][LREG(RESIZED_DATA)/4]);
            assert(regs[1][DREG(BLURRED_DATA)/4]!=regs[0][LREG(BLURRED_DATA)/4]);++simultaneous;}}
}
/** @brief Complete a fake engine with oracle bytes solely to test the PS controller. */
static void complete(unsigned c) {
    unsigned l=regs[c][(c?DREG(LEVEL_DATA):LREG(LEVEL_DATA))/4];const uint32_t *r=row(l);uint32_t *s;
    if(!c){unsigned n=(r[0]*r[1]+7)&~7u;
        assert(regs[0][LREG(BUDGET_DATA)/4]==r[11]);assert(regs[0][LREG(SELECTED_CAPACITY_DATA)/4]==1000);
        assert(!memcmp(MEM(regs[0][LREG(IMAGE_R_DATA)/4]),MEM(GOLD+r[4]),r[0]*r[1]));
        memcpy(MEM(regs[0][LREG(BLURRED_DATA)/4]),MEM(GOLD+r[5]),n);
        if(l<7)memcpy(MEM(regs[0][LREG(RESIZED_DATA)/4]),MEM(GOLD+row(l+1)[4]),(row(l+1)[0]*row(l+1)[1]+7)&~7u);
        memcpy(MEM(points(l)),MEM(GOLD+r[6]),r[2]*16);memcpy(MEM(summary(l)),MEM(GOLD+r[7]),64);
        s=(uint32_t*)MEM(lstats(l));memset(s,0,64);s[0]=0x600001;s[2]=l;s[3]=r[0];s[4]=r[1];s[5]=s[6]=n;
        s[7]=l<7?(row(l+1)[0]*row(l+1)[1]+7)&~7u:0;s[11]=1234;
        if(fault==1)((uint32_t*)MEM(summary(l)))[3]=257;
    }else{unsigned n=regs[1][DREG(COUNT_DATA)/4],o=regs[1][DREG(FRAME_OFFSET_DATA)/4];
        assert(n==r[2] && o==r[3] && o+n<=1000);
        memcpy(MEM(FEATURES+64*o),MEM(GOLD+gold[4]+64*o),64*n);
        s=(uint32_t*)MEM(dstats(l));memset(s,0,64);s[0]=0x600002;s[2]=l;s[3]=s[4]=n;s[5]=o;s[6]=r[8];s[7]=r[9];s[8]=16*n;s[9]=64*n;s[10]=4321;
        if(fault==2)MEM(n?FEATURES+64*o:FEATURES+64000)[0]^=1;
    }pending[c]=0;
}
/** @brief Poll the fake control register, with delayed done and error injection. */
uint32_t Xil_In32(uint32_t a) {
    unsigned c=a>=DBASE,off=(a-(c?DBASE:LBASE))/4;
    if(off==0){if(pending[c] && fault!=4 && --delay[c]==0)complete(c);return pending[c]?0:2;}
    assert(off==4);return fault==3?1:0;
}
/** @brief Cache API counting boundary; hardware coherency is not modeled. */
void Xil_DCacheFlushRange(uint32_t a,uint32_t n){assert(!(a&31) && !(n&31));}
void Xil_DCacheInvalidateRange(uint32_t a,uint32_t n){assert(!(a&31) && !(n&31));}
/** @brief Reset test state without modifying the accepted golden bytes. */
static void reset(unsigned m,unsigned f,unsigned iterations) {
    memset(ram,0,sizeof(ram));memset(mail_ram,0,sizeof(mail_ram));memset(regs,0,sizeof(regs));
    memset(layer_cycles,0,sizeof(layer_cycles));memset(desc_cycles,0,sizeof(desc_cycles));memset(layer_us,0,sizeof(layer_us));memset(desc_us,0,sizeof(desc_us));
    verification=cache_ticks=config_ticks=timer=0;pending[0]=pending[1]=launches[0]=launches[1]=simultaneous=0;fault=f;
    ((uint32_t*)mail_ram)[0]=0x60c00001;((uint32_t*)mail_ram)[1]=iterations;((uint32_t*)mail_ram)[7]=m;
}
/** @brief Check nominal overlap, exact/Production separation and bounded failures. */
int main(int argc,char **argv) {
    FILE *file;assert(argc==2);file=fopen(argv[1],"rb");assert(file);assert(fread(gold_ram,1,sizeof(gold_ram),file)>4096);fclose(file);
    reset(1,0,1);assert(orb_entry()==0 && mail[4]==2 && mail[6]==1 && simultaneous==7 && launches[0]==8 && launches[1]==8);
    reset(2,0,100);assert(orb_entry()==0 && mail[4]==4 && mail[6]==100 && simultaneous==700);
    for(unsigned f=1;f<=4;++f){reset(1,f,1);assert(orb_entry()==1 && mail[4]==3 && !mail[6]);}
    reset(3,0,1);assert(orb_entry()==1);reset(1,0,0);assert(orb_entry()==1);
    puts("ORB60_PS_MOCK_PASS nominal=1 production_frames=100 overlap=7 faults=4 invalid=2 NOT_HARDWARE=1");return 0;
}
