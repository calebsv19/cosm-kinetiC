#ifndef ATMOSPHERIC_FIELD_H
#define ATMOSPHERIC_FIELD_H

#include <stdbool.h>
#include <stddef.h>

#include "app/scene_presets.h"

typedef struct AtmosphericFieldSample {
    float density;
    float velocity_x;
    float velocity_y;
    float velocity_z;
} AtmosphericFieldSample;

AtmosphericPresetSettings atmospheric_preset_default_settings(void);
void atmospheric_preset_sanitize(AtmosphericPresetSettings *settings);
bool atmospheric_preset_enabled(const FluidScenePreset *preset);

AtmosphericFieldSample atmospheric_field_sample_2d(const AtmosphericPresetSettings *settings,
                                                   float x,
                                                   float y);
AtmosphericFieldSample atmospheric_field_sample_3d(const AtmosphericPresetSettings *settings,
                                                   float x,
                                                   float y,
                                                   float z);

size_t atmospheric_field_seed_2d(const AtmosphericPresetSettings *settings,
                                 int width,
                                 int height,
                                 float *density,
                                 float *velocity_x,
                                 float *velocity_y);

#endif
