#include "rdram_guard.h"
#include <windows.h>
#include <xboxkrnl/xboxkrnl.h>
#include <pbkit/pbkit.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "texture.h"
#include "hd_texture.h"
#include "hd_hash.h"
#include "gpu.h"
#include "console_budget.h"
#include "asset_file.h"
#include "menu_settings.h"
#define SLOT_BYTES (4u*1024u*1024u)
#define MAX_RECORDS 8192u
#define CACHE_SLOTS 512
#define PRIMARY_BYTES (24u*1024u*1024u)
#define SECONDARY_BYTES (13u*512u*1024u)
#define RECOVERED_PRIMARY_TRIAL (SECONDARY_BYTES+4u*1024u*1024u)
#define RECOVERED_RETRY_TRIAL (SECONDARY_BYTES+7u*512u*1024u)
static unsigned secondary_bytes;
static unsigned arena_bytes=PRIMARY_BYTES;
#define POOL_BYTES arena_bytes
/* Exact source keys; collisions cannot choose a different asset. */
typedef struct {uint32_t key[17],asset,width,height,levels,bytes;} record;
typedef struct {uint32_t lo,hi,asset,width,height,levels,bytes;} hash_record;
static hash_record* hashes;
static unsigned hash_count,pool_failed,index_loaded;
static record* records;
static unsigned count,initialized,enabled;
static uint8_t* pool,*extension;
static unsigned extension_tried;
static uint8_t* hd_address(unsigned off){return off<PRIMARY_BYTES?pool+off:extension+(off-PRIMARY_BYTES);}
volatile uint32_t sote_hd_layout[8]; /* banks, extension_denied/fail, compacts, bytes moved, move ticks, spans-not-fit, selective-victims, native slots */

static struct {uint32_t* pixels;unsigned asset,valid,offset,span,stamp,filled;} cache[CACHE_SLOTS];
volatile uint32_t sote_hd_status[16]; /* enabled,source count,lookups,matches,hits,misses,failures,resident,bytes read,last asset,last context,skipped,hash count,hash lookups,hash matches,budget denial */
volatile uint32_t sote_hd_last_key[17];
volatile uint32_t sote_hd_last_hash[2];
/* Bounded diagnostic history: scene, immutable asset ID, bytes, cache hit.
   Publication counter advances only after the complete record is written. */
volatile uint32_t sote_hd_trace[128][4],sote_hd_trace_count;
static void trace_asset(unsigned scene,unsigned asset,unsigned bytes,unsigned hit){
 unsigned i=sote_hd_trace_count&127u;sote_hd_trace[i][0]=scene;
 sote_hd_trace[i][1]=asset;sote_hd_trace[i][2]=bytes;sote_hd_trace[i][3]=hit;
 sote_hd_trace_count++;
}
static unsigned active_scene=~0u;
/* Diagnostic only. Tracks observed fixture pack IDs 1..8191. Overflow is
 * reported separately, never changes which textures the renderer uses. */
#define HOTSET_IDS 8192
static uint32_t hot_frame=1, hot_count, hot_bytes, hot_reads, hot_read_bytes,
                hot_waits, hot_wait_ms, hot_reuse[5], hot_unknown;
static struct {uint32_t last, span, present_prefix;} hot[HOTSET_IDS];
static unsigned hot_prefix;
volatile uint32_t sote_hd_prefix_frame[64][5]; /* frame,event,IDs,prefix,full */
volatile uint32_t sote_hd_prefix_stats[8]; /* binds base-only / multi; promotions inplace / relocate; file bytes not transferred on successful reads; range faults; largest needed prefix; scans */

volatile uint32_t sote_hd_hotset_count;
volatile uint32_t sote_hd_hotset[64][17];
/* Probe-only complete membership at each phase maximum (IDs 1..8191).
 * Indices 0..31 native event; 32 reserved other. Four windows: 1/4/16/32.
 * frame published last, after bitset and byte count.
 */
