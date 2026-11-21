#pragma once
#include "upload_budget.h"
/* One owner for each native renderer allocation. Share sizes with the
   optional admission guard; do not duplicate numbers in a second ledger. */
enum {
    XB_MIP_CACHE_SLOTS = 128,
    XB_MIP_CACHE_WORDS = 8192,
    XB_MIP_CACHE_BYTES = XB_MIP_CACHE_SLOTS * XB_MIP_CACHE_WORDS * sizeof(uint32_t),
    XB_MIP_FALLBACK_BYTES = 1024 * 1024,
    XB_VERTEX_RING_COUNT = 4095,
    XB_VERTEX_STRIDE = 48,
    XB_VERTEX_RING_BYTES = XB_VERTEX_RING_COUNT * XB_VERTEX_STRIDE
};
#define XB_GPU_PAGE_BYTES(n) (((n)+4095u)&~4095u)
#define XB_NATIVE_GPU_ALL_PENDING (XB_GPU_PAGE_BYTES(XB_LINEAR_UPLOAD_BYTES) + \
    XB_GPU_PAGE_BYTES(XB_MIP_CACHE_BYTES) + XB_GPU_PAGE_BYTES(XB_MIP_FALLBACK_BYTES) + \
    XB_GPU_PAGE_BYTES(XB_VERTEX_RING_BYTES))
