#ifndef SCENE_PRESETS_H
#define SCENE_PRESETS_H

#include <stdbool.h>
#include <stddef.h>

typedef enum FluidEmitterType {
    EMITTER_DENSITY_SOURCE = 0,
    EMITTER_VELOCITY_JET,
    EMITTER_SINK
} FluidEmitterType;

typedef struct FluidEmitter {
    FluidEmitterType type;
    float position_x;   // normalized 0..1
    float position_y;
    float radius;       // normalized radius (fraction of grid)
    float strength;     // general scalar (density per second or velocity magnitude)
    float dir_x;        // for velocity jets / sinks
    float dir_y;
} FluidEmitter;

#define MAX_FLUID_EMITTERS 8

typedef struct FluidScenePreset {
    const char *name;
    size_t emitter_count;
    bool   is_custom;
    FluidEmitter emitters[MAX_FLUID_EMITTERS];
} FluidScenePreset;

const FluidScenePreset *scene_presets_get_all(size_t *count);
const FluidScenePreset *scene_presets_get_default(void);

#endif // SCENE_PRESETS_H