volatile uint32_t sote_hd_hotset_peak[33][4][2+HOTSET_IDS/32];
static void hot_touch(unsigned asset,unsigned size,unsigned used) {
    if(asset>=HOTSET_IDS){hot_unknown++;return;}
    unsigned last=hot[asset].last;
    unsigned need=(used+63u)&~63u;
    if(last==hot_frame){
        if(need>hot[asset].present_prefix){hot_prefix+=need-hot[asset].present_prefix;hot[asset].present_prefix=need;}
        return;
    }
    hot[asset].present_prefix=need;hot_prefix+=need;
    if(last){unsigned gap=hot_frame-last;
        hot_reuse[gap<=1?0:gap<=4?1:gap<=16?2:gap<=32?3:4]++;}
    hot[asset].last=hot_frame;hot[asset].span=(size+63u)&~63u;
    hot_count++;hot_bytes+=hot[asset].span;
}
void xb_hd_probe_present(unsigned event,unsigned pause) {
    if(!enabled)return;
    unsigned rolling[3]={0};
    for(unsigned i=1;i<HOTSET_IDS;i++)if(hot[i].last){
        unsigned gap=hot_frame-hot[i].last;
        if(gap<32){rolling[2]+=hot[i].span;
            if(gap<16){rolling[1]+=hot[i].span;if(gap<4)rolling[0]+=hot[i].span;}}
    }

    unsigned scene=event<32?event:32;
    const unsigned w[4]={1,4,16,32};
    unsigned total[4]={hot_bytes,rolling[0],rolling[1],rolling[2]};
    for(unsigned n=0;n<4;n++){
        volatile uint32_t* peak=sote_hd_hotset_peak[scene][n];
        if(total[n]>peak[1]){
            peak[0]=0;
            for(unsigned i=0;i<HOTSET_IDS/32;i++)peak[2+i]=0;
            for(unsigned i=1;i<HOTSET_IDS;i++)
                if(hot[i].last && hot_frame-hot[i].last<w[n])
                    peak[2+(i>>5)]|=1u<<(i&31u);
            peak[1]=total[n];peak[0]=hot_frame;
        }
    }
    volatile uint32_t* p=sote_hd_hotset[sote_hd_hotset_count&63];
    volatile uint32_t* q=sote_hd_prefix_frame[sote_hd_hotset_count&63];
    q[0]=hot_frame;q[1]=event;q[2]=hot_count;q[3]=hot_prefix;q[4]=hot_bytes;hot_prefix=0;
    p[0]=hot_frame++;p[1]=event;p[2]=pause;p[3]=hot_count;p[4]=hot_bytes;
    for(unsigned i=0;i<3;i++)p[5+i]=rolling[i];
    for(unsigned i=0;i<5;i++){p[8+i]=hot_reuse[i];hot_reuse[i]=0;}
    p[13]=hot_reads;p[14]=hot_read_bytes;p[15]=hot_waits;
    /* low word: elapsed fence ms; high word: out-of-range lookup count */
    p[16]=(hot_wait_ms&65535u)|((hot_unknown&65535u)<<16);
    hot_count=hot_bytes=hot_reads=hot_read_bytes=hot_waits=hot_wait_ms=hot_unknown=0;
    sote_hd_hotset_count++;
}

static unsigned hd_clock;
volatile uint32_t sote_hd_reuse[4]; /* evictions, retired old bytes, same-present evictions, admitted active bytes */
static unsigned slot_frame[CACHE_SLOTS];
/* Advance strictly across occupied ranges. Search happens on misses only. */
static int fit_one(unsigned begin,unsigned end,unsigned span,unsigned* out) {
    if(span>end-begin)return 0;
    for(unsigned off=begin;off<=end-span;){
        unsigned cover=off,next=end;
        for(unsigned i=0;i<CACHE_SLOTS;i++)if(cache[i].valid){
            unsigned b=cache[i].offset,e=b+cache[i].span;
            if(b<=off&&e>cover)cover=e;
            if(b>off&&b<next)next=b;
        }
        if(cover>off){off=cover;continue;}
        if(span<=next-off){*out=off;return 1;}
        if(next==end)break;
        off=next;
    }
    return 0;
}
static int fit_gap(unsigned span,unsigned* off){
    return fit_one(0,PRIMARY_BYTES,span,off)||
          (extension&&fit_one(PRIMARY_BYTES,arena_bytes,span,off));
}
/* Bounded passive sample: attempts, ticket failures, kernel failures, successes,
   used/free at decision, requested, native liability, other pending, cushion,
   actual bank address and peak target. No probe value influences admission. */
