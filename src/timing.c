#include "timing.h"

static double clamp_double(double v, double lo, double hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

void timing_init(FrameTimer *t) {
    if (!t) return;
    t->last_ticks = SDL_GetTicks();
    t->dt = 0.0;
}

double timing_begin_frame(FrameTimer *t, const AppConfig *cfg) {
    if (!t || !cfg) return 0.0;

    Uint32 now = SDL_GetTicks();
    Uint32 elapsed = now - t->last_ticks;
    t->last_ticks = now;

    double dt = elapsed / 1000.0;
    dt = clamp_double(dt, cfg->min_dt, cfg->max_dt);
    t->dt = dt;
    return dt;
}
