#include <windows.h>
#include <xboxkrnl/xboxkrnl.h>
#include <pbkit/pbkit.h>
#include <string.h>
#include "gpu.h"
#include "starfield.h"
#include "aspect_xbox.h"
#include "texture.h"
#include "hd_texture.h"
#include "hardware_test.h"
#include "texture_window.h"
#include "native_gpu_budget.h"
#include "console_budget.h"
#include "performance.h"
#include "mip_stream.h"
void xb_unsupported(uint32_t id) __attribute__((noreturn));
extern int xb_test_disable_depth;
static uint32_t* pixels;
static void ensure_linear_upload(void) {
    if(pixels)return;
    pixels=xb_budget_native_gpu_allocate(XB_BUDGET_LINEAR);
    if(!pixels)xb_unsupported(0xE5030001);
}
static unsigned cached_generation=~0u,cached_tile=~0u,cached_tlut,width,height,pitch;
/* Small HUD rectangles fit above the largest native rectangle in the existing
   linear allocation. A full-resolution menu canvas owns this space while drawn. */
enum { RECT_SLOT_WORDS=4096, RECT_SLOTS=(XB_LINEAR_UPLOAD_WORDS-XB_RECT_MAX_WORDS)/RECT_SLOT_WORDS };
static struct {
    unsigned hash,tlut,stamp;
    xb_tile descriptor;
    uint8_t memory[4096] __attribute__((aligned(16)));
} rect_cache[RECT_SLOTS];
static unsigned rect_stamp;
static int canvas_owns_rect_slots;
static uint32_t* rect_image;
volatile struct {uint32_t uploads,rectangles;} sote_gpu_status;
volatile uint32_t sote_triangle_status[3]; /* submitted, mip uploads, state submissions */
static uint32_t triangle_state[12];
static int triangle_state_valid,triangle_state_ui;
static int rectangle_state_valid;
/* Fixed-size aligned state equality avoids the SDK byte-at-a-time memcmp. */
static int same_triangle_state(const uint32_t* a,const uint32_t* b) {
    for(unsigned i=0;i<12;i+=4)
        if((a[i]^b[i])|(a[i+1]^b[i+1])|(a[i+2]^b[i+2])|(a[i+3]^b[i+3]))return 0;
    return 1;
}
/* GPU-readable ring; never overwrite vertices until all prior reads retire. */
#define VERTEX_RING_COUNT XB_VERTEX_RING_COUNT
typedef struct {float position[4],color[4],texture[4];} array_vertex;
_Static_assert(sizeof(array_vertex)==XB_VERTEX_STRIDE,"native vertex stride/budget mismatch");
static array_vertex* vertex_ring;
static unsigned vertex_cursor,batch_start,batch_count;
/* 120 packed words fit one pbkit block with BEGIN, END and an odd index. */
static uint16_t world_indices[240];
static unsigned index_count;
static int array_layout_set,arrays_enabled;
#ifdef SOTE_XBOX
void xb_pb_end_packet(uint32_t* end);
void xb_pb_publish(void);
#else
#define xb_pb_end_packet pb_end
#define xb_pb_publish() ((void)0)
#endif
static uint64_t ring_generation=1;
volatile uint32_t sote_gpu_batches;
int xb_gpu_verify_inline;
int xb_gpu_verify_unfogged;
static void flush_inline(unsigned start,unsigned count){
    /* Inline vertices bypass the DMA vertex-array fetch and its cache. */
    while(count){
        unsigned n=count>9?9:count;
        uint32_t* p=pb_begin();
        p=pb_push1(p,NV097_SET_BEGIN_END,NV097_SET_BEGIN_END_OP_TRIANGLES);
        pb_push(p++,0x40000000|NV097_INLINE_ARRAY,n*12);
        memcpy(p,&vertex_ring[start],n*sizeof(array_vertex));p+=n*12;
        p=pb_push1(p,NV097_SET_BEGIN_END,NV097_SET_BEGIN_END_OP_END);
        pb_end(p);
        start+=n;count-=n;
    }
}
static void queue_vertex_batch(void) {
    if(!batch_count && !index_count)return;
    /* pb_start fences these WC vertex writes immediately before publishing
       the corresponding commands. Keep the compiler on this side of it. */
    __asm__ volatile("" ::: "memory");
    uint32_t* p=pb_begin();
    /* The CPU appended new vertices. NV2A can reuse stale prefetched data
       even before the ring wraps. Retire its vertex cache before DRAW_ARRAYS. */
    p=pb_push1(p,NV097_BREAK_VERTEX_BUFFER_CACHE,0);
    const unsigned attributes[3]={0,3,9};
    if(!array_layout_set){
        pb_push(p++,NV097_SET_VERTEX_DATA_ARRAY_FORMAT,16);
        for(unsigned i=0;i<16;i++)*p++=NV097_SET_VERTEX_DATA_ARRAY_FORMAT_TYPE_F;
        for(unsigned i=0;i<3;i++)p=pb_push1(p,NV097_SET_VERTEX_DATA_ARRAY_OFFSET+attributes[i]*4,
                                            ((uint32_t)vertex_ring+i*16)&0x03ffffff);
        array_layout_set=1;
    }
    if(!arrays_enabled){
        for(unsigned i=0;i<3;i++)p=pb_push1(p,NV097_SET_VERTEX_DATA_ARRAY_FORMAT+attributes[i]*4,
            NV097_SET_VERTEX_DATA_ARRAY_FORMAT_TYPE_F|(4<<4)|(sizeof(array_vertex)<<8));
        arrays_enabled=1;
    }
    if(index_count){
        xb_pb_end_packet(p);
        p=pb_begin();
        p=pb_push1(p,NV097_SET_BEGIN_END,NV097_SET_BEGIN_END_OP_TRIANGLES);
        pb_push(p++,0x40000000|NV097_ARRAY_ELEMENT16,index_count/2);
        for(unsigned i=0;i+1<index_count;i+=2)
            *p++=world_indices[i]|((uint32_t)world_indices[i+1]<<16);
        if(index_count&1)p=pb_push1(p,NV097_ARRAY_ELEMENT32,world_indices[index_count-1]);
        p=pb_push1(p,NV097_SET_BEGIN_END,NV097_SET_BEGIN_END_OP_END);
        xb_pb_end_packet(p);
        p=pb_begin();
    }else if(xb_gpu_verify_inline){
        pb_end(p);
        flush_inline(batch_start,batch_count);
        p=pb_begin();
    }else{
        p=pb_push1(p,NV097_SET_BEGIN_END,NV097_SET_BEGIN_END_OP_TRIANGLES);
        /* 255 is a whole number of triangles and fits DRAW_ARRAYS' count field. */
        for(unsigned start=batch_start,left=batch_count;left;) {
            unsigned count=left>255?255:left;
            p=pb_push1(p,0x40000000|NV097_DRAW_ARRAYS,((count-1)<<24)|start);
            start+=count;left-=count;
        }
        p=pb_push1(p,NV097_SET_BEGIN_END,NV097_SET_BEGIN_END_OP_END);
    }
    xb_pb_end_packet(p);batch_count=0;index_count=0;sote_gpu_batches++;
}
void xb_gpu_flush(void){
    queue_vertex_batch();
    xb_pb_publish();
}
void xb_gpu_invalidate_state(void){
    xb_gpu_flush();triangle_state_valid=0;rectangle_state_valid=0;
    if(arrays_enabled){
        uint32_t* p=pb_begin();
        const unsigned attributes[3]={0,3,9};
        for(unsigned i=0;i<3;i++)p=pb_push1(p,NV097_SET_VERTEX_DATA_ARRAY_FORMAT+attributes[i]*4,
            NV097_SET_VERTEX_DATA_ARRAY_FORMAT_TYPE_F);
        pb_end(p);arrays_enabled=0;
    }
}
static uint32_t* mip_pixels;
static unsigned mip_generation=~0u,mip_tile=~0u,mip_tlut,mip_levels,mip_format,mip_width,mip_height;
/* A rectangle can reuse GPU backing behind the triangle fast-path pointer.
   Submit old readers, invalidate both cached binding and state, then caller
   retires NV2A before overwriting. The next bind must establish all state. */
