#include <cassert>
#include <cstring>
#include "librecomp/rsp.hpp"
extern RspExitReason aspMain(uint8_t*,uint32_t);
extern "C" void xb_unsupported(uint32_t) __attribute__((noreturn));
uint8_t dmem[0x1000];
uint16_t rspReciprocals[512],rspInverseSquareRoots[512];
static bool initialized;
/* Ares reciprocal table construction, as used by librecomp/src/rsp.cpp.
   Its permissive copyright notice is preserved in rsp_vu.hpp. Binary search
   computes the identical inverse-square-root table without a long boot loop. */
static void constants() {
    if(initialized)return;
    rspReciprocals[0]=0xffff;
    for(unsigned i=1;i<512;i++)rspReciprocals[i]=(uint16_t)((((1ULL<<34)/(i+512))+1)>>8);
    for(unsigned i=0;i<512;i++) {
        uint64_t a=(i+512)>>((i&1)?1:0),lo=1<<17,hi=1<<19;
        while(lo+1<hi){uint64_t mid=(lo+hi)/2;if(a*mid*mid<(1ULL<<44))lo=mid;else hi=mid;}
        rspInverseSquareRoots[i]=(uint16_t)(lo>>1);
    }
    initialized=true;
}
extern "C" void xb_rsp_audio(uint8_t* rdram,const uint32_t* task) {
    constants();
    if(task[0]!=2 || (task[4]&0x1fffffff)!=0xCF890)xb_unsupported(0xE4000001);
    uint32_t data=task[6]&0x1fffffff;
    if(data>0x800000-0xF80)xb_unsupported(0xE4000002);
    std::memcpy(dmem+0xFC0,task,64);
    dma_rdram_to_dmem(rdram,0,data,0xF80-1);
    if(aspMain(rdram,task[4])!=RspExitReason::Broke)xb_unsupported(0xE4000003);
}
extern "C" unsigned xb_rsp_selftest(uint8_t* rdram) {
    constexpr uint32_t data=0x780000,commands=0x77F000;
    std::memcpy(rdram+data,rdram+0xEE480,0xF80);
    for(unsigned i=0x5B0;i<0x5E0;i++)rdram[(data+i)^3]=0xA5;
    /* Original ACMD 2 clears 16 bytes at DMEM 0x5C0. Follow with ACMD 0.
       Guard bytes prove the real generated vector-store routine's extent. */
    MEM_W(0,(int32_t)(0x80000000u|commands))=0x02000000;
    MEM_W(4,(int32_t)(0x80000000u|commands))=16;
    MEM_W(8,(int32_t)(0x80000000u|commands))=0;
    MEM_W(12,(int32_t)(0x80000000u|commands))=0;
    uint32_t task[16]={};task[0]=2;task[4]=0x800CF890;task[6]=data;
    task[12]=commands;task[13]=16;
    xb_rsp_audio(rdram,task);
    for(unsigned i=0x5B0;i<0x5E0;i++) {
        uint8_t expected=i>=0x5C0&&i<0x5D0?0:0xA5;
        if((uint8_t)RSP_MEM_BU(i,0)!=expected)return 0;
    }
    std::memset(rdram+data,0,0xF80);std::memset(rdram+commands,0,16);
    return 48;
}
