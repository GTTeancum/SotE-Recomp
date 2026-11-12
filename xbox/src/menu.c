#include "runtime.h"
#include "recomp_hooks.h"
#include "gpu.h"
#include <string.h>
#include <stdio.h>
/* Native menu bring-up. Desktop replacement widgets are not installed here:
   retain the original menu state machine and display lists, with observable
   state for the process-local navigation harness. Enhanced options pending. */
volatile struct {uint32_t updates,captures,options;int32_t story,mode,focus;} sote_menu_status;
extern volatile uint32_t sote_input_status[8];
volatile uint32_t sote_menu_trace[64][6];
/* Distinguish save rows and modal dialogs from the Options-page focus. */
volatile uint32_t sote_profile_trace[64][8];
/* Transitions, rather than just received Start presses, prove pause/resume. */
volatile uint32_t sote_pause_trace[64][8];
static unsigned pause_serial;
static int last_pause[5]={-999,-999,-999,-999,-999};
static unsigned profile_serial;
static int last_profile[5]={-999,-999,-999,-999,-999};
static unsigned trace_serial;
static int last_story=-999,last_mode=-999,last_focus=-999;
static int requested_mode=-1,cancel_dialog;
static uint16_t previous_buttons;
static int back_release;
static int restore_pause_options;
static int title_selection,title_options,confirm_release;
static int test_config_loaded,test_unlock_levels,test_unlock_applied;
volatile uint32_t sote_level_fixture_status[2];
static int front_mode(uint8_t* rdram) {
    int mode=MEM_H(0,(int32_t)0x800DD744);
    return mode>=0?mode:MEM_H(0,(int32_t)0x800DD748);
}
void xb_menu_input(uint8_t* rdram,uint16_t* buttons,int x,int y) {
    if(MEM_H(0,(int32_t)0x8013CE0E)==2 && front_mode(rdram)==0) {
        if(y>40 || x < -40)*buttons|=0x0800;
        else if(y < -40 || x>40)*buttons|=0x0400;
    }
    uint16_t pressed=*buttons&~previous_buttons;previous_buttons=*buttons;
    if(back_release) {
        if(*buttons&0x4000)*buttons&=~0x4000;
        else back_release=0;
    }
    if(confirm_release) {
        if(*buttons&0x9000)*buttons&=~0x9000;
        else confirm_release=0;
    }
    if(MEM_H(0,(int32_t)0x8013CE0E)!=2) {
        /* Start resumes from the pause rows regardless of the highlighted
           action. In Options it retains the native return/save behavior. */
        if(MEM_W(0,(int32_t)0x800DD2BC)==1 &&
            MEM_W(0,(int32_t)0x800D0948)!=0 &&
            MEM_W(0,(int32_t)0x800D094C)==0 && (pressed&0x1000))
            MEM_H(0,(int32_t)0x800D0944)=0;
        /* 80026124 returns DD2BC: the real pause-active state. 0948 only
           selects the pause rows versus Options and is 1 during gameplay too.
           Retail Options exits through Start (800163F4..80016450), including
           saving settings. Reuse that path, never rewrite progression state. */
        if(MEM_W(0,(int32_t)0x800DD2BC)==1 &&
            MEM_W(0,(int32_t)0x800D0948)==0 &&
            MEM_W(0,(int32_t)0x800DD5E4)!=0 && (pressed&0x4000)) {
            *buttons=0x1000;
            back_release=1;
            restore_pause_options=1;
        }
        return;
    }
    int mode=front_mode(rdram);
    if(mode==0) {
        *buttons=0;
        if(pressed&0x0820)title_selection=0;
        if(pressed&0x0410)title_selection=1;
        if(pressed&0x9000) {
            confirm_release=1;
            if(title_selection){title_options=1;requested_mode=4;}
            else *buttons=0x1000;
        }
        return;
    }
    if(mode==4 && title_options) {
        if(pressed&0x4000){*buttons=0x1000;back_release=1;}
        return;
    }
    if(mode!=2 && mode!=3)return;
    if(pressed&0x4000) {
        back_release=1;
        int dialog=MEM_W(0,(int32_t)0x800DD758)!=0 || MEM_H(0,(int32_t)0x800DD754)!=0 || MEM_H(0,(int32_t)0x800DD750)==3;
        if(mode==2)requested_mode=3;
        else if(dialog)cancel_dialog=1;
        else {requested_mode=0;title_selection=0;}
        *buttons=0;
    }
    /* B belongs to navigation, never the retail A/B-confirm path. */
    *buttons&=~0x4000;
}
static void apply_navigation(uint8_t* rdram) {
    if(!test_config_loaded) {
        FILE* file=fopen("D:\\xbox-test.cfg","rb");
        if(file){char line[96];while(fgets(line,sizeof(line),file))
            if(!strcmp(line,"unlock_levels=1\n"))test_unlock_levels=1;fclose(file);}
        test_config_loaded=1;
    }
    if(MEM_H(0,(int32_t)0x8013CE0E)!=2)return;
    if(test_unlock_levels && !test_unlock_applied && front_mode(rdram)==2) {
        /* Same isolated profile fixture as SOTE_DIAGNOSTIC_UNLOCK_LEVELS.
           Only unlock the picker; the retail loader and level code still run.
           Test harness gives this run a disposable child HDD overlay. */
        for(unsigned profile=0;profile<4;profile++) {
            uint32_t p=0x8018BBF8+profile*0x7a;
            MEM_B(8,(int32_t)p)=0;
            for(unsigned level=0;level<10;level++)MEM_H(0x60+level*2,(int32_t)p)=1;
        }
        test_unlock_applied=1;
        sote_level_fixture_status[0]=1;sote_level_fixture_status[1]=sote_input_status[0];
    }
    if(cancel_dialog) {
        MEM_H(0,(int32_t)0x800DD754)=0;
        MEM_H(0,(int32_t)0x800DD758)=0;MEM_H(0,(int32_t)0x800DD75A)=0;
        /* Match retail name-entry teardown at 8003B854..8003B86C.
           Clearing only the action flag leaves its keyboard renderer alive. */
        MEM_W(0,(int32_t)0x800DD724)=0;
        MEM_W(0,(int32_t)0x800DD768)=0;
        if(MEM_H(0,(int32_t)0x800DD750)==3)MEM_H(0,(int32_t)0x800DD750)=2;
        cancel_dialog=0;
    }
    if(requested_mode>=0) {
        uint32_t address=MEM_H(0,(int32_t)0x800DD744)>=0?0x800DD744:0x800DD748;
        MEM_H(0,(int32_t)address)=requested_mode;
        if(requested_mode==4)MEM_H(0,(int32_t)0x800DD5E0)=1;
        if(requested_mode==3){MEM_H(0,(int32_t)0x800DD750)=-1;MEM_H(0,(int32_t)0x800DD754)=0;}
        requested_mode=-1;
    }
    if(title_options && front_mode(rdram)==3) {
        uint32_t address=MEM_H(0,(int32_t)0x800DD744)>=0?0x800DD744:0x800DD748;
        MEM_H(0,(int32_t)address)=0;
        title_options=0;title_selection=1;
    }
    if(front_mode(rdram)==0) {
        uint32_t table=MEM_W(0,(int32_t)0x8013CE30);
        if(table>=0x80000000u && table<=0x807ffec0u)for(unsigned row=0;row<80;row++) {
            uint32_t text=MEM_W(row*4,(int32_t)table);
            if(text<0x80000000u || text>0x807fffe0u)continue;
            char plain[32];unsigned n=0;
            for(unsigned i=0;i<31;i++) {
                unsigned c=MEM_BU(i,(int32_t)text);if(!c)break;
                if(c=='~') {
                    unsigned op=MEM_BU(++i,(int32_t)text);
                    if(op=='f')i++;
                    continue;
                }
                if(c>=32 && c<=126 && n<31)plain[n++]=c;
            }
            while(n && plain[n-1]==' ')n--;
            plain[n]=0;
            if(!strcmp(plain,"Press Start"))MEM_H(row*4,(int32_t)0x80111110)=-1000;
        }
    }
}
static void observe(uint8_t* rdram) {
    if(restore_pause_options && MEM_W(0,(int32_t)0x800D0948)!=0) {
        if(MEM_W(0,(int32_t)0x800DD2BC)==1)MEM_H(0,(int32_t)0x800D0944)=1;
        restore_pause_options=0;
    }
    int pause[5]={MEM_W(0,(int32_t)0x800D0948),
        MEM_H(0,(int32_t)0x800D0944),MEM_W(0,(int32_t)0x800D094C),
        MEM_W(0,(int32_t)0x800DD2BC),MEM_W(0,(int32_t)0x800DD5E4)};
    int pause_changed=0;
    for(unsigned i=0;i<5;i++)if(pause[i]!=last_pause[i])pause_changed=1;
    if(pause_changed) {
        unsigned serial=++pause_serial;
        volatile uint32_t* row=sote_pause_trace[serial%64];
        row[0]=serial;row[1]=sote_input_status[0];
        for(unsigned i=0;i<3;i++){row[i+2]=pause[i];last_pause[i]=pause[i];}
        row[5]=sote_input_status[1];
        for(unsigned i=3;i<5;i++){row[i+3]=pause[i];last_pause[i]=pause[i];}
    }
    sote_menu_status.story=MEM_H(0,(int32_t)0x8013CE0E);
    int mode=MEM_H(0,(int32_t)0x800DD744);
    sote_menu_status.mode=mode>=0?mode:MEM_H(0,(int32_t)0x800DD748);
    sote_menu_status.focus=MEM_H(0,(int32_t)0x800DD5E0);
    if(sote_menu_status.story==2) {
        int profile[5]={sote_menu_status.mode,
            MEM_BU(0,(int32_t)0x8018BBFD),MEM_H(0,(int32_t)0x800DD750),
            MEM_H(0,(int32_t)0x800DD754),MEM_W(0,(int32_t)0x800DD758)};
        int changed=0;
        for(unsigned i=0;i<5;i++)if(profile[i]!=last_profile[i])changed=1;
        if(changed) {
            unsigned serial=++profile_serial;
            volatile uint32_t* row=sote_profile_trace[serial%64];
            row[0]=serial;row[1]=sote_input_status[0];
            for(unsigned i=0;i<5;i++){row[i+2]=profile[i];last_profile[i]=profile[i];}
            row[7]=sote_input_status[1];
        }
    }
    if(last_story!=sote_menu_status.story || last_mode!=sote_menu_status.mode || last_focus!=sote_menu_status.focus) {
        last_story=sote_menu_status.story;last_mode=sote_menu_status.mode;last_focus=sote_menu_status.focus;
        unsigned serial=++trace_serial;volatile uint32_t* row=sote_menu_trace[serial%64];
        row[0]=serial;row[1]=sote_input_status[0];row[2]=last_story;row[3]=last_mode;row[4]=last_focus;row[5]=sote_input_status[1];
    }
}
void sote_update_graphics_menu(uint8_t* rdram){apply_navigation(rdram);observe(rdram);sote_menu_status.updates++;}
void sote_capture_menu(uint8_t* rdram){observe(rdram);sote_menu_status.captures++;}
void sote_capture_native_options(uint8_t* rdram){observe(rdram);sote_menu_status.options++;}

