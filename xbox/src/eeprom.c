#include <windows.h>
#include <xboxkrnl/xboxkrnl.h>
#include <nxdk/mount.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "runtime.h"

/* Two independently checksummed copies. Never truncate the active copy;
   advance the in-memory image only after the other copy is flushed/verified. */
typedef struct {uint32_t magic,version,generation;uint8_t bytes[512];uint32_t crc;} record;
typedef struct {char path[2][96];record image;int active,ready;} journal;
static journal game;
static int initialized;
volatile struct {uint32_t probes,reads,writes,errors,generation,recovered,checks;} sote_save_status;
void xb_unsupported(uint32_t id) __attribute__((noreturn));
static uint32_t checksum(const record* r) {
    uint32_t crc=~0u;const uint8_t* p=(const uint8_t*)r;
    for(unsigned i=0;i<offsetof(record,crc);i++) {
        crc^=p[i];for(unsigned bit=0;bit<8;bit++)crc=(crc>>1)^((0u-(crc&1))&0xedb88320);
    }
    return ~crc;
}
static int read_copy(const char* path,record* r) {
    HANDLE h=CreateFileA(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
    if(h==INVALID_HANDLE_VALUE)return GetLastError()==ERROR_FILE_NOT_FOUND?0:-1;
    DWORD count=0;int ok=GetFileSize(h,NULL)==sizeof(*r) && ReadFile(h,r,sizeof(*r),&count,NULL) && count==sizeof(*r);
    CloseHandle(h);
    return ok && r->magic==0x534f5445 && r->version==1 && r->crc==checksum(r)?1:-1;
}
static int open_journal(journal* j,const char* stem) {
    memset(j,0,sizeof(*j));j->active=-1;
    for(unsigned i=0;i<2;i++)snprintf(j->path[i],sizeof(j->path[i]),"E:\\UDATA\\534f5445\\%s%u.bin",stem,i);
    record copies[2];int state[2]={read_copy(j->path[0],&copies[0]),read_copy(j->path[1],&copies[1])};
    if(state[0]==1)j->active=0;
    if(state[1]==1 && (j->active<0 || (int32_t)(copies[1].generation-copies[0].generation)>0))j->active=1;
    if(j->active>=0) {
        j->image=copies[j->active];
        if(state[1-j->active]<0)sote_save_status.recovered++;
    } else {
        /* Existing unreadable/corrupt data is not permission to erase it. */
        if(state[0]!=0 || state[1]!=0)return 0;
        j->image.magic=0x534f5445;j->image.version=1;memset(j->image.bytes,0xff,512);
    }
    j->ready=1;return 1;
}
static int commit(journal* j,const uint8_t bytes[512]) {
    if(!j->ready)return 0;
    record next=j->image;next.generation++;memcpy(next.bytes,bytes,512);next.crc=checksum(&next);
    unsigned slot=j->active==0?1:0;
    HANDLE h=CreateFileA(j->path[slot],GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
    if(h==INVALID_HANDLE_VALUE)return 0;
    DWORD count=0;IO_STATUS_BLOCK io;
    int ok=WriteFile(h,&next,sizeof(next),&count,NULL) && count==sizeof(next);
    if(ok)ok=NT_SUCCESS(NtFlushBuffersFile(h,&io));
    if(!CloseHandle(h))ok=0;
    record check;
    if(!ok || read_copy(j->path[slot],&check)!=1 || memcmp(&next,&check,sizeof(next)))return 0;
    j->image=next;j->active=slot;return 1;
}
static int directory(const char* path) {
    if(CreateDirectoryA(path,NULL))return 1;
    DWORD attributes=GetFileAttributesA(path);
    return attributes!=INVALID_FILE_ATTRIBUTES && (attributes&FILE_ATTRIBUTE_DIRECTORY);
}
static int mount_save(void) {
    if(!nxIsDriveMounted('E') && !nxMountDrive('E',"\\Device\\Harddisk0\\Partition1\\"))return 0;
    return directory("E:\\UDATA") && directory("E:\\UDATA\\534f5445");
}
static int initialize(void) {
    if(!initialized){initialized=1;if(!mount_save() || !open_journal(&game,"eeprom"))sote_save_status.errors++;}
    return game.ready;
}
static int bounds(recomp_context* ctx,unsigned* offset,unsigned* count,unsigned* address) {
    *offset=(uint8_t)ctx->r5*8;*count=(uint32_t)ctx->r7;*address=(uint32_t)ctx->r6&0x1fffffff;
    return *offset<=512 && *count<=512-*offset && !(*count&7) && *address<=0x800000-*count;
}
void osEepromProbe_recomp(uint8_t* rdram,recomp_context* ctx) {
    (void)rdram;sote_save_status.probes++;ctx->r2=initialize()?1:0;
}
void osEepromLongRead_recomp(uint8_t* rdram,recomp_context* ctx) {
    unsigned offset,count,address;ctx->r2=-1;
    if(!initialize() || !bounds(ctx,&offset,&count,&address)){sote_save_status.errors++;return;}
    for(unsigned i=0;i<count;i++)rdram[(address+i)^3]=game.image.bytes[offset+i];
    ctx->r2=0;sote_save_status.reads++;sote_save_status.generation=game.image.generation;
}
void osEepromLongWrite_recomp(uint8_t* rdram,recomp_context* ctx) {
    unsigned offset,count,address;ctx->r2=-1;
    if(!initialize() || !bounds(ctx,&offset,&count,&address)){sote_save_status.errors++;return;}
    uint8_t next[512];memcpy(next,game.image.bytes,512);
    for(unsigned i=0;i<count;i++)next[offset+i]=rdram[(address+i)^3];
    if(count && !commit(&game,next)){sote_save_status.errors++;return;}
    ctx->r2=0;sote_save_status.writes++;sote_save_status.generation=game.image.generation;
}
static void verify(int ok) {
    if(!ok)xb_unsupported(0xE6001000+sote_save_status.checks);
    sote_save_status.checks++;
}
void xb_save_selftest(void) {
    /* Isolated fixture files; never touch campaign EEPROM copies. */
    verify(mount_save());journal test;
    for(unsigned i=0;i<2;i++){char p[96];snprintf(p,sizeof(p),"E:\\UDATA\\534f5445\\fixture%u.bin",i);DeleteFileA(p);}
    verify(open_journal(&test,"fixture"));
    for(unsigned i=0;i<512;i++)verify(test.image.bytes[i]==255);
    uint8_t first[512],second[512];for(unsigned i=0;i<512;i++){first[i]=(i*37)^0xa5;second[i]=first[i]^255;}
    verify(commit(&test,first));verify(commit(&test,second));
    verify(open_journal(&test,"fixture"));verify(!memcmp(test.image.bytes,second,512) && test.image.generation==2);
    /* Interrupted/truncated newest write must recover the preceding copy. */
    HANDLE h=CreateFileA(test.path[test.active],GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
    verify(h!=INVALID_HANDLE_VALUE);IO_STATUS_BLOCK io;verify(NT_SUCCESS(NtFlushBuffersFile(h,&io)));CloseHandle(h);
    verify(open_journal(&test,"fixture"));verify(!memcmp(test.image.bytes,first,512) && test.image.generation==1);
    h=CreateFileA(test.path[test.active],GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
    verify(h!=INVALID_HANDLE_VALUE);verify(NT_SUCCESS(NtFlushBuffersFile(h,&io)));CloseHandle(h);
    verify(!open_journal(&test,"fixture"));
    for(unsigned i=0;i<2;i++)verify(DeleteFileA(test.path[i]));
}
