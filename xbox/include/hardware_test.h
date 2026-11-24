#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
extern int xb_hardware_test;
void xb_hardware_scanout(uint32_t front);
extern unsigned xb_audio_replay_poll;
void xb_hardware_capture_audio_task(const uint8_t* rdram, const uint8_t* dmem);
extern volatile uint32_t xb_gpu_fault[3];
void xb_hardware_test_init(void);
void xb_hardware_check_depth(void);
uint32_t xb_hardware_rgb_hash(void);
int xb_hardware_capture(unsigned event);
extern unsigned xb_trace_preview_tiles;
void xb_hardware_tile(unsigned tile,unsigned tlut,unsigned w,unsigned h,float x,float y,int asset);
void xb_hardware_note(const char* stage, unsigned value);
void xb_hardware_player_shot(uint32_t shooter);
void xb_hardware_frame(uint8_t* rdram, unsigned frame);
#ifdef __cplusplus
}
#endif
