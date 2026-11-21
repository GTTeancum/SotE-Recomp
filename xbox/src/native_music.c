#include "recomp_hooks.h"
/* Native N64 audio path. Replacement claims must stay false until the Xbox
   OGG/voice backend exists; returning true would suppress original sound. */
volatile struct {uint32_t command,resets,frames,requests;int32_t last_sound;} sote_native_music;
void sote_music_reset(uint8_t* rdram) {
    (void)rdram;sote_native_music.command=0;sote_native_music.last_sound=-1;sote_native_music.resets++;
}
void sote_music_command(uint8_t* rdram,uint32_t name_address) {
    (void)rdram;sote_native_music.command=name_address;
}
void sote_music_frame_begin(uint8_t* rdram) {(void)rdram;sote_native_music.frames++;}
void sote_music_frame_end(uint8_t* rdram) {(void)rdram;}
uint32_t sote_music_native_request(uint8_t* rdram,int32_t sound,int32_t volume,int32_t continuous) {
    (void)rdram;(void)volume;(void)continuous;
    sote_native_music.requests++;sote_native_music.last_sound=sound;return 0;
}
uint32_t sote_play_hd_sound_request(uint8_t* rdram,int32_t sound_id) {
    (void)rdram;(void)sound_id;return 0;
}
