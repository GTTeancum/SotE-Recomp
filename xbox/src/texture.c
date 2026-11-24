#include "rdram_guard.h"
#include "texture.h"
#include <string.h>
#include <xmmintrin.h>
void xb_unsupported(uint32_t id) __attribute__((noreturn));
/* TMEM contains N64 byte order, including the odd-row word exchange and
   split RG/BA banks. RDRAM itself is word-swapped by the recompiler. */
static uint8_t tmem[4096] __attribute__((aligned(16)));
const uint8_t* xb_texture_memory(void){return tmem;}
/* Equality only: fixed-size word loads avoid the SDK's byte-wise memcmp. */
static int same_bytes(const uint8_t* current,const uint8_t* memory,unsigned size) {
    /* Both callers compare complete 256-byte pages. Use SSE1 bitwise ops,
       including the final integer test (NaNs and signed zero are texels). */
    if((uintptr_t)memory&15)return memcmp(current,memory,size)==0;
    for(unsigned page=0;page<size;page+=256) {
        __m128 sums[4]={_mm_setzero_ps(),_mm_setzero_ps(),_mm_setzero_ps(),_mm_setzero_ps()};
        for(unsigned i=0;i<256;i+=64){
            for(unsigned lane=0;lane<4;lane++){
                unsigned off=page+i+lane*16;
                __m128 a=_mm_load_ps((const float*)(current+off));
                __m128 b=_mm_load_ps((const float*)(memory+off));
                sums[lane]=_mm_or_ps(sums[lane],_mm_xor_ps(a,b));
            }
        }
        __m128 diff=_mm_or_ps(_mm_or_ps(sums[0],sums[1]),_mm_or_ps(sums[2],sums[3]));
        diff=_mm_or_ps(diff,_mm_movehl_ps(diff,diff));
        diff=_mm_or_ps(diff,_mm_shuffle_ps(diff,diff,_MM_SHUFFLE(1,1,1,1)));
        uint32_t bits;
        __builtin_memcpy(&bits,&diff,sizeof(bits));
        if(bits)return 0;
    }
    return 1;
}
int xb_texture_memory_matches(const uint8_t* memory) {return same_bytes(tmem,memory,sizeof(tmem));}

/* Every cache hit still compares all bytes that the decoder might sample.
   Page rounding includes the odd-row word exchange and the last packed texel. */
unsigned xb_texture_pages(const xb_tile* t,unsigned width,unsigned height,int origin_s) {
    if(origin_s || (t->format==0 && t->size==3))return 0xffff;
    unsigned start=t->tmem*8;
    unsigned row_bytes=(((width<<t->size)+1)/2+7)&~7u;
    unsigned end=start+t->line*8*(height-1)+row_bytes;
    unsigned first=start>>8, last=(end-1)>>8;
    unsigned n=last-first+1;
    if(n>=16)return 0xffff;
    uint32_t run=((1u<<n)-1)<<first;
    unsigned pages=(run|(run>>16))&0xffff;
    if(t->format==2)
        pages|=t->size==0?(1u<<(8+t->palette/2)):0xff00u;
    return pages;
}
uint32_t xb_texture_page_hash(unsigned pages) {
    /* Only a bucket selector. Equality is checked even when these samples collide. */
    uint32_t hash=2166136261u;
    while(pages) {
        const uint8_t* p=tmem+256*__builtin_ctz(pages);
        hash=(hash^xb_texture_read_word(p)^xb_texture_read_word(p+76))*16777619u;
        hash=(hash^xb_texture_read_word(p+160)^xb_texture_read_word(p+252))*16777619u;
        pages&=pages-1;
    }
    return hash;
}
int xb_texture_pages_match(const uint8_t* memory,unsigned pages) {
    while(pages) {
        unsigned offset=256*__builtin_ctz(pages);
        if(!same_bytes(tmem+offset,memory+offset,256))return 0;
        pages&=pages-1;
    }
    return 1;
}
volatile uint32_t sote_texture_key_checks;
void xb_texture_memory_selftest(void) {
    static uint8_t candidate[4096] __attribute__((aligned(16)));memcpy(candidate,tmem,sizeof(candidate));
    if(!xb_texture_memory_matches(candidate))xb_unsupported(0xE5020010);
    sote_texture_key_checks++;
    for(unsigned i=0;i<4096;i++) {
        candidate[i]^=0x80;
        if(xb_texture_memory_matches(candidate))xb_unsupported(0xE5020011);
        sote_texture_key_checks++;candidate[i]^=0x80;
    }
}
xb_tile xb_tiles[8];
uint32_t xb_texture_generation;
int xb_texture_track_sources=1;
static unsigned image_address,image_width,image_size,image_format;
static uint8_t* texture_rdram;
static unsigned palette_sources[512];
/* A LOAD describes one source, even when it overwrites all 512 TMEM words.
   Keep the descriptor once and a handle on each affected word. Reclaim only
   descriptors no live word references, including partially overwritten loads. */
