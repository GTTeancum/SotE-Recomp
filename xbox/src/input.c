#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <stdio.h>
#include <string.h>
#include "runtime.h"
void xb_unsupported(uint32_t id) __attribute__((noreturn));
void xb_post_event(unsigned event);
void xb_menu_input(uint8_t* rdram,uint16_t* buttons,int x,int y);
void xb_modern_publish(int connected,float move_x,float move_y,float look_x,float look_y);
unsigned xb_modern_buttons(unsigned buttons);
static SDL_GameController* pad;
static unsigned channels=4;
static int initialized;
static int virtual_controller;
typedef struct {unsigned poll,buttons;int x,y;unsigned wait_communicator;int wait_story;} test_input;
static test_input script[128];
static unsigned script_count,script_index,input_poll;
static unsigned script_delay;
volatile uint32_t sote_input_status[8]; /* polls, buttons, x, y, steps, applied, next, present */
static void load_script(void) {
    FILE* file=fopen("D:\\xbox-input.txt","rb");if(!file)return;
    char line[128];
    while(fgets(line,sizeof(line),file)) {
        if(line[0]=='#' || line[0]=='\n' || line[0]=='\r')continue;
        test_input v={.wait_story=-1};
        if(sscanf(line,"%u %x %d %d %u %d",&v.poll,&v.buttons,&v.x,&v.y,&v.wait_communicator,&v.wait_story)<4 || !v.poll || v.wait_communicator>1 || v.wait_story < -1 || v.wait_story>30 ||
            v.buttons>65535 || v.x < -80 || v.x>80 || v.y < -80 || v.y>80 ||
            script_count==128 || (script_count && v.poll<=script[script_count-1].poll))
            xb_unsupported(0xE1000002);
        script[script_count++]=v;
    }
    fclose(file);sote_input_status[4]=script_count;
}
static int poll(void) {
    if(!initialized) {
        FILE* config=fopen("D:\\xbox-test.cfg","rb");
        if(config){char line[64]={0};fgets(line,sizeof(line),config);fclose(config);
            virtual_controller=strcmp(line,"virtual_controller=1\n")==0;}
        if(virtual_controller)load_script();
        if(SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER)<0)xb_unsupported(0xE1000001);
        initialized=1;
    }
    SDL_PumpEvents();SDL_GameControllerUpdate();
    if(pad && !SDL_GameControllerGetAttached(pad)){SDL_GameControllerClose(pad);pad=NULL;}
    if(!pad)for(int i=0;i<SDL_NumJoysticks();i++)if(SDL_IsGameController(i)) {
        pad=SDL_GameControllerOpen(i);if(pad)break;
    }
    return (pad!=NULL || virtual_controller) && channels!=0;
}
static void status(uint8_t* rdram,uint32_t out,int present) {
    for(unsigned i=0;i<4;i++) {
        MEM_H(i*4,(int32_t)out)=i==0&&present?0x0005:0;
        MEM_B(i*4+2,(int32_t)out)=0;
        MEM_B(i*4+3,(int32_t)out)=i==0&&present?0:8;
    }
}
void osContSetCh_recomp(uint8_t* rdram,recomp_context* ctx) {
    (void)rdram;channels=(uint32_t)ctx->r4&0xFF;if(channels>4)channels=4;ctx->r2=0;
}
void osContInit_recomp(uint8_t* rdram,recomp_context* ctx) {
    int present=poll();MEM_B(0,ctx->r5)=present?1:0;status(rdram,ctx->r6,present);ctx->r2=0;
}
void osContStartQuery_recomp(uint8_t* rdram,recomp_context* ctx) {
    (void)rdram;poll();ctx->r2=0;xb_post_event(5);
}
void osContGetQuery_recomp(uint8_t* rdram,recomp_context* ctx) {
    status(rdram,ctx->r4,poll());
}
void osContStartReadData_recomp(uint8_t* rdram,recomp_context* ctx) {
    (void)rdram;poll();ctx->r2=0;xb_post_event(5);
}
void osContGetReadData_recomp(uint8_t* rdram,recomp_context* ctx) {
    int present=poll();uint32_t out=ctx->r4;
    input_poll++;sote_input_status[0]=input_poll;sote_input_status[7]=present;
    for(unsigned i=0;i<4;i++) {
        MEM_H(i*6,(int32_t)out)=0;MEM_B(i*6+2,(int32_t)out)=0;
        MEM_B(i*6+3,(int32_t)out)=0;MEM_B(i*6+4,(int32_t)out)=i==0&&present?0:8;
    }
    if(!present){xb_modern_publish(0,0,0,0,0);return;}
    if(virtual_controller) {
        /* One poll per tap; neutral on every unspecified poll. No OS/window
           input is sent, and a real controller cannot affect a scripted run. */
        unsigned buttons=0;int x=0,y=0;
        if(script_index<script_count && script[script_index].poll+script_delay==input_poll) {
            test_input* v=&script[script_index];
            /* Optional story condition observes the original loader; it
               never changes progression. Legacy communicator rows use Echo. */
            int expected=v->wait_story>=0?v->wait_story:(v->wait_communicator?4:-1);
            if((expected>=0 && MEM_H(0,(int32_t)0x8013CE0E)!=expected) ||
                (v->wait_communicator && MEM_H(0,(int32_t)0x800DD2D0)!=1)) {
                script_delay++;
            } else {
                script_index++;buttons=v->buttons;x=v->x;y=v->y;
                sote_input_status[5]++;
            }
        }
        xb_modern_publish(1,x/80.0f,y/80.0f,0,0);
        uint16_t filtered=xb_modern_buttons(buttons);xb_menu_input(rdram,&filtered,x,y);buttons=filtered;
        MEM_H(0,(int32_t)out)=buttons;MEM_B(2,(int32_t)out)=x;MEM_B(3,(int32_t)out)=y;
        sote_input_status[1]=buttons;sote_input_status[2]=x;sote_input_status[3]=y;sote_input_status[6]=script_index;
        return;
    }
    if(!pad)return;
    static const struct {SDL_GameControllerButton button;uint16_t n64;} mapping[]={
        {SDL_CONTROLLER_BUTTON_A,0x8000},{SDL_CONTROLLER_BUTTON_B,0x4000},
        {SDL_CONTROLLER_BUTTON_START,0x1000},{SDL_CONTROLLER_BUTTON_DPAD_UP,0x0800},
        {SDL_CONTROLLER_BUTTON_DPAD_DOWN,0x0400},{SDL_CONTROLLER_BUTTON_DPAD_LEFT,0x0200},
        {SDL_CONTROLLER_BUTTON_DPAD_RIGHT,0x0100},{SDL_CONTROLLER_BUTTON_LEFTSHOULDER,0x0020},
        {SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,0x0010},{SDL_CONTROLLER_BUTTON_Y,0x0008},
        {SDL_CONTROLLER_BUTTON_X,0x0002}};
    uint16_t buttons=0;
    for(unsigned i=0;i<sizeof(mapping)/sizeof(mapping[0]);i++)
        if(SDL_GameControllerGetButton(pad,mapping[i].button))buttons|=mapping[i].n64;
    if(SDL_GameControllerGetAxis(pad,SDL_CONTROLLER_AXIS_TRIGGERRIGHT)>8192)buttons|=0x2000;
    int rx=SDL_GameControllerGetAxis(pad,SDL_CONTROLLER_AXIS_RIGHTX);
    int ry=SDL_GameControllerGetAxis(pad,SDL_CONTROLLER_AXIS_RIGHTY);
    if(rx>16384)buttons|=1;if(rx<-16384)buttons|=2;
    if(ry>16384)buttons|=4;if(ry<-16384)buttons|=8;
    int x=SDL_GameControllerGetAxis(pad,SDL_CONTROLLER_AXIS_LEFTX);
    int y=SDL_GameControllerGetAxis(pad,SDL_CONTROLLER_AXIS_LEFTY);
    xb_modern_publish(1,x/32768.0f,-y/32768.0f,rx/32768.0f,-ry/32768.0f);
    buttons=xb_modern_buttons(buttons);
    xb_menu_input(rdram,&buttons,x*80/32768,-y*80/32768);
    MEM_H(0,(int32_t)out)=buttons;
    MEM_B(2,(int32_t)out)=x>-4096&&x<4096?0:x*80/32768;
    MEM_B(3,(int32_t)out)=y>-4096&&y<4096?0:-y*80/32768;
}
