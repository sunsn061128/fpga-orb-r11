/* New ORB60 100 MHz PS application. GPL-3.0-or-later reference semantics.
 * Uses only PS standalone services; never calls legacy three-core drivers.
 * Golden at 0x06000000, mailbox at 0x01000000, buffers at 0x03000000.
 * Input copy/initial output preparation excluded; cache/config/wait included.
 */
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "xfastv2_layer2_hls_hw.h"
#include "xfastv2_descriptor_hls_hw.h"
#ifdef ORB60_HOST
#include "mock.h"
#else
#include "xil_cache.h"
#include "xil_io.h"
#include "xtime_l.h"
#define MEM(a) ((uint8_t *)(uintptr_t)(a))
#endif
#define LREG(n) XFASTV2_LAYER2_HLS_CONTROL_ADDR_##n
#define DREG(n) XFASTV2_DESCRIPTOR_HLS_CONTROL_ADDR_##n
#define LBASE 0x43C00000u
#define DBASE 0x43C10000u
#define MAIL 0x01000000u
#define GOLD 0x06000000u
#define PORT 691200u
#define CAP 1000u
#define FEATURES 0x03600000u
static const uint32_t raw_slot[3]={0x03000000u,0x03100000u,0x03200000u};
static const uint32_t blur_slot[2]={0x03300000u,0x03400000u};
static uint32_t *mail;
static const uint32_t *gold;
static uint32_t mode,frames,count[8],offset[8];
static uint64_t layer_cycles[8],desc_cycles[8],layer_us[8],desc_us[8];
static uint64_t verification,cache_ticks,config_ticks,started[2],frame_begin;
/** @brief Read the ARM global timer. @return Native half-CPU-clock ticks. */
static uint64_t now(void) {XTime t;XTime_GetTime(&t);return (uint64_t)t;}
/** @brief Convert ticks to microseconds without floating point. */
static uint32_t us(uint64_t t) {return (uint32_t)(t*1000000ull/COUNTS_PER_SECOND);}
/** @brief Return one validated level's independent CPU metadata. */
static const uint32_t *row(uint32_t l) {return gold+16+12*l;}
/** @brief Cache-align, maintain and order a PS-owned range. @param invalidate 1 after PL completion. */
static void cache(uint32_t a,uint32_t n,int invalidate) {
    uint64_t t=now(); uint32_t b=a&~31u, size=((a+n+31u)&~31u)-b;
    if(invalidate) Xil_DCacheInvalidateRange(b,size); else Xil_DCacheFlushRange(b,size);
    cache_ticks+=now()-t;
}
/** @brief Per-layer non-reused point storage. */
static uint32_t points(uint32_t l) {return 0x03500000u+0x4000u*l;}
/** @brief Return summary or hardware statistics address, all 64-byte aligned. */
static uint32_t summary(uint32_t l) {return 0x03580000u+0x100u*l;}
static uint32_t lstats(uint32_t l) {return 0x03590000u+0x100u*l;}
static uint32_t dstats(uint32_t l) {return 0x035a0000u+0x100u*l;}
/** @brief Write the generated 32-bit MMIO register and retain ordering in BSP I/O. */
static void reg(uint32_t base,uint32_t off,uint32_t value) {Xil_Out32(base+off,value);}
/** @brief Compare actual bytes with independent expected data, recording first mismatch. */
static int exact(uint32_t actual,const uint8_t *expected,uint32_t n,uint32_t tag) {
    uint32_t i;const uint8_t *p=MEM(actual);uint64_t t=now();
    for(i=0;i<n;++i) if(p[i]!=expected[i]) {
        mail[8]=tag;mail[9]=i;mail[76]=p[i];mail[77]=expected[i];
        verification+=now()-t;return 0;
    }
    verification+=now()-t;return 1;
}
/** @brief Check the untouched full-port tail against the original poison, including odd-width padding. */
static int image_exact(uint32_t addr,uint32_t expected,uint32_t n,uint32_t tag) {
    static const uint8_t poison[8]={0xf0,0x0f,0x5a,0x6a,0xef,0xbe,0xad,0xde};
    uint32_t i;uint64_t t;
    if(!exact(addr,MEM(GOLD+expected),n,tag))return 0;
    t=now();for(i=n;i<PORT;++i)if(MEM(addr)[i]!=poison[i%8]){
        mail[8]=tag;mail[9]=i;verification+=now()-t;return 0;
    }verification+=now()-t;return 1;
}
/** @brief Prepare owned outputs before either engine can write them. No pending descriptor reads overlap these slots. */
static void prepare_layer(uint32_t l) {
    uint32_t i;static const uint8_t poison[8]={0xf0,0x0f,0x5a,0x6a,0xef,0xbe,0xad,0xde};
    if(mode==1) {
        for(i=0;i<PORT;++i)MEM(blur_slot[l%2])[i]=poison[i%8];
        if(l<7)for(i=0;i<PORT;++i)MEM(raw_slot[(l+1)%3])[i]=poison[i%8];
        memset(MEM(points(l)),0xa5,16000);
        cache(blur_slot[l%2],PORT,0);
        if(l<7)cache(raw_slot[(l+1)%3],PORT,0);
        cache(points(l),16000,0);
    }
    memset(MEM(summary(l)),0,64);memset(MEM(lstats(l)),0,64);
    memset(MEM(dstats(l)),0,64);cache(summary(l),64,0);cache(lstats(l),64,0);cache(dstats(l),64,0);
}
/** @brief Configure/start image engine using the new generated ABI, not legacy offsets. */
static void start_layer(uint32_t l) {
    uint64_t t=now();mail[8]=0x100+l;
    reg(LBASE,LREG(IMAGE_R_DATA),raw_slot[l%3]);reg(LBASE,LREG(BLURRED_DATA),blur_slot[l%2]);
    reg(LBASE,LREG(RESIZED_DATA),raw_slot[(l+1)%3]);reg(LBASE,LREG(SELECTED_DATA),points(l));
    reg(LBASE,LREG(RESULT_DATA),summary(l));reg(LBASE,LREG(STATS_DATA),lstats(l));
    reg(LBASE,LREG(LEVEL_DATA),l);reg(LBASE,LREG(BUDGET_DATA),row(l)[11]);
    reg(LBASE,LREG(HIGH_THRESHOLD_DATA),20);reg(LBASE,LREG(LOW_THRESHOLD_DATA),7);
    reg(LBASE,LREG(DUAL_THRESHOLD_DATA),1);reg(LBASE,LREG(SELECTED_CAPACITY_DATA),CAP);
    config_ticks+=now()-t;started[0]=now();reg(LBASE,LREG(AP_CTRL),1);
}
/** @brief Start exact PL direction/BRIEF, appending to the complete capacity-1000 frame. */
static void start_desc(uint32_t l) {
    uint64_t t=now();mail[8]=0x200+l;
    reg(DBASE,DREG(RAW_DATA),raw_slot[l%3]);reg(DBASE,DREG(BLURRED_DATA),blur_slot[l%2]);
    reg(DBASE,DREG(POINTS_DATA),points(l));reg(DBASE,DREG(FEATURES_DATA),FEATURES);
    reg(DBASE,DREG(STATS_DATA),dstats(l));reg(DBASE,DREG(WIDTH_DATA),row(l)[0]);
    reg(DBASE,DREG(HEIGHT_DATA),row(l)[1]);reg(DBASE,DREG(COUNT_DATA),count[l]);
    reg(DBASE,DREG(LEVEL_DATA),l);reg(DBASE,DREG(FRAME_OFFSET_DATA),offset[l]);reg(DBASE,DREG(CAPACITY_DATA),CAP);
    config_ticks+=now()-t;started[1]=now();reg(DBASE,DREG(AP_CTRL),1);
}
/** @brief Wait for one completion with a one-second hardware watchdog; never restart a timed-out core. */
static int wait_core(uint32_t base,uint32_t which,uint32_t l) {
    while(!(Xil_In32(base)&2u))if(now()-started[which]>COUNTS_PER_SECOND){mail[5]=1;mail[9]=base;return 0;}
    if(Xil_In32(base+0x10)!=0){mail[5]=2;mail[9]=base;return 0;}
    if(which)desc_us[l]+=us(now()-started[1]);else layer_us[l]+=us(now()-started[0]);
    return 1;
}
/** @brief Validate image metadata and exact outputs after all image writes complete. */
static int finish_layer(uint32_t l) {
    const uint32_t *r=row(l);uint32_t *s,*v;uint32_t n=(r[0]*r[1]+7u)&~7u;
    cache(summary(l),64,1);cache(lstats(l),64,1);s=(uint32_t*)MEM(summary(l));v=(uint32_t*)MEM(lstats(l));
    count[l]=s[3];offset[l]=l?offset[l-1]+count[l-1]:0;
    if(s[0]!=0x20000 || s[1] || count[l]>256 || offset[l]+count[l]>CAP ||
       v[0]!=0x600001 || v[1] || v[2]!=l || v[3]!=r[0] || v[4]!=r[1] || !v[11] ||
       v[5]!=n || v[6]!=n || v[7]!=(l<7?((row(l+1)[0]*row(l+1)[1]+7)&~7u):0)) {mail[5]=3;return 0;}
    layer_cycles[l]+=v[11];mail[16]=offset[l]+count[l];mail[17]+=s[2];mail[52+l]=count[l];mail[60+l]=offset[l];
    if(mode==1) {
        cache(blur_slot[l%2],PORT,1);cache(points(l),16000,1);
        if(!image_exact(blur_slot[l%2],r[5],n,0x300+l) || !exact(points(l),MEM(GOLD+r[6]),16000,0x400+l) ||
           !exact(summary(l),MEM(GOLD+r[7]),64,0x500+l))return 0;
        if(l<7){cache(raw_slot[(l+1)%3],PORT,1);if(!image_exact(raw_slot[(l+1)%3],row(l+1)[4],
            (row(l+1)[0]*row(l+1)[1]+7)&~7u,0x600+l))return 0;}
    }
    return 1;
}
/** @brief Validate exact descriptor completion, append prefix and capacity guard. */
static int finish_desc(uint32_t l) {
    uint32_t *s;uint32_t end=(offset[l]+count[l])*64u;cache(dstats(l),64,1);s=(uint32_t*)MEM(dstats(l));
    if(s[0]!=0x600002 || s[1] || s[2]!=l || s[3]!=count[l] || s[4]!=count[l] ||
       s[5]!=offset[l] || s[8]!=count[l]*16 || s[9]!=count[l]*64 || !s[10]){mail[5]=4;return 0;}
    desc_cycles[l]+=s[10];
    if(mode==1) {
        uint32_t i;cache(FEATURES,64032,1);
        if(s[6]!=row(l)[8] || s[7]!=row(l)[9] || !exact(FEATURES,MEM(GOLD+gold[4]),end,0x700+l))return 0;
        for(i=end;i<64032;++i)if(MEM(FEATURES)[i]!=0xc3){mail[8]=0x800+l;mail[9]=i;return 0;}
    }
    return 1;
}
/** @brief Execute one eight-level frame; descriptor(n) overlaps image(n+1). */
static int frame(void) {
    uint32_t l;
    if(mode==1){memset(MEM(FEATURES),0xc3,64032);cache(FEATURES,64032,0);}
    for(l=0;l<8;++l)prepare_layer(l); /* Before any concurrent ownership. */
    /* Preparing all levels shares slots: final poison is the same. Load input
     * exactly once AFTER preparation, never duplicate the input DDR copy. */
    memcpy(MEM(raw_slot[0]),MEM(GOLD+row(0)[4]),PORT);cache(raw_slot[0],PORT,0);
    frame_begin=now();start_layer(0);
    if(!wait_core(LBASE,0,0) || !finish_layer(0))return 0;
    for(l=0;l<8;++l) {
        /* Verified tail poison must be restored for each newly owned slot.
         * Do this BEFORE launching desc(l) so CPU writes cannot race DMA. */
        if(l<7)prepare_layer(l+1);
        start_desc(l);if(l<7)start_layer(l+1);
        if(!wait_core(DBASE,1,l))return 0;
        if(l<7 && !wait_core(LBASE,0,l+1))return 0;
        if(!finish_desc(l) || (l<7 && !finish_layer(l+1)))return 0;
    }
    cache(FEATURES,64032,1);
    return 1;
}
/** @brief Run the explicit DDR command and publish a stable terminal mailbox; never print over DCC. */
int main(void) {
    uint32_t i, f;uint64_t total=0,wall,verify_start,min=UINT64_MAX,max=0;uint32_t requested,command;
    mail=(uint32_t*)MEM(MAIL);gold=(const uint32_t*)MEM(GOLD);
#ifndef ORB60_HOST
    Xil_DCacheDisable();Xil_DCacheEnable();
#endif
    requested=mail[1];command=mail[7];mode=command;frames=requested;
    if(mail[0]!=0x60c00001 || requested<1 || requested>1000 || (mode!=1 && mode!=2) ||
       gold[0]!=0x60b30001 || gold[1]!=1 || gold[2]>0x00800000 || gold[3]>1000 || gold[6]!=8){return 1;}
    for(i=2;i<128;++i)mail[i]=0;
    mail[2]=0x60c00002;mail[3]=1;mail[4]=1;mail[7]=command;
    wall=now();
    for(f=0;f<frames;++f) {
        uint64_t elapsed;verify_start=verification;
        if(!frame()){if(!mail[5])mail[5]=5;mail[4]=3;goto done;}
        elapsed=now()-frame_begin-(verification-verify_start);
        total+=elapsed;if(elapsed<min)min=elapsed;if(elapsed>max)max=elapsed;mail[6]=f+1;
    }
    mail[4]=mode==1?2:4;
done:
    mail[10]=mail[6]?us(total/mail[6]):0;mail[11]=us(now()-wall);mail[12]=us(verification);
    mail[13]=min==UINT64_MAX?0:us(min);mail[14]=us(max);mail[15]=mail[10]?1000000000u/mail[10]:0;
    mail[18]=us(cache_ticks);mail[19]=us(config_ticks);
    for(i=0;i<8;++i){uint32_t n=mail[6]?mail[6]:1;mail[20+i]=layer_cycles[i]/n;mail[28+i]=desc_cycles[i]/n;
        mail[36+i]=layer_us[i]/n;mail[44+i]=desc_us[i]/n;}
    cache(MAIL,512,0);
#ifndef ORB60_HOST
    for(;;)__asm__ volatile("wfe");
#endif
    return mail[4]==3?1:0;
}