typedef struct {
    uint32_t address;
    uint16_t width,uls,ult;
    uint8_t fmt,siz,type;
} texture_source;
static texture_source sources[1024];
static uint16_t word_source[512], free_sources[1023];
static unsigned next_source=1, free_count;

static unsigned remember_source(unsigned left,unsigned top,int block) {
    unsigned id;
    if(next_source<1024)id=next_source++;
    else {
        if(!free_count){
            uint32_t live[32]={0};
            for(unsigned i=0;i<512;i++)live[word_source[i]>>5]|=1u<<(word_source[i]&31);
            for(unsigned i=1;i<1024;i++)if(!(live[i>>5]&(1u<<(i&31))))free_sources[free_count++]=i;
        }
        id=free_sources[--free_count];
    }
    sources[id]=(texture_source){image_address,image_width,left,top,image_format,image_size,block};
    return id;
}
unsigned xb_texture_scene(void) {
    uint16_t event=0;
    if(texture_rdram)__builtin_memcpy_inline(&event,texture_rdram+(0x13CE0E^2),2);
    return event;
}
int xb_texture_slot_key(unsigned tile,unsigned tlut,unsigned w,unsigned h,uint32_t key[17]) {
    if(tile>=8 || !texture_rdram)return 0;
    xb_tile* t=&xb_tiles[tile];unsigned i=t->tmem&511;
    if(!word_source[i])return 0;
    const texture_source* src=&sources[word_source[i]];
    unsigned event=xb_texture_scene();
    unsigned values[17]={event,src->address,w,h,t->format,t->size,t->line,t->tmem,t->palette,
        tlut<<14,src->width,src->uls,src->ult,src->fmt,src->siz,
        src->type,(t->format==2)?palette_sources[256+(t->size==0?t->palette*16:0)]:0};
    __builtin_memcpy_inline(key,values,sizeof(values));return 1;
}
volatile struct {uint32_t images,loads,texels,format,size;} sote_texture_status;
static uint8_t source(uint8_t* ram,unsigned address) {
    if(address>=0x800000)xb_unsupported(0xE5020001);
    return ram[address^3];
}
static void load_word(uint8_t* ram,unsigned src,unsigned dst,unsigned swap,int rgba32,int palette) {
    /* Aligned guest words are already in host integer order in RDRAM.
       TMEM uses N64 byte order, and an odd row exchanges its two words. */
    if(!rgba32 && !palette && !(src&3)) {
        ((xb_texture_word*)(tmem+((dst^swap)&4095)))->value=
            __builtin_bswap32(xb_texture_read_word(ram+src));
        ((xb_texture_word*)(tmem+(((dst+4)^swap)&4095)))->value=
            __builtin_bswap32(xb_texture_read_word(ram+src+4));
        return;
    }
    if(rgba32) {
        for(unsigned i=0;i<8;i++) {
            unsigned a=(((dst+(i/4)*2+(i&1))^swap)&2047)|((i&2)?2048:0);
            tmem[a]=source(ram,src+(palette?(i&1):i));
        }
    } else for(unsigned i=0;i<8;i++)tmem[((dst+i)^swap)&4095]=source(ram,src+(palette?(i&1):i));
}
int xb_texture_command(uint8_t* ram,uint32_t w0,uint32_t w1) {
    texture_rdram=ram;unsigned op=w0>>24,index=(w1>>24)&7;xb_tile* t=&xb_tiles[index];
    if(op==0xFD) {
        image_address=w1;image_format=(w0>>21)&7;image_size=(w0>>19)&3;image_width=(w0&4095)+1;
        sote_texture_status.images++;return 1;
    }
    if(op==0xF5) {
        xb_texture_generation++;
        t->format=(w0>>21)&7;t->size=(w0>>19)&3;t->line=(w0>>9)&511;t->tmem=w0&511;
        t->palette=(w1>>20)&15;t->mode_t=(w1>>18)&3;t->mask_t=(w1>>14)&15;t->shift_t=(w1>>10)&15;
        t->mode_s=(w1>>8)&3;t->mask_s=(w1>>4)&15;t->shift_s=w1&15;return 1;
    }
    if(op!=0xF2 && op!=0xF3 && op!=0xF4 && op!=0xF0)return 0;
    unsigned left=(w0>>12)&4095,top=w0&4095,right=(w1>>12)&4095,bottom=w1&4095;
    if(op==0xF2){t->left=left;t->top=top;t->right=right;t->bottom=bottom;xb_texture_generation++;return 1;}
    int block=op==0xF3,palette=op==0xF0,rgba32=t->size==3&&t->format==0;
    unsigned stride=(image_width<<image_size)>>1;
    unsigned start=image_address+(((block?left:left>>2)<<image_size)>>1)+stride*(block?top:top>>2);
    unsigned words,rows;
    if(block){if(right<left)xb_unsupported(0xE5020002);words=((right-left)>>(4-t->size))+1;rows=1;}
    else {
        if(right<left || bottom<top)xb_unsupported(0xE5020002);
        rows=(bottom>>2)-(top>>2)+1;
        words=palette?(right>>2)-(left>>2)+1:(((right>>2)-(left>>2))>>(4-t->size))+1;
    }
    xb_rdram_offset(start,(rows-1)*stride+words*(palette?2:8),XB_RDRAM_VIDEO);
    unsigned mask=rgba32?2047:4095,advance=rgba32?4:8;
    unsigned tm_stride=t->line<<(palette?5:3),dst_row=(t->tmem<<3)&mask;
    unsigned source_id=xb_texture_track_sources&&!palette?remember_source(left,top,block):0;
    if(block && !rgba32 && !tm_stride && !(start&3)){
        /* Normal RDP LOADBLOCK of contiguous words. DXT bit 11 selects the
           two 32-bit lanes after each 64-bit transfer, including TMEM wrap. */
        unsigned dxt=0;
        for(unsigned i=0;i<words;i++,dxt+=bottom){
            unsigned at=(t->tmem+i)&511;
            uint8_t* dst=tmem+at*8;
            unsigned swap=(dxt>>9)&4;
            ((xb_texture_word*)(dst+swap))->value=__builtin_bswap32(xb_texture_read_word(ram+start+8*i));
            ((xb_texture_word*)(dst+(swap^4)))->value=__builtin_bswap32(xb_texture_read_word(ram+start+8*i+4));
            if(source_id)word_source[at]=source_id;
        }
    }else for(unsigned row=0;row<rows;row++) {
        unsigned src=start+row*stride,dst=dst_row,swap=(row&1)*4,dxt=0;
        for(unsigned i=0;i<words;i++) {
            load_word(ram,src,dst,swap,rgba32,palette);
            if(palette && xb_texture_track_sources)palette_sources[(dst&4095)>>3]=image_address;
            if(source_id)word_source[dst>>3]=source_id;
            if(block){dxt+=bottom;while(dxt>=2048){dst=(dst+tm_stride)&mask;dxt-=2048;swap^=4;}}
            src+=palette?2:8;dst=(dst+advance)&mask;
        }
        dst_row=(dst_row+tm_stride)&mask;
    }
    sote_texture_status.loads++;xb_texture_generation++;return 1;
}
static unsigned coordinate(int value,unsigned mask,unsigned mode,unsigned extent) {
    if(mode&2){if(value<0)value=0;if((unsigned)value>extent)value=extent;}
    if(mask){unsigned period=1u<<mask;if((mode&1)&&(value&period))value=~value;value&=period-1;}
    return (unsigned)value;
}
static uint32_t rgba16(unsigned v) {
    unsigned r=(v>>11)&31,g=(v>>6)&31,b=(v>>1)&31;
    return ((v&1)?0xff000000:0)|(((r<<3)|(r>>2))<<16)|(((g<<3)|(g>>2))<<8)|((b<<3)|(b>>2));
}
static inline uint32_t unpack_pixel(const xb_tile* d,unsigned x,unsigned a,unsigned tlut) {
    unsigned v=tmem[a&4095],intensity,alpha;
    if(d->size==0)v=(x&1)?v&15:v>>4;
    if(d->format==0) {
        if(d->size==2)return rgba16((v<<8)|tmem[(a+1)&4095]);
        if(d->size==3) {
            /* RGBA32 stores each pixel's two halves in separate TMEM banks. */
            a&=2047;
            return (tmem[(a+1)|2048]<<24)|(tmem[a]<<16)|(tmem[a+1]<<8)|tmem[a|2048];
        }
    } else if(d->format==2 && d->size<=1) {
        unsigned entry=d->size==0?d->palette*16+v:v;
        a=2048+entry*8;v=(tmem[a]<<8)|tmem[a+1];
        if(tlut==3)return ((v&255)<<24)|((v>>8)*0x010101);
        if(tlut==2)return rgba16(v);
    } else if(d->format==3) {
        if(d->size==0){intensity=(v>>1)*255/7;alpha=(v&1)*255;}
        else if(d->size==1){intensity=(v>>4)*17;alpha=(v&15)*17;}
        else if(d->size==2){intensity=v;alpha=tmem[(a+1)&4095];}
        else {xb_unsupported(0xE5020003);}
        return (alpha<<24)|(intensity*0x010101);
    } else if(d->format==4 && d->size<=1) {
        intensity=d->size==0?v*17:v;return intensity*0x01010101;
    }
    xb_unsupported(0xE5020003);
}
static inline unsigned texel_offset(const xb_tile* d,unsigned x) {
    return (d->size==3 && d->format==0)?x*2:((x<<d->size)>>1);
}
uint32_t xb_texture_pixel(unsigned tile,int s,int t,unsigned tlut) {
    const xb_tile* d=&xb_tiles[tile&7];
    unsigned x=coordinate(s,d->mask_s,d->mode_s,(d->right-d->left)>>2);
    unsigned y=coordinate(t,d->mask_t,d->mode_t,(d->bottom-d->top)>>2);
    unsigned a=((d->tmem+d->line*y)*8+texel_offset(d,x))^((y&1)*4);
    sote_texture_status.texels++;sote_texture_status.format=d->format;sote_texture_status.size=d->size;
    return unpack_pixel(d,x,a,tlut);
}

