#include <windows.h>
#include <xboxkrnl/xboxkrnl.h>
#include "asset_file.h"
#include "performance.h"
#include "console_budget.h"
#include <stdlib.h>

/* The title stores verified, pre-swizzled pixels on disc. Keep native file
   I/O in ordinary CPU memory; never assume kernel reads can DMA directly
   into NV2A write-combined pages. One serialized 8 KiB staging window, not
   a second texture. The lock is initialized before any guest workers run. */
static uint8_t bounce[XB_ASSET_BOUNCE_BYTES] __attribute__((aligned(64)));
static CRITICAL_SECTION lock;
static int ready;
volatile uint32_t sote_asset_io[8];
void xb_asset_io_init(void) {
    if(!ready){InitializeCriticalSection(&lock);ready=1;}
}
static HANDLE open_asset(const char* path) {
    return CreateFileA(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
}
static int last_open_missing(void) {
    unsigned long e=GetLastError();
    return e==ERROR_FILE_NOT_FOUND || e==ERROR_PATH_NOT_FOUND;
}
static void copy_wc(uint8_t* dst,const uint8_t* src,unsigned n) {
    /* NXDK's generic memcpy is byte-oriented. x86 MOVSD uses 32-bit
       source/destination units and works for unaligned addresses too.
       The tail is ordinarily empty (BGRA mip bytes are divisible by 4). */
    size_t words=n/4;
    __asm__ volatile("cld; rep movsl" : "+S"(src),"+D"(dst),"+c"(words)::"cc","memory");
    for(unsigned i=0;i<(n&3u);++i)dst[i]=src[i];
}
/* The bundle handle is opened once during HD initialization. Each transfer
   still uses an explicit offset and the same serialized WC staging path. */
static HANDLE bundle = INVALID_HANDLE_VALUE;
static uint32_t* offsets;
static unsigned entries;

static uint32_t fingerprint(const uint8_t* p, unsigned n) {
    uint32_t hash = 2166136261u;
    while (n--) hash = (hash ^ *p++) * 16777619u;
    return hash;
}

void xb_asset_open_bundle(const void* slots, unsigned slot_bytes,
                          const void* hashes, unsigned hash_bytes) {
    if (bundle != INVALID_HANDLE_VALUE) return;
    HANDLE file = open_asset("D:\\hdtextures\\textures.xhb");
    if (file == INVALID_HANDLE_VALUE) return;
    uint32_t header[4];
    DWORD actual;
    LARGE_INTEGER size;
    uint32_t* table = NULL;
    if (!GetFileSizeEx(file, &size) || size.QuadPart > UINT32_MAX ||
        !ReadFile(file, header, sizeof(header), &actual, NULL) || actual != sizeof(header) ||
        header[0] != 0x31424858u || !header[1] || header[1] > 65536u ||
        header[2] != fingerprint(slots, slot_bytes) ||
        header[3] != fingerprint(hashes, hash_bytes)) goto fail;
    unsigned bytes = (header[1] + 1) * sizeof(*table);
    unsigned ticket = xb_budget_optional_reserve(xb_budget_optional_heap_cost(bytes));
    if (!ticket) goto fail;
    table = malloc(bytes);
    xb_budget_optional_finish(ticket);
    if (!table || !ReadFile(file, table, bytes, &actual, NULL) || actual != bytes ||
        table[0] != sizeof(header) + bytes || table[header[1]] != size.QuadPart) goto fail;
    for (unsigned i = 0; i < header[1]; ++i)
        if (table[i+1] < table[i] || table[i+1] - table[i] > XB_ASSET_MAX_BYTES) goto fail;
    bundle = file;
    offsets = table;
    entries = header[1];
    return;
fail:
    free(table);
    CloseHandle(file);
}
static int read_asset(unsigned asset,const char* preferred,const char* legacy,void* destination,uint32_t file_bytes,uint32_t begin,uint32_t bytes) {
    if(!ready)return 0;
    EnterCriticalSection(&lock);
    ++sote_asset_io[0];int ok=0;unsigned pos=0;
    HANDLE h=INVALID_HANDLE_VALUE;
    if(!preferred || !*preferred || !destination || !bytes || file_bytes>XB_ASSET_MAX_BYTES || begin>file_bytes || bytes>file_bytes-begin)goto bad;
    int packed=bundle!=INVALID_HANDLE_VALUE && asset<entries;
    if(packed) {
        if(offsets[asset+1]-offsets[asset]!=file_bytes)goto bad;
        h=bundle;
        begin+=offsets[asset];
    }else{
        h=open_asset(preferred);
        if(h==INVALID_HANDLE_VALUE && !last_open_missing())goto bad;
        if(h==INVALID_HANDLE_VALUE && legacy && *legacy){
            h=open_asset(legacy);
            if(h!=INVALID_HANDLE_VALUE)++sote_asset_io[7];
            else if(!last_open_missing())goto bad;
        }
        if(h==INVALID_HANDLE_VALUE){++sote_asset_io[5];goto done;}
        ++sote_asset_io[1];
        LARGE_INTEGER size;
        if(!GetFileSizeEx(h,&size) || size.QuadPart!=file_bytes)goto bad;
    }
    while(pos<bytes) {
        unsigned n=bytes-pos;if(n>sizeof(bounce))n=sizeof(bounce);
        LARGE_INTEGER offset;offset.QuadPart=begin+pos;
        IO_STATUS_BLOCK io={0};++sote_asset_io[2];
        NTSTATUS st=NtReadFile(h,NULL,NULL,NULL,&io,bounce,n,&offset);
        if(st==STATUS_PENDING){
            st=NtWaitForSingleObject(h,FALSE,NULL);
            if(NT_SUCCESS(st))st=io.Status;
        }
        if(!NT_SUCCESS(st) || io.Information!=n)goto bad;
        copy_wc((uint8_t*)destination+pos,bounce,n);
        pos+=n;sote_asset_io[3]+=n;
    }
    ok=1;goto done;
bad:
    ++sote_asset_io[6];
done:
    /* Order all WC stores even on a later short read; caller may immediately
       reuse the failed, unpublished slot. Successful caller still owns GPU
       retirement and binding. A corrupt preferred asset never tries legacy. */
    if(pos)__asm__ volatile("sfence":::"memory");
    if(h!=INVALID_HANDLE_VALUE && h!=bundle && !CloseHandle(h)){++sote_asset_io[6];ok=0;}
    if(ok)++sote_asset_io[4];
    LeaveCriticalSection(&lock);return ok;
}
int xb_asset_read_range_exact(const char* p,const char* legacy,void* dst,uint32_t size,uint32_t begin,uint32_t bytes) {
    return read_asset(UINT32_MAX,p,legacy,dst,size,begin,bytes);
}
int xb_asset_read_hd(unsigned asset,const char* p,const char* legacy,void* dst,uint32_t size,uint32_t begin,uint32_t bytes) {
    uint64_t start = xb_perf_begin();
    int ok = read_asset(asset,p,legacy,dst,size,begin,bytes);
    xb_perf_end(XB_PERF_HD_READ,start);
    return ok;
}
int xb_asset_read_paths_exact(const char* p,const char* legacy,void* dst,uint32_t bytes) {
    return xb_asset_read_range_exact(p,legacy,dst,bytes,0,bytes);
}
int xb_asset_read_exact(const char* path,void* dst,uint32_t bytes) {
    return xb_asset_read_paths_exact(path,NULL,dst,bytes);
}
