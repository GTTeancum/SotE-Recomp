/* The GPU path bypasses polygon clipping only if the complete source load
   fits. Test the object-space bound against independently transformed corners. */
#include <assert.h>
#include <math.h>
#include "world_geometry.h"

static int all_corners_inside(const xb_world_transform *state, const int lo[3], const int hi[3]) {
    for (unsigned corner = 0; corner < 8; ++corner) {
        double position[4], eye[4] = {0}, clip[4] = {0};
        for (unsigned axis = 0; axis < 3; ++axis)
            position[axis] = (corner & (1u << axis)) ? hi[axis] : lo[axis];
        position[3] = 1;
        for (unsigned c = 0; c < 4; ++c)
            for (unsigned r = 0; r < 4; ++r)
                eye[c] += position[r] * state->model[c][r];
        for (unsigned c = 0; c < 4; ++c)
            for (unsigned r = 0; r < 4; ++r)
                clip[c] += eye[r] * state->projection[c][r];
        for (unsigned axis = 0; axis < 3; ++axis)
            if (!(fabs(clip[axis]) < clip[3])) return 0;
    }
    return 1;
}

int main(void) {
    xb_world_transform state = {0};
    xb_clip_plane planes[6];
    for (unsigned axis = 0; axis < 4; ++axis)
        state.model[axis][axis] = state.projection[axis][axis] = 1;
    state.model[0][0] = 0.8f;
    state.model[0][1] = -0.6f;
    state.model[1][0] = 0.6f;
    state.model[1][1] = 0.8f;
    state.model[2][3] = 1;
    state.projection[2][2] = 0.5f;
    state.projection[3][2] = 1;
    xb_world_planes(&state, planes);
    unsigned accepted = 0, rejected = 0;
    for (int z = -8; z < 24; z += 2) {
        for (int x = -25; x < 26; x += 2) {
            for (int y = -9; y < 10; y += 3) {
                const int lo[] = {x, y, z}, hi[] = {x + 1, y + 2, z + 3};
                if (xb_world_inside(planes, lo, hi)) {
                    assert(all_corners_inside(&state, lo, hi));
                    ++accepted;
                } else {
                    ++rejected;
                }
            }
        }
    }
    assert(accepted && rejected);

    const int origin[3] = {0};
    state.projection[2][2] = 2; /* At the origin, clip.z == clip.w. */
    xb_world_planes(&state, planes);
    assert(!xb_world_inside(planes, origin, origin));
    state.projection[2][2] = nextafterf(2.0f, 0.0f);
    xb_world_planes(&state, planes);
    assert(!xb_world_inside(planes, origin, origin));
    state.projection[2][2] = NAN;
    xb_world_planes(&state, planes);
    assert(!xb_world_inside(planes, origin, origin));
    return 0;
}
