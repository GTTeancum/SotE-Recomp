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
uint32_t sote_play_hd_sound_request(uint8_t* rdram, int32_t sound_id);
void sote_note_message_lookup(
    uint8_t* rdram,
    uint32_t message_key,
    uint32_t text_pointer,
    uint32_t caller);
void sote_note_droid_overlay_message_activate(
    uint8_t* rdram,
    uint32_t message_table,
    uint32_t message_key,
    uint32_t text_pointer);
void sote_note_droid_prompt_display(
    uint8_t* rdram,
    uint32_t source,
    uint32_t message_object);
void sote_trace_droid_visual_candidate(
    uint8_t* rdram,
    uint32_t source,
    uint32_t message_key,
    uint32_t message_object,
    uint32_t aux);
void sote_trace_droid_render_candidate(
    uint8_t* rdram,
    uint32_t source,
    uint32_t object,
    uint32_t aux0,
    uint32_t aux1,
    uint32_t aux2);
void sote_trace_text_draw_candidate(
    uint8_t* rdram,
    uint32_t display_list_pointer,
    uint32_t text_pointer,
    uint32_t caller);
void sote_trace_text_stage_candidate(
    uint8_t* rdram,
    uint32_t source,
    uint32_t slot,
    uint32_t text_pointer,
    uint32_t aux0,
    uint32_t aux1);
void sote_trace_text_slot_render_candidate(
    uint8_t* rdram,
    uint32_t source,
    uint32_t slot,
    uint32_t object,
    uint32_t slot_flags_pointer,
    uint32_t position_pointer);
void sote_note_droid_text_buffer_draw(
    uint8_t* rdram,
    uint32_t source,
    uint32_t slot,
    uint32_t text_pointer,
    uint32_t position_pointer,
    uint32_t color_pointer);
uint32_t sote_normalize_zero_velocity_motion(
    uint8_t* rdram,
    uint32_t object);
void sote_note_motion_loop_guard(uint8_t* rdram, uint32_t object);
void sote_update_graphics_menu(uint8_t* rdram);
uint32_t sote_is_bike_stage_active(void);

#ifdef __cplusplus
}
#endif
