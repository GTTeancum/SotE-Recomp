#include <windows.h>
#include <pbkit/pbkit.h>
#include <pbkit/outer.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include "runtime.h"
#include "texture.h"
#include "gpu.h"
#include "performance.h"
void xb_unsupported(uint32_t id) __attribute__((noreturn));
volatile struct {uint32_t tasks,commands,fills,presents,unknown,pc,w0,w1,color,depth;} sote_video_status;
/* Native capture metadata for the last framebuffer submitted for scanout. */
volatile uint32_t sote_video_capture[4];
/* Diagnostic-only boot-card pixels before presentation: frame, target,
   sampled nonblack pixels, pixel hash. Never enabled in normal gameplay. */
volatile uint32_t sote_boot_pixels[64][4];
volatile uint32_t sote_boot_ownership[6]; /* starts, start collisions, ends, end collisions, front, back */
volatile uint32_t sote_scanout_waits;
volatile uint32_t sote_present_sync[4]; /* target, displayed, wait ms, stage */
volatile uint32_t sote_graphics_tasks[4][3]; /* serial, list address, bytes */
static uint32_t segments[16],other_hi,other_lo,geometry,fill_color,color_size=2,color_width=320;
static uint32_t registers[256][2],move_memory[256];
static float matrices[2][32][4][4];
static unsigned matrix_top[2];
static struct {float color[3],direction[3];} lights[8];
static unsigned light_count;
/* Original F3D/F3DWAVE defaults, as initialized by the retail microcode. */
static float look_at[2][3]={{0,1,0},{1,0,0}};
/* F3DWAVE has a 32-entry vertex cache. Retain the original attributes as
   well as homogeneous coordinates: clipping must happen before division. */
typedef struct {
    float clip[4],st[2];
    uint32_t rgba,valid,clip_mask;
    int16_t position[3];
} xb_vertex;
static xb_vertex vertices[32];
volatile uint32_t sote_vertex_status[4]; /* loads, vertices, first, count */
volatile uint32_t sote_vertex_checks;
volatile uint32_t sote_material_status[8];
/* geometry, other H/L, combine H/L, triangles, clipped, face-culled,
   submitted, all-zero alpha, all-opaque alpha. Echo-only material census. */
volatile uint32_t sote_material_census[32][11];
volatile uint32_t sote_clip_checks;
/* Counts of MOVEMEM/MOVEWORD subcommands, to expose ignored geometry state. */
volatile uint32_t sote_move_mem_counts[256],sote_move_word_counts[256];
static int scissor[4]={0,0,320,240};
/* Last near-fullscreen pause draw: count, kind, float XY bounds, scissor,
   color, other modes, display-list PC, pause state, raw command. */
