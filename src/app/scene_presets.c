#include "app/scene_presets.h"

static const FluidScenePreset g_presets[] = {
    {
        .name = "Calm Box",
        .emitter_count = 0,
    },
    {
        .name = "Central Hotspot",
        .emitter_count = 1,
        .emitters = {
            {
                .type = EMITTER_DENSITY_SOURCE,
                .position_x = 0.5f,
                .position_y = 0.5f,
                .radius = 0.12f,
                .strength = 5.0f,
                .dir_x = 0.0f,
                .dir_y = -1.0f,
            },
        },
    },
    {
        .name = "Corner Jet",
        .emitter_count = 1,
        .emitters = {
            {
                .type = EMITTER_VELOCITY_JET,
                .position_x = 0.4f,
                .position_y = 0.85f,
                .radius = 0.08f,
                .strength = 55.0f,
                .dir_x = 0.9f,
                .dir_y = -0.2f,
            },
        },
    },
    {
        .name = "Dual Jets + Exhaust",
        .emitter_count = 3,
        .emitters = {
            {
                .type = EMITTER_VELOCITY_JET,
                .position_x = 0.15f,
                .position_y = 0.2f,
                .radius = 0.07f,
                .strength = 50.0f,
                .dir_x = 1.0f,
                .dir_y = 0.0f,
            },
            {
                .type = EMITTER_VELOCITY_JET,
                .position_x = 0.85f,
                .position_y = 0.8f,
                .radius = 0.07f,
                .strength = 50.0f,
                .dir_x = -1.0f,
                .dir_y = 0.0f,
            },
            {
                .type = EMITTER_SINK,
                .position_x = 0.5f,
                .position_y = 0.1f,
                .radius = 0.2f,
                .strength = 30.0f,
                .dir_x = 0.0f,
                .dir_y = -1.0f,
            },
        },
    },
};

const FluidScenePreset *scene_presets_get_all(size_t *count) {
    if (count) {
        *count = sizeof(g_presets) / sizeof(g_presets[0]);
    }
    return g_presets;
}

const FluidScenePreset *scene_presets_get_default(void) {
    return &g_presets[0];
}