/* HUD rectangles use linear NV2A surfaces. The last authored texel fills
   the aligned row tail; do the tile and palette setup once per row. */
void xb_texture_decode_linear(uint32_t* out,unsigned tile,unsigned tlut,unsigned width,unsigned height,unsigned pitch) {
    const xb_tile* d=&xb_tiles[tile&7];
    unsigned last_s=(d->right-d->left)>>2,last_t=(d->bottom-d->top)>>2;
    int direct=(!(d->mode_s&2)||width-1<=last_s) && (!d->mask_s||width<=(1u<<d->mask_s));
    for(unsigned row=0;row<height;row++,out+=pitch){
        unsigned y=coordinate(row,d->mask_t,d->mode_t,last_t);
        unsigned base=(d->tmem+d->line*y)*8,swap=(y&1)*4;
        uint32_t pixel=0;
        for(unsigned col=0;col<width;col++){
            unsigned x=direct?col:coordinate(col,d->mask_s,d->mode_s,last_s);
            pixel=unpack_pixel(d,x,(base+texel_offset(d,x))^swap,tlut);
            out[col]=pixel;
        }
        for(unsigned col=width;col<pitch;col++)out[col]=pixel;
    }
    sote_texture_status.texels+=height*pitch;sote_texture_status.format=d->format;sote_texture_status.size=d->size;
}