volatile uint32_t sote_hd_bank_probe[12];
extern volatile uint32_t sote_budget_native_gpu[4],sote_budget_optional[8];
/* Admission tickets also cover all outstanding liabilities. Larger banks
   are independent, contiguous and immutable once published. No mixed arena
   layout, no moving the primary bank, and no relaxation on failed attempts. */
volatile uint32_t sote_hd_recovered_bank[8]; /* 4M/3.5M/base attempt bits, reserve errors,
                                              alloc errors, postcheck reject, selected size,
                                              actual addr, gained, tried while live lower-4 */
static int try_bank(unsigned size,unsigned attempt_index) {
    unsigned bit=1u<<attempt_index;
    sote_hd_recovered_bank[0]|=bit;
    sote_hd_bank_probe[0]++;
    MM_STATISTICS m={0};m.Length=sizeof(m);
    if(MmQueryStatistics(&m)>=0 && m.TotalPhysicalPages>=m.AvailablePages){
        sote_hd_bank_probe[4]=(m.TotalPhysicalPages-m.AvailablePages)*4096u;
        sote_hd_bank_probe[5]=m.AvailablePages*4096u;
    }
    sote_hd_bank_probe[6]=size;
    sote_hd_bank_probe[7]=sote_budget_native_gpu[0];sote_hd_bank_probe[8]=sote_budget_optional[6];
    sote_hd_bank_probe[9]=XB_BUDGET_OPTIONAL_CUSHION;sote_hd_bank_probe[11]=XB_BUDGET_TARGET;
    uint32_t ticket=xb_budget_optional_reserve(size);
    if(!ticket){sote_hd_recovered_bank[1]|=bit;sote_hd_bank_probe[1]++;return 0;}
    void* candidate=MmAllocateContiguousMemoryEx(size,0,0x3ffb000,0,PAGE_READWRITE|PAGE_WRITECOMBINE);
    if(!candidate){xb_budget_optional_finish(ticket);sote_hd_recovered_bank[2]|=bit;sote_hd_bank_probe[2]++;return 0;}
    uint32_t physical=(uint32_t)(uintptr_t)candidate&0x03ffffffu;
    if((physical&4095u) || physical>0x03ffc000u-size ||
       !xb_budget_optional_recheck_resident(ticket)){
        MmFreeContiguousMemory(candidate);
        xb_budget_optional_finish(ticket);
        sote_hd_recovered_bank[3]|=bit;return 0;
    }
    xb_budget_optional_finish(ticket);
    extension=candidate;secondary_bytes=size;
    sote_hd_recovered_bank[4]=size;sote_hd_recovered_bank[5]=(uint32_t)(uintptr_t)extension;
    sote_hd_recovered_bank[6]=size-SECONDARY_BYTES;
    sote_hd_bank_probe[3]++;sote_hd_bank_probe[10]=(uint32_t)(uintptr_t)extension;
    arena_bytes=PRIMARY_BYTES+secondary_bytes;sote_hd_status[7]=arena_bytes;sote_hd_layout[0]=2;
    xb_budget_mark(XB_BUDGET_OPTIONAL_RESIDENT,arena_bytes+count*sizeof(record)+hash_count*sizeof(hash_record));
    return 1;
}
static int add_bank(void) {
    if(extension_tried)return 0;extension_tried=1;
    /* Only lean 4 MiB can try to reuse recovered headroom; diagnostic or
       full-memory configurations retain their original allocation size. */
    if(xb_rdram_committed_limit==XB_RDRAM_LIVE){
        sote_hd_recovered_bank[7]=1;
        if(try_bank(RECOVERED_PRIMARY_TRIAL,0)||try_bank(RECOVERED_RETRY_TRIAL,1))return 1;
    }
    if(try_bank(SECONDARY_BYTES,2))return 1;
    sote_hd_layout[1]++;return 0;
}

