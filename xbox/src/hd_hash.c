/* RT64 TMEM hash v5, matched to repository tools/rt64_tmem_hash.py.
   xxHash is the existing BSD-licensed repository dependency; scalar for PIII. */
#define XXH_INLINE_ALL
#define XXH_VECTOR 0
#include "../../third_party/N64ModernRuntime/thirdparty/xxHash/xxhash.h"
#include "hd_hash.h"
typedef struct {XXH3_state_t state;const uint8_t* mem;uint32_t bits[8];unsigned siz,tlut,size;} hash_ctx;
/* Cover exactly the possible banks and conservatively include row padding.
   Odd-row byte exchange stays inside its aligned 8-byte TMEM word. TLUT
   accesses are in the upper bank even if the tile's base address wraps. */
unsigned xb_hd_hash_pages(const xb_tile* t,unsigned width,unsigned height,unsigned tlut) {
    unsigned split=t->size==3 && t->format==0;
    unsigned bank_pages=split||tlut?8:16;
    unsigned start=(t->tmem*8)&(bank_pages*256-1);
    unsigned row=(width<<(split?2:t->size))>>1;
    unsigned span=(height-1)*t->line*8+(((row?row:1)+7)&~7u);
    unsigned n=((start&255)+span+255)>>8;
    unsigned mask=(1u<<bank_pages)-1, pages=mask;
    if(n<bank_pages){
        unsigned run=((1u<<n)-1)<<(start>>8);
        pages=(run|(run>>bank_pages))&mask;
    }
    if(split)pages|=pages<<8;
    if(tlut)pages|=t->size==0?1u<<(8+t->palette/2):0xff00u;
    return pages;
}
static void update(hash_ctx* c,unsigned start,unsigned n,int palette) {
 if(start>=4096)return;if(n>4096-start)n=4096-start;
 XXH3_64bits_update(&c->state,c->mem+start,n);
 if(c->tlut&&!palette){
  if(c->siz==0){
   unsigned used=c->bits[0];
   for(unsigned i=0;i<n && used!=0xffff;i++){
    unsigned v=c->mem[start+i];used|=(1u<<(v&15))|(1u<<(v>>4));
   }
   c->bits[0]=used;
  }else for(unsigned i=0;i<n;i++){
   unsigned v=c->mem[start+i];c->bits[v>>5]|=1u<<(v&31);
  }
 }
}
static void part(hash_ctx* c,unsigned base,unsigned bank,unsigned n,int odd) {
 if(base+n>c->size){unsigned first=c->size-base;update(c,base|bank,first,0);n-=first;if(n>base)n=base;base=0;}
 if(odd){unsigned words=n/8;if(words){update(c,base|bank,words*8,0);base+=words*8;n-=words*8;}
  if(n>4){update(c,base|bank,n-4,0);update(c,(base+4)|bank,4,0);}else if(n)update(c,(base+4)|bank,n,0);
 }else update(c,base|bank,n,0);
}
uint64_t xb_hd_hash(const uint8_t* memory,const xb_tile* t,unsigned width,unsigned height,unsigned tlut) {
 hash_ctx c;memset(&c,0,sizeof(c));XXH3_64bits_reset(&c.state);c.mem=memory;c.siz=t->size;c.tlut=tlut;
 unsigned rgba32=t->size==3&&t->format==0;unsigned half=rgba32||tlut;c.size=half?2048:4096;
 unsigned row=(width<<(rgba32?2:t->size))>>1;if(!row)row=1;
 unsigned stride=t->line<<3;unsigned n=stride*(height-1)+row;unsigned mask=c.size-1,addr=(t->tmem<<3)&mask;
 if(stride>row){for(unsigned y=0;y<height;y++)part(&c,(addr+y*stride)&mask,0,row,y&1);
  if(rgba32)for(unsigned y=0;y<height;y++)part(&c,(addr+y*stride)&mask,c.size,row,y&1);
 }else {part(&c,addr,0,n,0);if(rgba32)part(&c,addr,c.size,n,0);}
 if(tlut){unsigned ci4=t->size==0,pal=2048+(ci4?t->palette<<7:0);
  if(ci4&&c.bits[0]==0xffff)update(&c,pal,128,1);
  else for(unsigned i=0;i<(ci4?1u:8u);i++){
   uint32_t bits=c.bits[i];
   while(bits){
    unsigned first=__builtin_ctz(bits);
    uint32_t rest=~(bits>>first);
    unsigned count=rest?__builtin_ctz(rest):32;
    update(&c,pal+(i*32+first)*8,count*8,1);
    if(first+count==32)break;
    bits&=~((1u<<(first+count))-1);
   }
  }
 }
 uint16_t w=width,h=height,line=t->line;uint32_t lut=tlut;uint8_t size=t->size,fmt=t->format;
 XXH3_64bits_update(&c.state,&w,2);XXH3_64bits_update(&c.state,&h,2);XXH3_64bits_update(&c.state,&lut,4);
 XXH3_64bits_update(&c.state,&line,2);XXH3_64bits_update(&c.state,&size,1);XXH3_64bits_update(&c.state,&fmt,1);
 return XXH3_64bits_digest(&c.state);
}
