#pragma once
#include <stdint.h>
/* Global Xbox setting: first boot defaults on when an installed pack is valid.
   Existing HD marker is compatible, but a saved OFF always wins. Physical
   GPU profiles are fixed at boot; UI changes apply after saving/restarting. */
int xb_hd_texture(unsigned tile,unsigned tlut,unsigned width,unsigned height,unsigned levels,
                  uint32_t** pixels,unsigned* format);
int xb_hd_pack_available(void);
int xb_hd_session_active(void);
unsigned xb_hd_boot_texture_slots(void);
