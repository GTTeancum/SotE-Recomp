#include "recomp_hooks.h"
/* Original-font mode: record text lifecycle for validation while leaving the
   game's glyph/sprite display lists intact. Modern rendering is still pending. */
volatile struct {uint32_t text,glyphs,hidden,slot;int32_t x;} sote_text_status;
/* Retained native communicator observations. The PC voice backend is pending;
   recording a cue here must not be counted as having played its voice. */
volatile struct {uint32_t draws,source,slot,text,position,color,key,caller,lookups;} sote_communicator_status;
void sote_note_droid_text_buffer_draw(uint8_t* rdram,uint32_t source,uint32_t slot,
    uint32_t text,uint32_t position,uint32_t color) {
    (void)rdram;sote_communicator_status.draws++;sote_communicator_status.source=source;
    sote_communicator_status.slot=slot;sote_communicator_status.text=text;
    sote_communicator_status.position=position;sote_communicator_status.color=color;
}
void sote_note_message_lookup(uint8_t* rdram,uint32_t key,uint32_t text,uint32_t caller) {
    (void)rdram;sote_communicator_status.key=key;sote_communicator_status.text=text;
    sote_communicator_status.caller=caller;sote_communicator_status.lookups++;
}
void sote_hide_communicator_slot(uint32_t slot,int32_t x) {
    sote_text_status.hidden++;sote_text_status.slot=slot;sote_text_status.x=x;
}
void sote_native_text_begin(uint8_t* rdram,uint32_t text) {
    (void)rdram;sote_text_status.text=text;
}
int sote_native_text_glyph(uint8_t* rdram,uint32_t list,uint32_t character,int x,int y) {
    (void)rdram;(void)list;(void)character;(void)x;(void)y;
    sote_text_status.glyphs++;return 0;
}
int sote_native_text_suppress(void){return 0;}
int sote_draw_hud_without_label(uint8_t* rdram,void* context){(void)rdram;(void)context;return 0;}
void sote_diagnostic_communicators(uint8_t* rdram,void* context) {
    /* Desktop-only optional injection is disabled; normal communicator logic
       still executes in the original game. Xbox coverage uses its own harness. */
    (void)rdram;(void)context;
}