/* No hostile file data is trusted when finding a slot; indices are in-memory.
   One compact per presentation maximum, only if that bank's total free
   bytes suffice and no normal hole fits. Forward moves toward lower
   addresses preserve overlapping bytes. Every GPU reference retired in
   prepare_write() before entry; the CPU-side triangle fast path was reset. */
static unsigned compact_frame;
static int compact_for(unsigned span,unsigned* out){
    if(compact_frame==hot_frame)return 0;
    unsigned used[2]={0},cap[2]={PRIMARY_BYTES,extension?secondary_bytes:0};
    for(unsigned i=0;i<CACHE_SLOTS;i++)if(cache[i].valid)used[cache[i].offset>=PRIMARY_BYTES]+=cache[i].span;
    for(unsigned bank=0;bank<2;bank++)if(cap[bank]&&span<=cap[bank]-used[bank]){
        unsigned begin=bank?PRIMARY_BYTES:0,cursor=begin,scan=begin,start=GetTickCount();
        for(;;){
            unsigned slot=CACHE_SLOTS,best=~0u;
            for(unsigned i=0;i<CACHE_SLOTS;i++)if(cache[i].valid){
                unsigned off=cache[i].offset;if(off>=scan&&off<begin+cap[bank]&&off<best){best=off;slot=i;}
            }
            if(slot==CACHE_SLOTS)break;
            unsigned bytes=cache[slot].span;scan=best+bytes;
            if(cursor!=best){
                void* dst=hd_address(cursor);const void* src=hd_address(best);size_t words=bytes/4;
                __asm__ volatile("cld; rep movsl" : "+D"(dst),"+S"(src),"+c"(words)::"cc","memory");
                cache[slot].pixels=(uint32_t*)hd_address(cursor);cache[slot].offset=cursor;
                sote_hd_layout[3]+=bytes;
            }
            cursor+=bytes;
        }
        __asm__ volatile("sfence":::"memory");
        sote_hd_layout[2]++;sote_hd_layout[4]+=GetTickCount()-start;compact_frame=hot_frame;
        *out=cursor;return 1;
    }
    return 0;
}
static int can_grow(unsigned id,unsigned span) {
    unsigned off=cache[id].offset;
    if(span>(off<PRIMARY_BYTES?PRIMARY_BYTES:arena_bytes)-off)return 0;
    for(unsigned j=0;j<CACHE_SLOTS;j++)if(j!=id&&cache[j].valid&&
       cache[j].offset<off+span&&off<cache[j].offset+cache[j].span)return 0;
    return 1;
}
static void retire_slot(unsigned v){
    sote_hd_reuse[0]++;sote_hd_reuse[1]+=cache[v].span;
    if(slot_frame[v]==hot_frame)sote_hd_reuse[2]++;
    sote_hd_reuse[3]-=cache[v].span;
    cache[v].valid=0;
}
static unsigned retire_oldest(void) {
    unsigned v=CACHE_SLOTS;
    for(unsigned i=0;i<CACHE_SLOTS;i++)if(cache[i].valid &&
       (v==CACHE_SLOTS||(int32_t)(cache[i].stamp-cache[v].stamp)<0))v=i;
    if(v!=CACHE_SLOTS)retire_slot(v);
    return v;
}
static uint32_t scene_seen_assets[65536u/32u];
volatile uint32_t sote_hd_demand[5]; /* unique ids, scene bytes, overflow ids, overflow bytes, epochs */
volatile uint32_t sote_hd_first_overflow[64][3]; /* asset, bytes, scene; first 64 per scene */
volatile uint32_t sote_hd_admission[4]; /* scene, epochs, capacity skips, largest admitted end */
static unsigned log2u(unsigned v){unsigned n=0;while(v>1){v>>=1;n++;}return n;}
static int power2(unsigned v){return v && !(v&(v-1));}
/* Internal pack extension: the old low byte is still the mip count.
   Bit 8 describes sequential linear DXT5 (BC3) blocks, down to 4x4. */
