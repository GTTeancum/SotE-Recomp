#include <string.h>
#include "runtime.h"
#include "recomp_hooks.h"
extern volatile uint32_t sote_vi_count;
void xb_unsupported(uint32_t id) __attribute__((noreturn));
/* Production life-loss and timeline safeguards from src/main.cpp. Optional
   desktop teleport/health/forced-story diagnostics are not enabled here. */
typedef struct {
    int active,event,normal_since,last_event,last_vi;
    uint32_t source,object,controller;
} life_state;
static life_state life={.normal_since=-1,.last_event=-1,.last_vi=-10000};
volatile uint32_t sote_player_status[8]; /* calls, object, controller, action, event, normalized, committed, suppressed */
volatile uint32_t sote_player_checks;
static int level(int event) {
    if(event==2 || event==3)return 0;
    if(event==4)return 1;if(event==6)return 2;
    if(event>=7 && event<=10)return 3;if(event>=11 && event<=15)return 4;
    if(event==16 || event==17)return 5;if(event>=18 && event<=20)return 6;
    if(event==24 || event==25)return 7;if(event==26 || event==27)return 8;
    if(event>=28 && event<=30)return 9;return -1;
}
static void enter(life_state* s,int vi,int event,uint32_t object,uint32_t controller,uint32_t action) {
    if(!s->active || s->controller!=controller)return;
    if(level(event)<0 || level(event)!=level(s->event))s->active=0;
    else if(event!=s->event){s->object=object;s->event=event;s->normal_since=-1;}
    if(!s->active || s->object!=object)return;
    if(action>=4 && action<=6) {
        if(s->normal_since>=0 && vi-s->normal_since>=1800)s->active=0;
        else s->normal_since=-1;
    } else if(s->normal_since<0)s->normal_since=vi;
    else if(vi-s->normal_since>=1800)s->active=0;
}
static int allow(life_state* s,int vi,int event,int lives,uint32_t source,uint32_t object) {
    uint32_t controller=source==0x8008C638u?0x8008BF00u:source==0x8009724Cu?0x80096E14u:0;
    int elapsed=vi-s->last_vi;
    if(lives<0 || (controller && s->active && s->source==source && s->object==object && s->event==event) ||
       (!controller && event==s->last_event && elapsed>=0 && elapsed<1800))return 0;
    s->last_vi=vi;s->last_event=event;
    if(controller){s->active=1;s->source=source;s->object=object;s->controller=controller;s->event=event;s->normal_since=-1;}
    return 1;
}
void sote_enter_player_controller(uint8_t* rdram,uint32_t object,uint32_t controller,uint32_t action) {
    int event=MEM_H(0,(int32_t)0x8013CE0E);
    sote_player_status[0]++;sote_player_status[1]=object;sote_player_status[2]=controller;
    sote_player_status[3]=action;sote_player_status[4]=event;
    enter(&life,(int)sote_vi_count,event,object,controller,action);
    union {uint64_t bits;double value;} delta;
    delta.bits=LD((int32_t)0x8018E998,0);
    /* Comparisons reject NaN/infinity as well as absolute timeline values.
       Unlike the global sanitizer, controller calls cannot consume -1. */
    if(!(delta.value>=0 && delta.value<=0.1)) {
        delta.value=0.02;SD(delta.bits,0,(int32_t)0x8018E998);sote_player_status[5]++;
    }
}
uint32_t sote_allow_life_loss(uint8_t* rdram,uint32_t source,uint32_t object,uint32_t action) {
    (void)action;
    int result=allow(&life,(int)sote_vi_count,MEM_H(0,(int32_t)0x8013CE0E),
        (int32_t)MEM_W(0,(int32_t)0x800E0EB0),source,object);
    sote_player_status[result?6:7]++;return result;
}
void xb_player_selftest(void) {
    life_state s={.normal_since=-1,.last_event=-1,.last_vi=-10000};
#define CHECK(x) do{if(!(x))xb_unsupported(0xE6000001);sote_player_checks++;}while(0)
    CHECK(allow(&s,100,11,3,0x8008C638,0x80100000));
    CHECK(!allow(&s,101,11,2,0x8008C638,0x80100000));
    enter(&s,102,11,0x80100000,0x8008BF00,0);
    enter(&s,200,11,0x80100000,0x8008BF00,4);
    CHECK(s.active && s.normal_since==-1);
    enter(&s,201,11,0x80100000,0x8008BF00,0);
    enter(&s,2001,11,0x80100000,0x8008BF00,4);
    CHECK(!s.active);
    CHECK(allow(&s,2001,11,2,0x8008C638,0x80100000));
    enter(&s,2002,12,0x80110000,0x8008BF00,5);
    CHECK(s.active && s.event==12 && s.object==0x80110000);
    CHECK(!allow(&s,2003,12,1,0x8008C638,0x80110000));
    enter(&s,2004,16,0x80110000,0x8008BF00,0);CHECK(!s.active);
    CHECK(!allow(&s,3000,16,-1,0,0));
    CHECK(allow(&s,3000,16,0,0,0));
    CHECK(!allow(&s,4799,16,0,0,0));
    CHECK(allow(&s,4800,16,0,0,0));
#undef CHECK
}
