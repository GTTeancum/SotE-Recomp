#pragma once
#include <stdint.h>
/* Marks capture total console use (kernel + user + graphics), not just the C
   heap. Named optional marks are instrumentation points, not feature gates. */
enum xb_budget_site {
    XB_BUDGET_BOOT, XB_BUDGET_DISPLAY, XB_BUDGET_PBKIT, XB_BUDGET_RDRAM,
    XB_BUDGET_THREAD_OPEN, XB_BUDGET_THREAD_JOIN, XB_BUDGET_LINEAR,
    XB_BUDGET_TEXCACHE, XB_BUDGET_OVERSIZE, XB_BUDGET_VERTEX,
    XB_BUDGET_AUDIO_OPEN, XB_BUDGET_AUDIO_CLOSE, XB_BUDGET_STORY,
    XB_BUDGET_FIRST_PRESENT, XB_BUDGET_PAUSE, XB_BUDGET_PRESENT,
    XB_BUDGET_HEARTBEAT, XB_BUDGET_OPTIONAL_STAGE, XB_BUDGET_OPTIONAL_RESIDENT,
    XB_BUDGET_OPTIONAL_RELEASE, XB_BUDGET_SITES
};
#define XB_BUDGET_TARGET (60u * 1024u * 1024u)
#define XB_BUDGET_OPTIONAL_CUSHION (1024u * 1024u)
#define XB_BUDGET_NO_SCENE UINT32_MAX
/* Upper bound on incremental backing for one ordinary malloc in the pinned
   NXDK dlmalloc configuration, *not* the amount always committed. 32-bit
   chunk/segment metadata and alignment fit in 128 bytes; a fallback segment
   rounds to 64 KiB, direct mmap to only 4 KiB. This also covers an empty
   heap. Do not apply the heap estimate to an exact-sized contiguous pool. */
static inline uint32_t xb_budget_optional_heap_cost(uint32_t payload) {
    if(!payload || payload>UINT32_MAX-128u-65535u)return 0;
    return (payload+128u+65535u)&~65535u;
}
struct _MM_STATISTICS;
#ifdef SOTE_XBOX
void xb_budget_init(void);
unsigned xb_budget_fix_native_texture_profile(int hd);
void xb_budget_mark(unsigned site, unsigned detail);
void xb_budget_story(unsigned event);
void xb_budget_present(unsigned event, unsigned pause);
void xb_budget_heartbeat(const struct _MM_STATISTICS* m);
/* Optional content only. A ticket covers PAGE-rounded incremental peak
   bytes while its owner allocates; finish on both success and failure. */
uint32_t xb_budget_optional_reserve(uint32_t additional_peak_bytes);
/* Revalidate a ticket whose candidate is now resident. The other tickets,
   native GPU liability and usual cushion still count; the own pages are now
   present in used, so only own pending estimate is removed. */
static inline int xb_budget_optional_can_publish(unsigned used,unsigned available,unsigned pending,
    unsigned own,unsigned native) {
    if(!own || own>pending)return 0;
    uint64_t need=(uint64_t)pending-own+native+XB_BUDGET_OPTIONAL_CUSHION;
    return (uint64_t)used+need<=XB_BUDGET_TARGET && need<=available;
}
int xb_budget_optional_recheck_resident(uint32_t ticket);
void xb_budget_optional_finish(uint32_t ticket);
/* Four existing native WC pools, unchanged in size/flags. Not subject to an
   optional budget denial; serializes the real allocation with its liability. */
void* xb_budget_native_gpu_allocate(unsigned site);
extern int xb_budget_frame_probe;
#else
static inline void xb_budget_mark(unsigned site,unsigned detail){(void)site;(void)detail;}
#endif
