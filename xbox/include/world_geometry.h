#pragma once
#include <float.h>
#include <stdint.h>

/* Column vectors are uploaded in order for the NV2A DP4 instructions.
   The source N64 matrices operate on row vectors. */
typedef struct {
    float model[4][4], projection[4][4];
    float light_direction[4], diffuse[4], ambient[4];
    uint64_t serial;
} xb_world_transform;

typedef struct {
    float position[3], color[4], st[2];
    uint64_t ring_generation;
    unsigned ring_index;
} xb_world_vertex;

typedef struct {
    float scale[4], offset[4], texel[4];
} xb_world_view;

typedef struct { float coefficient[4], error[4]; } xb_clip_plane;

/* Pull the six homogeneous planes into object space once per matrix state.
   Keep the absolute pre-cancellation products for the floating-point guard. */
static inline void xb_world_planes(const xb_world_transform *state, xb_clip_plane planes[6]) {
    for (unsigned p = 0; p < 6; ++p) {
        for (unsigned i = 0; i < 4; ++i) {
            float value = 0, magnitude = 0;
            for (unsigned k = 0; k < 4; ++k) {
                float m = state->model[k][i];
                float w = state->projection[3][k];
                float axis = state->projection[p / 2][k];
                value += m * (w + ((p & 1) ? -axis : axis));
                magnitude += __builtin_fabsf(m) * (__builtin_fabsf(w) + __builtin_fabsf(axis));
            }
            planes[p].coefficient[i] = value;
            planes[p].error[i] = magnitude * (64.0f * FLT_EPSILON);
        }
    }
}

/* A positive lower bound proves that every point in the whole load is
   strictly inside. Near-plane crossings and uncertain cases use F3DWAVE. */
static inline int xb_world_inside(const xb_clip_plane planes[6], const int lo[3], const int hi[3]) {
    float extent[3];
    for (unsigned i = 0; i < 3; ++i)
        extent[i] = (float)(hi[i] > -lo[i] ? hi[i] : -lo[i]);
    for (unsigned p = 0; p < 6; ++p) {
        float lower = planes[p].coefficient[3], guard = planes[p].error[3];
        for (unsigned i = 0; i < 3; ++i) {
            float c = planes[p].coefficient[i];
            lower += c * (c < 0 ? hi[i] : lo[i]);
            guard += planes[p].error[i] * extent[i];
        }
        if (!(lower > guard)) return 0;
    }
    return 1;
}