void xb_gpu_hd_prepare_write(void) {
    xb_gpu_invalidate_state();
    mip_tile=~0u;
}
static int mip_origin_s;
static unsigned mip_window_width;
volatile uint32_t sote_texture_s_window[4]; /* draws, signed origin, width, failures */
/* 4 MiB GPU cache. Compare all TMEM pages the tile can read, including
   palettes and odd-row banks. Unbounded footprints compare all of TMEM. */
#define MIP_CACHE_SLOTS XB_MIP_CACHE_SLOTS
#define MIP_CACHE_WORDS XB_MIP_CACHE_WORDS
static unsigned native_slots;
static uint32_t* mip_cache_pixels;
static uint32_t* mip_fallback;
static uint32_t mip_scratch[MIP_CACHE_WORDS];
/* diagnostic totals after a completed native oversize stream: uploads, blocks,
   last words, largest upload, cumulative words. No per-pixel probes. */
volatile uint32_t sote_mip_chunked[5];
static struct {
    unsigned valid,hash,tlut,levels,stamp,next,window_width;
    int origin_s;
    uint8_t memory[4096] __attribute__((aligned(16)));
    xb_tile tiles[8];
} mip_cache[MIP_CACHE_SLOTS];
static unsigned mip_cache_stamp;
/* Buckets/links store slot+1; zero terminates a chain. Exact comparisons
   remain mandatory, so hash collisions cannot select the wrong texture. */
static unsigned mip_cache_buckets[256];
volatile uint32_t sote_texture_cache_status[4]; /* hits, misses, evictions, oversized */
static unsigned texture_hash(const void* data,unsigned bytes,unsigned hash) {
    const uint8_t* p=data;
    for(unsigned i=0;i<bytes;i+=4)hash=(hash^xb_texture_read_word(p+i))*16777619u;
    return hash;
}
volatile uint32_t sote_gpu_flushes;
static uint32_t* window_clip(uint32_t* p,int left,int top,int right,int bottom) {
    /* NV2A accepts the union of all eight clip windows. pbkit initializes
       every window to fullscreen, so changing only window zero does not
       actually restrict drawing. Keep all eight identical. */
    p=pb_push1(p,NV097_SET_WINDOW_CLIP_TYPE,0);
    pb_push(p++,NV097_SET_WINDOW_CLIP_HORIZONTAL,8);
    for(unsigned i=0;i<8;i++)*p++=left|((right-1)<<16);
    pb_push(p++,NV097_SET_WINDOW_CLIP_VERTICAL,8);
    for(unsigned i=0;i<8;i++)*p++=top|((bottom-1)<<16);
    return p;
}
static void upload_program(unsigned start,const uint32_t* code,unsigned words) {
    uint32_t* p=pb_begin();
    p=pb_push1(p,NV097_SET_TRANSFORM_PROGRAM_LOAD,start);
    pb_end(p);
    for(unsigned i=0;i<words;){
        unsigned end=i+64<words?i+64:words;
        p=pb_begin();
        for(;i<end;i+=4)p=pb_push4(p,NV097_SET_TRANSFORM_PROGRAM,code[i],code[i+1],code[i+2],code[i+3]);
        pb_end(p);
    }
}
static void shader(int world) {
    static int selected=-1;
    if(selected==world)return;
    xb_gpu_invalidate_state();
    uint64_t timer=xb_perf_begin();
    static const uint32_t screen_program[]={
#include "screen.inl"
    };
    static const uint32_t world_program[]={
#include "world.inl"
    };
    enum { world_start=sizeof(screen_program)/16 };
    _Static_assert(world_start+sizeof(world_program)/16<=136,"NV2A vertex program memory");
    uint32_t* p;
    if(selected<0){
        upload_program(0,screen_program,sizeof(screen_program)/4);
        upload_program(world_start,world_program,sizeof(world_program)/4);
        p=pb_begin();
        p=pb_push1(p,NV097_SET_TRANSFORM_EXECUTION_MODE,6);
        p=pb_push1(p,NV097_SET_TRANSFORM_PROGRAM_CXT_WRITE_EN,0);
        p=pb_push1(p,NV097_SET_TRANSFORM_CONSTANT_LOAD,96);
        p=pb_push4f(p,NV097_SET_TRANSFORM_CONSTANT,pb_back_buffer_width()/320.0f,pb_back_buffer_height()/240.0f,16777215,1);
        pb_end(p);
    }
    p=pb_begin();
    p=pb_push1(p,NV097_SET_TRANSFORM_PROGRAM_START,world?world_start:0);
    pb_end(p);
    selected=world;
    xb_perf_end(XB_PERF_SHADER,timer);
}
void xb_gpu_clear_depth(int x,int y,int width,int height) {
    xb_gpu_invalidate_state();
    /* A preceding HUD rectangle disables depth writes. A depth clear must
       establish its own write mask and clip region, independent of that draw. */
    uint32_t* p=pb_begin();
    p=window_clip(p,x,y,x+width,y+height);
    p=pb_push1(p,NV097_SET_DEPTH_MASK,1);
    pb_end(p);
    pb_erase_depth_stencil_buffer(x,y,width,height);
}
static uint32_t* rectangle_color(uint32_t* p,uint32_t color) {
    return pb_push4f(p,NV097_SET_VERTEX_DATA4F_M+3*16,(color>>24)/255.0f,((color>>16)&255)/255.0f,((color>>8)&255)/255.0f,(color&255)/255.0f);
}
static uint32_t* vertex(uint32_t* p,float x,float y,float s,float t,uint32_t color) {
    if(xb_gpu_verify_inline)p=rectangle_color(p,color);
    p=pb_push4f(p,NV097_SET_VERTEX_DATA4F_M+9*16,s,t,0,1);
    return pb_push4f(p,NV097_SET_VERTEX_DATA4F_M,x,y,0,1);
}
static void draw_rectangle(const uint32_t* image,unsigned enhanced_format,uint32_t color,
    float x0,float y0,float x1,float y1,float s0,float t0,float s1,float t1,int flip);
volatile uint32_t sote_hd_rect_status[2]; /* attempts, successful submissions */

/* Track pending readers of the shared linear upload buffer in 4 KiB pages.
   A CPU upload to unused pages can overlap NV2A work. Clear the reader set
   only after the same FIFO/PGRAPH retirement used by the original path. */
