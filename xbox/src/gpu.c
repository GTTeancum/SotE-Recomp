#include <windows.h>
#include <xboxkrnl/xboxkrnl.h>
#include <pbkit/pbkit.h>
#include <string.h>
#include "gpu.h"
#include "aspect_xbox.h"
#include "texture.h"
#include "hd_texture.h"
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
volatile struct {uint32_t uploads,rectangles;} sote_gpu_status;
volatile uint32_t sote_triangle_status[3]; /* submitted, mip uploads, state submissions */
static uint32_t triangle_state[12];
static int triangle_state_valid,triangle_state_ui;
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
volatile uint32_t sote_gpu_batches;
void xb_gpu_flush(void) {
    if(!batch_count)return;
    __asm__ volatile("sfence" ::: "memory");
    uint32_t* p=pb_begin();
    /* Explicitly disable unused arrays; rectangles continue using immediate
       attributes after the arrays are disabled again below. */
    pb_push(p++,NV097_SET_VERTEX_DATA_ARRAY_FORMAT,16);
    for(unsigned i=0;i<16;i++)*p++=NV097_SET_VERTEX_DATA_ARRAY_FORMAT_TYPE_F;
    const unsigned attributes[3]={0,3,9};
    for(unsigned i=0;i<3;i++) {
        p=pb_push1(p,NV097_SET_VERTEX_DATA_ARRAY_FORMAT+attributes[i]*4,
            NV097_SET_VERTEX_DATA_ARRAY_FORMAT_TYPE_F|(4<<4)|(sizeof(array_vertex)<<8));
        p=pb_push1(p,NV097_SET_VERTEX_DATA_ARRAY_OFFSET+attributes[i]*4,
            ((uint32_t)vertex_ring+i*16)&0x03ffffff);
    }
    p=pb_push1(p,NV097_SET_BEGIN_END,NV097_SET_BEGIN_END_OP_TRIANGLES);
    /* 255 is a whole number of triangles and fits DRAW_ARRAYS' count field. */
    for(unsigned start=batch_start,left=batch_count;left;) {
        unsigned count=left>255?255:left;
        p=pb_push1(p,0x40000000|NV097_DRAW_ARRAYS,((count-1)<<24)|start);
        start+=count;left-=count;
    }
    p=pb_push1(p,NV097_SET_BEGIN_END,NV097_SET_BEGIN_END_OP_END);
    for(unsigned i=0;i<3;i++)p=pb_push1(p,NV097_SET_VERTEX_DATA_ARRAY_FORMAT+attributes[i]*4,
        NV097_SET_VERTEX_DATA_ARRAY_FORMAT_TYPE_F);
    pb_end(p);batch_count=0;sote_gpu_batches++;
}
void xb_gpu_invalidate_state(void){xb_gpu_flush();triangle_state_valid=0;}
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
static unsigned submission_draws;
/* 4 MiB GPU cache plus exact CPU-side keys. No disk cache and no guest asset
   changes. Full TMEM comparison includes palettes and odd-row bank layout. */
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
    uint8_t memory[4096];
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
void xb_gpu_reserve_commands(void) {
    /* Bound worst-case emitted commands below the512KiB pushbuffer limit.
       A less frequent reset candidate was slower in XEMU gameplay tests;
       retain the established cadence until a controlled replacement wins. */
    if(++submission_draws>=512) {
        xb_gpu_flush();
        {uint64_t timer=xb_perf_begin();while(pb_busy())Sleep(0);xb_perf_end(XB_PERF_BATCH_WAIT,timer);}
        pb_reset();submission_draws=0;sote_gpu_flushes++;
    }
}
static uint32_t* window_clip(uint32_t* p,int left,int top,int right,int bottom) {
    /* NV2A accepts the union of all eight clip windows. pbkit initializes
       every window to fullscreen, so changing only window zero does not
       actually restrict drawing. Keep all eight identical. */
    p=pb_push1(p,NV097_SET_WINDOW_CLIP_TYPE,0);
    for(unsigned i=0;i<8;i++) {
        p=pb_push1(p,NV097_SET_WINDOW_CLIP_HORIZONTAL+i*4,left|((right-1)<<16));
        p=pb_push1(p,NV097_SET_WINDOW_CLIP_VERTICAL+i*4,top|((bottom-1)<<16));
    }
    return p;
}
static void shader(void) {
    xb_gpu_reserve_commands();
    /* This is the only vertex program used by this renderer. Clears, target
       changes and pushbuffer resets do not replace NV2A program state. */
    static int loaded;
    if(loaded)return;
    uint64_t timer=xb_perf_begin();
    static const uint32_t code[]={
#include "screen.inl"
    };
    uint32_t* p=pb_begin();
    p=pb_push1(p,NV097_SET_TRANSFORM_PROGRAM_START,0);
    /* Screen-space shader positions require the privileged range, as in
       NXDK's viewport-transformed samples (PROGRAM | PRIV << 2). */
    p=pb_push1(p,NV097_SET_TRANSFORM_EXECUTION_MODE,6);
    p=pb_push1(p,NV097_SET_TRANSFORM_PROGRAM_CXT_WRITE_EN,0);
    p=pb_push1(p,NV097_SET_TRANSFORM_PROGRAM_LOAD,0);
    for(unsigned i=0;i<sizeof(code)/sizeof(code[0]);i+=4)p=pb_push4(p,NV097_SET_TRANSFORM_PROGRAM,code[i],code[i+1],code[i+2],code[i+3]);
    /* C0 is raster dimensions only. Aspect correction is a separate
       logical transform; 480p anamorphic still has a 640x480 framebuffer. */
    p=pb_push1(p,NV097_SET_TRANSFORM_CONSTANT_LOAD,96);
    p=pb_push4f(p,NV097_SET_TRANSFORM_CONSTANT,pb_back_buffer_width()/320.0f,pb_back_buffer_height()/240.0f,16777215,1);
    pb_end(p);
    loaded=1;
    xb_perf_end(XB_PERF_SHADER,timer);
}
void xb_gpu_clear_depth(int x,int y,int width,int height) {
    xb_gpu_invalidate_state();
    xb_gpu_reserve_commands();
    /* A preceding HUD rectangle disables depth writes. A depth clear must
       establish its own write mask and clip region, independent of that draw. */
    uint32_t* p=pb_begin();
    p=window_clip(p,x,y,x+width,y+height);
    p=pb_push1(p,NV097_SET_DEPTH_MASK,1);
    pb_end(p);
    pb_erase_depth_stencil_buffer(x,y,width,height);
}
static uint32_t* vertex(uint32_t* p,float x,float y,float s,float t,uint32_t color) {
    p=pb_push4f(p,NV097_SET_VERTEX_DATA4F_M+3*16,(color>>24)/255.0f,((color>>16)&255)/255.0f,((color>>8)&255)/255.0f,(color&255)/255.0f);
    p=pb_push4f(p,NV097_SET_VERTEX_DATA4F_M+9*16,s,t,0,1);
    return pb_push4f(p,NV097_SET_VERTEX_DATA4F_M,x,y,0,1);
}
static void draw_rectangle(const uint32_t* image,unsigned enhanced_format,uint32_t color,
    float x0,float y0,float x1,float y1,float s0,float t0,float s1,float t1,int flip);
