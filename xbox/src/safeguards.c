#include "runtime.h"
#include "recomp_hooks.h"
void xb_unsupported(uint32_t id) __attribute__((noreturn));
volatile uint32_t sote_audio_exponent_status[2];
volatile uint32_t sote_motion_guard_status[5];
volatile uint32_t sote_safeguard_checks;
void sote_note_audio_negative_exponent(int32_t exponent) {
    /* The translated routine itself performs the reciprocal correction. */
    sote_audio_exponent_status[0]++;sote_audio_exponent_status[1]=(uint32_t)exponent;
}
static int repaired_motion_state(int state,float velocity) {
    return (state==2 || state==3) && velocity==0.0f?0:state;
}
void sote_note_motion_loop_guard(uint8_t* rdram,uint32_t object) {
    /* Called only after the original motion loop exceeds its64-iteration
       bound; finite immediate-keyframe sequences are allowed to finish. */
    if(object<0x80000000u || object>0x807fff54u)xb_unsupported(0xE6000003);
    int state=MEM_H(0xa0,(int32_t)object);
    union {uint32_t bits;float value;} velocity;
    velocity.bits=MEM_W(0xa8,(int32_t)object);
    int repaired=repaired_motion_state(state,velocity.value);
    sote_motion_guard_status[0]++;sote_motion_guard_status[1]=object;
    sote_motion_guard_status[2]=state;sote_motion_guard_status[3]=MEM_H(0xa2,(int32_t)object);
    if(repaired!=state) {
        MEM_H(0xa0,(int32_t)object)=repaired;sote_motion_guard_status[4]++;
    }
}
void xb_safeguards_selftest(void) {
#define CHECK(x) do{if(!(x))xb_unsupported(0xE6000004);sote_safeguard_checks++;}while(0)
    CHECK(repaired_motion_state(0,0)==0);
    CHECK(repaired_motion_state(1,0)==1);
    CHECK(repaired_motion_state(2,0)==0);
    CHECK(repaired_motion_state(3,0)==0);
    CHECK(repaired_motion_state(2,-0.0f)==0);
    CHECK(repaired_motion_state(2,1)==2);
    CHECK(repaired_motion_state(3,-1)==3);
    union {uint32_t bits;float value;} special={.bits=0x7fc00000u};
    CHECK(repaired_motion_state(2,special.value)==2);
#undef CHECK
}