#define HD_DXT5 0x100u
#define HD_DXT1 0x200u
#define HD_CODEC (HD_DXT1|HD_DXT5)
static unsigned hd_mips(unsigned encoded){return encoded&255u;}
static uint64_t hd_payload_size(unsigned w,unsigned h,unsigned encoded){
    unsigned m=hd_mips(encoded);
    if(!m||m>12||(encoded&~(255u|HD_CODEC))||(encoded&HD_CODEC)==HD_CODEC)return 0;
    uint64_t bytes=0;
    for(unsigned i=0;i<m;i++){
        if(encoded&HD_CODEC){
            if(w<4||h<4)return 0;
            bytes+=(uint64_t)(w/4)*(h/4)*(encoded&HD_DXT1?8u:16u);
        }else bytes+=(uint64_t)w*h*4;
        w=w>1?w/2:1;h=h>1?h/2:1;
    }
    return bytes;
}
static void load_index(void) {
    FILE* f=fopen("D:\\hdtextures\\index.bin","rb");if(!f){sote_hd_status[6]++;return;}
    uint32_t h[3];
    if(fread(h,1,sizeof(h),f)!=sizeof(h)||h[0]!=0x31444858u||h[1]!=1||h[2]>MAX_RECORDS){fclose(f);sote_hd_status[6]++;return;}
    uint32_t ticket=0;record* r=0;
    if(h[2]){
        ticket=xb_budget_optional_reserve(xb_budget_optional_heap_cost(h[2]*sizeof(record)));
        if(!ticket){fclose(f);sote_hd_status[15]++;return;}
        r=malloc(h[2]*sizeof(record));xb_budget_optional_finish(ticket);
        if(!r){fclose(f);sote_hd_status[6]++;return;}
        if(fread(r,sizeof(record),h[2],f)!=h[2]){free(r);fclose(f);sote_hd_status[6]++;return;}
    }
    fclose(f);
    for(unsigned i=0;i<h[2];i++) {
        if(!power2(r[i].width)||!power2(r[i].height)||r[i].width>2048||r[i].height>2048||
          !r[i].bytes||r[i].bytes>SLOT_BYTES||r[i].asset>65535||
          !r[i].key[2]||!r[i].key[3]){free(r);sote_hd_status[6]++;return;}
        uint64_t bytes=hd_payload_size(r[i].width,r[i].height,r[i].levels);
        if(bytes!=r[i].bytes){free(r);sote_hd_status[6]++;return;}
    }
    for(unsigned i=1;i<h[2];i++) {
        int cmp=0;for(unsigned k=0;k<17;k++)if(r[i-1].key[k]!=r[i].key[k]){cmp=r[i-1].key[k]<r[i].key[k]?-1:1;break;}
        if(cmp>=0){free(r);sote_hd_status[6]++;return;}
    }
    records=r;count=h[2];index_loaded=1;sote_hd_status[1]=count;
    xb_budget_mark(XB_BUDGET_OPTIONAL_RESIDENT,count*sizeof(record));
    f=fopen("D:\\hdtextures\\hash_index.bin","rb");if(!f)return;
    if(fread(h,1,12,f)!=12||h[0]!=0x31484858||h[1]!=1||h[2]>16384){fclose(f);sote_hd_status[6]++;return;}
    ticket=xb_budget_optional_reserve(xb_budget_optional_heap_cost(h[2]*sizeof(hash_record)));
    if(!ticket){fclose(f);sote_hd_status[15]++;return;}
    hash_record* hr=malloc(h[2]*sizeof(hash_record));xb_budget_optional_finish(ticket);
    if(!hr){fclose(f);sote_hd_status[6]++;return;}
    if(fread(hr,sizeof(hash_record),h[2],f)!=h[2]){free(hr);fclose(f);sote_hd_status[6]++;return;}fclose(f);
    for(unsigned i=0;i<h[2];i++){
        if(!power2(hr[i].width)||!power2(hr[i].height)||hr[i].width>2048||hr[i].height>2048||
          !hr[i].bytes||hr[i].bytes>SLOT_BYTES||hr[i].asset>65535){free(hr);sote_hd_status[6]++;return;}
        uint64_t bytes=hd_payload_size(hr[i].width,hr[i].height,hr[i].levels);
        if(bytes!=hr[i].bytes || (i && (((uint64_t)hr[i-1].hi<<32)|hr[i-1].lo)>=(((uint64_t)hr[i].hi<<32)|hr[i].lo))){free(hr);sote_hd_status[6]++;return;}
    }
    hashes=hr;hash_count=h[2];sote_hd_status[12]=hash_count;
    xb_budget_mark(XB_BUDGET_OPTIONAL_RESIDENT,count*sizeof(record)+hash_count*sizeof(hash_record));
}
/* Validate the installed index header/extent without loading its tables or any
   image when OFF. An empty source-key table is legal for a hash-only pack. */
