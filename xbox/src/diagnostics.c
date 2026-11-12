#include "runtime.h"
#include "recomp_hooks.h"
/* Desktop-only forced damage, collision relocation and switch-ray injections
   are opt-in fixtures, not production game services. Record their call sites
   without changing RDRAM or the recompiled register context. Xbox trigger
   testing must explicitly implement and record any future placement fixture. */
volatile uint32_t sote_diagnostic_status[8];
extern volatile uint32_t sote_input_status[8];
/* serial, poll, buttons, stick x/y, position x/y/z, heading, raw object+1F0. */
volatile uint32_t sote_player_trace[64][10];
/* poll, slot, camera pointer, eye xyz, forward xyz, active profile,
   packed options, actor flags. */
volatile uint32_t sote_camera_status[12];
static unsigned player_serial,last_player_poll,last_buttons,last_x,last_y;
void sote_diagnostic_skyhook_damage(uint8_t* rdram,void* context) {
    (void)rdram;(void)context;sote_diagnostic_status[0]++;
}
void sote_diagnostic_palace_ray(uint8_t* rdram,uint32_t object) {
    (void)rdram;sote_diagnostic_status[1]++;sote_diagnostic_status[4]=object;
}
void sote_diagnostic_ord_region(uint8_t* rdram,uint32_t object,uint32_t position,uint32_t after_collision) {
    (void)rdram;sote_diagnostic_status[2]++;sote_diagnostic_status[4]=object;
    sote_diagnostic_status[5]=position;sote_diagnostic_status[6]=after_collision;
}
void sote_diagnostic_active_player(uint8_t* rdram,uint32_t object) {
    sote_diagnostic_status[3]++;sote_diagnostic_status[4]=object;
    sote_diagnostic_status[7]=MEM_H(0,(int32_t)0x8013CE0E);
    if(object<0x80000000u || object>0x807ffe0cu)return;
    uint32_t camera=MEM_W(0,(int32_t)0x800D08A4);
    sote_camera_status[0]=sote_input_status[0];
    sote_camera_status[1]=MEM_H(0,(int32_t)0x8018DDE8);
    sote_camera_status[2]=camera;
    if(camera>=0x80000000u && camera<=0x807fff80u)for(unsigned i=0;i<3;i++) {
        sote_camera_status[3+i]=MEM_W(0x44+i*4,(int32_t)camera);
        sote_camera_status[6+i]=MEM_W(0x24+i*4,(int32_t)camera);
    }
    unsigned profile=MEM_BU(0,(int32_t)0x8018BBFD);
    sote_camera_status[9]=profile;
    if(profile<4) {
        uint32_t p=0x8018BBF8+profile*0x7a;
        sote_camera_status[10]=(MEM_BU(0x1a,(int32_t)p)<<16)|(MEM_BU(0x1b,(int32_t)p)<<8)|MEM_BU(0x1c,(int32_t)p);
    }
    sote_camera_status[11]=MEM_W(0x74,(int32_t)object);
    unsigned poll=sote_input_status[0],buttons=sote_input_status[1];
    unsigned x=sote_input_status[2],y=sote_input_status[3];
    if(!player_serial || poll-last_player_poll>=30 || buttons!=last_buttons || x!=last_x || y!=last_y) {
        unsigned serial=++player_serial;volatile uint32_t* row=sote_player_trace[serial%64];
        row[0]=serial;row[1]=poll;row[2]=buttons;row[3]=x;row[4]=y;
        for(unsigned i=0;i<3;i++)row[5+i]=MEM_W(0x50+i*4,(int32_t)object);
        row[8]=MEM_W(0x14,(int32_t)object);row[9]=MEM_W(0x1f0,(int32_t)object);
        last_player_poll=poll;last_buttons=buttons;last_x=x;last_y=y;
    }
}
