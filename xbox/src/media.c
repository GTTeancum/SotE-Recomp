#include "recomp_hooks.h"
#include "runtime.h"
extern volatile uint32_t sote_input_status[8];
/* Observe native communicator timing/input without advancing its state. */
volatile uint32_t sote_communicator_trace[64][10];
static unsigned comm_serial,last_poll,last_state,last_input;
static unsigned was_a,consume_a,advance;
unsigned xb_dialogue_buttons(uint8_t* rdram,unsigned buttons) {
    unsigned a=buttons&0x8000;
    int state=MEM_H(0,(int32_t)0x800DD2D0);
    unsigned word=MEM_W(0,(int32_t)0x800D07FC);
    int visible=MEM_W(0,(int32_t)0x800D07F0)!=-1 && (int32_t)word>0;
    int dialogue=MEM_H(0,(int32_t)0x8013CE0E)!=2 && MEM_W(0,(int32_t)0x800DD2BC)!=1 &&
        (state==1 || ((state==0 || state==10) && visible));
    if(!a)consume_a=0;
    if(a && !was_a && dialogue){consume_a=1;advance=1;}
    was_a=a;
    return consume_a?(buttons&~0x8000u):buttons;
}
uint32_t sote_take_communicator_advance(void) {
    unsigned pending=advance;advance=0;return pending;
}
void sote_communicator_input_state(uint8_t* rdram) {
    int state=MEM_H(0,(int32_t)0x800DD2D0);
    if(state!=1 && sote_take_communicator_advance() && (state==0 || state==10)){
        MEM_W(0,(int32_t)0x800D07FC)=0;
        MEM_H(0,(int32_t)0x800DD2D0)=0;
    }
}

/* Native media hooks only. Direct SAN integration remains pending. */
volatile struct {uint32_t frames,text,cues,intro_sprites;} sote_media_status;
void sote_pc_voice_frame(uint8_t* rdram){
    sote_media_status.frames++;
    unsigned poll=sote_input_status[0],state=MEM_H(0,(int32_t)0x800DD2D0);
    unsigned input=MEM_W(0,(int32_t)0x801130A8);
    if(MEM_H(0,(int32_t)0x8013CE0E)==4 &&
        (!comm_serial || poll-last_poll>=30 || state!=last_state || input!=last_input)) {
        unsigned serial=++comm_serial;
        volatile uint32_t* row=sote_communicator_trace[serial%64];
        row[0]=serial;row[1]=poll;row[2]=state;
        row[3]=MEM_W(0,(int32_t)0x800DD2CC);
        row[4]=MEM_W(0,(int32_t)0x8018E998);row[5]=MEM_W(0,(int32_t)0x8018E99C);
        row[6]=input;row[7]=MEM_W(0,(int32_t)0x800DD2BC);
        row[8]=MEM_W(0,(int32_t)0x800D07FC);row[9]=sote_input_status[1];
        last_poll=poll;last_state=state;last_input=input;
    }
}
void sote_pc_voice_text(uint8_t* rdram,uint32_t pointer){(void)rdram;sote_media_status.text=pointer;}
void sote_pc_voice_harpoon(uint8_t* rdram,uint32_t source){(void)rdram;(void)source;sote_media_status.cues++;}
int sote_intro_story_sprite(uint8_t* rdram,uint32_t sprite) {
    (void)rdram;(void)sprite;sote_media_status.intro_sprites++;return 0;
}
