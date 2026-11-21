#include <assert.h>
#include <stdlib.h>
#include "../../src/menu.c"
volatile uint32_t sote_input_status[8];
static int setting,save_ok=1,movies,pref=1,avail=1,session=1,observed;
int xb_hd_pack_available(void){return avail;}int xb_hd_session_active(void){return session;}
int xb_hd_textures_preferred(void){return pref;}void xb_hd_textures_toggle(void){pref=!pref;}
int xb_pc_movies_preferred(void){return movies;}
void xb_pc_movies_toggle(void){movies=!movies;}
int xb_pc_movies_available(void){return 0;}
int xb_controls_value(int row){return setting;}
void xb_controls_change(int row,int dir){setting+=dir;}
int xb_controls_save(void){return save_ok;}
int xb_pc_control_count(void){return 10;}
const char* xb_pc_control_label(int r){return "On Foot";}
const char* xb_pc_control_value(int r){return "Classic";}
const char* xb_pc_control_help(int r){return "Help";}
void xb_pc_control_adjust(int r,int d){setting+=d;}
int xb_cheat_count(void){return 9;}
const char* xb_cheat_label(int r){return "Infinite Health";}
const char* xb_cheat_value(int r){return "Off";}
const char* xb_cheat_help(int r){return "Help";}
void xb_cheat_adjust(int r,int d){setting+=d;}
unsigned xb_binding_get(int c,int a){return 0xffff;}
unsigned xb_binding_effective(int c,int a,unsigned n){return n;}
int xb_binding_set(int c,int a,unsigned m){return a!=3;}
void xb_binding_reset_context(int c){}
int xb_menu_canvas_render(const uint8_t* r,const xb_menu_snapshot* s){
 if(s->page==XB_MENU_OPTIONS&&s->rows[7].selected){assert(s->row_count==8&&!strcmp(s->rows[7].label,"HD Textures"));
  assert(!strcmp(s->rows[7].value,pref?"On":"Off"));assert(strstr(s->help,!avail?"unavailable":pref!=session?"pending":"restart"));observed++;}
 return 1;}
void xb_gpu_image(const uint32_t* a,unsigned b,unsigned c,int d,int e){}
static uint16_t press(uint8_t* ram,uint16_t b){uint16_t zero=0;xb_menu_input(ram,&zero,0,0);xb_menu_input(ram,&b,0,0);return b;}
int main(){uint8_t* rdram=calloc(1,0x800000);MEM_H(0,0x8013CE0E)=2;MEM_H(0,0x800DD744)=0;
press(rdram,0x0400);assert(title_selection==1);press(rdram,0x0400);assert(title_selection==0);press(rdram,0x0400);press(rdram,0x8000);apply_navigation(rdram);assert(front_mode(rdram)==4);
press(rdram,0);assert(controls_open&&pc_tab==0);press(rdram,0x0100);assert(pc_tab==1&&pc_selection==1);
press(rdram,0x8000);assert(pc_adjusting);press(rdram,0x0100);assert(setting==1&&pc_tab==1);press(rdram,0x4000);assert(!pc_adjusting&&controls_open);
press(rdram,0x0100);assert(pc_tab==2);press(rdram,0x0100);assert(pc_tab==0);press(rdram,0x8000);assert(pc_adjusting);assert(press(rdram,0x0100)==0x0100);press(rdram,0x8000);assert(!pc_adjusting);
save_ok=0;press(rdram,0x4000);assert(controls_save_error&&controls_open);save_ok=1;press(rdram,0x4000);assert(!controls_save_error&&controls_open);assert(press(rdram,0x4000)==0x1000);
MEM_H(0,0x8013CE0E)=4;MEM_W(0,0x800DD2BC)=1;MEM_W(0,0x800D0948)=0;MEM_W(0,0x800DD5E4)=1;pc_selection=6;press(rdram,0x8000);assert(pc_bindings);press(rdram,0x4000);assert(!pc_bindings);
pc_tab=0;pc_selection=7;pc_adjusting=0;uint16_t old_native=MEM_H(0,0x800DD5E0);press(rdram,0x8000);assert(pc_adjusting);assert(press(rdram,0x0100)==0);assert(movies==1&&MEM_H(0,0x800DD5E0)==old_native);press(rdram,0x4000);assert(!pc_adjusting);pc_selection=0;press(rdram,0x0400);assert(pc_selection==1);pc_selection=7;press(rdram,0x0400);assert(pc_selection==8);
 xb_menu_draw(rdram);assert(observed==1);old_native=MEM_H(0,0x800DD5E0);
 press(rdram,0x8000);assert(pc_adjusting);assert(press(rdram,0x0200)==0&&!pref&&session==1);
 assert(MEM_H(0,0x800DD5E0)==old_native&&pc_tab==0);xb_menu_draw(rdram);assert(observed==2);
 avail=0;xb_menu_draw(rdram);assert(observed==3);avail=1;press(rdram,0x4000);assert(!pc_adjusting&&controls_open);
 press(rdram,0x0400);assert(pc_selection==0);press(rdram,0x0800);assert(pc_selection==8);
 press(rdram,0x8000);press(rdram,0x0100);press(rdram,0x8000);assert(pref&&!pc_adjusting&&movies==1);
 press(rdram,0x0400);assert(pc_selection==0);
puts("PC-layout navigation/tabs/adjust/return/save-failure/bindings tests passed");free(rdram);}