enum { LINEAR_PAGE_WORDS=1024, LINEAR_MASKS=(XB_LINEAR_UPLOAD_WORDS+32767)/32768 };
static uint32_t linear_readers[LINEAR_MASKS];
static unsigned linear_cursor;
static void linear_range(const uint32_t* image,unsigned words,int writing){
    unsigned begin=(unsigned)(image-pixels)/LINEAR_PAGE_WORDS;
    unsigned end=((unsigned)(image-pixels)+words-1)/LINEAR_PAGE_WORDS;
    for(unsigned i=begin/32;i<=end/32;i++){
        uint32_t mask=~0u;
        if(i==begin/32)mask&=~0u<<(begin&31);
        if(i==end/32)mask&=~0u>>(31-(end&31));
        if(!writing)linear_readers[i]|=mask;
        else if(linear_readers[i]&mask){
            uint64_t timer=xb_perf_begin();
            while(pb_busy())Sleep(0);
            xb_perf_end(XB_PERF_TEXTURE_WAIT,timer);
            __builtin_memset_inline(linear_readers,0,sizeof(linear_readers));
            break;
        }
    }
}

static uint32_t* linear_rect_storage(unsigned tile,unsigned tlut,int* upload) {
    *upload=1;
    if(pitch*height>RECT_SLOT_WORDS){
        unsigned words=(pitch*height+31)&~31u;
        if(words>XB_RECT_MAX_WORDS-linear_cursor)linear_cursor=0;
        uint32_t* image=pixels+linear_cursor;
        linear_cursor+=words;
        return image;
    }
    const xb_tile* d=&xb_tiles[tile];
    unsigned pages=xb_texture_pages(d,width,height,0);
    unsigned hash=texture_hash(d,sizeof(*d),xb_texture_page_hash(pages));
    unsigned slot=0;
    for(unsigned i=0;i<RECT_SLOTS;i++) {
        if(rect_cache[i].stamp && rect_cache[i].hash==hash && rect_cache[i].tlut==tlut &&
           !memcmp(d,&rect_cache[i].descriptor,sizeof(*d)) && xb_texture_pages_match(rect_cache[i].memory,pages)) {
            slot=i;*upload=0;break;
        }
        if(rect_cache[i].stamp<rect_cache[slot].stamp)slot=i;
    }
    if(*upload) {
        rect_cache[slot].hash=hash;rect_cache[slot].tlut=tlut;
        rect_cache[slot].descriptor=*d;
        memcpy(rect_cache[slot].memory,xb_texture_memory(),4096);
    }
    rect_cache[slot].stamp=++rect_stamp;
    return pixels+XB_RECT_MAX_WORDS+slot*RECT_SLOT_WORDS;
}

void xb_gpu_rectangle(unsigned tile,unsigned tlut,uint32_t color,float x0,float y0,
    float x1,float y1,float s0,float t0,float s1,float t1,int flip) {
    xb_gpu_flush();
    xb_tile* d=&xb_tiles[tile];
    if(d->right<d->left || d->bottom<d->top)xb_unsupported(0xE5030002);
    const unsigned logical_width=((d->right-d->left)>>2)+1;
    const unsigned logical_height=((d->bottom-d->top)>>2)+1;
    uint32_t* enhanced;
    unsigned enhanced_format;
    sote_hd_rect_status[0]++;
    int matched=xb_hd_rectangle(tile,tlut,logical_width,logical_height,rectangle_state_valid && !xb_gpu_verify_inline,
                                &enhanced,&enhanced_format);
    if(xb_trace_preview_tiles && d->format==2 && (logical_width==32 || logical_width==64)) {
        extern volatile uint32_t sote_hd_status[];
        xb_hardware_tile(tile,tlut,logical_width,logical_height,x0,y0,matched?(int)sote_hd_status[9]:-1);
    }
    if(matched) {
        /* RDP rectangles have already been clipped and shifted in texels.
           Swizzled NV2A textures instead use normalized coordinates. */
        const float u=1.0f/logical_width,v=1.0f/logical_height;
        draw_rectangle(enhanced,enhanced_format,color,x0,y0,x1,y1,s0*u,t0*v,s1*u,t1*v,flip);
        sote_hd_rect_status[1]++;
        if(xb_wide_projection)sote_geometry[5]++;
        return;
    }
    ensure_linear_upload();
    if(cached_generation!=xb_texture_generation || cached_tile!=tile || cached_tlut!=tlut) {
        width=logical_width;height=logical_height;
        if(!xb_linear_rect(width,height,&pitch))xb_unsupported(0xE5030002);
        if(canvas_owns_rect_slots) {
            for(unsigned i=0;i<RECT_SLOTS;i++)rect_cache[i].stamp=0;
            canvas_owns_rect_slots=0;
        }
        int upload;
        rect_image=linear_rect_storage(tile,tlut,&upload);
        if(upload) {
            linear_range(rect_image,pitch*height,1);
            uint64_t decode_timer=xb_perf_begin();
            xb_texture_decode_linear(rect_image,tile,tlut,width,height,pitch);
            __asm__ volatile("sfence" ::: "memory");
            xb_perf_end(XB_PERF_TEXTURE_DECODE,decode_timer);
            sote_gpu_status.uploads++;
        }
        cached_generation=xb_texture_generation;cached_tile=tile;cached_tlut=tlut;
    }
    draw_rectangle(rect_image,0,color,x0,y0,x1,y1,s0,t0,s1,t1,flip);
    if(xb_wide_projection)sote_geometry[5]++;
}
static void draw_rectangle(const uint32_t* image,unsigned enhanced_format,uint32_t color,
    float x0,float y0,float x1,float y1,float s0,float t0,float s1,float t1,int flip) {
    if(!enhanced_format)linear_range(image,pitch*height,0);
    /* Each rectangle still binds its own texture. The screen shader, clip,
       blend and combiner state can span adjacent rectangle draws. */
    uint32_t* p;
    if(!rectangle_state_valid || xb_gpu_verify_inline) {
        xb_gpu_invalidate_state();
        shader(0);
        p=pb_begin();
        p=pb_push1(p,NV097_SET_TRANSFORM_CONSTANT_LOAD,97);
        p=pb_push4f(p,NV097_SET_TRANSFORM_CONSTANT,1,1,1,1);
        /* Rectangles have already been clipped in RDP coordinates on the CPU. */
        p=window_clip(p,0,0,pb_back_buffer_width(),pb_back_buffer_height());
        p=pb_push1(p,NV097_SET_LIGHTING_ENABLE,0);
        p=pb_push1(p,NV097_SET_CULL_FACE_ENABLE,0);
        p=pb_push1(p,NV097_SET_DEPTH_TEST_ENABLE,0);
        p=pb_push1(p,NV097_SET_DEPTH_MASK,0);
        p=pb_push1(p,NV097_SET_ALPHA_TEST_ENABLE,0);
        p=pb_push1(p,NV097_SET_BLEND_ENABLE,1);
        p=pb_push1(p,NV097_SET_BLEND_FUNC_SFACTOR,0x302);
        p=pb_push1(p,NV097_SET_BLEND_FUNC_DFACTOR,0x303);
        p=pb_push1(p,NV097_SET_BLEND_EQUATION,0x8006);
        p=pb_push1(p,NV097_SET_COLOR_MASK,0x01010101);
        for(unsigned i=1;i<4;i++)p=pb_push1(p,NV097_SET_TEXTURE_CONTROL0+i*64,0);
        p=pb_push1(p,NV097_SET_SHADER_STAGE_PROGRAM,1);
        p=pb_push1(p,NV097_SET_SHADER_OTHER_STAGE_INPUT,0);
        /* Register combiner: R0 = texture0 * primary color, including alpha. */
        p=pb_push1(p,NV097_SET_COMBINER_CONTROL,1);
        p=pb_push1(p,NV097_SET_COMBINER_COLOR_ICW,0x08040000);
        p=pb_push1(p,NV097_SET_COMBINER_ALPHA_ICW,0x18140000);
        p=pb_push1(p,NV097_SET_COMBINER_COLOR_OCW,0xC0);
        p=pb_push1(p,NV097_SET_COMBINER_ALPHA_OCW,0xC0);
        p=pb_push1(p,NV097_SET_COMBINER_SPECULAR_FOG_CW0,0x0000000C);
        p=pb_push1(p,NV097_SET_COMBINER_SPECULAR_FOG_CW1,0x00001C00);
        pb_end(p);
        rectangle_state_valid=1;
    }
    p=pb_begin();
    p=pb_push1(p,NV097_SET_TEXTURE_OFFSET,(uint32_t)image&0x03ffffff);
    p=pb_push1(p,NV097_SET_TEXTURE_FORMAT,enhanced_format?enhanced_format:1|8|0x20|(0x12<<8)|(1<<16));
    p=pb_push1(p,NV097_SET_TEXTURE_ADDRESS,0x00030303); /* clamp to edge */
    p=pb_push1(p,NV097_SET_TEXTURE_CONTROL0,1<<30);
    /* NV2A validates the kernel selector even for nearest and linear filtering. */
    p=pb_push1(p,NV097_SET_TEXTURE_FILTER,0x01012000); /* nearest, matching native texels */
    if(!enhanced_format) {
        p=pb_push1(p,NV097_SET_TEXTURE_CONTROL1,pitch*4<<16);
        p=pb_push1(p,NV097_SET_TEXTURE_IMAGE_RECT,(width<<16)|height);
    }
    if(xb_gpu_verify_inline){pb_end(p);p=pb_begin();}
    else p=rectangle_color(p,color);
    p=pb_push1(p,NV097_SET_BEGIN_END,NV097_SET_BEGIN_END_OP_TRIANGLE_STRIP);
    if(flip==XB_RECT_EDGE_EXTEND || flip==XB_RECT_EDGE_EXTEND_BOTTOM){
        /* Eight vertices fit the 128-word packet, including the texture bind.
           The reference writes color at every vertex in its separate packet.
           One triangle strip preserves authored UVs in the central 4:3 area. */
        const float xs[4]={0,x0,x1,320},us[4]={s0,s0,s1,s1};
        for(unsigned j=0;j<4;j++){p=vertex(p,xs[j],y0,us[j],t0,color);p=vertex(p,xs[j],y1,us[j],t1,color);}
    }else{
    p=vertex(p,x0,y0,s0,t0,color);
    p=vertex(p,x1,y0,flip?s0:s1,flip?t1:t0,color);
    p=vertex(p,x0,y1,flip?s1:s0,flip?t0:t1,color);
    p=vertex(p,x1,y1,s1,t1,color);
    }
    p=pb_push1(p,NV097_SET_BEGIN_END,NV097_SET_BEGIN_END_OP_END);
    pb_end(p);
    if(flip==XB_RECT_EDGE_EXTEND_BOTTOM){
        /* Preserve the central transform: the original bottom edge is at
           y=239. Extend only its endpoint through the remaining row after
           the first strip. This remains on the one existing texture bind. */
        const float xs[4]={0,x0,x1,320},us[4]={s0,s0,s1,s1};
        p=pb_begin();
        p=pb_push1(p,NV097_SET_BEGIN_END,NV097_SET_BEGIN_END_OP_TRIANGLE_STRIP);
        for(unsigned j=0;j<4;j++){p=vertex(p,xs[j],y1,us[j],t1,color);p=vertex(p,xs[j],240,us[j],t1,color);}
        p=pb_push1(p,NV097_SET_BEGIN_END,NV097_SET_BEGIN_END_OP_END);
        pb_end(p);
    }
    sote_gpu_status.rectangles++;
}
/* Fault-only diagnostic: site, offending extent, backing/levels, spare.
   Descriptor bytes remain available in xb_tiles for exact reproduction. */
