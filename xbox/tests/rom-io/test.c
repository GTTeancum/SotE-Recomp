#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include "rom_io.h"
#include <xboxkrnl/xboxkrnl.h>

static uint8_t ram[XB_RDRAM_BYTES];
static unsigned opens, closes, reads, waits, generation, live, failed, short_read, pending, fail_at;
static int no_file, bad_size;
static unsigned actual_reads, actual_bytes;
static uint8_t data(unsigned at) {
    return (uint8_t)((at * 29u) ^ (at >> 7) ^ (at >> 13) ^ (at >> 17) ^ generation);
}
HANDLE CreateFileA(const char* p,unsigned access,unsigned sharing,void* sa,
                   unsigned creation,unsigned attributes,void* template) {
    assert(!strcmp(p,"D:\\sote.us.v1.2.z64") && access==GENERIC_READ && sharing==FILE_SHARE_READ);
    assert(!sa && !template && creation==OPEN_EXISTING && attributes==FILE_ATTRIBUTE_NORMAL);
    assert(!live); opens++;
    if(no_file)return INVALID_HANDLE_VALUE;
    live=1;return 1;
}
int CloseHandle(HANDLE h){assert(live && h==1);closes++;live=0;return 1;}
int GetFileSizeEx(HANDLE h,LARGE_INTEGER* sz){assert(live && h==1);sz->QuadPart=XB_ROM_BYTES+bad_size;return 1;}
void InitializeCriticalSection(CRITICAL_SECTION* g){assert(!pthread_mutex_init(g,NULL));}
void EnterCriticalSection(CRITICAL_SECTION* g){assert(!pthread_mutex_lock(g));}
void LeaveCriticalSection(CRITICAL_SECTION* g){assert(!pthread_mutex_unlock(g));}
NTSTATUS NtReadFile(HANDLE h, void* event,void* apc,void* ctx,IO_STATUS_BLOCK* io,
                    void* out,unsigned n,LARGE_INTEGER* pos) {
    assert(live && h==1 && !event && !apc && !ctx && pos->QuadPart>=0);
    assert((uint64_t)pos->QuadPart+n<=XB_ROM_BYTES);
    reads++; actual_reads++; actual_bytes+=n;
    io->Status=(failed || (fail_at && reads==fail_at))?-1:0;
    io->Information=io->Status<0?0:n-(short_read?1u:0u);
    for(unsigned i=0;i<io->Information;i++)((uint8_t*)out)[i]=data((uint32_t)pos->QuadPart+i);
    return pending?STATUS_PENDING:io->Status;
}
NTSTATUS NtWaitForSingleObject(HANDLE h,int alertable,void* timeout){
    assert(live && h==1 && !alertable && !timeout);waits++;return 0;
}
static void copy(unsigned src,unsigned dst,unsigned n){
    assert(!xb_rom_guest(ram,src,dst,n));
    for(unsigned i=0;i<n;i++)assert(ram[(dst+i)^3u]==data(src+i));
}
static void* worker(void* arg){
    unsigned id=*(unsigned*)arg;
    for(unsigned i=0;i<1500;i++)copy(((i*431u+id*253011u)%(XB_ROM_BYTES-300u)),
                                   id*1024u+7u,(i%257u)+1u);
    return NULL;
}
int main(void){
    assert(xb_rom_guest(ram,0,0,4)==-1);xb_rom_close();
    no_file=1;assert(xb_rom_open()==XB_ROM_NO_FILE && !live);
    no_file=0;bad_size=1;assert(xb_rom_open()==XB_ROM_BAD_SIZE && closes==1 && !live);
    bad_size=0;assert(!xb_rom_open() && !xb_rom_open() && opens==3 && live==1);
    /* Bounded deterministic locality trace, not a physical-drive benchmark. */
    actual_reads=actual_bytes=0;
    for(unsigned i=0;i<10000;i++)copy(4*i,(i&63)*4,4);
    assert(actual_reads==20 && actual_bytes==40960);
    assert(sote_rom_io_status[XB_ROM_ALL_HIT]==9980 && sote_rom_io_status[XB_ROM_OPENS]==1);
    printf("10,000 sequential word DMAs: %u backend reads, %u backend bytes, 1 live image\n",actual_reads,actual_bytes);
    actual_reads=actual_bytes=0;copy(256*1024,0,1024*1024);
    assert(actual_reads==128 && actual_bytes==1024*1024);
    printf("1 MiB aligned DMA: %u x 8 KiB reads, no excess bytes\n",actual_reads);
    for(unsigned off=0;off<8;off++)for(unsigned dst=1;dst<9;dst++)
    for(unsigned n=0;n<20;n++){
        memset(ram,0xa5,36);copy(2043+off,dst,n);
        for(unsigned i=0;i<32;i++)if(i<dst || i>=dst+n)assert(ram[i^3u]==0xa5);
    }
    copy(XB_ROM_BYTES-321,XB_RDRAM_BYTES-321,321);copy(XB_ROM_BYTES,XB_RDRAM_BYTES,0);
    unsigned pre=reads;
    assert(xb_rom_guest(ram,XB_ROM_BYTES,0,1)==-1 && xb_rom_guest(ram,1,XB_RDRAM_BYTES,1)==-1);
    assert(xb_rom_guest(ram,0,UINT32_MAX,256)==-1 && xb_rom_guest(ram,1,0,UINT32_MAX)==-1);
    assert(xb_rom_raw(UINT32_MAX,ram,1)==-1 && pre==reads);
    pending=1;failed=1;assert(xb_rom_guest(ram,0,0,4)==-1);
    failed=0;short_read=1;assert(xb_rom_guest(ram,0,0,4)==-1);
    short_read=0;copy(0,0,4);assert(waits==3 && reads==pre+3);
    uint8_t raw[19];assert(!xb_rom_raw(XB_ROM_BYTES-19,raw,19));
    for(unsigned i=0;i<19;i++)assert(raw[i]==data(XB_ROM_BYTES-19+i));
    failed=1;assert(xb_rom_raw(0,raw,sizeof raw)==-1);failed=pending=0;
    unsigned mid=3*1024*1024,src=5*1024*1024;
    memset(ram+mid,0xa5,16384);fail_at=reads+2;
    assert(xb_rom_guest(ram,src,mid,8192+333)==-1);
    for(unsigned i=0;i<8192;i++)assert(ram[(mid+i)^3u]==data(src+i));
    for(unsigned i=8192;i<8192+333;i++)assert(ram[(mid+i)^3u]==0xa5);
    fail_at=0;copy(src,mid,8192+333);
    /* Repeated quiescent release is safe, and the cache cannot outlive a
       different handle on reopen. Concurrent readers serialize the window. */
    xb_rom_close();xb_rom_close();assert(closes==2 && !live);
    assert(xb_rom_guest(ram,0,0,4)==-1 && xb_rom_raw(0,raw,4)==-1);
    generation=99;assert(!xb_rom_open());copy(0,0,4);
    pthread_t t[3];unsigned id[3]={0,1,2};
    for(unsigned i=0;i<3;i++)assert(!pthread_create(&t[i],NULL,worker,&id[i]));
    for(unsigned i=0;i<3;i++)assert(!pthread_join(t[i],NULL));
    xb_rom_close();assert(closes==3 && !live);
    puts("Odd/split/EOF/canary ranges, mid-transfer error/retry, close/reopen, 4500 interleaved reads PASS");
}
