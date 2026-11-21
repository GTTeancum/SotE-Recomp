/* Xbox host-only presentation port of PC src/menu_canvas.cpp at 8016d37.
   Layout coordinates, palette, panels and focus chrome are intentionally the
   PC design. Platform exclusions are supplied by the caller. The PC960x540
   layout coordinates map into the conservative SD544x408 safe rectangle.
   Glyph aspect is retained with vertical scale and width fitting. This
   disclosed4:3 readability adaptation is not Xbox certification.
   Render at native 640x480 so original glyph detail is not lost to a
   320x240 downsample followed by 2x display expansion. */
#include "rdram_guard.h"
#include "menu_canvas_xbox.h"
#include "gpu.h"
#include <stddef.h>
#include <string.h>
#include <math.h>

typedef uint32_t Color; /* RRGGBBAA, as in the PC canvas. */
static const Color gold=0xA8802EFF,green=0x79F06AFF,teal=0x3F8078FF;
static const Color cyan=0x6FE9FFFF,purple=0x6A63C9FF;
enum { CANVAS_WIDTH=640, CANVAS_HEIGHT=480 };
static uint32_t pixels[CANVAS_WIDTH*CANVAS_HEIGHT]; /* host-owned; never borrows guest allocations */
typedef struct { int advance,by,bx,x,y,w,h; } Glyph;
typedef struct { Glyph glyphs[95]; uint8_t alpha[64*128]; int line_height; } Font;
static Font font;
static const float scale=544.0f/960.0f,scale_y=408.0f/540.0f,offset_x=48.0f,offset_y=36.0f;
static int imin(int a,int b){return a<b?a:b;}
static int imax(int a,int b){return a>b?a:b;}
static int iround(float v){return (int)floorf(v+0.5f);}
static unsigned byte(const uint8_t *r,unsigned a){return r[xb_rdram_offset((a&0x7fffff)^3,1,XB_RDRAM_MENU)];}
static unsigned half(const uint8_t *r,unsigned a){return byte(r,a)*256+byte(r,a+1);}
static unsigned word(const uint8_t *r,unsigned a){return half(r,a)*65536+half(r,a+2);}
static int guest_valid(unsigned a,unsigned n){return a>=0x80000000u&&a<=0x80800000u-n;}
static int load_font(const uint8_t *r){
    const unsigned base=0x800DA818,atlas=word(r,base);
    if(half(r,base+4)!=13||!guest_valid(atlas,4096))return 0;
    font.line_height=13;
    for(unsigned i=0;i<95;i++){
        unsigned a=base+i*14;Glyph *g=&font.glyphs[i];
        g->advance=(int16_t)half(r,a+6);g->by=(int16_t)half(r,a+8);g->bx=(int16_t)half(r,a+10);
        g->x=(int16_t)half(r,a+12);g->y=(int16_t)half(r,a+14);
        g->w=(int16_t)half(r,a+16);g->h=(int16_t)half(r,a+18);
        if(g->advance<0||g->advance>64)return 0;
        if(g->x<0||g->y<0||g->w<0||g->h<0||g->x+g->w>64||g->y+g->h>128){g->w=g->h=0;}
    }
    for(unsigned i=0;i<8192;i++){unsigned b=byte(r,atlas+i/2);font.alpha[i]=(uint8_t)(((i&1)?b&15:b>>4)*17);}
    return 1;
}
static void pixel(int x,int y,Color c,unsigned coverage){
    if(x<0||x>=CANVAS_WIDTH||y<0||y>=CANVAS_HEIGHT)return;
    unsigned a=(c&255)*coverage/255;if(!a)return;
    uint32_t old=pixels[y*CANVAS_WIDTH+x];unsigned oa=old>>24,na=a+(oa*(255-a)+127)/255;
    uint32_t v=na<<24;
    for(unsigned k=0;k<3;k++){
        unsigned channel=(c>>(24-k*8))&255,previous=(old>>(16-k*8))&255;
        unsigned out=(channel*a+previous*oa*(255-a)/255+na/2)/na;
        v|=out<<(16-k*8);
    }
    pixels[y*CANVAS_WIDTH+x]=v;
}
static void rect(float x,float y,float w,float h,Color c){
    int x0=imax(0,iround(offset_x+x*scale)),y0=imax(0,iround(offset_y+y*scale_y));
    int x1=imin(CANVAS_WIDTH,iround(offset_x+(x+w)*scale)),y1=imin(CANVAS_HEIGHT,iround(offset_y+(y+h)*scale_y));
    /* Keep one-design-pixel dividers visible at SD output. */
    if(w>0&&x1<=x0)x1=imin(CANVAS_WIDTH,x0+1);
    if(h>0&&y1<=y0)y1=imin(CANVAS_HEIGHT,y0+1);
    for(int yy=y0;yy<y1;yy++)for(int xx=x0;xx<x1;xx++)pixel(xx,yy,c,255);
}
static void border(float x,float y,float w,float h,Color c,float t){
    rect(x,y,w,t,c);rect(x,y+h-t,w,t,c);rect(x,y,t,h,c);rect(x+w-t,y,t,h,c);
}
static float measure(const char*s,float height){float n=0;if(!s)return 0;for(;*s;s++){unsigned c=(unsigned char)*s;if(c>=32&&c<=126)n+=font.glyphs[c-32].advance*height/font.line_height;}return n;}
static void text(const char*s,float x,float y,float height,Color color,float max_width,int align,float box_height){
    if(!s||!*s||max_width<=0)return;
    float ph=height*scale_y,width=measure(s,ph);if(width>max_width*scale)ph*=max_width*scale/width;
    if(ph<1)ph=1;
    float pen=offset_x+x*scale,top=offset_y+y*scale_y,k=ph/font.line_height;
    float measured=measure(s,ph);if(align==1)pen-=measured/2;else if(align==2)pen-=measured;
    if(box_height>0){
        float advance=0,left=1e9f,right=-1e9f,upper=1e9f,lower=-1e9f;
        for(const char*p=s;*p;p++){unsigned c=(unsigned char)*p;if(c<32||c>126)continue;Glyph*g=&font.glyphs[c-32];
            if(g->w&&g->h){float l=advance-g->bx*k,r=advance+(g->w-g->bx)*k,u=-g->by*k,d=(g->h-g->by)*k;
                if(l<left)left=l;
                if(r>right)right=r;
                if(u<upper)upper=u;
                if(d>lower)lower=d;}
            advance+=g->advance*k;}
        if(right>=left){pen=offset_x+x*scale-(left+right)/2;top+=(box_height*scale_y-upper-lower)/2;}
    }
    for(;*s;s++){unsigned c=(unsigned char)*s;if(c<32||c>126)continue;Glyph*g=&font.glyphs[c-32];
        int l=iround(pen-g->bx*k),r=iround(pen+(g->w-g->bx)*k),t=iround(top-g->by*k),b=iround(top+(g->h-g->by)*k);
        for(int yy=t;yy<b;yy++)for(int xx=l;xx<r;xx++){
            /* Match PC original-font raster sampling. At native640x480 the
               glyphs retain their interior gaps; max-coverage pooling into
               a320px surface merged those gaps and made letters blocky. */
            int gx=imin(g->w-1,imax(0,(int)((xx-l)/k)));
            int gy=imin(g->h-1,imax(0,(int)((yy-t)/k)));
            pixel(xx,yy,color,font.alpha[(g->y+gy)*64+g->x+gx]);
        }
        pen+=g->advance*k;
    }
}
static unsigned random_next(unsigned *seed){*seed^=*seed<<13;*seed^=*seed>>17;*seed^=*seed<<5;return *seed;}
static void stars(void){unsigned seed=0x534f5445;for(int i=0;i<260;i++){
    float x=(random_next(&seed)%96000)/100.f,y=(random_next(&seed)%54000)/100.f;
    unsigned v=28+random_next(&seed)%92;float size=i%13==0?1.7f:.8f;
    rect(x,y,size,size,(v<<24)|(v<<16)|((v+12)<<8)|255);}}