volatile uint32_t sote_texture_size_fault[4];
static unsigned log2_size(unsigned size) {
    if(!size || (size&(size-1))) {
        sote_texture_size_fault[0]=1;sote_texture_size_fault[1]=size;
        xb_unsupported(0xE5050001);
    }
    return __builtin_ctz(size);
}
static unsigned tile_extent(unsigned first,unsigned last,unsigned mask,unsigned mode) {
    /* Unclamped N64 tiles repeat at the mask period even when the tile-size
       command includes the boundary texel (Hoth's I4 mask is 33x33, period 32).
       Uploading the region extent would change wrapping or reject valid tiles. */
    if(mask && !(mode&2))return 1u<<mask;
    if(last<first)xb_unsupported(0xE5050003);
    return ((last-first)>>2)+1;
}
static unsigned backing_size(unsigned extent) {
    return extent<=1?1u:1u<<(32-__builtin_clz(extent-1));
}
static void mip_texture(unsigned tile,unsigned tlut,unsigned levels,int origin_s,unsigned window_width) {
    if(!levels || tile+levels>8)xb_unsupported(0xE5050002);
    if(mip_generation==xb_texture_generation && mip_tile==tile && mip_tlut==tlut && mip_levels==levels &&
       mip_origin_s==origin_s && mip_window_width==window_width)return;
    if(!mip_cache_pixels) {
        native_slots=xb_hd_boot_texture_slots();
        mip_cache_pixels=xb_budget_native_gpu_allocate(XB_BUDGET_TEXCACHE);
        if(!mip_cache_pixels)xb_unsupported(0xE5030001);
    }
    xb_tile* d=&xb_tiles[tile];
    unsigned logical_width=tile_extent(d->left,d->right,d->mask_s,d->mode_s);
    unsigned logical_height=tile_extent(d->top,d->bottom,d->mask_t,d->mode_t);
    mip_width=backing_size(logical_width);mip_height=backing_size(logical_height);
    if(window_width) {
        /* The caller provides a bounded, guarded sampling window. Never use
           this for a mip chain or a masked/clamped axis. */
        if(levels!=1 || d->mask_s || (d->mode_s&2))xb_unsupported(0xE5050001);
        mip_width=window_width;
    } else if(mip_width!=logical_width && !(d->mode_s&2)) {
        sote_texture_size_fault[0]=2;sote_texture_size_fault[1]=logical_width;
        sote_texture_size_fault[2]=mip_width;sote_texture_size_fault[3]=levels;
        xb_unsupported(0xE5050001);
    }
    if(mip_height!=logical_height && !(d->mode_t&2)) {
        /* Palace's64x30 CI4 tile has no T mask/clamp. Its T period is the
           physical TMEM row-address period, not30 or32. Materialize that
           period so NV2A wrapping agrees with the original sampler. */
        if(levels!=1 || d->mask_t || !d->line) {
            sote_texture_size_fault[0]=3;sote_texture_size_fault[1]=logical_height;
            sote_texture_size_fault[2]=mip_height;sote_texture_size_fault[3]=levels;
            xb_unsupported(0xE5050001);
        }
        unsigned stride=d->line*8;
        unsigned period=d->format==0 && d->size==3?2048:4096;
        while(period>1 && !(stride&1)){period>>=1;stride>>=1;}
        /* Odd rows exchange TMEM banks even if the byte address repeats. */
        if(period<2)period=2;
        mip_height=period;
    }
    unsigned lw=log2_size(mip_width),lh=log2_size(mip_height),words=0,pages=0;
    for(unsigned level=0;level<levels;level++) {
        unsigned w=mip_width>>level,h=mip_height>>level;
        if(!w)w=1;if(!h)h=1;
        d=&xb_tiles[tile+level];
        if((levels>1 && (tile_extent(d->left,d->right,d->mask_s,d->mode_s)!=w ||
           tile_extent(d->top,d->bottom,d->mask_t,d->mode_t)!=h)) || words+w*h>256*1024)
            xb_unsupported(0xE5050003);
        words+=w*h;
        pages|=xb_texture_pages(d,w,h,origin_s);
    }
    /* Replacement uses original normalized UV extent; only GPU image dimensions change.
       Unbounded windows/period-extension cannot be replaced by a finite source image. */
    if(!origin_s && !window_width && mip_width==logical_width && mip_height==logical_height &&
       xb_hd_texture(tile,tlut,logical_width,logical_height,levels,&mip_pixels,&mip_format)) {
        mip_generation=xb_texture_generation;mip_tile=tile;mip_tlut=tlut;mip_levels=levels;
        mip_origin_s=origin_s;mip_window_width=window_width;return;
    }
    const uint8_t* memory=xb_texture_memory();
    unsigned hash=xb_texture_page_hash(pages);
    hash=texture_hash(xb_tiles+tile,levels*sizeof(xb_tile),hash);
    hash=(hash^tlut)*16777619u;hash=(hash^levels)*16777619u;
    hash=(hash^(unsigned)origin_s)*16777619u;hash=(hash^window_width)*16777619u;
    unsigned bucket=(hash^(hash>>16))&255;
    unsigned selected=0,oldest=~0u;
    if(words<=MIP_CACHE_WORDS) {
        for(unsigned link=mip_cache_buckets[bucket];link;link=mip_cache[link-1].next) {
            unsigned i=link-1;
            if(mip_cache[i].valid && mip_cache[i].hash==hash && mip_cache[i].tlut==tlut && mip_cache[i].levels==levels &&
               mip_cache[i].origin_s==origin_s && mip_cache[i].window_width==window_width &&
               xb_texture_pages_match(mip_cache[i].memory,pages) && !memcmp(mip_cache[i].tiles,xb_tiles+tile,levels*sizeof(xb_tile))) {
                mip_pixels=mip_cache_pixels+i*MIP_CACHE_WORDS;
                mip_cache[i].stamp=++mip_cache_stamp;
                sote_texture_cache_status[0]++;
                goto ready;
            }
        }
        for(unsigned i=0;i<native_slots;i++) {
            unsigned stamp=mip_cache[i].valid?mip_cache[i].stamp:0;
            if(stamp<oldest){oldest=stamp;selected=i;}
        }
        mip_pixels=mip_cache_pixels+selected*MIP_CACHE_WORDS;
    } else {
        if(!mip_fallback)mip_fallback=xb_budget_native_gpu_allocate(XB_BUDGET_OVERSIZE);
        if(!mip_fallback)xb_unsupported(0xE5030001);
        mip_pixels=mip_fallback;sote_texture_cache_status[3]++;
    }
    sote_texture_cache_status[1]++;
    /* A cache hit is immutable and may retain the current vertex batch.
       Only misses can overwrite texture storage; submit pending readers
       before decoding/waiting for or replacing that storage. */
    xb_gpu_flush();
    if(words>MIP_CACHE_WORDS) {
        /* Single shared oversize GPU buffer: retire every preceding reader
           before the first block overwrite. All subsequent copies are forward
           WC stores, never reads, and retain the original full mip chain. */
        uint64_t timer=xb_perf_begin();while(pb_busy())Sleep(0);xb_perf_end(XB_PERF_TEXTURE_WAIT,timer);
        unsigned offset=0,n_blocks=0;
        for(unsigned level=0;level<levels;level++) {
            unsigned w=mip_width>>level,h=mip_height>>level;
            if(!w)w=1;if(!h)h=1;
            xb_mip_blocks b=xb_mip_block_plan(w,h,MIP_CACHE_WORDS);
            for(unsigned index=0;index<b.grid_w*b.grid_h;index++) {
                unsigned x0,y0;
                uint64_t decode_timer=xb_perf_begin();
                xb_mip_block_origin(b,index,&x0,&y0);
                xb_texture_decode_block(mip_scratch,tile+level,tlut,(int)x0+origin_s,(int)y0,b.w,b.h);
                xb_perf_end(XB_PERF_TEXTURE_DECODE,decode_timer);
                memcpy(mip_pixels+offset,mip_scratch,b.words*4);
                offset+=b.words;n_blocks++;
            }
        }
        sote_mip_chunked[0]++;sote_mip_chunked[1]+=n_blocks;sote_mip_chunked[2]=offset;
        if(offset>sote_mip_chunked[3])sote_mip_chunked[3]=offset;sote_mip_chunked[4]+=offset;
    } else {
        /* Decode into cached CPU memory, then copy sequentially to GPU memory.
           Scattered swizzled writes directly to write-combined memory are costly. */
        uint64_t decode_timer=xb_perf_begin();
        unsigned offset=0;
        for(unsigned level=0;level<levels;level++) {
            unsigned w=mip_width>>level,h=mip_height>>level;
            if(!w)w=1;if(!h)h=1;
            xb_texture_decode_block(mip_scratch+offset,tile+level,tlut,origin_s,0,w,h);
            offset+=w*h;
        }
        xb_perf_end(XB_PERF_TEXTURE_DECODE,decode_timer);
        /* A hit is immutable and needs no wait. Reused slots must retire their
           previous GPU reads before being overwritten. New slots need no stall. */
        if(mip_cache[selected].valid) {
            uint64_t timer=xb_perf_begin();while(pb_busy())Sleep(0);xb_perf_end(XB_PERF_TEXTURE_WAIT,timer);
            sote_texture_cache_status[2]++;
        }
        memcpy(mip_pixels,mip_scratch,words*4);
    }
    __asm__ volatile("sfence" ::: "memory");
    if(words<=MIP_CACHE_WORDS) {
        if(mip_cache[selected].valid) {
            unsigned old_hash=mip_cache[selected].hash;
            unsigned* link=&mip_cache_buckets[(old_hash^(old_hash>>16))&255];
            while(*link && *link!=selected+1)link=&mip_cache[*link-1].next;
            if(*link)*link=mip_cache[selected].next;
        }
        mip_cache[selected].next=mip_cache_buckets[bucket];
        mip_cache_buckets[bucket]=selected+1;
        mip_cache[selected].valid=1;mip_cache[selected].hash=hash;
        mip_cache[selected].tlut=tlut;mip_cache[selected].levels=levels;
        mip_cache[selected].origin_s=origin_s;mip_cache[selected].window_width=window_width;
        mip_cache[selected].stamp=++mip_cache_stamp;
        memcpy(mip_cache[selected].memory,memory,4096);
        memcpy(mip_cache[selected].tiles,xb_tiles+tile,levels*sizeof(xb_tile));
    }
    sote_triangle_status[1]++;
ready:
    mip_format=1|8|0x20|(6<<8)|(levels<<16)|(lw<<20)|(lh<<24);
    mip_generation=xb_texture_generation;mip_tile=tile;mip_tlut=tlut;mip_levels=levels;
    mip_origin_s=origin_s;mip_window_width=window_width;
}

