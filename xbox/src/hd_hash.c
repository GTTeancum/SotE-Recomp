/* RT64 TMEM hash v5, matched to repository tools/rt64_tmem_hash.py.
   xxHash is the existing BSD-licensed repository dependency; scalar for PIII. */
#define XXH_INLINE_ALL
#define XXH_VECTOR 0
#include "../../third_party/N64ModernRuntime/thirdparty/xxHash/xxhash.h"
#include "hd_hash.h"
typedef struct {XXH3_state_t state;const uint8_t* mem;uint64_t bits[4];unsigned siz,tlut,size;} hash_ctx;
static void update(hash_ctx* c,unsigned start,unsigned n,int palette) {
 if(start>=4096)return;if(n>4096-start)n=4096-start;
 XXH3_64bits_update(&c->state,c->mem+start,n);
 if(c->tlut&&!palette)for(unsigned i=0;i<n;i++){
  unsigned v=c->mem[start+i];if(c->siz==0){c->bits[0]|=1ULL<<(v&15);c->bits[0]|=1ULL<<(v>>4);}
  else c->bits[v>>6]|=1ULL<<(v&63);
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
  else for(unsigned i=0;i<(ci4?1u:4u);i++){
   uint64_t bits=c.bits[i];if(bits==UINT64_MAX){update(&c,pal+i*512,512,1);continue;}
   while(bits){unsigned j=__builtin_ctzll(bits);update(&c,pal+(i*64+j)*8,8,1);bits&=bits-1;}
  }
 }
 uint16_t w=width,h=height,line=t->line;uint32_t lut=tlut;uint8_t size=t->size,fmt=t->format;
 XXH3_64bits_update(&c.state,&w,2);XXH3_64bits_update(&c.state,&h,2);XXH3_64bits_update(&c.state,&lut,4);
 XXH3_64bits_update(&c.state,&line,2);XXH3_64bits_update(&c.state,&size,1);XXH3_64bits_update(&c.state,&fmt,1);
 return XXH3_64bits_digest(&c.state);
}
