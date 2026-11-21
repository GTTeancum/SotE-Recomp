#ifndef SOTE_XBOX_BOSS_MOVIE_QUEUE_H
#define SOTE_XBOX_BOSS_MOVIE_QUEUE_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
enum { XB_BOSS_MOVIE_GALL=1u, XB_BOSS_MOVIE_PALACE=2u };
typedef struct { uint32_t movie, event, actor; } xb_boss_movie_cue;
/* Future Xbox movie integration configures the user option and usable assets.
 * Availability must mean a functioning backend AND the corresponding asset.
 * Call configuration/take on the game thread. No playback backend exists here.
 */
void xb_boss_movies_configure(int enabled,uint32_t available_mask);
void xb_boss_movies_set_enabled(int enabled);
void xb_boss_movies_set_available(uint32_t available_mask);
int xb_boss_movies_enabled(void);
uint32_t xb_boss_movies_available_mask(void);
/* Coalesced, bounded one-pending-cue-per-boss queue, like PC pending flags.
 * Wrong-level cues are discarded. Successful take consumes exactly once.
 */
int xb_boss_movies_take(uint32_t current_event,xb_boss_movie_cue* out);
#ifdef __cplusplus
}
#endif
#endif
