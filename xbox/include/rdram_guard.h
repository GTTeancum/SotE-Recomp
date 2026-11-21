#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
enum { XB_RDRAM_RESERVED = 0x800000, XB_RDRAM_LIVE = 0x400000 };
enum { XB_RDRAM_CPU=1, XB_RDRAM_PI, XB_RDRAM_SP, XB_RDRAM_EEPROM, XB_RDRAM_BOSS, XB_RDRAM_MENU, XB_RDRAM_AI, XB_RDRAM_VIDEO, XB_RDRAM_SI };
extern unsigned xb_rdram_committed_limit; /* published before guest threads start */
void xb_rdram_bounds_fault(uint32_t kind, uint64_t offset, uint32_t bytes) __attribute__((noreturn));
#ifdef __cplusplus
}
#endif
static inline int xb_rdram_range_valid(uint64_t offset, uint32_t bytes, uint32_t limit) {
    return offset <= limit && bytes <= limit-offset;
}
static inline uint32_t xb_rdram_offset(uint64_t offset, uint32_t length, uint32_t kind) {
    /* Near-never-taken or folded out for addresses conclusively in lower RAM. */
    if (__builtin_expect(!xb_rdram_range_valid(offset,length,XB_RDRAM_LIVE),0) &&
        !xb_rdram_range_valid(offset,length,xb_rdram_committed_limit))
        xb_rdram_bounds_fault(kind,offset,length);
    return (uint32_t)offset;
}
static inline uint32_t xb_rdram_guest_offset(uint64_t address,uint32_t size,uint32_t xor_mask) {
    return xb_rdram_offset((address^xor_mask)-UINT64_C(0xffffffff80000000),size,XB_RDRAM_CPU);
}