static int bind_triangle(unsigned tile,unsigned tlut,unsigned levels,uint32_t other_lo,int combine_mode,
    const int* scissor,const float s[3],int center_ui,int world,unsigned cull,int* origin) {
    unsigned primitive_opacity=(unsigned)combine_mode&~255u;
    combine_mode&=255;
    int upper_mip=combine_mode==5;
    int origin_s=0;unsigned window_width=0;
    if(upper_mip)combine_mode=0;
    if(xb_test_disable_depth)other_lo&=~0x30u;
    if(combine_mode!=4) {
    unsigned base_width=tile_extent(xb_tiles[tile].left,xb_tiles[tile].right,xb_tiles[tile].mask_s,xb_tiles[tile].mode_s);
    unsigned base_height=tile_extent(xb_tiles[tile].top,xb_tiles[tile].bottom,xb_tiles[tile].mask_t,xb_tiles[tile].mode_t);
    if(levels==1 && !xb_tiles[tile].mask_s && !(xb_tiles[tile].mode_s&2) &&
       backing_size(base_width)!=base_width) {
        if(!xb_texture_s_window(s,&origin_s,&window_width)) {
            sote_texture_s_window[3]++;xb_unsupported(0xE5050001);
        }
        sote_texture_s_window[0]++;sote_texture_s_window[1]=(unsigned)origin_s;
        sote_texture_s_window[2]=window_width;
    }
    /* No mip-count calculation is needed for a one-level upload. Its
       padded/clamped dimensions are validated by mip_texture below. */
    if(levels>1) {
        unsigned last=log2_size(base_width>base_height?base_width:base_height)+1;
        if(levels>last)levels=last;
    }
    uint64_t lookup_timer=xb_perf_begin();
    mip_texture(tile,tlut,levels,origin_s,window_width);
    xb_perf_end(XB_PERF_TEXTURE_LOOKUP,lookup_timer);
    /* Clamp the max LOD register to the chain actually bound. A scaled
       replacement can have fewer available mips than the native tile. */
    levels=(mip_format>>16)&15u;
    }
    rectangle_state_valid=0;
    shader(world);
    *origin=origin_s;
    uint32_t state[12]={other_lo,(uint32_t)combine_mode|primitive_opacity,(uint32_t)upper_mip,
        (uint32_t)mip_pixels,mip_format,levels,xb_tiles[tile].mode_s|(window_width?4u:0u),xb_tiles[tile].mode_t,
        (uint32_t)scissor[0],(uint32_t)scissor[1],(uint32_t)scissor[2],(uint32_t)scissor[3]};
    /* TMEM generation and TLUT are CPU cache keys, not GPU state. The exact
       decoded texture identity/format above already captures their effect. */
    uint32_t* p;
    static unsigned last_cull;
    /* F3D blender cycle 0: fog * shade alpha + pixel * (1 - shade alpha). */
    int fog=(other_lo&0xcccc0000u)==0xc8000000u && !xb_gpu_verify_unfogged;
    static uint32_t last_fog;
    static unsigned last_width,last_height;
    int raster_matches=triangle_state_valid && triangle_state_ui==center_ui && last_cull==cull &&
        (!fog || last_fog==xb_rdp_fog_color) && last_width==mip_width && last_height==mip_height;
    if(raster_matches && state[3]!=triangle_state[3] && combine_mode!=4){
        uint32_t address=state[3];state[3]=triangle_state[3];
        int only_address=same_triangle_state(state,triangle_state);
        state[3]=address;
        if(only_address){
            queue_vertex_batch();
            p=pb_begin();p=pb_push1(p,NV097_SET_TEXTURE_OFFSET,address&0x03ffffff);xb_pb_end_packet(p);
            triangle_state[3]=address;sote_triangle_status[2]++;return 1;
        }
    }
    if(!raster_matches || !same_triangle_state(state,triangle_state)) {
    const xb_pixel_rect clip=xb_aspect_pixels(scissor,pb_back_buffer_width(),pb_back_buffer_height(),center_ui);
    if(clip.right<=clip.left||clip.bottom<=clip.top)return 0;
    queue_vertex_batch();
    triangle_state_ui=center_ui;
    last_cull=cull;
    last_fog=xb_rdp_fog_color;
    last_width=mip_width;last_height=mip_height;
    __builtin_memcpy_inline(triangle_state,state,sizeof(state));triangle_state_valid=1;
    sote_triangle_status[2]++;
    p=pb_begin();
    p=pb_push1(p,NV097_SET_TRANSFORM_CONSTANT_LOAD,97);
    p=pb_push4f(p,NV097_SET_TRANSFORM_CONSTANT,
        combine_mode==4?1:1.0f/mip_width,combine_mode==4?1:1.0f/mip_height,1.0f/255.0f,1);
    p=pb_push1(p,NV097_SET_LIGHTING_ENABLE,0);
    p=pb_push1(p,NV097_SET_CULL_FACE_ENABLE,cull!=0);
    if(cull){
        /* Positive signed area is clockwise on the Xbox's top-left-origin surface.
           F3DWAVE flag 1 discards negative area, flag 2 discards positive. */
        p=pb_push1(p,NV097_SET_FRONT_FACE,NV097_SET_FRONT_FACE_V_CW);
        p=pb_push1(p,NV097_SET_CULL_FACE,cull==1?NV097_SET_CULL_FACE_V_BACK:NV097_SET_CULL_FACE_V_FRONT);
    }
    /* Z_UPD without Z_CMP still writes depth: enable the depth stage with
       an unconditional comparison, rather than disabling the stage. */
    /* Both vertex programs emit projected Z with HPOS.w = 1. Select fixed Z
       after pb_target_back_buffer and keep texture perspective enabled. */
    p=pb_push1(p,NV097_SET_CONTROL0,NV097_SET_CONTROL0_STENCIL_WRITE_ENABLE |
        NV097_SET_CONTROL0_TEXTURE_PERSPECTIVE_ENABLE);
    p=pb_push1(p,NV097_SET_DEPTH_TEST_ENABLE,(other_lo&0x30)!=0);
    p=pb_push1(p,NV097_SET_DEPTH_FUNC,(other_lo&0x10)?NV097_SET_DEPTH_FUNC_V_LEQUAL:NV097_SET_DEPTH_FUNC_V_ALWAYS);
    p=pb_push1(p,NV097_SET_DEPTH_MASK,(other_lo&0x20)!=0);
    /* Coverage-times-alpha must not let fully transparent texels write RGB
       or depth. Subpixel N64 coverage is still a separate fidelity task. */
    p=pb_push1(p,NV097_SET_ALPHA_TEST_ENABLE,(other_lo&0x1000)!=0);
    p=pb_push1(p,NV097_SET_ALPHA_FUNC,NV097_SET_ALPHA_FUNC_V_GREATER);
    p=pb_push1(p,NV097_SET_ALPHA_REF,0);
    p=pb_push1(p,NV097_SET_BLEND_ENABLE,(other_lo&0x4000)!=0);
    p=pb_push1(p,NV097_SET_BLEND_FUNC_SFACTOR,0x302);
    p=pb_push1(p,NV097_SET_BLEND_FUNC_DFACTOR,0x303);
    p=pb_push1(p,NV097_SET_BLEND_EQUATION,0x8006);
    p=pb_push1(p,NV097_SET_COLOR_MASK,0x01010101);
    p=window_clip(p,clip.left,clip.top,clip.right,clip.bottom);
    for(unsigned i=1;i<4;i++)p=pb_push1(p,NV097_SET_TEXTURE_CONTROL0+i*64,0);
    if(combine_mode!=4) {
    p=pb_push1(p,NV097_SET_TEXTURE_OFFSET,(uint32_t)mip_pixels&0x03ffffff);
    p=pb_push1(p,NV097_SET_TEXTURE_FORMAT,mip_format);
    xb_tile* d=&xb_tiles[tile];
    unsigned u=window_width?3:(d->mode_s&2)?3:(d->mode_s&1)?2:1;
    unsigned t=(d->mode_t&2)?3:(d->mode_t&1)?2:1;
    p=pb_push1(p,NV097_SET_TEXTURE_ADDRESS,u|(t<<8)|(3<<16));
    p=pb_push1(p,NV097_SET_TEXTURE_CONTROL0,(1<<30)|((levels-1)*256<<6));
    /* Upper mip uses floor(LOD)+1: nearest-mip selection biased by 0.5.
       Another +1 accounts for 640x480 versus the native 320x240 derivatives.
       NV2A LOD bias is signed fixed point with eight fractional bits.
       Hardware derivative/filter fidelity still differs from the RDP. */
    p=pb_push1(p,NV097_SET_TEXTURE_FILTER,upper_mip?0x02042180:0x02062000);
    p=pb_push1(p,NV097_SET_SHADER_STAGE_PROGRAM,1);
    } else {
        p=pb_push1(p,NV097_SET_TEXTURE_CONTROL0,0);
        p=pb_push1(p,NV097_SET_SHADER_STAGE_PROGRAM,0);
    }
    p=pb_push1(p,NV097_SET_SHADER_OTHER_STAGE_INPUT,0);
    p=pb_push1(p,NV097_SET_COMBINER_CONTROL,1);
    /* Mode 2 carries primitive RGB in primary color and samples only alpha. */
    p=pb_push1(p,NV097_SET_COMBINER_COLOR_ICW,combine_mode==7?0x08200000:(combine_mode==2 || combine_mode==4 || combine_mode==6)?0x04200000:0x08040000);
    if(primitive_opacity)p=pb_push1(p,NV097_SET_COMBINER_FACTOR0,(primitive_opacity>>XB_COMBINE_ALPHA_SHIFT)<<24);
    p=pb_push1(p,NV097_SET_COMBINER_ALPHA_ICW,(combine_mode==1 || combine_mode==4 || combine_mode==7)?0x14200000:(combine_mode==2 || combine_mode==3)?0x18200000:primitive_opacity?0x18110000:0x18140000);
    p=pb_push1(p,NV097_SET_COMBINER_COLOR_OCW,0xC0);
    p=pb_push1(p,NV097_SET_COMBINER_ALPHA_OCW,0xC0);
    if(fog)p=pb_push1(p,NV097_SET_SPECULAR_FOG_FACTOR,0xff000000u|(xb_rdp_fog_color>>8));
    /* The RSP writes fog to SHADE alpha. Use that primary alpha directly.
       The independent final alpha input below preserves coverage/opacity. */
    p=pb_push1(p,NV097_SET_COMBINER_SPECULAR_FOG_CW0,fog?0x14010c00u:0xCu);
    p=pb_push1(p,NV097_SET_COMBINER_SPECULAR_FOG_CW1,0x1C00);
    xb_pb_end_packet(p);
    }
    return 1;
}
static void reserve_triangle_vertices(void) {
    if(!vertex_ring) {
        vertex_ring=xb_budget_native_gpu_allocate(XB_BUDGET_VERTEX);
        if(!vertex_ring)xb_unsupported(0xE5030001);
    }
    if(vertex_cursor+3>VERTEX_RING_COUNT) {
        xb_gpu_flush();
        uint64_t timer=xb_perf_begin();while(pb_busy())Sleep(0);
        xb_perf_end(XB_PERF_BATCH_WAIT,timer);vertex_cursor=0;
        ++ring_generation;
    }
}
void xb_gpu_task_retired(void){
    __builtin_memset_inline(linear_readers,0,sizeof(linear_readers));
    /* The caller has retired the FIFO and PGRAPH. No live draw can use
       this range, even if the game submits another task before VI swap. */
    vertex_cursor=0;
    ++ring_generation;
}
static void triangle_impl(unsigned tile,unsigned tlut,unsigned levels,uint32_t other_lo,int combine_mode,
    const int* scissor,const xb_gpu_vertex* v,int center_ui) {
    const float s[3]={v[0].s,v[1].s,v[2].s};
    int origin_s;
    if(!bind_triangle(tile,tlut,levels,other_lo,combine_mode,scissor,s,center_ui,0,0,&origin_s))return;
    reserve_triangle_vertices();
    if(!batch_count)batch_start=vertex_cursor;
    for(unsigned i=0;i<3;i++) {
        array_vertex* out=&vertex_ring[vertex_cursor++];
        out->position[0]=v[i].x;out->position[1]=v[i].y;
        out->position[2]=v[i].z;out->position[3]=1;
        for(unsigned c=0;c<4;c++)out->color[c]=v[i].color[c];
        out->texture[0]=combine_mode==4?0:v[i].s-(float)origin_s;
        out->texture[1]=combine_mode==4?0:v[i].t;
        out->texture[2]=0;out->texture[3]=v[i].q;
    }
    batch_count+=3;sote_triangle_status[0]++;
    if(center_ui)sote_geometry[4]++;
}

