#ifndef MATH2D_H
#define MATH2D_H

#include <math.h>

typedef struct Vec2 {
    float x;
    float y;
} Vec2;

// Basic constructors
static inline Vec2 vec2(float x, float y) {
    Vec2 v; v.x = x; v.y = y; return v;
}

// Add / sub / scale
static inline Vec2 vec2_add(Vec2 a, Vec2 b) {
    return vec2(a.x + b.x, a.y + b.y);
}

static inline Vec2 vec2_sub(Vec2 a, Vec2 b) {
    return vec2(a.x - b.x, a.y - b.y);
}

static inline Vec2 vec2_scale(Vec2 v, float s) {
    return vec2(v.x * s, v.y * s);
}

// Dot, length, normalize
static inline float vec2_dot(Vec2 a, Vec2 b) {
    return a.x * b.x + a.y * b.y;
}

static inline float vec2_len_sq(Vec2 v) {
    return vec2_dot(v, v);
}

static inline float vec2_len(Vec2 v) {
    return sqrtf(vec2_len_sq(v));
}

static inline Vec2 vec2_normalize(Vec2 v) {
    float len = vec2_len(v);
    if (len > 0.0f) {
        float inv = 1.0f / len;
        return vec2(v.x * inv, v.y * inv);
    }
    return vec2(0.0f, 0.0f);
}

// Clamp, lerp, misc
static inline float math_clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

static inline float math_lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

static inline float math_minf(float a, float b) {
    return a < b ? a : b;
}

static inline float math_maxf(float a, float b) {
    return a > b ? a : b;
}

static inline Vec2 vec2_lerp(Vec2 a, Vec2 b, float t) {
    return vec2(math_lerp(a.x, b.x, t), math_lerp(a.y, b.y, t));
}

#endif // MATH2D_H
