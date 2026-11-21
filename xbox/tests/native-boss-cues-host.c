#include <assert.h>
#include <string.h>
#include "../src/native_boss_cues.c"
static uint8_t guest[0x800000],original[0x800000];
static const uint32_t actor=0x80190000;
static void putword(uint32_t a,uint32_t v){memcpy(guest+(a&0x7fffff),&v,4);}
static void setup(int palace) {
    uint16_t event=palace?27:15;memset(guest,0,sizeof guest);
    memcpy(guest+(0x13ce0e^2),&event,2);putword(actor,0x426f7373);
    putword(actor+0x78,palace?0x676c6164:0x626f6261);putword(actor+0x584,10);
    memcpy(original,guest,sizeof guest);
}
static void cue(int palace) {
    if(palace)sote_palace_boss_movie_cue(guest,actor);
    else sote_gall_boss_movie_cue(guest,actor);
    assert(!memcmp(guest,original,sizeof guest));
}
int main(void) {
    xb_boss_movie_cue out;
    for(int palace=0;palace<2;palace++) {
        unsigned event=palace?27:15,bit=1u<<palace;setup(palace);
        xb_boss_movies_configure(0,3);cue(palace);
        xb_boss_movies_configure(1,3);assert(!xb_boss_movies_take(event,&out));
        xb_boss_movies_configure(1,0);cue(palace);
        xb_boss_movies_configure(1,3);assert(!xb_boss_movies_take(event,&out));
        cue(palace);cue(palace);assert(xb_boss_movies_take(event,&out));
        assert(out.movie==bit && out.event==event && out.actor==actor);
        assert(!xb_boss_movies_take(event,&out));
        cue(palace);assert(!xb_boss_movies_take(3,&out));
        assert(!xb_boss_movies_take(event,&out));
        cue(palace);xb_boss_movies_configure(0,3);xb_boss_movies_configure(1,3);
        assert(!xb_boss_movies_take(event,&out));
        putword(actor+0x584,1);memcpy(original,guest,sizeof guest);cue(palace);
        assert(!xb_boss_movies_take(event,&out));
    }
    xb_boss_movies_configure(1,3);
    setup(1);cue(1);setup(0);cue(0);
    assert(xb_boss_movies_take(15,&out));
    assert(!xb_boss_movies_take(27,&out));
    xb_boss_movies_set_enabled(0);assert(!xb_boss_movies_enabled());
    xb_boss_movies_set_available(2);assert(xb_boss_movies_available_mask()==2);
    sote_gall_boss_movie_cue(0,actor);sote_palace_boss_movie_cue(guest,0);
    return 0;
}