volatile uint32_t sote_hd_rect_status[2]; /* attempts, successful submissions */

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
    if(xb_hd_texture(tile,tlut,logical_width,logical_height,1,&enhanced,&enhanced_format)) {
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
        {uint64_t timer=xb_perf_begin();while(pb_busy())Sleep(0);xb_perf_end(XB_PERF_TEXTURE_WAIT,timer);} /* Don't overwrite a texture still in use. */
        uint64_t decode_timer=xb_perf_begin();
        for(unsigned y=0;y<height;y++)for(unsigned x=0;x<pitch;x++)pixels[y*pitch+x]=xb_texture_pixel(tile,x<width?x:width-1,y,tlut);
        __asm__ volatile("sfence" ::: "memory");
        xb_perf_end(XB_PERF_TEXTURE_DECODE,decode_timer);
        cached_generation=xb_texture_generation;cached_tile=tile;cached_tlut=tlut;sote_gpu_status.uploads++;
    }
    draw_rectangle(pixels,0,color,x0,y0,x1,y1,s0,t0,s1,t1,flip);
    if(xb_wide_projection)sote_geometry[5]++;
}
static void draw_rectangle(const uint32_t* image,unsigned enhanced_format,uint32_t color,
    float x0,float y0,float x1,float y1,float s0,float t0,float s1,float t1,int flip) {
    xb_gpu_invalidate_state();
    shader();
    uint32_t* p=pb_begin();
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
    p=pb_push1(p,NV097_SET_TEXTURE_OFFSET,(uint32_t)image&0x03ffffff);
    p=pb_push1(p,NV097_SET_TEXTURE_FORMAT,enhanced_format?enhanced_format:1|8|0x20|(0x12<<8)|(1<<16));
    p=pb_push1(p,NV097_SET_TEXTURE_ADDRESS,0x00030303); /* clamp to edge */
    p=pb_push1(p,NV097_SET_TEXTURE_CONTROL0,1<<30);
    p=pb_push1(p,NV097_SET_TEXTURE_FILTER,0x01010000); /* nearest, matching native texels */
    if(!enhanced_format) {
        p=pb_push1(p,NV097_SET_TEXTURE_CONTROL1,pitch*4<<16);
        p=pb_push1(p,NV097_SET_TEXTURE_IMAGE_RECT,(width<<16)|height);
    }
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
    pb_end(p);p=pb_begin(); /* Keep each pbkit command block under 128 DWORDs. */
    p=pb_push1(p,NV097_SET_BEGIN_END,NV097_SET_BEGIN_END_OP_TRIANGLE_STRIP);
    if(flip==XB_RECT_EDGE_EXTEND || flip==XB_RECT_EDGE_EXTEND_BOTTOM){
        /* 8*15 + two 2-DWORD begin/end commands = 124 DWORDs. Culling off.
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
static unsigned swizzle(unsigned x,unsigned y,unsigned width,unsigned height) {
    unsigned index=0,bit=1;
    for(unsigned mask=1;mask<width || mask<height;mask<<=1) {
        if(mask<width){if(x&mask)index|=bit;bit<<=1;}
        if(mask<height){if(y&mask)index|=bit;bit<<=1;}
    }
    return index;
}
/* Fault-only diagnostic: site, offending extent, backing/levels, spare.
   Descriptor bytes remain available in xb_tiles for exact reproduction. */
volatile uint32_t sote_texture_size_fault[4];
static unsigned log2_size(unsigned size) {
    unsigned exponent=0;
    if(!size || (size&(size-1))) {
        sote_texture_size_fault[0]=1;sote_texture_size_fault[1]=size;
        xb_unsupported(0xE5050001);
    }
    while((1u<<exponent)<size)exponent++;
    return exponent;
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
    unsigned size=1;while(size<extent)size<<=1;return size;
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
    unsigned lw=log2_size(mip_width),lh=log2_size(mip_height),words=0;
    for(unsigned level=0;level<levels;level++) {
        unsigned w=mip_width>>level,h=mip_height>>level;
        if(!w)w=1;if(!h)h=1;
        d=&xb_tiles[tile+level];
        if((levels>1 && (tile_extent(d->left,d->right,d->mask_s,d->mode_s)!=w ||
           tile_extent(d->top,d->bottom,d->mask_t,d->mode_t)!=h)) || words+w*h>256*1024)
            xb_unsupported(0xE5050003);
        words+=w*h;
    }
    /* Replacement uses original normalized UV extent; only GPU image dimensions change.
       Unbounded windows/period-extension cannot be replaced by a finite source image. */
    if(!origin_s && !window_width && mip_width==logical_width && mip_height==logical_height &&
       xb_hd_texture(tile,tlut,logical_width,logical_height,levels,&mip_pixels,&mip_format)) {
        mip_generation=xb_texture_generation;mip_tile=tile;mip_tlut=tlut;mip_levels=levels;
        mip_origin_s=origin_s;mip_window_width=window_width;return;
    }
    const uint8_t* memory=xb_texture_memory();
    unsigned hash=xb_texture_fingerprint();
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
               xb_texture_memory_matches(mip_cache[i].memory) && !memcmp(mip_cache[i].tiles,xb_tiles+tile,levels*sizeof(xb_tile))) {
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
                XB_MIP_DECODE_BLOCK(mip_scratch,b,x0,y0,tile+level,origin_s,tlut,swizzle,xb_texture_pixel);
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
            for(unsigned y=0;y<h;y++)for(unsigned x=0;x<w;x++)
                mip_scratch[offset+swizzle(x,y,w,h)]=xb_texture_pixel(tile+level,(int)x+origin_s,y,tlut);
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

static void triangle_impl(unsigned tile,unsigned tlut,unsigned levels,uint32_t other_lo,int combine_mode,
    const int* scissor,const xb_gpu_vertex* v,int center_ui) {
    int upper_mip=combine_mode==5;
    int origin_s=0;unsigned window_width=0;
    if(upper_mip)combine_mode=0;
    if(xb_test_disable_depth)other_lo&=~0x30u;
    if(combine_mode!=4) {
    unsigned base_width=tile_extent(xb_tiles[tile].left,xb_tiles[tile].right,xb_tiles[tile].mask_s,xb_tiles[tile].mode_s);
    unsigned base_height=tile_extent(xb_tiles[tile].top,xb_tiles[tile].bottom,xb_tiles[tile].mask_t,xb_tiles[tile].mode_t);
    if(levels==1 && !xb_tiles[tile].mask_s && !(xb_tiles[tile].mode_s&2) &&
       backing_size(base_width)!=base_width) {
        float s[3]={v[0].s,v[1].s,v[2].s};
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
    }
    shader();
    uint32_t state[12]={other_lo,(uint32_t)combine_mode,(uint32_t)upper_mip,
        (uint32_t)mip_pixels,mip_format,levels,xb_tiles[tile].mode_s|(window_width?4u:0u),xb_tiles[tile].mode_t,
        (uint32_t)scissor[0],(uint32_t)scissor[1],(uint32_t)scissor[2],(uint32_t)scissor[3]};
    /* TMEM generation and TLUT are CPU cache keys, not GPU state. The exact
       decoded texture identity/format above already captures their effect. */
    uint32_t* p;
    if(!triangle_state_valid || triangle_state_ui!=center_ui || !same_triangle_state(state,triangle_state)) {
    const xb_pixel_rect clip=xb_aspect_pixels(scissor,pb_back_buffer_width(),pb_back_buffer_height(),center_ui);
    if(clip.right<=clip.left||clip.bottom<=clip.top)return;
    xb_gpu_flush();
    triangle_state_ui=center_ui;
    __builtin_memcpy_inline(triangle_state,state,sizeof(state));triangle_state_valid=1;
    sote_triangle_status[2]++;
    p=pb_begin();
    p=pb_push1(p,NV097_SET_TRANSFORM_CONSTANT_LOAD,97);
    p=pb_push4f(p,NV097_SET_TRANSFORM_CONSTANT,
        combine_mode==4?1:1.0f/mip_width,combine_mode==4?1:1.0f/mip_height,1,1);
    p=pb_push1(p,NV097_SET_LIGHTING_ENABLE,0);
    p=pb_push1(p,NV097_SET_CULL_FACE_ENABLE,0);
    /* Z_UPD without Z_CMP still writes depth: enable the depth stage with
       an unconditional comparison, rather than disabling the stage. */
    /* pb_target_back_buffer selects W buffering. Our CPU has already
       projected positions and HPOS.w is 1, so W cannot encode distance.
       Select fixed Z depth explicitly; keep texture perspective enabled. */
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
    p=pb_push1(p,NV097_SET_TEXTURE_FILTER,upper_mip?0x02040180:0x02060000);
    p=pb_push1(p,NV097_SET_SHADER_STAGE_PROGRAM,1);
    } else {
        p=pb_push1(p,NV097_SET_TEXTURE_CONTROL0,0);
        p=pb_push1(p,NV097_SET_SHADER_STAGE_PROGRAM,0);
    }
    p=pb_push1(p,NV097_SET_SHADER_OTHER_STAGE_INPUT,0);
    p=pb_push1(p,NV097_SET_COMBINER_CONTROL,1);
    /* Mode 2 carries primitive RGB in primary color and samples only alpha. */
    p=pb_push1(p,NV097_SET_COMBINER_COLOR_ICW,combine_mode==7?0x08200000:(combine_mode==2 || combine_mode==4 || combine_mode==6)?0x04200000:0x08040000);
    p=pb_push1(p,NV097_SET_COMBINER_ALPHA_ICW,(combine_mode==1 || combine_mode==4 || combine_mode==7)?0x14200000:(combine_mode==2 || combine_mode==3)?0x18200000:0x18140000);
    p=pb_push1(p,NV097_SET_COMBINER_COLOR_OCW,0xC0);
    p=pb_push1(p,NV097_SET_COMBINER_ALPHA_OCW,0xC0);
    p=pb_push1(p,NV097_SET_COMBINER_SPECULAR_FOG_CW0,0xC);
    p=pb_push1(p,NV097_SET_COMBINER_SPECULAR_FOG_CW1,0x1C00);
    pb_end(p);
    }
    if(!vertex_ring) {
        vertex_ring=xb_budget_native_gpu_allocate(XB_BUDGET_VERTEX);
        if(!vertex_ring)xb_unsupported(0xE5030001);
    }
    if(vertex_cursor+3>VERTEX_RING_COUNT) {
        xb_gpu_flush();
        uint64_t timer=xb_perf_begin();while(pb_busy())Sleep(0);
        xb_perf_end(XB_PERF_BATCH_WAIT,timer);vertex_cursor=0;
    }
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
    {uint64_t timer=xb_perf_begin();while(pb_busy())Sleep(0);xb_perf_end(XB_PERF_TEXTURE_WAIT,timer);}
    memcpy(pixels,image,w*h*4);__asm__ volatile("sfence" ::: "memory");
    width=w;height=h;pitch=w;cached_generation=~0u;
    draw_rectangle(pixels,0,0xffffffff,xb_aspect_ui_x(x,xb_wide_projection),y,
        xb_aspect_ui_x(x+(float)w,xb_wide_projection),y+h,0,0,w,h,0);
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

/* Retain the authored 640x480 glyph raster, but display it proportionally. */
void xb_gpu_canvas(const uint32_t* image,unsigned w,unsigned h,uint32_t background) {
    if(!xb_linear_canvas(w,h))xb_unsupported(0xE5030006);
    xb_gpu_flush();
    ensure_linear_upload();
    {uint64_t timer=xb_perf_begin();while(pb_busy())Sleep(0);xb_perf_end(XB_PERF_TEXTURE_WAIT,timer);}
    memcpy(pixels,image,w*h*4);__asm__ volatile("sfence" ::: "memory");
    width=w;height=h;pitch=w;cached_generation=~0u;
    const float left=xb_aspect_ui_x(0,xb_wide_projection),right=xb_aspect_ui_x(320,xb_wide_projection);
    if(xb_wide_projection&&(background>>24)){
        canvas_side(0,left,background);canvas_side(right,320,background);
    }
    draw_rectangle(pixels,0,0xffffffff,left,0,right,240,0,0,w,h,0);
    if(xb_wide_projection)sote_geometry[6]++;
}
