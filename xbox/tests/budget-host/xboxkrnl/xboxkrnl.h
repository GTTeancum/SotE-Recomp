#pragma once
#include <stdint.h>
typedef struct _MM_STATISTICS {
    uint32_t Length, TotalPhysicalPages, AvailablePages, VirtualMemoryBytesCommitted,
             VirtualMemoryBytesReserved, CachePagesCommitted, PoolPagesCommitted,
             StackPagesCommitted, ImagePagesCommitted;
} MM_STATISTICS;
int MmQueryStatistics(MM_STATISTICS*);
void* MmAllocateContiguousMemoryEx(unsigned,unsigned,unsigned,unsigned,unsigned);
#define PAGE_READWRITE 4u
#define PAGE_WRITECOMBINE 0x400u
