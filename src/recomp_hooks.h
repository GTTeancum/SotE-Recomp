#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

uint32_t sote_enter_bike_controller(uint8_t* rdram, uint32_t object);
void sote_enter_player_controller(
    uint8_t* rdram,
    uint32_t object,
    uint32_t controller,
    uint32_t action);
void sote_wait_for_game_frame(void);
double sote_sanitize_frame_delta(double delta);
void sote_sanitize_global_frame_delta(uint8_t* rdram);
uint32_t sote_allow_life_loss(
    uint8_t* rdram,
    uint32_t source,
    uint32_t object,
    uint32_t action);
void sote_note_bike_life_loss(uint8_t* rdram, uint32_t source);
void sote_note_audio_negative_exponent(int32_t exponent);
uint32_t sote_normalize_zero_velocity_motion(
    uint8_t* rdram,
    uint32_t object);
void sote_note_motion_loop_guard(uint8_t* rdram, uint32_t object);
void sote_update_graphics_menu(uint8_t* rdram);
uint32_t sote_is_bike_stage_active(void);

#ifdef __cplusplus
}
#endif
