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
#include "display_mode.h"
#include "asset_file.h"
#include "menu_settings.h"
#define SLOT_BYTES (4u*1024u*1024u)
#define MAX_RECORDS 8192u
#define CACHE_SLOTS 512
/* NV097_SET_TEXTURE_OFFSET accepts 128-byte-aligned image bases. Row pitch
   alignment (64) is insufficient for adjacent textures in this arena. */
static unsigned texture_span(unsigned bytes){return (bytes+127u)&~127u;}
static unsigned primary_bytes=24u*1024u*1024u;
#define SECONDARY_BYTES (13u*512u*1024u)
#define RECOVERED_PRIMARY_TRIAL (SECONDARY_BYTES+4u*1024u*1024u)
#define RECOVERED_RETRY_TRIAL (SECONDARY_BYTES+7u*512u*1024u)
static unsigned secondary_bytes;
static unsigned arena_bytes=24u*1024u*1024u;
#define POOL_BYTES arena_bytes
/* Exact source keys; collisions cannot choose a different asset. */
typedef struct {uint32_t key[17],asset,width,height,levels,bytes;} record;
typedef struct {uint32_t lo,hi,asset,width,height,levels,bytes;} hash_record;
static hash_record* hashes;
static unsigned hash_count,pool_failed,index_loaded;
static record* records;
static unsigned count,initialized,enabled;
/* Most original slots have no override. Index the immutable pack by scene
   and source address so a miss usually needs one small table read. */
static uint16_t source_head[4096],source_next[MAX_RECORDS];
static unsigned source_bucket(const uint32_t* key) {
    uint32_t h=(key[1]^(key[0]*0x9e3779b9u))*0x85ebca6bu;
    return h>>20;
}
static uint8_t* pool,*extension;
static unsigned extension_tried;
static uint8_t* hd_address(unsigned off){return off<primary_bytes?pool+off:extension+(off-primary_bytes);}
volatile uint32_t sote_hd_layout[8]; /* banks, extension_denied/fail, compacts, bytes moved, move ticks, spans-not-fit, selective-victims, native slots */

static struct {uint32_t* pixels;unsigned asset,valid,offset,span,stamp,filled;} cache[CACHE_SLOTS];
/* Direct hint only. Distinct IDs can share a bucket, and eviction can leave a
   stale hint, so always validate the resident ID before reusing its pixels. */
