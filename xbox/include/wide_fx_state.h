#pragma once
#include <stdint.h>
/* The identical full-viewport white procedural mask is observed with and without
 * native alpha compare bit 0. All geometry, exact shader pairs, source checks
 * and central authored UV conditions remain independently mandatory. */
static inline int xb_wide_fx_render_state(uint32_t hi,uint32_t lo){
    return hi==0xc80u && (lo==0x00504340u || lo==0x00504341u);
}