volatile uint32_t sote_overlay_status[16];
static void observe_overlay(uint8_t* rdram,unsigned kind,float x0,float y0,float x1,float y1,uint32_t color) {
    if(!MEM_W(0,(int32_t)0x800DD2BC) || x0>1 || y0>1 || x1<318 || y1<238)return;
    sote_overlay_status[0]++;sote_overlay_status[1]=kind;
    union {float f;uint32_t u;} value;
    value.f=x0;sote_overlay_status[2]=value.u;value.f=y0;sote_overlay_status[3]=value.u;
    value.f=x1;sote_overlay_status[4]=value.u;value.f=y1;sote_overlay_status[5]=value.u;
    for(unsigned i=0;i<4;i++)sote_overlay_status[6+i]=scissor[i];
    sote_overlay_status[10]=color;sote_overlay_status[11]=other_hi;sote_overlay_status[12]=other_lo;
    sote_overlay_status[13]=sote_video_status.pc;sote_overlay_status[14]=MEM_W(0,(int32_t)0x800DD2BC);
    sote_overlay_status[15]=sote_video_status.w0;
}
static uint32_t current_framebuffer,next_framebuffer;
static int pending;
void xb_menu_draw(uint8_t* rdram);
volatile uint32_t sote_video_trace[64][8];
static unsigned trace_serial,task_rectangles,task_fills;
static void trace(unsigned type,unsigned swap) {
    unsigned serial=++trace_serial;volatile uint32_t* record=sote_video_trace[serial%64];
    record[0]=serial;record[1]=type;record[2]=sote_video_status.color;record[3]=swap;
    record[4]=task_rectangles;record[5]=task_fills;record[6]=(uint32_t)pb_back_buffer();record[7]=sote_video_status.presents;
}
static int test_probe;
static int test_disable_dl_cull;
static int test_flat_lit;
static int test_late_lit;
static xb_gpu_vertex* test_lit_triangles;
static unsigned test_lit_count;
volatile uint32_t sote_lit_probe[12]; /* enabled, collected, last XYZ float bits */
int xb_test_disable_depth;
volatile uint32_t sote_dl_cull_status[3]; /* tested, rejected, diagnostic bypass */
void xb_video_init(void) {
    xb_texture_memory_selftest();
    if(pb_init()!=0)xb_unsupported(0xE5000001);
    /* Select pbkit scanout once. Its vblank queue rotates completed buffers;
       repeating this call would force framebuffer zero back onto scanout. */
    pb_show_front_screen();
    FILE* f=fopen("D:\\xbox-test.cfg","rb");
    if(f){char line[96];while(fgets(line,sizeof(line),f)) {
        if(strstr(line,"video_probe=1"))test_probe=1;
        if(strstr(line,"disable_dl_cull=1"))test_disable_dl_cull=1;
        if(strstr(line,"disable_depth=1"))xb_test_disable_depth=1;
        if(strstr(line,"flat_lit=1"))test_flat_lit=1;
        if(strstr(line,"late_lit=1"))test_late_lit=1;
        if(strstr(line,"profile_renderer=1"))xb_perf_enabled=1;
    }fclose(f);}
    if(test_late_lit){test_lit_triangles=malloc(1024*3*sizeof(xb_gpu_vertex));if(!test_lit_triangles)xb_unsupported(0xE5000010);}
    sote_lit_probe[0]=test_flat_lit|(test_late_lit<<1);
}
void xb_video_wait_vblank(void){pb_wait_for_vbl();}
static uint32_t physical(uint32_t address) {
    /* RSP segment selection uses only four bits, including for 0x8xxxxxxx
       addresses. Preserve wrapping/negative bases used by retail models. */
    unsigned segment=(address>>24)&15;
    uint32_t result=(segments[segment]+(address&0xffffff))&0xffffff;
    if(result>=0x800000)xb_unsupported(0xE5000002);
    return result;
}
static uint32_t word(uint8_t* rdram,uint32_t address) {
    if((address&3)||address>0x800000-4)xb_unsupported(0xE5000002);
    return MEM_W(0,(int32_t)(0x80000000|address));
}
static void matrix(uint8_t* rdram,uint32_t flags,uint32_t address) {
    unsigned which=flags&1,top=matrix_top[which];
    float incoming[4][4],result[4][4];uint32_t p=physical(address);
    for(unsigned i=0;i<16;i++) {
        uint32_t integer=word(rdram,p+(i/2)*4),fraction=word(rdram,p+32+(i/2)*4);
        unsigned shift=(i&1)?0:16;
        incoming[i/4][i%4]=(float)(int16_t)(integer>>shift)+(float)((fraction>>shift)&65535)/65536.0f;
    }
    if(flags&2)__builtin_memcpy_inline(result,incoming,sizeof(result));
    else for(unsigned r=0;r<4;r++)for(unsigned c=0;c<4;c++) {
        result[r][c]=0;
        for(unsigned k=0;k<4;k++)result[r][c]+=incoming[r][k]*matrices[which][top][k][c];
    }
    if(flags&4){if(top==31)xb_unsupported(0xE500000B);matrix_top[which]=++top;}
    __builtin_memcpy_inline(matrices[which][top],result,sizeof(result));
}
static float light_dot(const float* normal,const float* direction) {
    float local[3]={0,0,0},length=0,result=0;
    /* Transform a camera-space light into model space; normalize it there,
       as the RSP does, rather than renormalizing each vertex normal. */
    for(unsigned i=0;i<3;i++) {
        for(unsigned j=0;j<3;j++)local[i]+=matrices[0][matrix_top[0]][i][j]*direction[j];
        length+=local[i]*local[i];
    }
    if(length>0) {
        length=sqrtf(length);
        for(unsigned i=0;i<3;i++)result+=normal[i]*local[i]/length;
    }
    return result;
}
static void load_light(uint8_t* rdram,unsigned type,uint32_t p) {
    if(type<0x82 || type>0x94 || (type&1))return;
    uint32_t direction=word(rdram,p+8),color=word(rdram,p);
    float* dir;
    if(type<=0x84)dir=look_at[(type-0x82)/2];
    else {
        unsigned index=(type-0x86)/2;dir=lights[index].direction;
        for(unsigned i=0;i<3;i++)lights[index].color[i]=(color>>(24-i*8))&255;
    }
    for(unsigned i=0;i<3;i++)dir[i]=(int8_t)(direction>>(24-i*8))/127.0f;
}
static void load_vertices(uint8_t* rdram,uint32_t w0,uint32_t address) {
    /* Same encoding as the retail microcode identified in RT64's F3D_WAVE
       table for Shadows: count is bits 9..15, first index is byte / 5. */
    unsigned encoded=(w0>>16)&255,first=encoded/5,count=(w0>>9)&127;
    uint32_t p=physical(address);
    if(encoded%5 || !count || first+count>32 || p>0x800000-count*16)
        xb_unsupported(0xE5040001);
    for(unsigned i=0;i<count;i++,p+=16) {
        xb_vertex* v=&vertices[first+i];
        uint32_t xy=word(rdram,p),zf=word(rdram,p+4),st=word(rdram,p+8);
        v->position[0]=(int16_t)(xy>>16);v->position[1]=(int16_t)xy;
        v->position[2]=(int16_t)(zf>>16);
        float source[4]={v->position[0],v->position[1],v->position[2],1},eye[4];
        for(unsigned c=0;c<4;c++) {
            eye[c]=0;
            for(unsigned r=0;r<4;r++)eye[c]+=source[r]*matrices[0][matrix_top[0]][r][c];
        }
        for(unsigned c=0;c<4;c++) {
            v->clip[c]=0;
            for(unsigned r=0;r<4;r++)v->clip[c]+=eye[r]*matrices[1][matrix_top[1]][r][c];
        }
        v->st[0]=(int16_t)(st>>16)/32.0f;v->st[1]=(int16_t)st/32.0f;
        v->rgba=word(rdram,p+12);v->valid=1;v->clip_mask=0;
        if(geometry&0x20000) {
            if(light_count>=8)xb_unsupported(0xE5040006);
            float normal[3];
            for(unsigned j=0;j<3;j++)normal[j]=(int8_t)(v->rgba>>(24-j*8))/127.0f;
            float color[3];__builtin_memcpy_inline(color,lights[light_count].color,sizeof(color));
            for(unsigned light=0;light<light_count;light++) {
                float intensity=light_dot(normal,lights[light].direction);
                if(intensity>0)for(unsigned j=0;j<3;j++)color[j]+=intensity*lights[light].color[j];
            }
            v->rgba&=255;
            for(unsigned j=0;j<3;j++) {
                unsigned value=color[j]>255?255:color[j]<0?0:(unsigned)(color[j]+.5f);
                v->rgba|=value<<(24-j*8);
            }
            if(geometry&0x40000)for(unsigned j=0;j<2;j++) {
                float dot=light_dot(normal,look_at[j]);if(dot<-1)dot=-1;if(dot>1)dot=1;
                v->st[j]=(geometry&0x80000)?acosf(-dot)*325.94932f:(dot+1)*512;
            }
        }
        for(unsigned axis=0;axis<3;axis++) {
            if(v->clip[axis]<-v->clip[3])v->clip_mask|=1u<<(axis*2);
            if(v->clip[axis]>v->clip[3])v->clip_mask|=2u<<(axis*2);
        }
    }
    sote_vertex_status[0]++;sote_vertex_status[1]+=count;
    sote_vertex_status[2]=first;sote_vertex_status[3]=count;
}
static int cull_vertices(uint32_t w0,uint32_t w1) {
    /* Original F3D encoding uses 40-byte transformed vertex entries, with
       an exclusive end. See GLideN64 issue 2774's original gbi.h macro;
       BE000000 00000140 tests vertices 0 through 7, not 0 through 8. */
    unsigned start=w0&0xffffff,end=w1;
    if(start%40 || end%40)xb_unsupported(0xE5040002);
    start/=40;end/=40;
    if(!end)end=16; /* (vend + 1) wraps through the original four-bit mask. */
    if(start>=end || end>32)xb_unsupported(0xE5040002);
    unsigned common=63;
    for(unsigned i=start;i<end;i++) {
        if(!vertices[i].valid)xb_unsupported(0xE5040003);
        common&=vertices[i].clip_mask;
    }
    return common!=0;
}
void xb_vertex_selftest(uint8_t* rdram) {
    /* Run before the ROM is decompressed. Exercise signed attributes, an
       offset cache destination, row-vector transforms and exclusive culling. */
    for(unsigned m=0;m<2;m++)for(unsigned i=0;i<4;i++)matrices[m][0][i][i]=1;
    matrices[0][0][3][0]=2;matrices[1][0][0][0]=2;
    for(unsigned i=0;i<8;i++) {
        uint32_t p=0x807f0000+i*16;
        MEM_W(0,(int32_t)p)=(uint32_t)(uint16_t)(int16_t)(i-4)<<16;
        MEM_W(0,(int32_t)(p+4))=0;
        MEM_W(0,(int32_t)(p+8))=0xffc00020; /* S=-2, T=1 */
        MEM_W(0,(int32_t)(p+12))=0x12345678;
    }
    load_vertices(rdram,0x0419107f,0x807f0000);
#define VCHECK(condition) do { if(!(condition))xb_unsupported(0xE5040010+sote_vertex_checks);sote_vertex_checks++; } while(0)
    segments[4]=0xffcb9150;
    VCHECK(physical(0x8452d430)==0x1e6580);
    VCHECK(physical(0x0452d430)==0x1e6580);
    segments[4]=0;
    VCHECK(sote_vertex_status[2]==5 && sote_vertex_status[3]==8 && !vertices[4].valid && !vertices[13].valid);
    for(unsigned i=0;i<8;i++) {
        xb_vertex* v=&vertices[5+i];float x=(int)i*2-4;
        VCHECK(v->clip[0]==x && v->clip[1]==0 && v->clip[2]==0 && v->clip[3]==1);
        VCHECK(v->st[0]==-2 && v->st[1]==1);
        VCHECK(v->rgba==0x12345678);
        VCHECK(v->clip_mask==(x < -1?1u:x > 1?2u:0u));
    }
    VCHECK(cull_vertices(0xBE000000+5*40,7*40));
    VCHECK(!cull_vertices(0xBE000000+5*40,13*40));
    VCHECK(cull_vertices(0xBE000000+9*40,13*40));
    /* Ambient + directional light, alpha retention and F3DWAVE reflection
       axes. Reversing the normal must remove diffuse light, not subtract it. */
    light_count=1;geometry=0x60000;lights[0].direction[0]=1;
    for(unsigned i=0;i<3;i++){lights[0].color[i]=40+i*10;lights[1].color[i]=10+i*10;}
    MEM_W(0,(int32_t)0x807f000c)=0x7f000080;
    load_vertices(rdram,0x0400020f,0x807f0000);
    VCHECK(vertices[0].rgba==0x32465a80);
    VCHECK(vertices[0].st[0]==512 && vertices[0].st[1]==1024);
    /* -127 in the high byte, alpha unchanged. */
    MEM_W(0,(int32_t)0x807f000c)=0x81000080u;
    load_vertices(rdram,0x0400020f,0x807f0000);
    VCHECK(vertices[0].rgba==0x0a141e80);
    VCHECK(vertices[0].st[0]==512 && vertices[0].st[1]==0);
    geometry|=0x80000;
    load_vertices(rdram,0x0400020f,0x807f0000);
    VCHECK(fabsf(vertices[0].st[0]-512)<.01f && vertices[0].st[1]==0);
    float normal[3]={1,0,0},direction[3]={0,1,0};
    matrices[0][0][0][0]=0;matrices[0][0][0][1]=1;
    matrices[0][0][1][0]=-1;matrices[0][0][1][1]=0;
    VCHECK(light_dot(normal,direction)==1);
    geometry=0;light_count=0;memset(lights,0,sizeof(lights));
#undef VCHECK
    memset(matrices,0,sizeof(matrices));memset(vertices,0,sizeof(vertices));
    memset((void*)sote_vertex_status,0,sizeof(sote_vertex_status));
    memset(rdram+0x7f0000,0,128);
}
static void fill(uint32_t w0,uint32_t w1) {
    unsigned cycle=(other_hi>>20)&3;
    if(cycle!=3) {
        /* The licensing text ends with one-cycle rectangles whose blender is
           pixel*ZERO + memory*ONE. They preserve RGB while replacing coverage;
           applying the fill-color register here erases the text. NV2A does not
           store the N64 coverage plane, so there is no color operation to emit.
           N64 VI coverage/AA emulation remains separate unfinished work. */
        unsigned alpha=(other_lo>>26)&3,memory=(other_lo>>22)&3,beta=(other_lo>>18)&3;
        if(cycle==0 && (other_lo&0x4000) && alpha==3 && memory==1 && beta==2)return;
        xb_unsupported(0xE5030005);
    }
    int x0=((w1>>12)&0xfff)/4,y0=(w1&0xfff)/4;
    int x1=((w0>>12)&0xfff)/4+1,y1=(w0&0xfff)/4+1;
    if(x0<scissor[0])x0=scissor[0];if(y0<scissor[1])y0=scissor[1];
    if(x1>scissor[2])x1=scissor[2];if(y1>scissor[3])y1=scissor[3];
    if(x1<=x0 || y1<=y0)return;
    if(color_width!=320)xb_unsupported(0xE5000003);
    if(sote_video_status.color==sote_video_status.depth) {
        xb_gpu_clear_depth(x0*2,y0*2,(x1-x0)*2,(y1-y0)*2);
    } else {
        uint32_t color;
        if(color_size==2) {
            uint16_t v=fill_color;
            unsigned r=(v>>11)&31,g=(v>>6)&31,b=(v>>1)&31;
            color=0xff000000|(((r<<3)|(r>>2))<<16)|(((g<<3)|(g>>2))<<8)|((b<<3)|(b>>2));
        } else if(color_size==3)color=(fill_color>>8)|0xff000000;
        else xb_unsupported(0xE5000004);
        xb_gpu_reserve_commands();
        xb_gpu_flush();
        pb_fill(x0*2,y0*2,(x1-x0)*2,(y1-y0)*2,color);
    }
    sote_video_status.fills++;
    task_fills++;
}
static float shift_coord(float value,unsigned shift){return shift<=10?value/(1u<<shift):value*(1u<<(16-shift));}
typedef struct {float clip[4],st[2],color[4];} clip_vertex;
static float plane_distance(const clip_vertex* v,unsigned plane) {
    return v->clip[3]+((plane&1)?-v->clip[plane/2]:v->clip[plane/2]);
}
static unsigned clip_polygon(const clip_vertex* in,unsigned count,clip_vertex* out,unsigned plane) {
    unsigned used=0;
    for(unsigned i=0;i<count;i++) {
        const clip_vertex* a=&in[i],*b=&in[(i+1)%count];
        float da=plane_distance(a,plane),db=plane_distance(b,plane);
        if(da>=0)out[used++]=*a;
        if((da<0)!=(db<0)) {
            float t=da/(da-db);clip_vertex* c=&out[used++];
            for(unsigned j=0;j<4;j++){c->clip[j]=a->clip[j]+(b->clip[j]-a->clip[j])*t;c->color[j]=a->color[j]+(b->color[j]-a->color[j])*t;}
            for(unsigned j=0;j<2;j++)c->st[j]=a->st[j]+(b->st[j]-a->st[j])*t;
        }
    }
    return used;
}
/* Preserve plane order and interpolation for crossing polygons. Fully inside
   polygons need no copy; wholly outside polygons need no interpolation. */
