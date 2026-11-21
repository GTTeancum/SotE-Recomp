#pragma once
#include <cstdio>
#include <cstdint>
struct CRITICAL_SECTION{};
inline void InitializeCriticalSection(CRITICAL_SECTION*){}
inline void EnterCriticalSection(CRITICAL_SECTION*){}
inline void LeaveCriticalSection(CRITICAL_SECTION*){}
using HANDLE=FILE*;using DWORD=unsigned;struct IO_STATUS_BLOCK{};
#define INVALID_HANDLE_VALUE nullptr
#define GENERIC_WRITE 1
#define CREATE_ALWAYS 1
#define FILE_ATTRIBUTE_NORMAL 0
#define NT_SUCCESS(x) ((x)==0)
extern unsigned fake_ticks;extern bool fail_write;
inline unsigned GetTickCount(){return fake_ticks;}
inline int CreateDirectoryA(const char*,void*){return 1;}
inline HANDLE CreateFileA(const char* p,int,int,void*,int,int,void*){return std::fopen(p,"wb");}
inline int WriteFile(HANDLE f,const void* p,unsigned n,DWORD* out,void*){*out=fail_write?0:std::fwrite(p,1,n,f);return *out==n;}
inline int NtFlushBuffersFile(HANDLE f,IO_STATUS_BLOCK*){return std::fflush(f);}
inline int CloseHandle(HANDLE f){return std::fclose(f)==0;}
