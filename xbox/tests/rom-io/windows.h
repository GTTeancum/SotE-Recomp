#pragma once
#include <stddef.h>
#include <stdint.h>
#include <pthread.h>
typedef intptr_t HANDLE;
typedef struct { int64_t QuadPart; } LARGE_INTEGER;
typedef pthread_mutex_t CRITICAL_SECTION;
#define INVALID_HANDLE_VALUE ((HANDLE)-1)
#define GENERIC_READ 1
#define FILE_SHARE_READ 2
#define OPEN_EXISTING 3
#define FILE_ATTRIBUTE_NORMAL 4
#define FALSE 0
HANDLE CreateFileA(const char*, unsigned, unsigned, void*, unsigned, unsigned, void*);
int CloseHandle(HANDLE);
int GetFileSizeEx(HANDLE, LARGE_INTEGER*);
void InitializeCriticalSection(CRITICAL_SECTION*);
void EnterCriticalSection(CRITICAL_SECTION*);
void LeaveCriticalSection(CRITICAL_SECTION*);