void xb_gpu_world_triangle(unsigned tile,unsigned tlut,unsigned levels,uint32_t other_lo,int combine_mode,
    const int* scissor,unsigned cull,const xb_world_transform* transform,const xb_world_view* view,
    xb_world_vertex* const vertices[3]) {
    static uint64_t last_transform;
    static xb_world_view last_view;
    float s[3];
    for(unsigned i=0;i<3;i++)s[i]=vertices[i]->st[0]*view->texel[0]+view->texel[2];
    int origin;
    if(!bind_triangle(tile,tlut,levels,other_lo,combine_mode,scissor,s,0,1,cull,&origin))return;
    xb_world_view actual=*view;
    actual.texel[2]-=(float)origin;
    if(combine_mode==4)memset(actual.texel,0,sizeof(actual.texel));
    int matrix_change=last_transform!=transform->serial;
    int view_change=memcmp(&last_view,&actual,sizeof(actual))!=0;
    if(matrix_change || view_change){
        queue_vertex_batch();
        uint32_t* p=pb_begin();
        if(matrix_change){
            p=pb_push1(p,NV097_SET_TRANSFORM_CONSTANT_LOAD,98);
            for(unsigned i=0;i<4;i++)p=pb_push4fv(p,NV097_SET_TRANSFORM_CONSTANT,transform->model[i]);
            for(unsigned i=0;i<4;i++)p=pb_push4fv(p,NV097_SET_TRANSFORM_CONSTANT,transform->projection[i]);
            p=pb_push1(p,NV097_SET_TRANSFORM_CONSTANT_LOAD,109);
            p=pb_push4fv(p,NV097_SET_TRANSFORM_CONSTANT,transform->light_direction);
            p=pb_push4fv(p,NV097_SET_TRANSFORM_CONSTANT,transform->diffuse);
            p=pb_push4fv(p,NV097_SET_TRANSFORM_CONSTANT,transform->ambient);
            last_transform=transform->serial;
        }
        if(view_change){
            p=pb_push1(p,NV097_SET_TRANSFORM_CONSTANT_LOAD,106);
            p=pb_push4fv(p,NV097_SET_TRANSFORM_CONSTANT,actual.scale);
            p=pb_push4fv(p,NV097_SET_TRANSFORM_CONSTANT,actual.offset);
            p=pb_push4fv(p,NV097_SET_TRANSFORM_CONSTANT,actual.texel);
            last_view=actual;
        }
        xb_pb_end_packet(p);
    }
    reserve_triangle_vertices();
    if(index_count==sizeof(world_indices)/sizeof(world_indices[0]))queue_vertex_batch();
    for(unsigned i=0;i<3;i++){
        xb_world_vertex* v=vertices[i];
        if(v->ring_generation!=ring_generation){
            v->ring_generation=ring_generation;
            v->ring_index=vertex_cursor++;
            array_vertex* out=&vertex_ring[v->ring_index];
            __builtin_memcpy_inline(out->position,v->position,3*sizeof(float));out->position[3]=1;
            __builtin_memcpy_inline(out->color,v->color,sizeof(out->color));
            out->texture[0]=v->st[0];out->texture[1]=v->st[1];out->texture[2]=0;out->texture[3]=1;
        }
        world_indices[index_count++]=(uint16_t)v->ring_index;
    }
    sote_triangle_status[0]++;
}

