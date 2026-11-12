#pragma once
#include <stdint.h>
/* NXDK builds freestanding: even fixed-size memcpy may remain a library call.
   x86 permits unaligned word loads; packed/may_alias expresses that explicitly. */
typedef struct __attribute__((packed,may_alias)) {uint32_t value;} xb_texture_word;
static inline uint32_t xb_texture_read_word(const void* p){return ((const xb_texture_word*)p)->value;}
typedef struct {
    unsigned format,size,line,tmem,palette,mask_s,mask_t,shift_s,shift_t,mode_s,mode_t;
    unsigned left,top,right,bottom;
} xb_tile;
extern xb_tile xb_tiles[8];
extern uint32_t xb_texture_generation;
int xb_texture_command(uint8_t* rdram,uint32_t w0,uint32_t w1);
uint32_t xb_texture_pixel(unsigned tile,int s,int t,unsigned tlut);
const uint8_t* xb_texture_memory(void);
uint32_t xb_texture_fingerprint(void);
int xb_texture_memory_matches(const uint8_t* memory);
void xb_texture_memory_selftest(void);
