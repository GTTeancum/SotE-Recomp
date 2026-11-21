#include "runtime.h"
#include "recomp_hooks.h"

/* Retail control presets remain authoritative for the current N64-compatible
   pad path. Observe their live action masks; custom Xbox rebinding is pending. */
volatile uint32_t sote_controls_status[6]; /* calls, valid, context, preset, event, updates */
volatile uint32_t sote_controls_masks[48];
void sote_bindings_context(uint8_t* rdram,int context) {
    sote_controls_status[0]++;
    if(context<0 || context>=5)return;
    int preset=MEM_H(0,(int32_t)0x800D252C);
    int event=MEM_H(0,(int32_t)0x8013CE0E);
    if(preset<0 || preset>=8)return;
    uint32_t table=0x800E6808+96*preset;
    if(MEM_HU(6,(int32_t)table)!=0x1000)return;
    /* Shared ship objects also run the inactive controller, as on desktop. */
    if((event==6 || event==29) && context==4)return;
    if(event==30 && context==2)return;
    for(unsigned i=0;i<48;i++)sote_controls_masks[i]=MEM_HU(i*2,(int32_t)table);
    sote_controls_status[1]=1;sote_controls_status[2]=context;
    sote_controls_status[3]=preset;sote_controls_status[4]=event;
    sote_controls_status[5]++;
}

/* Process-local evidence from original vehicle controllers, with no placement,
   health or progression changes. Positions are retained as guest float bits. */
volatile uint32_t sote_vehicle_status[10];
extern volatile uint32_t sote_input_status[8];
/* serial, poll, context, buttons, stick XY, rotation XYZ, turret yaw/pitch,
   camera forward XYZ, story event, object. Values are original guest bits. */
volatile uint32_t sote_vehicle_trace[64][16];
static unsigned vehicle_serial,vehicle_poll,vehicle_buttons,vehicle_x,vehicle_y;
extern volatile uint32_t sote_vi_count;
/* Match the desktop hook: identify the current ship for host features.
   Flight simulation and control remain in the original game routines. */
volatile uint32_t sote_outrider_status[3]; /* object, last VI, calls */
void sote_outrider_object(uint8_t* rdram,uint32_t object) {
    (void)rdram;
    sote_outrider_status[0]=object;sote_outrider_status[1]=sote_vi_count;
    sote_outrider_status[2]++;
}
void sote_diagnostic_vehicle(uint8_t* rdram,uint32_t object,int context) {
    if(object<0x80000000u || object>0x807FFF80u)return;
    sote_vehicle_status[0]++;
    sote_vehicle_status[1]=object;sote_vehicle_status[2]=context;
    sote_vehicle_status[3]=MEM_H(0,(int32_t)0x8013CE0E);
    for(unsigned i=0;i<3;i++) {
        sote_vehicle_status[4+i]=MEM_W(8+i*4,(int32_t)object);
        sote_vehicle_status[7+i]=MEM_W(0x50+i*4,(int32_t)object);
    }
    unsigned event=sote_vehicle_status[3];
    if(((event==6 || event==29) && context==4) || (event==30 && context==2))return;
    unsigned poll=sote_input_status[0],buttons=sote_input_status[1];
    unsigned x=sote_input_status[2],y=sote_input_status[3];
    if(!vehicle_serial || poll-vehicle_poll>=30 || buttons!=vehicle_buttons || x!=vehicle_x || y!=vehicle_y) {
        unsigned serial=++vehicle_serial;volatile uint32_t* r=sote_vehicle_trace[serial%64];
        r[0]=serial;r[1]=poll;r[2]=context;r[3]=buttons;r[4]=x;r[5]=y;
        for(unsigned i=0;i<3;i++)r[6+i]=MEM_W(0x14+i*4,(int32_t)object);
        if(context==2 && object<=0x807ffee4u){r[9]=MEM_W(0x114,(int32_t)object);r[10]=MEM_W(0x118,(int32_t)object);}
        else r[9]=r[10]=0;
        uint32_t camera=MEM_W(0,(int32_t)0x800D08A4);
        for(unsigned i=0;i<3;i++)r[11+i]=(camera>=0x80000000u && camera<=0x807fff80u)?MEM_W(0x24+i*4,(int32_t)camera):0;
        r[14]=event;r[15]=object;
        vehicle_poll=poll;vehicle_buttons=buttons;vehicle_x=x;vehicle_y=y;
    }
}

volatile uint32_t sote_hoth_status[4]; /* calls, initialization calls, action, event */
void sote_diagnostic_hoth_trip(uint8_t* rdram,void* context,int initialize) {
    /* The desktop hook only mutates gameplay under the explicit HOTH_TRIP
       diagnostic environment flag. Normal Xbox runs preserve the retail wave
       and cable logic, recording the call without injecting a completed trip. */
    recomp_context* caller=(recomp_context*)context;
    sote_hoth_status[0]++;
    if(initialize)sote_hoth_status[1]++;
    sote_hoth_status[2]=(uint32_t)caller->r5;
    sote_hoth_status[3]=MEM_H(0,(int32_t)0x8013CE0E);
}