void xb_gpu_triangle(unsigned tile,unsigned tlut,unsigned levels,uint32_t other_lo,int combine_mode,
    const int* scissor,const xb_gpu_vertex* v) {
    uint64_t timer=xb_perf_begin();
    triangle_impl(tile,tlut,levels,other_lo,combine_mode,scissor,v,0);
    xb_perf_end(XB_PERF_GPU_TRIANGLE,timer);
}

void xb_gpu_ui_triangle(unsigned tile,unsigned tlut,unsigned levels,uint32_t other_lo,int combine_mode,
    const int* scissor,const xb_gpu_vertex* v) {
    uint64_t timer=xb_perf_begin();
    triangle_impl(tile,tlut,levels,other_lo,combine_mode,scissor,v,1);
    xb_perf_end(XB_PERF_GPU_TRIANGLE,timer);
}

void xb_gpu_image(const uint32_t* image,unsigned w,unsigned h,int x,int y) {
    xb_gpu_flush();
    if(!xb_linear_story(w,h))xb_unsupported(0xE5030006);
    ensure_linear_upload();
    linear_range(pixels,w*h,1);
    memcpy(pixels,image,w*h*4);__asm__ volatile("sfence" ::: "memory");
    width=w;height=h;pitch=w;cached_generation=~0u;
    draw_rectangle(pixels,0,0xffffffff,xb_aspect_ui_x(x,xb_wide_projection),y,
        xb_aspect_ui_x(x+(float)w,xb_wide_projection),y+h,0,0,w,h,0);
}

