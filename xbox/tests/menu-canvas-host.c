/* Synthetic atlas only: no ROM/game assets needed. */
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "../src/menu_canvas_xbox.c"
static unsigned submits;
void xb_gpu_canvas(const uint32_t *p,unsigned w,unsigned h,uint32_t background){(void)background;assert(p==pixels&&w==640&&h==480);submits++;}
static void wb(uint8_t*r,unsigned a,unsigned v){r[(a&0x7fffff)^3]=(uint8_t)v;}
static void wh(uint8_t*r,unsigned a,unsigned v){wb(r,a,v>>8);wb(r,a+1,v);}
static void ww(uint8_t*r,unsigned a,unsigned v){wh(r,a,v>>16);wh(r,a+2,v);}
int main(void){
 uint8_t*r=calloc(1,0x800000),*copy=malloc(0x800000);assert(r&&copy);
 const unsigned base=0x800da818,atlas=0x80200000;ww(r,base,atlas);wh(r,base+4,13);
 for(unsigned i=0;i<95;i++){unsigned a=base+i*14;wh(r,a+6,7);wh(r,a+8,9);wh(r,a+10,0);wh(r,a+12,0);wh(r,a+14,0);wh(r,a+16,6);wh(r,a+18,10);}
 for(unsigned i=0;i<4096;i++)wb(r,atlas+i,0xff);
 memcpy(copy,r,0x800000);
 xb_menu_snapshot s={0};s.title_focus=1;assert(xb_menu_canvas_render(r,&s));
 for(unsigned y=0;y<480;y++)for(unsigned x=0;x<640;x++)if(pixels[y*640+x]>>24)assert(x>=48&&x<=592&&y>=36&&y<=444);
 s.title="Controls";s.tabs="Options   < Controls >   Cheats";s.return_label="Return to Pause Menu";s.help="Original controller settings";s.preset="Standard";s.row_count=16;
 s.header[0]="Player: Dash";s.header[1]="Difficulty: Medium";s.header[2]="Time 0:00:10";s.header[3]="Challenge Points 0";
 for(unsigned i=0;i<16;i++){s.rows[i].label="Long setting label";s.rows[i].value="Standard";s.rows[i].selected=i==0;}
 for(int p=XB_MENU_OPTIONS;p<=XB_MENU_BINDINGS;p++){s.page=p;assert(xb_menu_canvas_render(r,&s));}
 assert(submits==7);
 s.page=XB_MENU_BINDINGS;s.modal_title="Press a controller button";
 s.modal_detail="This action is already assigned to another action.\nChoose whether to swap the assignments or cancel. ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789LongWordContinues";
 s.modal_hint="A: swap   B: cancel";assert(xb_menu_canvas_render(r,&s));
 assert(submits==8);assert(memcmp(copy,r,0x800000)==0);
 wh(r,base+4,0);assert(!xb_menu_canvas_render(r,&s));assert(submits==8);
 free(copy);free(r);puts("canvas pages/bounds/invalid-atlas/no-guest-write tests passed");
}