static unsigned clip_view(clip_vertex polygon[2][12],unsigned* side) {
    unsigned count=3;
    *side=0;
    for(unsigned plane=0;plane<6 && count;plane++) {
        unsigned inside=0,outside=0;
        for(unsigned i=0;i<count;i++) {
            float distance=plane_distance(&polygon[*side][i],plane);
            inside+=distance>=0;outside+=distance<0;
        }
        if(inside==count)continue;
        if(outside==count)return 0;
        /* Unordered distances retain the original clipping behavior too. */
        count=clip_polygon(polygon[*side],count,polygon[*side^1],plane);
        *side^=1;
    }
    return count;
}
void xb_clip_selftest(void) {
    for(unsigned plane=0;plane<6;plane++) {
        clip_vertex in[3]={0},out[12];unsigned axis=plane/2;
        for(unsigned i=0;i<3;i++)in[i].clip[3]=1;
        in[0].clip[axis]=(plane&1)?2:-2;
        in[1].clip[(axis+1)%3]=-.5f;in[2].clip[(axis+1)%3]=.5f;
        in[1].st[0]=in[2].st[0]=2;in[1].color[0]=in[2].color[0]=1;
        unsigned count=clip_polygon(in,3,out,plane);
#define CCHECK(condition) do {if(!(condition))xb_unsupported(0xE5060000+sote_clip_checks);sote_clip_checks++;}while(0)
        CCHECK(count==4);
        for(unsigned i=0;i<count;i++) {
            CCHECK(plane_distance(&out[i],plane)>=0);
            if(plane_distance(&out[i],plane)==0)CCHECK(out[i].st[0]==1 && out[i].color[0]==.5f);
        }
        for(unsigned i=0;i<3;i++)in[i].clip[axis]=(plane&1)?2:-2;
        CCHECK(clip_polygon(in,3,out,plane)==0);
#undef CCHECK
    }
    /* Compare complete clipped geometry and interpolated attributes against
       the original six-plane path, including negative W and boundary points. */
    uint32_t seed=0x534f5445;
    for(unsigned trial=0;trial<512;trial++) {
        clip_vertex reference[2][12]={0},candidate[2][12]={0};
        for(unsigned i=0;i<3;i++) {
            for(unsigned j=0;j<4;j++) {
                seed=seed*1664525u+1013904223u;
                reference[0][i].clip[j]=((int)(seed>>28)-7)*0.25f;
                reference[0][i].color[j]=(i*4+j)*0.125f;
            }
            reference[0][i].st[0]=i*3;reference[0][i].st[1]=i*7;
        }
        __builtin_memcpy_inline(candidate[0],reference[0],3*sizeof(clip_vertex));
        unsigned expected=3,reference_side=0,candidate_side=0;
        for(unsigned plane=0;plane<6 && expected;plane++) {
            expected=clip_polygon(reference[reference_side],expected,reference[reference_side^1],plane);
            reference_side^=1;
        }
        unsigned actual=clip_view(candidate,&candidate_side);
        if(actual!=expected || memcmp(candidate[candidate_side],reference[reference_side],actual*sizeof(clip_vertex)))
            xb_unsupported(0xE5061000+trial);
        sote_clip_checks++;
    }
}
static void triangle_impl(uint8_t* rdram,unsigned a,unsigned b,unsigned c) {
    unsigned indices[3]={a,b,c};clip_vertex polygon[2][12];unsigned count=3,side=0;
    volatile uint32_t* census=0;
    /* The detailed material census is a visual-debug tool, not production
       rendering work. Avoid scanning/updating32 volatile records per triangle
       during normal Echo Base gameplay. Last-material fault telemetry remains. */
    if((test_flat_lit || test_probe) && MEM_H(0,(int32_t)0x8013CE0E)==4)for(unsigned n=0;n<32;n++) {
        volatile uint32_t* r=sote_material_census[n];
        if(!r[5] || (r[0]==geometry && r[1]==other_hi && r[2]==other_lo &&
            r[3]==registers[0xFC][0] && r[4]==registers[0xFC][1])) {
            r[0]=geometry;r[1]=other_hi;r[2]=other_lo;r[3]=registers[0xFC][0];r[4]=registers[0xFC][1];
            r[5]++;census=r;break;
        }
    }
    sote_material_status[0]=geometry;sote_material_status[1]=other_hi;sote_material_status[2]=other_lo;
    sote_material_status[3]=registers[0xFC][0];sote_material_status[4]=registers[0xFC][1];
    sote_material_status[5]=registers[0xBB][0];sote_material_status[6]=registers[0xBB][1];sote_material_status[7]=move_memory[0x80];
    /* Vertex loading already records the six view-plane outcodes. Reject a
       triangle outside the same plane before preparing colors/materials or
       copying/interpolating vertices. Crossing triangles still use clip_view. */
    unsigned outside=63;
    for(unsigned i=0;i<3;i++) {
        if(indices[i]>=32 || !vertices[indices[i]].valid)xb_unsupported(0xE5040003);
        outside&=vertices[indices[i]].clip_mask;
    }
    if(outside){if(census)census[6]++;return;}
    unsigned cycle=(other_hi>>20)&3;
    /* Echo Base: cycle 0 = TEXEL0 * SHADE RGB / SHADE alpha;
       cycle 1 passes COMBINED RGB and alpha through unchanged. */
    int shade_passthrough=registers[0xFC][0]==0xFC127FFF && registers[0xFC][1]==0xFFFFF838 && cycle==1;
    int shade_alpha=registers[0xFC][1]==0x1FFC93FC || registers[0xFC][1]==0xFFFFF9FC || shade_passthrough;
    int mip_blend=registers[0xFC][0]==0xFC26A004 && (registers[0xFC][1]==0x1F1093FF || shade_alpha) && cycle==1;
    /* Cycle 1 TEXEL0 refers to the second fetched tile. This variant
       discards cycle 0's mip interpolation and uses that tile * SHADE. */
    int upper_mip=registers[0xFC][0]==0xFC26A024 && registers[0xFC][1]==0x1F3093FF && cycle==1;
    int texture_shade=((registers[0xFC][0]==0xFC121824 && registers[0xFC][1]==0xFF33FFFF) ||
        (registers[0xFC][0]==0xFC127E24 && registers[0xFC][1]==0xFFFFF9FC)) && cycle<=1;
    /* FCFFE3FF/FFC5FEFF: RGB = primitive, alpha = TEXEL0 alpha.
       The texture is an opacity mask, not a color multiplier. */
    int primitive_modulated=registers[0xFC][0]==0xFCFF97FF && registers[0xFC][1]==0xFF2DFEFF && cycle==0;
    int primitive_mask=(registers[0xFC][0]==0xFCFFE3FF && registers[0xFC][1]==0xFFC5FEFF && cycle==0) || primitive_modulated;
    int texture_alpha=(registers[0xFC][0]==0xFC127E24 && registers[0xFC][1]==0xFFFFF3F9 && cycle==0) ||
        (registers[0xFC][0]==0xFC127FFF && registers[0xFC][1]==0xFFFFF238 && cycle==1);
    int shade_only=registers[0xFC][0]==0xFCFFFFFF &&
        ((registers[0xFC][1]==0xFFFE793C && cycle==0) ||
         (registers[0xFC][1]==0xFFFE7838 && cycle==1));
    /* Suprosa: cycle0 RGB=TEXEL0, alpha=SHADE; cycle1 passes COMBINED.
       Vertex RGB must not tint the texture in this replacement mode. */
    int texture_replace=registers[0xFC][0]==0xFCFFFFFF && registers[0xFC][1]==0xFFFCF838 && cycle==1;
    if(!mip_blend && !upper_mip && !texture_shade && !primitive_mask && !texture_alpha && !shade_only && !shade_passthrough && !texture_replace)xb_unsupported(0xE5050004);
    for(unsigned i=0;i<3;i++) {
        if(indices[i]>=32 || !vertices[indices[i]].valid)xb_unsupported(0xE5040003);
        xb_vertex* v=&vertices[indices[i]];clip_vertex* d=&polygon[0][i];
        __builtin_memcpy_inline(d->clip,v->clip,sizeof(d->clip));__builtin_memcpy_inline(d->st,v->st,sizeof(d->st));
        uint32_t rgba=(geometry&0x200)?v->rgba:vertices[a].rgba;
        if(primitive_mask)rgba=registers[0xFA][1];
        for(unsigned j=0;j<4;j++)d->color[j]=((rgba>>(24-j*8))&255)/255.0f;
    }
    uint64_t clip_timer=xb_perf_begin();
    count=clip_view(polygon,&side);
    xb_perf_end(XB_PERF_CLIPPING,clip_timer);
    if(count<3){if(census)census[6]++;return;}
    uint32_t vp=move_memory[0x80];if(!vp)xb_unsupported(0xE5050005);
    uint32_t scale_xy=word(rdram,vp),scale_z=word(rdram,vp+4),trans_xy=word(rdram,vp+8),trans_z=word(rdram,vp+12);
    float sx=(int16_t)(scale_xy>>16)/4.0f,sy=(int16_t)scale_xy/4.0f;
    float tx=(int16_t)(trans_xy>>16)/4.0f,ty=(int16_t)trans_xy/4.0f;
    float sz=(int16_t)(scale_z>>16)/1024.0f,tz=(int16_t)(trans_z>>16)/1024.0f;
    unsigned tile=(registers[0xBB][0]>>8)&7,levels=((registers[0xBB][0]>>11)&7)+1;
    if(!mip_blend && !upper_mip)levels=1;
    float ss=(registers[0xBB][1]>>16)/65536.0f,ts=(registers[0xBB][1]&65535)/65536.0f;
    xb_tile* td=&xb_tiles[tile];xb_gpu_vertex projected[12];
    for(unsigned i=0;i<count;i++) {
        clip_vertex* v=&polygon[side][i];xb_gpu_vertex* d=&projected[i];
        if(v->clip[3]<=0)return;
        float q=1.0f/v->clip[3];
        d->x=v->clip[0]*q*sx+tx;d->y=-v->clip[1]*q*sy+ty;d->z=v->clip[2]*q*sz+tz;
        d->q=(other_hi&0x80000)?q:1;
        d->s=shift_coord(v->st[0]*ss,td->shift_s)-td->left*0.25f;
        d->t=shift_coord(v->st[1]*ts,td->shift_t)-td->top*0.25f;
        __builtin_memcpy_inline(d->color,v->color,sizeof(d->color));
    }
    for(unsigned i=1;i+1<count;i++) {
        xb_gpu_vertex tri[3]={projected[0],projected[i],projected[i+1]};
        if(shade_only)observe_overlay(rdram,1,
            fminf(tri[0].x,fminf(tri[1].x,tri[2].x)),fminf(tri[0].y,fminf(tri[1].y,tri[2].y)),
            fmaxf(tri[0].x,fmaxf(tri[1].x,tri[2].x)),fmaxf(tri[0].y,fmaxf(tri[1].y,tri[2].y)),vertices[a].rgba);
        float area=(tri[1].x-tri[0].x)*(tri[2].y-tri[0].y)-(tri[1].y-tri[0].y)*(tri[2].x-tri[0].x);
        if(area==0 || ((geometry&0x1000)&&area<0) || ((geometry&0x2000)&&area>0)) {
            if(census)census[7]++;continue;
        }
        if(census) {
            census[8]++;
            if(tri[0].color[3]==0 && tri[1].color[3]==0 && tri[2].color[3]==0)census[9]++;
            if(tri[0].color[3]==1 && tri[1].color[3]==1 && tri[2].color[3]==1)census[10]++;
        }
        if(test_flat_lit && (geometry&0x20000)) {
            for(unsigned j=0;j<3;j++){tri[j].color[0]=1;tri[j].color[1]=0;tri[j].color[2]=1;tri[j].color[3]=1;}
            sote_lit_probe[1]++;
            for(unsigned j=0;j<3;j++)__builtin_memcpy_inline((void*)&sote_lit_probe[2+j*3],&tri[j].x,3*sizeof(float));
            if(test_late_lit && test_lit_count<1024){__builtin_memcpy_inline(test_lit_triangles+test_lit_count*3,tri,sizeof(tri));test_lit_count++;}
            xb_gpu_triangle(tile,0,1,other_lo&~0x5000u,4,scissor,tri);
        } else xb_gpu_triangle(tile,(other_hi>>14)&3,levels,other_lo,texture_replace?7:upper_mip?5:shade_only?4:primitive_modulated?6:primitive_mask?2:texture_alpha?3:shade_alpha,scissor,tri);
    }
}
static void triangle(uint8_t* rdram,unsigned a,unsigned b,unsigned c) {
    uint64_t timer=xb_perf_begin();triangle_impl(rdram,a,b,c);
    xb_perf_end(XB_PERF_TRIANGLE_TOTAL,timer);
}
static unsigned vertex_index(unsigned encoded) {
    if(encoded%5 || encoded/5>=32)xb_unsupported(0xE5040005);
    return encoded/5;
}
static void rectangle(uint8_t* rdram,uint32_t w0,uint32_t w1,uint32_t* pc) {
    uint32_t half1=word(rdram,*pc),st=word(rdram,*pc+4);
    uint32_t half2=word(rdram,*pc+8),step=word(rdram,*pc+12);
    if((half1>>24)!=0xB3 || (half2>>24)!=0xB2)xb_unsupported(0xE5030003);
    *pc+=16;
    /* Both modes multiply RGB by primitive color. Fonts also multiply alpha;
       the intro transition takes TEXEL0 alpha directly. */
    uint32_t color=registers[0xFA][1];
    if(registers[0xFC][0]==0xFC11FE23 && registers[0xFC][1]==0xFFFFF3F9)color|=255;
    else if(registers[0xFC][0]!=0xFC119623 || registers[0xFC][1]!=0xFF2FFFFF)xb_unsupported(0xE5030004);
    unsigned tile=(w1>>24)&7,cycle=(other_hi>>20)&3;int flip=(w0>>24)==0xE5;
    float x0=((w1>>12)&4095)*0.25f,y0=(w1&4095)*0.25f;
    float x1=((w0>>12)&4095)*0.25f,y1=(w0&4095)*0.25f;
    float s=(int16_t)(st>>16)/32.0f,t=(int16_t)st/32.0f;
    float ds=(int16_t)(step>>16)/1024.0f,dt=(int16_t)step/1024.0f;
    if(cycle==2){ds*=0.25f;x1++;y1++;}
    /* The native pause dimmer uses last-pixel coordinates319/239 in a
       one-cycle texture rectangle. On our full640x480 output that leaves
       the final two rows/columns undimmed. Expand only this fullscreen black
       translucent pause draw; preserve glyphs and other rectangle endpoints. */
    if(cycle==0 && MEM_W(0,(int32_t)0x800DD2BC) && (color>>8)==0 &&
        (color&255)>0 && (color&255)<255 && x0==0 && y0==0 && x1==319 && y1==239 &&
        scissor[0]==0 && scissor[1]==0 && scissor[2]==320 && scissor[3]==240) {
        x1=320;y1=240;
    }
    observe_overlay(rdram,2,x0,y0,x1,y1,color);
    float cx0=x0<scissor[0]?scissor[0]:x0,cy0=y0<scissor[1]?scissor[1]:y0;
    float cx1=x1>scissor[2]?scissor[2]:x1,cy1=y1>scissor[3]?scissor[3]:y1;
    if(cx1<=cx0 || cy1<=cy0)return;
    xb_tile* d=&xb_tiles[tile];
    float s0=shift_coord(s+(flip?cy0-y0:cx0-x0)*ds,d->shift_s)-d->left*0.25f;
    float t0=shift_coord(t+(flip?cx0-x0:cy0-y0)*dt,d->shift_t)-d->top*0.25f;
    float s1=shift_coord(s+(flip?cy1-y0:cx1-x0)*ds,d->shift_s)-d->left*0.25f;
    float t1=shift_coord(t+(flip?cx1-x0:cy1-y0)*dt,d->shift_t)-d->top*0.25f;
    xb_gpu_rectangle(tile,(other_hi>>14)&3,color,cx0,cy0,cx1,cy1,s0,t0,s1,t1,flip);
    task_rectangles++;
}
static void list(uint8_t* rdram,uint32_t address,unsigned depth,unsigned* budget) {
    if(depth>=32)xb_unsupported(0xE5000005);
    uint32_t pc=physical(address);
    for(;;) {
        if(!(*budget)--)xb_unsupported(0xE5000006);
        uint32_t w0=word(rdram,pc),w1=word(rdram,pc+4),op=w0>>24;
        sote_video_status.pc=pc;sote_video_status.w0=w0;sote_video_status.w1=w1;
        sote_video_status.commands++;registers[op][0]=w0;registers[op][1]=w1;pc+=8;
        if(xb_texture_command(rdram,w0,op==0xFD?physical(w1):w1))continue;
        switch(op) {
        case 0:case 0xC0:case 0xE6:case 0xE7:case 0xE8:case 0xE9:break; /* Synchronization, no geometry. */
        case 0x06: if((w0>>16)&1){pc=physical(w1);}else list(rdram,w1,depth+1,budget);break;
        case 0xB8:return;
        case 0x01:matrix(rdram,(w0>>16)&255,w1);break;
        case 0x04:{uint64_t timer=xb_perf_begin();load_vertices(rdram,w0,w1);xb_perf_end(XB_PERF_VERTICES,timer);break;}
        case 0xBE:
            sote_dl_cull_status[0]++;
            if(cull_vertices(w0,w1)) {
                sote_dl_cull_status[1]++;
                if(!test_disable_dl_cull)return;
                sote_dl_cull_status[2]++;
            }
            break;
        case 0xBF:triangle(rdram,vertex_index((w1>>16)&255),vertex_index((w1>>8)&255),vertex_index(w1&255));break;
        case 0xB1:
            triangle(rdram,vertex_index((w0>>16)&255),vertex_index((w0>>8)&255),vertex_index(w0&255));
            triangle(rdram,vertex_index((w1>>16)&255),vertex_index((w1>>8)&255),vertex_index(w1&255));break;
        case 0xB5: {
            unsigned a=vertex_index(w1>>24),b=vertex_index((w1>>16)&255),c=vertex_index((w1>>8)&255),d=vertex_index(w1&255);
            triangle(rdram,a,b,c);triangle(rdram,a,c,d);break;
        }
        case 0xBD: {
            unsigned which=w1&1;
            if(!matrix_top[which])xb_unsupported(0xE500000C);
            matrix_top[which]--;break;
        }
        case 0x03: {
            unsigned type=(w0>>16)&255;sote_move_mem_counts[type]++;
            move_memory[type]=physical(w1);load_light(rdram,type,move_memory[type]);break;
        }
        case 0xBC:
            sote_move_word_counts[w0&255]++;
            if((w0&255)==6){unsigned s=((w0>>8)&0xffff)/4;if(s>=16)xb_unsupported(0xE5000007);segments[s]=w1;}
            else if((w0&255)==2) {
                if(w1<0x80000020 || ((w1-0x80000000)&31))xb_unsupported(0xE5040006);
                light_count=((w1-0x80000000)>>5)-1;if(light_count>=8)xb_unsupported(0xE5040006);
            } else if((w0&255)==10) {
                unsigned index=((w0>>8)&65535)/32;if(index>=8)xb_unsupported(0xE5040006);
                for(unsigned i=0;i<3;i++)lights[index].color[i]=(w1>>(24-i*8))&255;
            }
            break;
        case 0xB6:geometry&=~w1;break;
        case 0xB7:geometry|=w1;break;
        case 0xBA:case 0xB9: {
            unsigned shift=(w0>>8)&255,length=w0&255;
            if(!length || length>32 || shift+length>32)xb_unsupported(0xE5000008);
            uint32_t mask=(uint32_t)(((1ULL<<length)-1)<<shift);
            uint32_t* state=op==0xBA?&other_hi:&other_lo;*state=(*state&~mask)|(w1&mask);break;
        }
        case 0xBB:case 0xB3:case 0xB4:case 0xFC:case 0xF8:case 0xF9:case 0xFA:case 0xFB:break; /* State retained above. */
        case 0xED:scissor[0]=((w0>>12)&0xfff)/4;scissor[1]=(w0&0xfff)/4;
            scissor[2]=((w1>>12)&0xfff)/4;scissor[3]=(w1&0xfff)/4;break;
        case 0xFE:sote_video_status.depth=physical(w1);break;
        case 0xFF:sote_video_status.color=physical(w1);color_size=(w0>>19)&3;color_width=(w0&0xfff)+1;break;
        case 0xF7:fill_color=w1;break;
        case 0xF6:fill(w0,w1);break;
        case 0xE4:case 0xE5:rectangle(rdram,w0,w1,&pc);break;
        default:sote_video_status.unknown=op;xb_unsupported(0xE5010000|op);
        }
    }
}
void xb_video_task(uint8_t* rdram,const uint32_t* task) {
    if(task[0]!=1)xb_unsupported(0xE5000009);
    xb_perf_scene(MEM_HU(0,(int32_t)0x8013CE0E));
    unsigned serial=sote_video_status.tasks+1;
    sote_graphics_tasks[serial%4][0]=serial;
    sote_graphics_tasks[serial%4][1]=task[12];
    sote_graphics_tasks[serial%4][2]=task[13];
    /* The game can submit multiple graphics tasks before requesting a VI
       swap. Keep their drawing in order on the same pending backbuffer. */
    if(!pending){
        /* Local pbkit fixes dropped presentations. Keep asynchronous drawing,
           but never clear a buffer that the display still owns. */
        unsigned target=(uint32_t)pb_back_buffer()&0x03ffffff;
        unsigned wait_start=GetTickCount();
        sote_present_sync[0]=target;sote_present_sync[3]=1;
        if(sote_video_status.presents && VIDEOREG(PCRTC_START)==target) {
            sote_scanout_waits++;
            while(VIDEOREG(PCRTC_START)==target) {
                sote_present_sync[1]=VIDEOREG(PCRTC_START);
                sote_present_sync[2]=GetTickCount()-wait_start;
                if(sote_present_sync[2]>2000)xb_unsupported(0xE5000010);
                Sleep(1);
            }
        }
        sote_present_sync[3]=0;
        xb_gpu_invalidate_state();pb_reset();pb_target_back_buffer();test_lit_count=0;
    }
    if(!pending && MEM_HU(0,(int32_t)0x8013CE0E)==0) {
        unsigned front=VIDEOREG(PCRTC_START);
        unsigned back=(uint32_t)pb_back_buffer()&0x03ffffff;
        sote_boot_ownership[0]++;sote_boot_ownership[1]+=front==back;
        sote_boot_ownership[4]=front;sote_boot_ownership[5]=back;
    }
    unsigned budget=100000;
    task_rectangles=task_fills=0;
    uint64_t timer=xb_perf_begin();
    list(rdram,task[12],0,&budget);
    xb_perf_end(XB_PERF_DISPLAY_LIST,timer);
    timer=xb_perf_begin();
    xb_gpu_flush();
    while(pb_busy())Sleep(0);
    xb_perf_end(XB_PERF_TASK_WAIT,timer);
    pending=1;sote_video_status.tasks++;
    trace(1,0);
}
void osViSwapBuffer_recomp(uint8_t* rdram,recomp_context* ctx) {
    (void)rdram;next_framebuffer=(uint32_t)ctx->r4;
    trace(2,next_framebuffer);
    if(pending) {
        if(test_late_lit){const int full[4]={0,0,320,240};for(unsigned i=0;i<test_lit_count;i++)xb_gpu_triangle(0,0,1,0,4,full,test_lit_triangles+i*3);}
        xb_menu_draw(rdram);
        xb_gpu_flush();
        if(test_probe){pb_fill(0,0,8,8,0xffff0000);while(pb_busy())Sleep(0);}
        sote_video_capture[0]=(uint32_t)pb_back_buffer();
        sote_video_capture[1]=pb_back_buffer_width();sote_video_capture[2]=pb_back_buffer_height();
        sote_video_capture[3]=pb_back_buffer_pitch();
        if(MEM_HU(0,(int32_t)0x8013CE0E)==0) {
            sote_boot_ownership[2]++;
            sote_boot_ownership[3]+=VIDEOREG(PCRTC_START)==((uint32_t)pb_back_buffer()&0x03ffffff);
        }
        if(xb_perf_enabled && MEM_HU(0,(int32_t)0x8013CE0E)==0) {
            while(pb_busy())Sleep(0);
            const volatile DWORD* surface=pb_back_buffer();
            unsigned stride=pb_back_buffer_pitch()/4,count=0,hash=2166136261u;
            for(unsigned y=0;y<pb_back_buffer_height();y+=4)
                for(unsigned x=0;x<pb_back_buffer_width();x+=4) {
                    unsigned pixel=surface[y*stride+x]&0xffffff;
                    count+=pixel!=0;hash=(hash^pixel)*16777619u;
                }
            volatile uint32_t* record=sote_boot_pixels[sote_video_status.presents%64];
            record[0]=sote_video_status.presents;record[1]=(uint32_t)surface;
            record[2]=count;record[3]=hash;
        }
        uint64_t timer=xb_perf_begin();
        while(pb_finished())Sleep(0);
        xb_perf_end(XB_PERF_PRESENT_WAIT,timer);
        current_framebuffer=next_framebuffer;pending=0;sote_video_status.presents++;
        xb_perf_present();
    }
}
void osViGetCurrentFramebuffer_recomp(uint8_t* rdram,recomp_context* ctx){(void)rdram;ctx->r2=(int32_t)current_framebuffer;}
void osViGetNextFramebuffer_recomp(uint8_t* rdram,recomp_context* ctx){(void)rdram;ctx->r2=(int32_t)next_framebuffer;}