/* The star mask is walked in ascending pixel order. Clear the gaps as we go
   so writes to the write-combined upload buffer stay sequential. */
static inline void clear_star_gap(uint32_t *out, unsigned count) {
    __asm__ volatile("rep stosl" : "+D"(out), "+c"(count) : "a"(0) : "memory");
}

void xb_gpu_starfield(const uint8_t* rdram, const xb_star_layer* layer) {
    unsigned w = layer->width, h = layer->height, stride = (w + 15) & ~15u;
    xb_gpu_flush();
    ensure_linear_upload();
    linear_range(pixels,stride*h,1);
    unsigned cursor = 0;
    const uint16_t* source16 = (const uint16_t*)(rdram + layer->target);
    const uint32_t* source32 = (const uint32_t*)(rdram + layer->target);
    for (unsigned group = 0; group < (w * h + 31) / 32; ++group) {
        uint32_t dirty = layer->dirty[group];
        while (dirty) {
            unsigned i = group * 32 + __builtin_ctz(dirty);
            uint32_t rgb;
            if (layer->bpp == 16) {
                unsigned v = source16[i ^ 1]; /* guest halfwords are word-swapped */
                unsigned r = v >> 11, g = (v >> 6) & 31, b = (v >> 1) & 31;
                rgb = ((r * 8 + (r >> 2)) << 16) | ((g * 8 + (g >> 2)) << 8) | (b * 8 + (b >> 2));
            } else rgb = source32[i] >> 8;
            unsigned index = stride == w ? i : (i / w) * stride + i % w;
            clear_star_gap(pixels + cursor, index - cursor);
            pixels[index] = 0xff000000u | rgb;
            cursor = index + 1;
            dirty &= dirty - 1;
        }
    }
    clear_star_gap(pixels + cursor, stride * h - cursor);
    __asm__ volatile("sfence" ::: "memory");
    if (stride * h > XB_RECT_MAX_WORDS) canvas_owns_rect_slots = 1;
    width = w; height = h; pitch = stride; cached_generation = ~0u;
    draw_rectangle(pixels, 0, 0xffffffff, 0, 0, 320, 240, 0, 0, w, h, 0);
}

/* Use the existing untextured shader/ring to fill only the two UI side
   panels; no extra surface or texture. The alpha matches the authored
   canvas background, so pause remains uniformly dim, not side seams. */
static void canvas_side(float left,float right,uint32_t rgba) {
    const int full[4]={0,0,320,240};
    const float a=(rgba>>24)/255.0f,r=((rgba>>16)&255)/255.0f,
        g=((rgba>>8)&255)/255.0f,b=(rgba&255)/255.0f;
    xb_gpu_vertex v[4]={{left,0,0,1,0,0,{r,g,b,a}}, {right,0,0,1,0,0,{r,g,b,a}},
        {left,240,0,1,0,0,{r,g,b,a}}, {right,240,0,1,0,0,{r,g,b,a}}};
    xb_gpu_vertex t[3]={v[1],v[3],v[2]};
    xb_gpu_triangle(0,0,1,0x4000u,4,full,v);
    xb_gpu_triangle(0,0,1,0x4000u,4,full,t);
}

/* Map each canvas band to the matching scanlines of the central 4:3 area. */
void xb_gpu_canvas(const uint32_t* image,unsigned w,unsigned h,uint32_t background,unsigned top,unsigned full_height) {
    if(!xb_linear_canvas(w,h))xb_unsupported(0xE5030006);
    xb_gpu_flush();
    ensure_linear_upload();
    linear_range(pixels,w*h,1);
    memcpy(pixels,image,w*h*4);__asm__ volatile("sfence" ::: "memory");
    canvas_owns_rect_slots=1;
    width=w;height=h;pitch=w;cached_generation=~0u;
    const float left=xb_aspect_ui_x(0,xb_wide_projection),right=xb_aspect_ui_x(320,xb_wide_projection);
    if(top==0&&xb_wide_projection&&(background>>24)){
        canvas_side(0,left,background);canvas_side(right,320,background);
    }
    draw_rectangle(pixels,0,0xffffffff,left,240.0f*top/full_height,right,240.0f*(top+h)/full_height,0,0,w,h,0);
    if(xb_wide_projection)sote_geometry[6]++;
}
