#ifndef COLLIDER_UTILS_H
#define COLLIDER_UTILS_H

#include <math.h>
#include <stdbool.h>
#include "physics/rigid/collider_types.h"

static inline int collider_clamp_int(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static inline float collider_clamp_float(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static inline bool collider_nearly_equal(HullPoint a, HullPoint b, float eps) {
    return fabsf(a.x - b.x) <= eps && fabsf(a.y - b.y) <= eps;
}

#endif // COLLIDER_UTILS_H
