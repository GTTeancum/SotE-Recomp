#include <stdint.h>
#include <string.h>
#include "boss_movie_queue.h"
#include "rdram_guard.h"
/* Preserve the PC's optional movie notification queue, not a fatal import.
 * No guest state, native reveal timer or actor command is modified here.
 * Until an enabled, available Xbox movie backend consumes a cue, the native
 * N64 reveal continues. Disabled/unavailable cues are consumed as fallback
 * immediately, so enabling movies later cannot play an old encounter.
 */
volatile uint32_t sote_native_boss_cues[4]; /* Gall count/actor, Palace count/actor */
static struct {uint32_t pending,actor;} queue[2];
static uint32_t movies_enabled,available_movies;
static uint32_t word(const uint8_t* rdram,uint32_t address) {
    uint32_t v;memcpy(&v,rdram+xb_rdram_offset(address&0x7fffffu,4,XB_RDRAM_BOSS),sizeof v);return v;
}
void xb_boss_movies_configure(int enabled,uint32_t available_mask) {
    movies_enabled=enabled!=0;available_movies=available_mask&3u;
    for(unsigned i=0;i<2;i++)
        if(!movies_enabled || !(available_movies&(1u<<i)))
            __atomic_store_n(&queue[i].pending,0,__ATOMIC_RELEASE);
}
void xb_boss_movies_set_enabled(int enabled) {xb_boss_movies_configure(enabled,available_movies);}
void xb_boss_movies_set_available(uint32_t available_mask) {xb_boss_movies_configure(movies_enabled,available_mask);}
int xb_boss_movies_enabled(void) {return movies_enabled!=0;}
uint32_t xb_boss_movies_available_mask(void) {return available_movies;}
static void notify(uint8_t* rdram,uint32_t actor,unsigned index) {
    uint16_t event;
    if(!rdram || actor<0x80000000u || actor>0x80800000u-0x58cu)return;
    memcpy(&event,rdram+(0x13ce0eu^2u),sizeof event);
    const uint32_t expected_event=index?27u:15u;
    const uint32_t expected_kind=index?0x676c6164u:0x626f6261u;
    if(event!=expected_event || word(rdram,actor)!=0x426f7373u ||
       word(rdram,actor+0x78u)!=expected_kind || word(rdram,actor+0x584u)!=10u)return;
    sote_native_boss_cues[index*2]++;
    sote_native_boss_cues[index*2+1]=actor;
    queue[index].actor=actor;
    __atomic_store_n(&queue[index].pending,1,__ATOMIC_RELEASE);
    if(!movies_enabled || !(available_movies&(1u<<index)))
        __atomic_store_n(&queue[index].pending,0,__ATOMIC_RELEASE);
}
int xb_boss_movies_take(uint32_t current_event,xb_boss_movie_cue* out) {
    /* Clear every stale slot before returning a valid cue from any slot. */
    for(unsigned i=0;i<2;i++) {
        const uint32_t event=i?27u:15u,bit=1u<<i;
        if(current_event!=event || !movies_enabled || !(available_movies&bit))
            __atomic_store_n(&queue[i].pending,0,__ATOMIC_RELEASE);
    }
    for(unsigned i=0;i<2;i++) {
        const uint32_t event=i?27u:15u,bit=1u<<i;
        if(current_event==event && movies_enabled && (available_movies&bit) &&
           out && __atomic_exchange_n(&queue[i].pending,0,__ATOMIC_ACQUIRE)) {
            out->movie=bit;out->event=event;out->actor=queue[i].actor;return 1;
        }
    }
    return 0;
}
void sote_gall_boss_movie_cue(uint8_t* rdram,uint32_t actor) {notify(rdram,actor,0);}
void sote_palace_boss_movie_cue(uint8_t* rdram,uint32_t actor) {notify(rdram,actor,1);}
