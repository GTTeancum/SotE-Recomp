#include "texture.h"
#include <string.h>
void xb_unsupported(uint32_t id) __attribute__((noreturn));
/* TMEM contains N64 byte order, including the odd-row word exchange and
   split RG/BA banks. RDRAM itself is word-swapped by the recompiler. */
static uint8_t tmem[4096];
static unsigned memory_generation,hash_generation=~0u;
static uint32_t memory_hash;
const uint8_t* xb_texture_memory(void){return tmem;}
/* Equality only: fixed-size word loads avoid the SDK's byte-wise memcmp. */
int xb_texture_memory_matches(const uint8_t* memory) {
    for(unsigned i=0;i<4096;i+=16) {
        if((xb_texture_read_word(tmem+i)^xb_texture_read_word(memory+i)) |
           (xb_texture_read_word(tmem+i+4)^xb_texture_read_word(memory+i+4)) |
           (xb_texture_read_word(tmem+i+8)^xb_texture_read_word(memory+i+8)) |
           (xb_texture_read_word(tmem+i+12)^xb_texture_read_word(memory+i+12)))return 0;
    }
    return 1;
}
volatile uint32_t sote_texture_key_checks;
void xb_texture_memory_selftest(void) {
    static uint8_t candidate[4096];memcpy(candidate,tmem,sizeof(candidate));
    if(!xb_texture_memory_matches(candidate))xb_unsupported(0xE5020010);
    sote_texture_key_checks++;
    for(unsigned i=0;i<4096;i++) {
        candidate[i]^=0x80;
        if(xb_texture_memory_matches(candidate))xb_unsupported(0xE5020011);
        sote_texture_key_checks++;candidate[i]^=0x80;
    }
}
uint32_t xb_texture_fingerprint(void) {
    if(hash_generation!=memory_generation) {
        memory_hash=2166136261u;
        for(unsigned i=0;i<4096;i+=4)memory_hash=(memory_hash^xb_texture_read_word(tmem+i))*16777619u;
        hash_generation=memory_generation;
    }
    return memory_hash;
}
xb_tile xb_tiles[8];
uint32_t xb_texture_generation;
static unsigned image_address,image_width,image_size;
volatile struct {uint32_t images,loads,texels,format,size;} sote_texture_status;
static uint8_t source(uint8_t* ram,unsigned address) {
    if(address>=0x800000)xb_unsupported(0xE5020001);
    return ram[address^3];
}
static void load_word(uint8_t* ram,unsigned src,unsigned dst,unsigned swap,int rgba32,int palette) {
    if(rgba32) {
        for(unsigned i=0;i<8;i++) {
            unsigned a=(((dst+(i/4)*2+(i&1))^swap)&2047)|((i&2)?2048:0);
            tmem[a]=source(ram,src+(palette?(i&1):i));
        }
    } else for(unsigned i=0;i<8;i++)tmem[((dst+i)^swap)&4095]=source(ram,src+(palette?(i&1):i));
}
int xb_texture_command(uint8_t* ram,uint32_t w0,uint32_t w1) {
    unsigned op=w0>>24,index=(w1>>24)&7;xb_tile* t=&xb_tiles[index];
    if(op==0xFD) {
        image_address=w1;image_size=(w0>>19)&3;image_width=(w0&4095)+1;
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
    unsigned mask=rgba32?2047:4095,advance=rgba32?4:8;
    unsigned tm_stride=t->line<<(palette?5:3),dst_row=(t->tmem<<3)&mask;
    for(unsigned row=0;row<rows;row++) {
        unsigned src=start+row*stride,dst=dst_row,swap=(row&1)*4,dxt=0;
        for(unsigned i=0;i<words;i++) {
            load_word(ram,src,dst,swap,rgba32,palette);
            if(block){dxt+=bottom;while(dxt>=2048){dst=(dst+tm_stride)&mask;dxt-=2048;swap^=4;}}
            src+=palette?2:8;dst=(dst+advance)&mask;
        }
        dst_row=(dst_row+tm_stride)&mask;
    }
    sote_texture_status.loads++;xb_texture_generation++;memory_generation++;return 1;
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
uint32_t xb_texture_pixel(unsigned tile,int s,int t,unsigned tlut) {
    xb_tile* d=&xb_tiles[tile&7];
    unsigned x=coordinate(s,d->mask_s,d->mode_s,(d->right-d->left)>>2);
    unsigned y=coordinate(t,d->mask_t,d->mode_t,(d->bottom-d->top)>>2);
    unsigned a=((d->tmem+d->line*y)*8+((x<<d->size)>>1))^((y&1)*4);
    unsigned v=tmem[a&4095],intensity,alpha;
    sote_texture_status.texels++;sote_texture_status.format=d->format;sote_texture_status.size=d->size;
    if(d->size==0)v=(x&1)?v&15:v>>4;
    if(d->format==0) {
        if(d->size==2)return rgba16((v<<8)|tmem[(a+1)&4095]);
        if(d->size==3) {
            a=(((d->tmem+d->line*y)*8+x*2)^((y&1)*4))&2047;
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
