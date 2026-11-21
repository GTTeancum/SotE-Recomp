#include "runtime.h"
#include "recomp_hooks.h"
#include "gpu.h"
#include "menu_canvas_xbox.h"
#include "menu_settings.h"
#include "hd_texture.h"
volatile uint32_t sote_hd_menu[4]; /* active Options page, selection, adjusting, save error */
#include <string.h>
#include <stdio.h>
/* Xbox-only PC-layout presentation over the original native menu state machine.
   Canvas/settings/bindings are host-owned; native profile and Options state
   remain authoritative. Diagnostics observe navigation without replacing it. */
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
static int controls_open,controls_row,controls_save_error,controls_from_title;
extern int xb_controls_value(int row);
extern void xb_controls_change(int row,int direction);
extern int xb_controls_save(void);
int xb_menu_captures_stick(void){return controls_open;}

static int test_config_loaded,test_unlock_levels,test_unlock_applied;
volatile uint32_t sote_level_fixture_status[2];
static int front_mode(uint8_t* rdram) {
    int mode=MEM_H(0,(int32_t)0x800DD744);
    return mode>=0?mode:MEM_H(0,(int32_t)0x800DD748);
}
static int pc_tab,pc_selection=1,pc_adjusting,pc_bindings,pc_binding_scroll;
static int binding_focus,binding_capture,binding_count,binding_groups[256],binding_actions[256];
static const char* binding_status="";
int xb_menu_bindings_active(void){return pc_bindings;}
static int native_options(uint8_t* r) { uint8_t* rdram=r;
 return (MEM_H(0,(int32_t)0x8013CE0E)==2&&front_mode(r)==4)||
 (MEM_W(0,(int32_t)0x800DD2BC)==1&&MEM_W(0,(int32_t)0x800D0948)==0&&MEM_W(0,(int32_t)0x800DD5E4)!=0);
}
static int pc_options_input(uint8_t* rdram,uint16_t* buttons,uint16_t pressed) {
 if(!native_options(rdram)) {pc_tab=0;pc_adjusting=0;pc_bindings=0;return 0;}
 controls_open=1;
 /* Match PC host-menu activity: do not accumulate retail attract-mode timeout
    while the host tabs consume input. These are existing native UI counters. */
 if(MEM_H(0,(int32_t)0x8013CE0E)==2){MEM_H(0,(int32_t)0x800DD740)=0;MEM_H(0,(int32_t)0x800DD742)=0;}
 if(pc_bindings) {
  if(binding_capture) {
   if(pressed&0x1000){binding_capture=0;binding_status="Binding change cancelled.";}
   else if(pressed){binding_status=xb_binding_set(binding_groups[binding_focus],binding_actions[binding_focus],pressed)?"Binding changed. B saves and returns.":"Button conflict or reserved action. Choose another.";binding_capture=0;}
   *buttons=0;return 1;
  }
  if((pressed&0x0800)&&binding_count>0){do{binding_focus=binding_focus>0?binding_focus-1:binding_count-1;}while(binding_count>0&&binding_actions[binding_focus]<0);}
  if((pressed&0x0400)&&binding_count>0){do{binding_focus=(binding_focus+1)%binding_count;}while(binding_count>0&&binding_actions[binding_focus]<0);}
  if(binding_focus<pc_binding_scroll)pc_binding_scroll=binding_focus;
  if(binding_focus>pc_binding_scroll+8)pc_binding_scroll=binding_focus-8;
  if((pressed&0x8000)&&binding_count>0){binding_capture=1;binding_status="Press a controller button. Start cancels.";}
  if((pressed&2)&&binding_count>0){xb_binding_set(binding_groups[binding_focus],binding_actions[binding_focus],0);binding_status="Binding cleared.";}
  if((pressed&8)&&binding_count>0){xb_binding_reset_context(binding_groups[binding_focus]);binding_status="Section defaults restored.";}
  if(pressed&0x5000){controls_save_error=!xb_controls_save();if(!controls_save_error){pc_bindings=0;confirm_release=1;}}
  if(pressed&0x0300){MEM_H(0,(int32_t)0x800DD5E0)=6;*buttons=pressed&0x0300;return 1;}
  *buttons=0;return 1;
 }
 if(pressed&0x2000)pressed|=0x0100;
 if(pressed&0x40)pressed|=0x0200;
 if(controls_save_error) {
  if(pressed&0xC000)controls_save_error=!xb_controls_save();
  *buttons=0;return 1;
 }
 int count=pc_tab==0?8:pc_tab==1?xb_pc_control_count()+2:xb_cheat_count();
 if(pc_selection>count)pc_selection=1;
 if(!pc_adjusting && (pressed&0x0300)) {
  if(MEM_H(0,(int32_t)0x8013CE0E)!=2&&((pc_tab==0&&(pressed&0x0200))||(pc_tab==2&&(pressed&0x0100)))) {
   if(!xb_controls_save()){controls_save_error=1;*buttons=0;return 1;}
   controls_open=0;pc_tab=0;pc_selection=1;*buttons=0x1000;back_release=1;restore_pause_options=1;return 1;
  }
  pc_tab=(pc_tab+((pressed&0x0100)?1:2))%3;pc_selection=1;*buttons=0;return 1;
 }
 if(!pc_adjusting) {
  if(pressed&0x0800)pc_selection=(pc_selection+count)%(count+1);
  if(pressed&0x0400)pc_selection=(pc_selection+1)%(count+1);
 }
 if((pressed&0x5000)||((pressed&0x8000)&&pc_selection==0)) {
  if(pc_adjusting){pc_adjusting=0;*buttons=0;return 1;}
  if(!xb_controls_save()){controls_save_error=1;*buttons=0;return 1;}
  controls_open=0;pc_tab=0;pc_selection=1;*buttons=0x1000;back_release=1;restore_pause_options=1;return 1;
 }
 if(pc_tab==0) {
  if(pc_selection==7 || pc_selection==8) {
   /* Host-only rows: never pass their indices or edit inputs into retail Options. */
   if(pressed&0x8000)pc_adjusting=!pc_adjusting;
   if(pc_adjusting&&(pressed&0x0300)){if(pc_selection==7)xb_pc_movies_toggle();else xb_hd_textures_toggle();}
   *buttons=0;return 1;
  }
  MEM_H(0,(int32_t)0x800DD5E0)=pc_selection;
  if((pressed&0x8000)&&pc_selection==6){pc_bindings=1;pc_binding_scroll=0;binding_focus=1;binding_capture=0;binding_status="";*buttons=0;return 1;}
  if(pressed&0x8000)pc_adjusting=!pc_adjusting;
  *buttons=pc_adjusting?(pressed&0x0300):0;return 1;
 }
 if(pc_tab==1&&pc_selection==count) {
  if(pressed&0x8000){pc_bindings=1;pc_binding_scroll=0;binding_focus=1;binding_capture=0;binding_status="";}
 } else if(pc_tab==1&&pc_selection==count-1) {
  if(pressed&0x8000)controls_save_error=!xb_controls_save();
 } else {
  if(pressed&0x8000)pc_adjusting=!pc_adjusting;
  int direction=(pressed&0x0100)?1:(pressed&0x0200)?-1:0;
  if(pc_adjusting&&direction&&pc_selection>0) {
   if(pc_tab==1)xb_pc_control_adjust(pc_selection-1,direction);
   else xb_cheat_adjust(pc_selection-1,direction);
  }
 }
 *buttons=0;return 1;
}
void xb_menu_input(uint8_t* rdram,uint16_t* buttons,int x,int y) {
    if(MEM_H(0,(int32_t)0x8013CE0E)==2 && front_mode(rdram)==0) {
        if(y>40 || x < -40)*buttons|=0x0800;
        else if(y < -40 || x>40)*buttons|=0x0400;
    }
    uint16_t pressed=*buttons&~previous_buttons;previous_buttons=*buttons;
    int story=MEM_H(0,(int32_t)0x8013CE0E);
    int pause=MEM_W(0,(int32_t)0x800DD2BC)==1;
    if(pc_options_input(rdram,buttons,pressed))return;
    controls_open=0;
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
        if(pressed&0x0820)title_selection=(title_selection+1)%2;
        if(pressed&0x0410)title_selection=(title_selection+1)%2;
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

/* Read-only presentation mirrors PC menu_model.cpp; no text/atlas allocation
   is repurposed. Original native options/profile values remain authoritative. */
static void guest_text(uint8_t* r,uint32_t address,char* out,unsigned cap) { uint8_t* rdram=r;
 unsigned n=0;if(address<0x80000000u||address>0x807fff00u){out[0]=0;return;}
 for(unsigned i=0;i<240&&n+1<cap;i++) {
  unsigned c=MEM_BU(i,(int32_t)address);if(!c)break;
  if(c=='~'){unsigned op=MEM_BU(++i,(int32_t)address);if(op=='f')i++;continue;}
  if(c>=32&&c<127)out[n++]=(char)c;
 }out[n]=0;
}
static void profile_option(uint8_t* r,int option,char* out,unsigned cap) { uint8_t* rdram=r;
 unsigned player=MEM_BU(0,(int32_t)0x8018BBFD);if(player>=4){out[0]=0;return;}
 uint32_t profile=0x8018BBF8+player*0x7a;
 unsigned value=option==0?1:(MEM_BU(0x1a+option/2,(int32_t)profile)>>((option&1)*4))&15;
 if(value>8){out[0]=0;return;}
 unsigned index=MEM_H(option*18+value*2,(int32_t)0x800DD674);
 if(index>=22){out[0]=0;return;}
 guest_text(r,MEM_W(index*4,(int32_t)0x800DD61C),out,cap);
}
static unsigned bindings(uint8_t* r,xb_menu_snapshot* s,char names[16][96],char values[16][96]) { uint8_t* rdram=r;
 const char* group_names[]={"On Foot","Ship - Snowspeeder","Ship - Outrider","Speeder Bike","Turret"};
 int ranges[][2]={{12,24},{24,31},{39,48},{31,36},{36,39}};
 const unsigned bits[]={0x8000,0x4000,0x2000,0x1000,0x0800,0x0400,0x0200,0x0100,0x20,0x10,8,4,2,1,0x40};
 const char* pads[]={"A","B","RT","Start","D-Up","D-Down","D-Left","D-Right","White","Black","Y / RS-Up","RS-Down","X / RS-Left","RS-Right","LT"};
 unsigned player=MEM_BU(0,(int32_t)0x8018BBFD);if(player>=4)return 0;
 unsigned preset=MEM_BU(0x1d,(int32_t)(0x8018BBF8+player*0x7a))&15;if(preset>7)return 0;
 int line=0;unsigned count=0;const int contexts[]={0,1,4,3,2};
 for(int group=0;group<5;group++) {
  binding_groups[line]=contexts[group];binding_actions[line]=-1;
  if(line>=pc_binding_scroll&&count<10){snprintf(names[count],96,"%s",group_names[group]);values[count][0]=0;s->rows[count]=(xb_menu_row){names[count],0,0};count++;}line++;
  for(int action=0;action<48;action++) {
   if(!(action<12||(action>=ranges[group][0]&&action<ranges[group][1])))continue;
   unsigned mask=MEM_H(preset*96+action*2,(int32_t)0x800E6808);if(!mask||action==3)continue;
   char label[96];guest_text(r,MEM_W(action*4,(int32_t)0x800DA70C),label,96);if(!label[0])continue;
   binding_groups[line]=contexts[group];binding_actions[line]=action;
   unsigned custom=xb_binding_get(contexts[group],action);int physical_labels=custom!=0xffffu||(contexts[group]==0&&xb_controls_value(0));mask=xb_binding_effective(contexts[group],action,mask);
   if(line>=pc_binding_scroll&&count<10) {
    snprintf(names[count],96,"%s",label);values[count][0]=0;
    for(unsigned k=0;k<15;k++)if(mask&bits[k]){if(values[count][0])strncat(values[count]," / ",95-strlen(values[count]));strncat(values[count],physical_labels&&k==10?"Y":physical_labels&&k==12?"X":pads[k],95-strlen(values[count]));}
    if(custom==0)snprintf(values[count],96,"Unbound");
    s->rows[count]=(xb_menu_row){names[count],values[count],line==binding_focus};count++;
   }line++;
  }
 }
 binding_count=line;
 if(pc_binding_scroll>line-10)pc_binding_scroll=line>10?line-10:0;
 s->scroll=pc_binding_scroll;s->total_rows=line;return count;
}
void xb_menu_draw(uint8_t* rdram) {
 sote_hd_menu[0]=0;
 int story=MEM_H(0,(int32_t)0x8013CE0E),mode=front_mode(rdram);
 int pause=MEM_W(0,(int32_t)0x800DD2BC)==1;
 xb_menu_snapshot s={0};char labels[16][96]={{0}},values[16][96]={{0}},preset[96];
 if(story==2&&mode==0){s.page=XB_MENU_TITLE;s.title_focus=title_selection;xb_menu_canvas_render(rdram,&s);return;}
 if(native_options(rdram)) {
  s.pause_background=story!=2;s.return_label="Return to previous menu";s.return_selected=pc_selection==0;
  s.navigation=pc_adjusting?"D-Pad Left/Right: adjust   A/B: done":"D-Pad or LT/RT: page   Up/Down: select   A: adjust   B: back";
  s.help=controls_save_error?"Save failed. A/B retries; settings remain open.":"";
  if(pc_bindings) {
   s.page=XB_MENU_BINDINGS;s.row_count=bindings(rdram,&s,labels,values);profile_option(rdram,6,preset,96);s.preset=preset;
   s.navigation=binding_capture?"Press a button; Start cancels":"Up/Down: action   A: change   X: clear   Y: reset   B: save";s.help=binding_status;
   if(binding_capture){s.modal_title="Change Controller Binding";s.modal_detail="Press the controller button to assign. Start remains reserved for pause and cancels this change.";s.modal_hint="Press a button to assign; Start to cancel.";}
  } else if(pc_tab==0) {
   s.page=XB_MENU_OPTIONS;s.title="Options";s.tabs=story==2?"< Options >   Controls   Cheats":"Main   < Options >   Controls   Cheats";
   const char* help[]={"Return to the previous screen.","Show or hide on-screen gameplay information.","Choose the seeker camera behavior.","Adjust sound effects volume.","Adjust music volume.","Choose the sound channel arrangement.","Inspect controller bindings and choose a native preset."};
   s.help=help[pc_selection>=0&&pc_selection<=6?pc_selection:0];
   for(int i=1;i<=6;i++) {
    guest_text(rdram,MEM_W(i*4,(int32_t)0x800DD5E8),labels[i-1],96);profile_option(rdram,i,values[i-1],96);
    if(i==6)snprintf(labels[i-1],96,"Control Bindings");
    s.rows[i-1]=(xb_menu_row){labels[i-1],values[i-1],pc_selection==i};
   }
   s.rows[6]=(xb_menu_row){"Cutscenes",xb_pc_movies_preferred()?(xb_pc_movies_available()?"PC":"PC (unavailable)"):"N64",pc_selection==7};
   if(pc_selection==7)s.help=xb_pc_movies_available()?"Choose original N64 scenes or PC video cutscenes.":"PC video playback unavailable; N64 scenes remain active.";
   int hd_wanted=xb_hd_textures_preferred();
   s.rows[7]=(xb_menu_row){"HD Textures",hd_wanted?"On":"Off",pc_selection==8};
   if(pc_selection==8){
    if(!xb_hd_pack_available())s.help="HD pack unavailable. Save preference; install pack and restart.";
    else if(hd_wanted!=xb_hd_session_active())s.help="Change pending. Save and restart the game to apply.";
    else s.help="Use enhanced textures throughout the game. Changes require a restart.";
   }
   sote_hd_menu[1]=pc_selection;sote_hd_menu[2]=pc_adjusting;sote_hd_menu[3]=controls_save_error;sote_hd_menu[0]=1;
   s.row_count=8;
  } else {
   int n=pc_tab==1?xb_pc_control_count():xb_cheat_count();s.page=pc_tab==1?XB_MENU_SCHEMES:XB_MENU_CHEATS;s.title=pc_tab==1?"Controls":"Cheats";
   s.tabs=pc_tab==1?(story==2?"Options   < Controls >   Cheats":"Main   Options   < Controls >   Cheats"):(story==2?"Options   Controls   < Cheats >":"Main   Options   Controls   < Cheats >");
   for(int i=0;i<n&&i<16;i++) {
    snprintf(values[i],96,"%s",pc_tab==1?xb_pc_control_value(i):xb_cheat_value(i));
    s.rows[i]=(xb_menu_row){pc_tab==1?xb_pc_control_label(i):xb_cheat_label(i),values[i],pc_selection==i+1};
   }s.row_count=n;
   if(pc_selection>0&&pc_selection<=n)s.help=pc_tab==1?xb_pc_control_help(pc_selection-1):xb_cheat_help(pc_selection-1);
   if(pc_tab==1){s.actions[0]=(xb_menu_row){"Apply",0,pc_selection==n+1};s.actions[1]=(xb_menu_row){"Edit Bindings",0,pc_selection==n+2};}
  }
  if(controls_save_error)s.help="Save failed. A/B retries; settings remain open.";
  xb_menu_canvas_render(rdram,&s);return;
 }
 if(pause&&MEM_W(0,(int32_t)0x800D0948)!=0) {
  s.page=XB_MENU_PAUSE;s.pause_background=1;s.row_count=3;
  const char* ordinary[]={"Resume","Options","Quit"},*confirm[]={"Return to title?","Cancel","Quit to title"};
  for(int i=0;i<3;i++)s.rows[i]=(xb_menu_row){MEM_W(0,(int32_t)0x800D094C)?confirm[i]:ordinary[i],0,MEM_H(0,(int32_t)0x800D0944)==i};
  xb_menu_canvas_render(rdram,&s);return;
 }
 /* Ordinary native profile picker only, never overwrite rename/clear modals. */
 if(story==2&&mode==3&&MEM_H(0,(int32_t)0x800DD754)==0&&MEM_W(0,(int32_t)0x800DD758)==0&&MEM_H(0,(int32_t)0x800DD750)!=3) {
  uint32_t table=MEM_W(0,(int32_t)0x8013CE30);if(table<0x80000000u||table>0x807ffec0u)return;
  s.page=XB_MENU_PROFILES;s.row_count=4;
  for(int i=0;i<4;i++){guest_text(rdram,MEM_W((51+i)*4,(int32_t)table),labels[i],96);guest_text(rdram,MEM_W((70+i)*4,(int32_t)table),values[i],96);s.rows[i]=(xb_menu_row){labels[i],values[i],((MEM_W((51+i)*4,(int32_t)0x80111250)>>16)&255)>=160};}
  for(int i=0;i<2;i++){guest_text(rdram,MEM_W((77+i)*4,(int32_t)table),labels[4+i],96);s.actions[i]=(xb_menu_row){labels[4+i],0,((MEM_W((77+i)*4,(int32_t)0x80111250)>>16)&255)>150};}
  xb_menu_canvas_render(rdram,&s);
 }
}
