#ifndef XB_MENU_SETTINGS_H
#define XB_MENU_SETTINGS_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
int xb_pc_control_count(void);
const char* xb_pc_control_label(int row);
const char* xb_pc_control_value(int row);
const char* xb_pc_control_help(int row);
void xb_pc_control_adjust(int row,int direction);
int xb_cheat_count(void);
const char* xb_cheat_label(int row);
const char* xb_cheat_value(int row);
const char* xb_cheat_help(int row);
void xb_cheat_adjust(int row,int direction);
int xb_controls_save(void);
/* Saved next-boot preference, independent of pack presence and this boot's active GPU profile. */
int xb_hd_textures_preferred(void);
void xb_hd_textures_toggle(void);
/* Preference survives missing assets/decoder; availability is a separate gate. */
int xb_pc_movies_preferred(void);
void xb_pc_movies_toggle(void);
int xb_pc_movies_available(void);
/* 0xffff means original mapping; 0 means disabled. Capture physical canonical
   Xbox buttons (A/B/X/Y/black/white/D-pad/RT and host-only LT0x40), NOT native action masks. */
unsigned xb_binding_get(int context,int action);
int xb_binding_set(int context,int action,unsigned mask);
void xb_binding_reset_context(int context);
unsigned xb_binding_buttons(uint8_t* rdram,unsigned physical,unsigned translated);
unsigned xb_controller_physical(unsigned buttons,float lt,float rt);
/* Effective physical button mask for display, including Modern aliases. */
unsigned xb_binding_effective(int context,int action,unsigned native_mask);
/* Normalized raw inputs; only invoke for live gameplay, never menu navigation.
   Publish RAW mx/my/rx/ry to Modern first; filtered mx/my are guest stick axes. */
void xb_controller_filter(uint8_t* rdram,uint16_t* buttons,float* mx,float* my,
                          float rx,float ry,float lt,float rt);
void xb_cheats_tick(uint8_t* rdram);
unsigned xb_rapid_fire(unsigned buttons);
#ifdef __cplusplus
}
#endif
#endif