void xb_texture_decode_block(uint32_t* out,unsigned tile,unsigned tlut,int x0,int y0,unsigned width,unsigned height) {
    const xb_tile* d=&xb_tiles[tile];
    unsigned mx=0,my=0,output_bit=1;
    for(unsigned bit=1;bit<width || bit<height;bit<<=1) {
        if(bit<width){mx|=output_bit;output_bit<<=1;}
        if(bit<height){my|=output_bit;output_bit<<=1;}
    }
    unsigned last_s=(d->right-d->left)>>2, last_t=(d->bottom-d->top)>>2;
    int direct_s=x0>=0 && (!(d->mode_s&2) || (unsigned)x0+width-1<=last_s) &&
                 (!d->mask_s || (unsigned)x0+width<=(1u<<d->mask_s));
    unsigned iy=0;
    for(unsigned row=0;row<height;row++,iy=(iy-my)&my) {
        unsigned y=coordinate(y0+(int)row,d->mask_t,d->mode_t,last_t);
        unsigned base=(d->tmem+d->line*y)*8, swap=(y&1)*4, ix=0;
        /* Subtracting a Morton mask advances its bit positions only. Avoid
           reconstructing the interleaved address at every output texel. */
        if(direct_s && d->format==0 && d->size==2) {
            for(unsigned col=0;col<width;col++,ix=(ix-mx)&mx) {
                unsigned a=((base+2*((unsigned)x0+col))^swap)&4095;
                out[iy|ix]=rgba16((tmem[a]<<8)|tmem[a+1]);
            }
        } else {
            for(unsigned col=0;col<width;col++,ix=(ix-mx)&mx) {
                unsigned x=direct_s?(unsigned)x0+col:coordinate(x0+(int)col,d->mask_s,d->mode_s,last_s);
                out[iy|ix]=unpack_pixel(d,x,(base+texel_offset(d,x))^swap,tlut);
            }
        }
    }
    sote_texture_status.texels+=width*height;sote_texture_status.format=d->format;sote_texture_status.size=d->size;
}