static int header_entries(const char* path,unsigned magic,unsigned limit,unsigned stride) {
    FILE* f=fopen(path,"rb");if(!f)return -1;
    uint32_t h[3];int count=-2;
    if(fread(h,1,sizeof(h),f)==sizeof(h)&&h[0]==magic&&h[1]==1&&h[2]<=limit&&
       !fseek(f,0,SEEK_END)&&ftell(f)==(long)(sizeof(h)+h[2]*stride))count=(int)h[2];
    fclose(f);return count;
}
volatile uint32_t sote_hd_boot[5]; /* legacy .on marker, valid pack headers, preferred, effective, initialized */
static void init(void) {
    initialized=1;
    FILE* marker=fopen("D:\\hd-textures.on","rb");sote_hd_boot[0]=marker!=NULL;if(marker)fclose(marker);
    int preferred=xb_hd_textures_preferred();
    int n=header_entries("D:\\hdtextures\\index.bin",0x31444858u,MAX_RECORDS,sizeof(record));
    int nh=n>=0?header_entries("D:\\hdtextures\\hash_index.bin",0x31484858u,16384,sizeof(hash_record)):-1;
    int installed=n>=0&&nh!=-2&&(n>0||nh>0);
    sote_hd_boot[1]=installed;sote_hd_boot[2]=preferred;
    /* The legacy ON marker remains supported; a valid pack is now also
       sufficient without it. A saved OFF always wins, with or without marker.
       No texture IO or HD allocation on stock boots. */
    if(preferred&&installed){
        load_index();
        enabled=index_loaded&&count==(unsigned)n&&(nh<0||hash_count==(unsigned)nh)&&(count||hash_count);
        if(!enabled){ /* No partial indexes are advertised as a successfully loaded pack. */
            sote_hd_boot[1]=0;free(records);free(hashes);records=NULL;hashes=NULL;index_loaded=0;
            count=hash_count=0;sote_hd_status[1]=sote_hd_status[12]=0;
        }
    } else if(preferred&&(n==-2||nh==-2||sote_hd_boot[0])) {
        /* Missing/malformed marked installation has an explicit diagnostic. */
        sote_hd_status[6]++;
    }
    sote_hd_layout[7]=xb_budget_fix_native_texture_profile(enabled);
    sote_hd_status[0]=enabled;sote_hd_boot[3]=enabled;sote_hd_boot[4]=1;
}
unsigned xb_hd_boot_texture_slots(void){if(!initialized)init();return sote_hd_layout[7];}
int xb_hd_pack_available(void){if(!initialized)init();return sote_hd_boot[1]!=0;}
int xb_hd_session_active(void){if(!initialized)init();return enabled;}
int xb_hd_texture(unsigned tile,unsigned tlut,unsigned width,unsigned height,unsigned levels,
                  uint32_t** pixels,unsigned* format) {
    if(!initialized)init();if(!enabled||(!records&&!hashes))return 0;
    uint32_t key[17];int has_key=xb_texture_slot_key(tile,tlut,width,height,key);if(!has_key)memset(key,0,sizeof(key));
    unsigned scene=xb_texture_scene();
    if(active_scene!=scene){
        active_scene=scene; /* Immutable GPU assets can survive native scene changes. */
        memset(scene_seen_assets,0,sizeof(scene_seen_assets));
        memset(hot,0,sizeof(hot));
        for(unsigned i=0;i<4;i++)sote_hd_demand[i]=0;
        sote_hd_demand[4]++;
        sote_hd_admission[0]=scene;sote_hd_admission[1]++;
    }
    for(unsigned i=0;i<17;i++)sote_hd_last_key[i]=key[i];
    sote_hd_status[2]++;sote_hd_status[10]=key[0];
    record* r=0;
    unsigned lo=0,hi=has_key?count:0;
    while(lo<hi){unsigned mid=lo+(hi-lo)/2;int cmp=0;
        for(unsigned k=0;k<17;k++)if(records[mid].key[k]!=key[k]){cmp=records[mid].key[k]<key[k]?-1:1;break;}
        if(!cmp){r=records+mid;break;}if(cmp<0)lo=mid+1;else hi=mid;
    }
    record hash_result;
    if(!r&&hashes){
        sote_hd_status[13]++;uint64_t hash=xb_hd_hash(xb_texture_memory(),&xb_tiles[tile],width,height,tlut<<14);
        sote_hd_last_hash[0]=(uint32_t)hash;sote_hd_last_hash[1]=(uint32_t)(hash>>32);
        lo=0;hi=hash_count;while(lo<hi){unsigned mid=lo+(hi-lo)/2;uint64_t v=((uint64_t)hashes[mid].hi<<32)|hashes[mid].lo;
            if(v==hash){memcpy(&hash_result.asset,&hashes[mid].asset,5*sizeof(uint32_t));r=&hash_result;sote_hd_status[14]++;break;}
            if(v<hash)lo=mid+1;else hi=mid;
        }
    }
    if(!r)return 0;
    if(!levels||levels>hd_mips(r->levels)){sote_hd_status[11]++;return 0;}
    /* NV2A FORMAT and CONTROL0 clamp to exactly these levels. The file
       layout is consecutive top-down mips; don't upload unreachable suffixes. */
    unsigned need=(unsigned)hd_payload_size(r->width,r->height,(r->levels&HD_CODEC)|levels);
    if(!need||need>r->bytes){sote_hd_prefix_stats[5]++;return 0;}
    sote_hd_prefix_stats[levels>1]++;if(need>sote_hd_prefix_stats[6])sote_hd_prefix_stats[6]=need;
    hot_touch(r->asset,r->bytes,need);
    sote_hd_status[3]++;sote_hd_status[9]=r->asset;
    unsigned chosen=CACHE_SLOTS,promote=CACHE_SLOTS;
    for(unsigned i=0;i<CACHE_SLOTS;i++) {
        if(cache[i].valid&&cache[i].asset==r->asset){
            if(cache[i].filled>=need){chosen=i;sote_hd_status[4]++;trace_asset(scene,r->asset,need,1);goto found;}
            promote=i;break;
        }
        if(!cache[i].valid&&chosen==CACHE_SLOTS)chosen=i;
    }
    unsigned span=(need+63)&~63u;
    unsigned seen=r->asset>>5;
    uint32_t mask=1u<<(r->asset&31u);
    unsigned overflow=span>POOL_BYTES;
    if(!(scene_seen_assets[seen]&mask)){
        scene_seen_assets[seen]|=mask;
        sote_hd_demand[0]++;
        sote_hd_demand[1]=span>UINT32_MAX-sote_hd_demand[1]?UINT32_MAX:sote_hd_demand[1]+span;
        if(overflow){
            unsigned i=sote_hd_demand[2]++;
            sote_hd_demand[3]=span>UINT32_MAX-sote_hd_demand[3]?UINT32_MAX:sote_hd_demand[3]+span;
            if(i<64){
                sote_hd_first_overflow[i][0]=r->asset;
                sote_hd_first_overflow[i][1]=span;
                sote_hd_first_overflow[i][2]=scene;
            }
        }
    }
    if(overflow){sote_hd_admission[2]++;return 0;}
    trace_asset(scene,r->asset,need,0);
    if(pool_failed)return 0;
    unsigned hot_start=GetTickCount();
    xb_gpu_hd_prepare_write();while(pb_busy())Sleep(0);
    hot_waits++;hot_wait_ms+=GetTickCount()-hot_start;
    if(!pool) {
        uint32_t ticket=xb_budget_optional_reserve(POOL_BYTES);
        if(!ticket){pool_failed=1;sote_hd_status[15]++;return 0;}
        pool=MmAllocateContiguousMemoryEx(POOL_BYTES,0,0x3ffb000,0,PAGE_READWRITE|PAGE_WRITECOMBINE);
        xb_budget_optional_finish(ticket);
        if(!pool){pool_failed=1;sote_hd_status[6]++;return 0;}
        sote_hd_layout[0]=1;sote_hd_status[7]=POOL_BYTES;xb_budget_mark(XB_BUDGET_OPTIONAL_RESIDENT,POOL_BYTES+count*sizeof(record)+hash_count*sizeof(hash_record));
    }
    /* GPU was fenced above, so retired bytes are no longer referenced. */
    unsigned offset=0,start=0,oldspan=0;
    if(promote!=CACHE_SLOTS&&can_grow(promote,span)){
        chosen=promote;offset=cache[chosen].offset;start=cache[chosen].filled;oldspan=cache[chosen].span;
    }else{
        /* Never read back WC pages. If moving is necessary, retire the old
           descriptor, then read an exact contiguous prefix at the new place. */
        if(promote!=CACHE_SLOTS){retire_slot(promote);chosen=promote;sote_hd_prefix_stats[3]++;}
        if(chosen==CACHE_SLOTS)chosen=retire_oldest();
        while(!fit_gap(span,&offset)){
            if(!extension_tried&&sote_hd_layout[7]==32&&add_bank())continue;
            if(compact_for(span,&offset))break;
            sote_hd_layout[5]++;

            unsigned retired=retire_oldest();
            if(retired==CACHE_SLOTS){sote_hd_admission[2]++;return 0;}
            if(chosen==CACHE_SLOTS)chosen=retired;
        }
        if(chosen==CACHE_SLOTS){sote_hd_admission[2]++;return 0;}
    }
    cache[chosen].pixels=(uint32_t*)hd_address(offset);
    char path[64],legacy[64];
    snprintf(path,sizeof(path),"D:\\hdtextures\\%02x\\%04x.bin",r->asset>>8,r->asset);
    snprintf(legacy,sizeof(legacy),"D:\\hdtextures\\%04x.bin",r->asset);
    if(!xb_asset_read_range_exact(path,legacy,hd_address(offset)+start,r->bytes,start,need-start)){
        sote_hd_status[6]++;return 0;
    }
    hot_reads++;hot_read_bytes+=need-start;sote_hd_status[8]+=need-start;
    sote_hd_prefix_stats[4]+=r->bytes-(need-start);
    if(start)sote_hd_prefix_stats[2]++;
    cache[chosen].filled=need;
    __asm__ volatile("sfence":::"memory");
    cache[chosen].offset=offset;cache[chosen].span=span;sote_hd_reuse[3]+=span-oldspan;
    if(sote_hd_reuse[3]>sote_hd_admission[3])sote_hd_admission[3]=sote_hd_reuse[3];
    cache[chosen].asset=r->asset;cache[chosen].valid=1;sote_hd_status[5]++;
found:
    cache[chosen].stamp=++hd_clock;slot_frame[chosen]=hot_frame;
    *pixels=cache[chosen].pixels;
    *format=1|8|0x20|(((r->levels&HD_DXT1)?0x0c:(r->levels&HD_DXT5)?0x0f:6)<<8)|(levels<<16)|(log2u(r->width)<<20)|(log2u(r->height)<<24);
    return 1;
}