/* Decode the same original glyph atlas as menu_model.cpp. Transparent outlined
   title labels retain the original title artwork, without a panel or box. */
void xb_menu_draw(uint8_t* rdram) {
    if(MEM_H(0,(int32_t)0x8013CE0E)!=2 || front_mode(rdram)!=0)return;
    const uint32_t base=0x800DA818;
    uint32_t atlas=MEM_W(0,(int32_t)base);
    if(MEM_H(4,(int32_t)base)!=13 || atlas<0x80000000u || atlas>0x807ff000u)return;
    static uint32_t image[128*40];
    memset(image,0,sizeof(image));
    const char* labels[2]={"START GAME","OPTIONS"};
    for(unsigned row=0;row<2;row++) {
        const char* label=labels[row];int length=0;
        for(const char* c=label;*c;c++)length+=MEM_H(6+(*c-32)*14,(int32_t)base);
        for(int pass=0;pass<2;pass++) {
            int pen=(128-length)/2;
            for(const char* c=label;*c;c++) {
                uint32_t g=base+(*c-32)*14;
                int advance=MEM_H(6,(int32_t)g),by=MEM_H(8,(int32_t)g),bx=MEM_H(10,(int32_t)g);
                int gx=MEM_H(12,(int32_t)g),gy=MEM_H(14,(int32_t)g),w=MEM_H(16,(int32_t)g),h=MEM_H(18,(int32_t)g);
                if(gx>=0 && gy>=0 && w>=0 && h>=0 && gx+w<=64 && gy+h<=128)
                for(int y=0;y<h;y++)for(int x=0;x<w;x++) {
                    unsigned texel=(gy+y)*64+gx+x;
                    unsigned value=MEM_BU(texel/2,(int32_t)atlas);
                    unsigned alpha=((texel&1)?value&15:value>>4)*17;
                    if(!alpha)continue;
                    int px=pen-bx+x,py=8+row*12-by+y;
                    for(int oy=pass?0:-1;oy<=(pass?0:1);oy++)for(int ox=pass?0:-1;ox<=(pass?0:1);ox++) {
                        int xx=px+ox,yy=py+oy;
                        if(xx<0 || xx>=128 || yy<0 || yy>=40)continue;
                        uint32_t color=pass?(title_selection==(int)row?0xf2ffff:0xadb7b9):0;
                        uint32_t* dst=&image[yy*128+xx];
                        if(pass || (*dst>>24)<alpha)*dst=(alpha<<24)|color;
                    }
                }
                pen+=advance;
            }
        }
    }
    xb_gpu_image(image,128,40,96,170);
}
