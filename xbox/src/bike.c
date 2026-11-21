#include "runtime.h"
#include "recomp_hooks.h"
void xb_unsupported(uint32_t id) __attribute__((noreturn));
extern volatile uint32_t sote_vi_count;
/* Calls, object, VI, normalized deltas, life-loss calls/source/lives/stage. */
volatile uint32_t sote_bike_status[8];
volatile uint32_t sote_bike_checks;
static double controller_delta(double value) {
    return value>=0.0 && value<=0.1?value:0.02;
}
uint32_t sote_enter_bike_controller(uint8_t* rdram,uint32_t object) {
    sote_bike_status[0]++;sote_bike_status[1]=object;
    sote_bike_status[2]=sote_vi_count;
    /* Retail 8002A154 selects the player-input path with object+E0 bit0. */
    if(object>=0x80000000u && object<=0x807fff1cu &&
        (MEM_W(0xe0,(int32_t)object)&1))sote_diagnostic_vehicle(rdram,object,3);
    union {uint64_t bits;double value;} delta;
    delta.bits=LD((int32_t)0x8018E998,0);
    double fixed=controller_delta(delta.value);
    if(fixed!=delta.value) {
        delta.value=fixed;SD(delta.bits,0,(int32_t)0x8018E998);
        sote_bike_status[3]++;
    }
    /* Match the production hook: repair invalid timeline values, then
       always execute the original bike controller, never skip its work. */
    return 0;
}
void sote_note_bike_life_loss(uint8_t* rdram,uint32_t source) {
    sote_bike_status[4]++;sote_bike_status[5]=source;
    sote_bike_status[6]=MEM_W(0,(int32_t)0x800E0EB0);
    sote_bike_status[7]=MEM_W(0,(int32_t)0x800DD340);
}
void xb_bike_selftest(void) {
#define CHECK(x) do{if(!(x))xb_unsupported(0xE6000002);sote_bike_checks++;}while(0)
    CHECK(controller_delta(0)==0);
    CHECK(controller_delta(0.02)==0.02);
    CHECK(controller_delta(0.1)==0.1);
    CHECK(controller_delta(-0.01)==0.02);
    CHECK(controller_delta(0.10001)==0.02);
    CHECK(controller_delta(1000)==0.02);
    union {uint64_t bits;double value;} special;
    special.bits=0x7ff0000000000000ULL;CHECK(controller_delta(special.value)==0.02);
    special.bits=0xfff0000000000000ULL;CHECK(controller_delta(special.value)==0.02);
    special.bits=0x7ff8000000000000ULL;CHECK(controller_delta(special.value)==0.02);
#undef CHECK
}
