#include <stdint.h>
#define MEM_H(o,a) (*(int16_t*)(rdram+((((uint32_t)(a)+(o))&0x7fffff)^2)))
#define MEM_W(o,a) (*(uint32_t*)(rdram+(((uint32_t)(a)+(o))&0x7fffff)))
#define MEM_B(o,a) (*(int8_t*)(rdram+((((uint32_t)(a)+(o))&0x7fffff)^3)))
#define MEM_BU(o,a) (*(uint8_t*)(rdram+((((uint32_t)(a)+(o))&0x7fffff)^3)))