static void heading(const char*s){float half=measure(s,30)*scale_y/scale/2+16;if(half>330)half=330;
    rect(92,72,388-half,2,purple);rect(480+half,72,388-half,2,purple);text(s,480,57,30,0x66C2B6FF,640,1,0);}
static void footer(const xb_menu_snapshot*s){
    if(!s->return_label)return;
    if(s->return_selected){rect(254,507,452,31,0x17383CFF);border(254,507,452,31,cyan,1);}
    text(s->return_label,480,510,24,s->return_selected?cyan:teal,432,1,0);
}
static const char*nav(const xb_menu_snapshot*s){return s->navigation?s->navigation:"Left/Right: page   Up/Down: select   A: adjust   B: back";}
static void title(const xb_menu_snapshot*s){const char*labels[]={"START GAME","OPTIONS"};for(int i=0;i<2;i++){
    float y=404+i*24;
    for(int dy=-1;dy<=1;dy++)for(int dx=-1;dx<=1;dx++)if(dx||dy)text(labels[i],480+dx*1.5f,y+dy*1.5f,27,0x000000FF,270,1,0);
    text(labels[i],480,y,27,s->title_focus==i?0xF2FFFFFF:0xADB7B9FF,270,1,0);}}
static void options(const xb_menu_snapshot*s){
    heading(s->title?s->title:"Options");text(s->tabs,480,84,18,0x697388FF,820,1,0);
    for(unsigned i=0;i<s->row_count&&i<8;i++){const xb_menu_row*r=&s->rows[i];float y=s->row_count==8 ? 122+i*38 : 126+i*40;
        if(y>447)break;
        if(r->selected){rect(184,y-3,665,36,0x17383CFF);border(184,y-3,665,36,cyan,1);}
        text(r->label,200,y,27,r->selected?cyan:teal,375,0,0);text(r->value,660,y,27,r->selected?cyan:teal,201,0,0);}
    rect(92,430,776,2,purple);text(s->help,480,450,16,0x98B6D0FF,776,1,0);
    text(nav(s),480,480,14,0xB1BDE3FF,850,1,0);footer(s);
}
static void schemes(const xb_menu_snapshot*s){
    heading(s->title?s->title:(s->page==XB_MENU_CHEATS?"Cheats":"Controls"));
    text(s->tabs,480,84,18,0x697388FF,820,1,0);rect(92,104,776,1,purple);
    for(unsigned i=0;i<s->row_count&&i<11;i++){const xb_menu_row*r=&s->rows[i];float y=128+i*30;
        if(r->selected){rect(98,y-3,748,28,0x17383CFF);border(98,y-3,748,28,cyan,1);}
        text(r->label,106,y,19,r->selected?cyan:0x8F9ADEFF,300,0,0);text(r->value,810,y,18,r->selected?cyan:teal,520,2,0);}
    rect(92,445,776,2,purple);
    for(unsigned i=0;i<2;i++){const xb_menu_row*r=&s->actions[i];float x=i?625:335;
        if(r->selected){rect(x-120,449,240,26,0x17383CFF);border(x-120,449,240,26,cyan,1);}
        text(r->label,x,452,19,r->selected?cyan:teal,230,1,0);}
    text(s->help,480,472,16,0x98B6D0FF,776,1,0);text(nav(s),480,490,14,0xB1BDE3FF,850,1,0);footer(s);
}
static void pause_page(const xb_menu_snapshot*s){
    /* PC preserves the retail live header above its host action panel. Use
       only when supplied: normally the caller retains the native header.
       Native box (64,35)-(256,85) mapped to PC960x540 coordinates. */
    int have_header=0;
    for(unsigned i=0;i<4;i++)if(s->header[i]&&*s->header[i])have_header=1;
    if(have_header){
        rect(192,79,576,113,0x00000060);
        border(192,79,576,113,0x408040FF,2);
        for(unsigned i=0;i<4;i++)text(s->header[i],480,90+i*24,24,0x4A9A45FF,550,1,0);
    }
    rect(264,207,432,145,0x071019FF);border(264,207,432,145,teal,2);
    for(unsigned i=0;i<s->row_count&&i<3;i++){const xb_menu_row*r=&s->rows[i];float y=220+i*43;
        if(r->selected){rect(278,y-4,404,37,0x17383CFF);border(278,y-4,404,37,cyan,1);}
        text(r->label,480,y,29,r->selected?cyan:teal,375,1,0);}
}
static void profiles(const xb_menu_snapshot*s){
    text("A: select game",480,48,28,gold,760,1,0);
    for(unsigned i=0;i<s->row_count&&i<4;i++){const xb_menu_row*r=&s->rows[i];float y=107+i*59;int filled=r->value&&*r->value;
        Color frame=r->selected?0x3FDD4AFF:filled?0x1D5C4CFF:0x1A4C44FF,ink=r->selected?green:filled?0x2F7160FF:0x28564CFF;
        rect(88,y,545,47,0x000000C0);rect(654,y,218,47,0x000000C0);
        border(88,y,545,47,frame,r->selected?2:1);border(654,y,218,47,frame,r->selected?2:1);
        text(r->label,360.5f,y,27,ink,508,1,47);text(r->value,763,y,25,ink,182,1,47);}
    rect(183,364,594,2,0x2C7A35FF);border(282,391,396,48,0x1F4D2EFF,2);
    for(unsigned i=0;i<2;i++){const xb_menu_row*r=&s->actions[i];float x=381+i*198;
        if(r->selected)border(x-99,391,198,48,0x3FDD4AFF,2);
        text(r->label,x,391,25,r->selected?green:0x34783FFF,183,1,48);}
    rect(348,458,264,2,0x2C7A35FF);text(s->navigation?s->navigation:"Up/Down: select save   Left/Right: Rename / Clear   B: back",480,478,21,gold,850,1,0);
}
/* Match the PC capture dialog geometry; wrap detail text to six bounded
   lines so long names cannot cross the dialog's bottom action area. */