static uint16_t resident_hint[4096];
volatile uint32_t sote_hd_status[16]; /* enabled,source count,lookups,matches,hits,misses,failures,resident,bytes read,last asset,last context,shorter mip chain,hash count,hash lookups,hash matches,budget denial */
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
    unsigned need=texture_span(used);
    if(last==hot_frame){
        if(need>hot[asset].present_prefix){hot_prefix+=need-hot[asset].present_prefix;hot[asset].present_prefix=need;}
        return;
    }
    hot[asset].present_prefix=need;hot_prefix+=need;
    if(last){unsigned gap=hot_frame-last;
        hot_reuse[gap<=1?0:gap<=4?1:gap<=16?2:gap<=32?3:4]++;}
    hot[asset].last=hot_frame;hot[asset].span=texture_span(size);
    hot_count++;hot_bytes+=hot[asset].span;
}
void xb_hd_probe_present(unsigned event,unsigned pause) {
    if(!enabled)return;
    if(!xb_budget_frame_probe){++hot_frame;return;}
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
/* Keep occupied ranges in address order. Evictions and admissions are rare,
   while a fragmented arena may require many first-fit searches per miss. */
static uint16_t range_slots[CACHE_SLOTS];
static unsigned range_count;
static void remove_range(unsigned slot) {
    for(unsigned i=0;i<range_count;i++)if(range_slots[i]==slot){
        memmove(range_slots+i,range_slots+i+1,(--range_count-i)*sizeof(*range_slots));
        return;
    }
}
static void insert_range(unsigned slot) {
    unsigned i=range_count;
    while(i && cache[range_slots[i-1]].offset>cache[slot].offset){
        range_slots[i]=range_slots[i-1];i--;
    }
    range_slots[i]=slot;range_count++;
}
static int fit_one(unsigned begin,unsigned end,unsigned span,unsigned* out) {
    unsigned cursor=begin;
    for(unsigned i=0;i<range_count;i++){
        unsigned slot=range_slots[i],off=cache[slot].offset;
        if(off<begin)continue;
        if(off>=end)break;
        if(span<=off-cursor){*out=cursor;return 1;}
        cursor=off+cache[slot].span;
    }
    if(span>end-cursor)return 0;
    *out=cursor;return 1;
}
static int fit_gap(unsigned span,unsigned* off){
    return fit_one(0,primary_bytes,span,off)||
          (extension&&fit_one(primary_bytes,arena_bytes,span,off));
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
    arena_bytes=primary_bytes+secondary_bytes;sote_hd_status[7]=arena_bytes;sote_hd_layout[0]=2;
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
    unsigned used[2]={0},cap[2]={primary_bytes,extension?secondary_bytes:0};
    for(unsigned i=0;i<CACHE_SLOTS;i++)if(cache[i].valid)used[cache[i].offset>=primary_bytes]+=cache[i].span;
    for(unsigned bank=0;bank<2;bank++)if(cap[bank]&&span<=cap[bank]-used[bank]){
        unsigned begin=bank?primary_bytes:0,cursor=begin,start=GetTickCount();
        for(unsigned i=0;i<range_count;i++){
            unsigned slot=range_slots[i],best=cache[slot].offset;
            if(best<begin)continue;
            if(best>=begin+cap[bank])break;
            unsigned bytes=cache[slot].span;
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
    if(span>(off<primary_bytes?primary_bytes:arena_bytes)-off)return 0;
    for(unsigned j=0;j<CACHE_SLOTS;j++)if(j!=id&&cache[j].valid&&
       cache[j].offset<off+span&&off<cache[j].offset+cache[j].span)return 0;
    return 1;
}
static void retire_slot(unsigned v){
    sote_hd_reuse[0]++;sote_hd_reuse[1]+=cache[v].span;
    if(slot_frame[v]==hot_frame)sote_hd_reuse[2]++;
    sote_hd_reuse[3]-=cache[v].span;
    remove_range(v);
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
static unsigned log2u(unsigned v){return __builtin_ctz(v);} /* validated power of two */
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
    xb_asset_open_bundle(records,count*sizeof(*records),hashes,hash_count*sizeof(*hashes));
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
    /* At 720p scanout and render surfaces occupy 17.6 MiB. BC textures
       stream through a smaller bank; retain the original SD cache profile. */
    if(xb_display_height>=720)primary_bytes=arena_bytes=6u*1024u*1024u;
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
    for(unsigned i=count;i>0;i--){
        unsigned bucket=source_bucket(records[i-1].key);
        source_next[i-1]=source_head[bucket];
        source_head[bucket]=i;
    }
    sote_hd_status[0]=enabled;sote_hd_boot[3]=enabled;sote_hd_boot[4]=1;
}
unsigned xb_hd_boot_texture_slots(void){if(!initialized)init();return sote_hd_layout[7];}
int xb_hd_pack_available(void){if(!initialized)init();return sote_hd_boot[1]!=0;}
int xb_hd_session_active(void){if(!initialized)init();return enabled;}

/* RT64 hashing inspects texels and, for indexed images, their palette. Cache
   the immutable digest, independently of HD residency. Sampled words select
   a bucket only; every byte that might affect the hash must still match. */
typedef struct {
    uint32_t key[4], selector;
    uint8_t memory[4096] __attribute__((aligned(16)));
    uint64_t digest;
    const hash_record* match;
} digest_entry;
static digest_entry digests[64][2];
static uint8_t digest_victim[64];
volatile uint32_t sote_hd_digest_cache[2]; /* exact reuses, fresh computations */
static const hash_record* find_digest(uint64_t digest) {
    unsigned lo=0,hi=hash_count;
    while(lo<hi){
        unsigned mid=lo+(hi-lo)/2;
        uint64_t value=((uint64_t)hashes[mid].hi<<32)|hashes[mid].lo;
        if(value==digest)return hashes+mid;
        if(value<digest)lo=mid+1;else hi=mid;
    }
    return NULL;
}
static const digest_entry* content_match(const xb_tile* tile,unsigned tlut,unsigned width,unsigned height) {
    const unsigned pages=xb_hd_hash_pages(tile,width,height,tlut);
    const uint32_t key[]={width,height,tile->tmem|(tile->line<<9),
                         tile->size|(tile->format<<2)|(tile->palette<<5)|(tlut<<9)};
    unsigned selector=xb_texture_page_hash(pages);
    for(unsigned i=0;i<4;i++)selector=(selector^key[i])*16777619u;
    const unsigned bucket=(selector^(selector>>16))&63;
    for(unsigned way=0;way<2;way++){
        digest_entry* e=&digests[bucket][way];
        if(e->selector==selector && !__builtin_memcmp(e->key,key,sizeof(key)) &&
           xb_texture_pages_match(e->memory,pages)){
            digest_victim[bucket]=way^1;
            sote_hd_digest_cache[0]++;
            return e;
        }
    }
    digest_entry* e=&digests[bucket][digest_victim[bucket]];
    digest_victim[bucket]^=1;
    __builtin_memcpy_inline(e->key,key,sizeof(key));e->selector=selector;
    const uint8_t* memory=xb_texture_memory();
    for(unsigned remaining=pages;remaining;remaining&=remaining-1){
        unsigned offset=256*__builtin_ctz(remaining);
        __builtin_memcpy_inline(e->memory+offset,memory+offset,256);
    }
    sote_hd_digest_cache[1]++;
    e->digest=xb_hd_hash(memory,tile,width,height,tlut<<14);
    e->match=NULL;
    e->match=find_digest(e->digest);
    return e;
}
static int resident_texture(const record* r,unsigned scene,unsigned levels,uint32_t** pixels,
                            unsigned* format,int warm) {
    /* Short rectangular BC chains end when the short side reaches four.
       The sampler repeats that final mip at greater draw distances. */
    if(levels>hd_mips(r->levels)){levels=hd_mips(r->levels);sote_hd_status[11]++;}
    /* NV2A FORMAT and CONTROL0 clamp to exactly these levels. The file
       layout is consecutive top-down mips; don't upload unreachable suffixes. */
    unsigned need=(unsigned)hd_payload_size(r->width,r->height,(r->levels&HD_CODEC)|levels);
    if(!need||need>r->bytes){sote_hd_prefix_stats[5]++;return 0;}
    sote_hd_prefix_stats[levels>1]++;if(need>sote_hd_prefix_stats[6])sote_hd_prefix_stats[6]=need;
    if(xb_budget_frame_probe)hot_touch(r->asset,r->bytes,need);
    sote_hd_status[3]++;sote_hd_status[9]=r->asset;
    unsigned chosen=CACHE_SLOTS,promote=CACHE_SLOTS;
    unsigned hint=resident_hint[r->asset&4095];
    if(cache[hint].valid && cache[hint].asset==r->asset && cache[hint].filled>=need){
        chosen=hint;sote_hd_status[4]++;trace_asset(scene,r->asset,need,1);goto found;
    }
    for(unsigned i=0;i<CACHE_SLOTS;i++) {
        if(cache[i].valid&&cache[i].asset==r->asset){
            if(cache[i].filled>=need){chosen=i;sote_hd_status[4]++;trace_asset(scene,r->asset,need,1);goto found;}
            promote=i;break;
        }
        if(!cache[i].valid&&chosen==CACHE_SLOTS)chosen=i;
    }
    unsigned span=texture_span(need);
    if(warm){
        unsigned offset;
        if(chosen==CACHE_SLOTS || promote!=CACHE_SLOTS || !fit_gap(span,&offset))return 0;
    }
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
        for(unsigned size=primary_bytes;size>=1024u*1024u && !pool;size/=2){
            uint32_t ticket=xb_budget_optional_reserve(size);
            if(ticket){
                pool=MmAllocateContiguousMemoryEx(size,0,0x3ffb000,0,PAGE_READWRITE|PAGE_WRITECOMBINE);
                if(pool && !xb_budget_optional_recheck_resident(ticket)){
                    MmFreeContiguousMemory(pool);pool=NULL;
                }
                xb_budget_optional_finish(ticket);
            }
            if(pool)primary_bytes=arena_bytes=size;
        }
        if(!pool){pool_failed=1;sote_hd_status[15]++;return 0;}
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
    if(!xb_asset_read_hd(r->asset,path,legacy,hd_address(offset)+start,r->bytes,start,need-start)){
        sote_hd_status[6]++;return 0;
    }
    hot_reads++;hot_read_bytes+=need-start;sote_hd_status[8]+=need-start;
    sote_hd_prefix_stats[4]+=r->bytes-(need-start);
    if(start)sote_hd_prefix_stats[2]++;
    cache[chosen].filled=need;
    __asm__ volatile("sfence":::"memory");
    cache[chosen].offset=offset;cache[chosen].span=span;sote_hd_reuse[3]+=span-oldspan;
    if(sote_hd_reuse[3]>sote_hd_admission[3])sote_hd_admission[3]=sote_hd_reuse[3];
    if(!cache[chosen].valid)insert_range(chosen);
    cache[chosen].asset=r->asset;cache[chosen].valid=1;sote_hd_status[5]++;
found:
    resident_hint[r->asset&4095]=chosen;
    cache[chosen].stamp=++hd_clock;slot_frame[chosen]=hot_frame;
    *pixels=cache[chosen].pixels;
    *format=1|8|0x20|(((r->levels&HD_DXT1)?0x0c:(r->levels&HD_DXT5)?0x0f:6)<<8)|(levels<<16)|(log2u(r->width)<<20)|(log2u(r->height)<<24);
    return 1;
}

/* Cold reads for these textures coincided in the first moving encounter.
   Keep pack order to reduce seeks, and reserve only the mips it samples.
   The normal lookup remains authoritative for custom or missing packs. */
static const struct { uint64_t digest; unsigned levels; } sewer_entry_textures[] = {
    {UINT64_C(0x31838adfd1014781), 1},
    {UINT64_C(0xe9558972f55c08fa), 6},
    {UINT64_C(0xf5b3767ca57b1a49), 6},
    {UINT64_C(0x3c75d158f030d28c), 1},
    {UINT64_C(0xc6dc53ebdb4dce6c), 1},
    {UINT64_C(0x22d0543feaa05021), 1},
    {UINT64_C(0xc7a856a844397628), 1},
    {UINT64_C(0x3469bbec04eefb2d), 1},
    {UINT64_C(0x50815d784a016070), 1},
    {UINT64_C(0x1a8dd8dd2eebcd39), 1},
    {UINT64_C(0x0213c05e773009ef), 1},
    {UINT64_C(0xc22e7fc0fced0d4f), 1},
    {UINT64_C(0x70ead73130f2c5e3), 1},
};

static void warm_digest(uint64_t digest, unsigned levels, unsigned scene) {
    const hash_record* match = find_digest(digest);
    if (!match) return;
    record texture;
    __builtin_memcpy_inline(&texture.asset, &match->asset, 5 * sizeof(uint32_t));
    uint32_t* pixels;
    unsigned format;
    resident_texture(&texture, scene, levels ? levels : hd_mips(texture.levels), &pixels, &format, 1);
}

static int lookup_texture(unsigned tile,unsigned tlut,unsigned width,unsigned height,unsigned levels,
                          uint32_t** pixels,unsigned* format,int* absent) {
    if(!initialized)init();if(!enabled||(!records&&!hashes))return 0;
    uint32_t key[17];int has_key=xb_texture_slot_key(tile,tlut,width,height,key);if(!has_key)memset(key,0,sizeof(key));
    unsigned scene=xb_texture_scene();
    static int warm_on_entry;
    if(active_scene!=scene){
        warm_on_entry=(scene==8 || scene==25);
        active_scene=scene; /* Immutable GPU assets can survive native scene changes. */
        memset(scene_seen_assets,0,sizeof(scene_seen_assets));
        memset(hot,0,sizeof(hot));
        for(unsigned i=0;i<4;i++)sote_hd_demand[i]=0;
        sote_hd_demand[4]++;
        sote_hd_admission[0]=scene;sote_hd_admission[1]++;
    }
    if(warm_on_entry && pool){
        warm_on_entry=0;
        if (scene == 8) {
            warm_digest(UINT64_C(0x69a4ebc90c78687c), 0, scene);
        } else {
            for (unsigned i = 0; i < sizeof(sewer_entry_textures) / sizeof(sewer_entry_textures[0]); i++) {
                warm_digest(sewer_entry_textures[i].digest, sewer_entry_textures[i].levels, scene);
            }
        }
    }
    for(unsigned i=0;i<17;i++)sote_hd_last_key[i]=key[i];
    sote_hd_status[2]++;sote_hd_status[10]=key[0];
    record* r=0;
    if(has_key)for(unsigned id=source_head[source_bucket(key)];id;id=source_next[id-1]){
        if(!__builtin_memcmp(records[id-1].key,key,sizeof(key))){r=records+id-1;break;}
    }
    record hash_result;
    if(!r&&hashes){
        sote_hd_status[13]++;
        const digest_entry* e=content_match(&xb_tiles[tile],tlut,width,height);
        sote_hd_last_hash[0]=(uint32_t)e->digest;sote_hd_last_hash[1]=(uint32_t)(e->digest>>32);
        if(e->match){
            __builtin_memcpy_inline(&hash_result.asset,&e->match->asset,5*sizeof(uint32_t));
            r=&hash_result;sote_hd_status[14]++;
        }
    }
    if(!r){
        if(absent)*absent=1;
        return 0;
    }
    if(!levels)return 0;
    return resident_texture(r,scene,levels,pixels,format,0);
}


int xb_hd_texture(unsigned tile,unsigned tlut,unsigned width,unsigned height,unsigned levels,
                  uint32_t** pixels,unsigned* format) {
    return lookup_texture(tile,tlut,width,height,levels,pixels,format,NULL);
}

/* Texture loads, palette loads and tile descriptor changes advance the
   generation. Source slot keys also depend on the current scene. Only
   a definitive absence from the installed indexes is reusable. */
int xb_hd_rectangle(unsigned tile,unsigned tlut,unsigned width,unsigned height,int consecutive,
                    uint32_t** pixels,unsigned* format) {
    static unsigned last_tile,last_tlut,last_width,last_height,last_generation,last_scene;
    static int absent;
    unsigned scene=xb_texture_scene();
    if(consecutive && absent && last_generation==xb_texture_generation && last_scene==scene &&
       last_tile==tile && last_tlut==tlut && last_width==width && last_height==height)return 0;
    absent=0;
    int found=lookup_texture(tile,tlut,width,height,1,pixels,format,&absent);
    last_generation=xb_texture_generation;last_scene=scene;last_tile=tile;last_tlut=tlut;
    last_width=width;last_height=height;
    return found;
}
