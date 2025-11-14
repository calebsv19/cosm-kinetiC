#ifndef TIMING_H
#define TIMING_H

#include <SDL2/SDL.h>
#include "app/app_config.h"

typedef struct FrameTimer {
    Uint32 last_ticks;
    double dt;
} FrameTimer;

void   timing_init(FrameTimer *t);
double timing_begin_frame(FrameTimer *t, const AppConfig *cfg);

#endif // TIMING_H
