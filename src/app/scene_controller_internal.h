#ifndef SCENE_CONTROLLER_INTERNAL_H
#define SCENE_CONTROLLER_INTERNAL_H

#include "app/app_config.h"
#include "app/scene_state.h"
#include "input/input.h"
#include "input/stroke_buffer.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct CommandDispatchContext {
    SceneState *scene;
    bool snapshot_requested;
} CommandDispatchContext;

typedef struct StrokeSampler {
    StrokeBuffer buffer;
    BrushMode brush_mode;
    bool pointer_down;
    float last_emit_x;
    float last_emit_y;
    float current_x;
    float current_y;
    double accumulator;
    double sample_interval;
    float sample_spacing;
} StrokeSampler;

typedef struct SceneControllerRs1DiagTotals {
    uint64_t frame_count;
    uint64_t derive_ns_total;
    uint64_t submit_ns_total;
    uint64_t invalidation_reason_bits_total;
    uint64_t dispatched_commands_total;
} SceneControllerRs1DiagTotals;

typedef struct SceneControllerIr1DiagTotals {
    uint64_t frame_count;
    uint64_t routed_global_total;
    uint64_t routed_scene_total;
    uint64_t routed_fallback_total;
    uint64_t invalidation_reason_bits_total;
} SceneControllerIr1DiagTotals;

#define SCENE_CONTROLLER_DEFAULT_SAMPLE_RATE 240.0
#define SCENE_CONTROLLER_DEFAULT_SAMPLE_SPACING 3.0f
#define SCENE_CONTROLLER_MAX_SAMPLES_PER_FRAME 512u

void stroke_sampler_init(StrokeSampler *sampler,
                         size_t capacity,
                         const AppConfig *cfg);
void stroke_sampler_shutdown(StrokeSampler *sampler);
void stroke_sampler_capture(StrokeSampler *sampler,
                            const InputCommands *cmds,
                            double dt);
void stroke_sampler_apply(StrokeSampler *sampler,
                          SceneState *scene,
                          size_t max_samples);

#endif