static void binding_modal(const xb_menu_snapshot*s){
    if(!s->modal_title||!*s->modal_title)return;
    rect(0,0,960,540,0x000008BE);
    rect(116,140,728,292,0x0B1220FF);
    border(116,140,728,292,purple,1);
    rect(139,191,682,1,purple);
    text(s->modal_title,480,162,25,cyan,664,1,0);
    const char *p=s->modal_detail?s->modal_detail:"";
    unsigned count=0,used=0,scanned=0;char line[63];
    while(*p&&count<6&&scanned<4096){
        if(*p=='\n'){
            if(used){line[used]=0;text(line,480,210+count*21.85f,19,0xCCD8ECFF,660,1,0);count++;used=0;}
            p++;scanned++;continue;
        }
        if(*p==' '||*p=='\t'||*p=='\r'){p++;scanned++;continue;}
        unsigned n=0;
        while(p[n]&&p[n]!=' '&&p[n]!='\t'&&p[n]!='\r'&&p[n]!='\n'&&n<62&&scanned+n<4096)n++;
        if(used&&used+1+n>62){line[used]=0;text(line,480,210+count*21.85f,19,0xCCD8ECFF,660,1,0);count++;used=0;if(count>=6)break;}
        if(used)line[used++]=' ';
        memcpy(line+used,p,n);used+=n;p+=n;scanned+=n;
        if(used==62){line[used]=0;text(line,480,210+count*21.85f,19,0xCCD8ECFF,660,1,0);count++;used=0;}
    }
    if(used&&count<6){line[used]=0;text(line,480,210+count*21.85f,19,0xCCD8ECFF,660,1,0);}
    text(s->modal_hint,480,393,16,0x98B6D0FF,674,1,0);
}
static void bindings(const xb_menu_snapshot*s){
    heading("Controls");text(s->help,480,91,13,0xBAD2E7FF,756,1,0);text("Action",115,108,19,teal,250,0,0);text("Controller",763,108,19,teal,200,1,0);rect(92,138,776,1,purple);
    for(unsigned i=0;i<s->row_count&&i<10;i++){const xb_menu_row*r=&s->rows[i];float y=150+i*29;
        if(y>412)break;
        if(!r->value){text(r->label,115,y,24,0x939FEAFF,720,0,0);continue;}
        if(r->selected)rect(103,y-1,765,28,0x333055C0);
        text(r->label,115,y+3,22,r->selected?cyan:teal,480,0,0);
        rect(666,y,195,26,0x262A36FF);border(666,y,195,26,r->selected?cyan:0x535768FF,1);
        text(r->value,763.5f,y+4,20,0xD2D5E7FF,182,1,0);}
    rect(886,150,3,288,0x26243BFF);int total=imax(10,s->total_rows);float thumb=2880.f/total;if(thumb<24)thumb=24;
    float scroll=s->scroll;if(scroll<0)scroll=0;if(scroll>total-10)scroll=total-10;
    rect(884,150+(288-thumb)*scroll/imax(1,total-10),7,thumb,purple);
    rect(92,455,776,2,purple);text("Preset",116,479,25,teal,190,0,0);text(s->preset,850,479,25,cyan,489,2,0);
    text(s->navigation?s->navigation:"Up/Down: action   A: change   B: back   Y: restore section",480,513,16,0x76819FFF,850,1,0);
    binding_modal(s);
}
int xb_menu_canvas_render(const uint8_t*r,const xb_menu_snapshot*s){
    if(!r||!s||!load_font(r))return 0;
    uint32_t background=s->page==XB_MENU_TITLE?0:(s->pause_background||s->page==XB_MENU_PAUSE?0xC0000000:0xFF000000);
    for(unsigned i=0;i<CANVAS_WIDTH*CANVAS_HEIGHT;i++)pixels[i]=background;
    if(s->page!=XB_MENU_TITLE&&!s->pause_background&&s->page!=XB_MENU_PAUSE)stars();
    switch(s->page){
        case XB_MENU_TITLE:title(s);break;case XB_MENU_OPTIONS:options(s);break;
        case XB_MENU_SCHEMES:case XB_MENU_CHEATS:schemes(s);break;
        case XB_MENU_PAUSE:pause_page(s);break;case XB_MENU_PROFILES:profiles(s);break;
        case XB_MENU_BINDINGS:bindings(s);break;default:return 0;
    }
    xb_gpu_canvas(pixels,CANVAS_WIDTH,CANVAS_HEIGHT,background);return 1;
}
