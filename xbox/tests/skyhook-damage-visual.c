/* TEST ONLY. Replaces diagnostics object in separate non-shipping test XBE.
 * Mirrors PC's opted-in damage fixture; no production source is modified. */
#define sote_diagnostic_skyhook_damage production_skyhook_telemetry
#include "../src/diagnostics.c"
#undef sote_diagnostic_skyhook_damage
extern void func_80084528(uint8_t*,recomp_context*);
static uint32_t seen[64],phases[64];
void sote_diagnostic_skyhook_damage(uint8_t* rdram,void* context) {
 production_skyhook_telemetry(rdram,context);
 recomp_context* caller=context;
 uint32_t object=(uint32_t)caller->r4,poll=sote_input_status[0];
 if(MEM_H(0,(int32_t)0x8013CE0E)!=30 || poll<2600 || poll>4400 || object<0x80000000u || object>0x807ffdecu || MEM_W(0,(int32_t)object)!=0x50524254u)return;
 unsigned phase=(poll-2600)/400,slot;
 for(slot=0;slot<64 && seen[slot] && seen[slot]!=object;slot++);
 if(slot==64 || (seen[slot] && phases[slot]==phase+1))return;
 uint32_t types=MEM_W(0,(int32_t)0x80112830),shooter=0;
 for(unsigned i=0;i<64;i++){uint32_t t=MEM_W(i*4,(int32_t)types);if(!t)break;if(MEM_W(0,(int32_t)t)==0x53746674 && MEM_W(8,(int32_t)t)==1)shooter=MEM_W(16,(int32_t)t);}
 if(!shooter)return;
 uint32_t sp=((uint32_t)caller->r29-0x100u)&~15u;if(sp<0x80002000u || sp>0x807fff00u)return;
 seen[slot]=object;phases[slot]=phase+1;
 recomp_context ctx=*caller;
 ctx.f_odd=ctx.mips3_float_mode?&ctx.f1.u32l:&ctx.f0.u32h;
 ctx.r29=(int32_t)sp;
 for(unsigned off=0x40;off<0xa0;off+=4)MEM_W(off,(int32_t)sp)=0;
 MEM_W(0x40,(int32_t)sp)=phase?0x70726f78:0x73686f74;
 MEM_W(0x44,(int32_t)sp)=shooter;MEM_W(0x48,(int32_t)sp)=0x40;MEM_W(0x94,(int32_t)sp)=0x447a0000;
 ctx.r4=(int32_t)object;ctx.r5=(int32_t)(sp+0x40);ctx.r6=(int32_t)(sp+0x80);
 func_80084528(rdram,&ctx);
 sote_diagnostic_status[5]++;sote_diagnostic_status[6]=MEM_H(0,(int32_t)0x8018E452);sote_diagnostic_status[7]=MEM_W(0,(int32_t)0x800E1B24);
 if(!MEM_H(0,(int32_t)0x8018E452) && (uint32_t)MEM_W(0,(int32_t)0x800E1B24)>0 && (uint32_t)MEM_W(0,(int32_t)0x800E1B24)<0x7f800000u){MEM_W(0x50,(int32_t)shooter)=0x461c4000;MEM_W(8,(int32_t)shooter)=0x461c4000;}
}
