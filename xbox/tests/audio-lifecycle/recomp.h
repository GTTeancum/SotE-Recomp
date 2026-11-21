#pragma once
#include <stdint.h>
typedef struct {int64_t r2,r4,r5;} recomp_context;
#define MEM_HU(off,base) (*(uint16_t*)(rdram+(((uint32_t)(base)+(off))&0x7fffff)))
