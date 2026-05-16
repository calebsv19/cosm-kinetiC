#include "app/atmospheric/atmospheric_field.h"

#include <math.h>
#include <stdint.h>

static const float ATMOSPHERIC_DENSITY_EPSILON = 0.0001f;

static float clampf_local(float v, float min_v, float max_v) {
    if (v < min_v) return min_v;
    if (v > max_v) return max_v;
    return v;
}

static float sane_or(float value, float fallback) {
    return isfinite(value) ? value : fallback;
}

static uint32_t hash_u32(uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}

static float hash_to_unit(uint32_t x) {
    return (float)(hash_u32(x) & 0x00ffffffu) / 16777215.0f;
}

static float fade(float t) {
    return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

static float lerpf_local(float a, float b, float t) {
    return a + (b - a) * t;
}

static float value_noise_3d(float x, float y, float z, uint32_t seed) {
    int ix = (int)floorf(x);
    int iy = (int)floorf(y);
    int iz = (int)floorf(z);
    float fx = x - (float)ix;
    float fy = y - (float)iy;
    float fz = z - (float)iz;
    float ux = fade(fx);
    float uy = fade(fy);
    float uz = fade(fz);

    float c000 = hash_to_unit(seed ^ (uint32_t)ix * 73856093u ^ (uint32_t)iy * 19349663u ^ (uint32_t)iz * 83492791u);
    float c100 = hash_to_unit(seed ^ (uint32_t)(ix + 1) * 73856093u ^ (uint32_t)iy * 19349663u ^ (uint32_t)iz * 83492791u);
    float c010 = hash_to_unit(seed ^ (uint32_t)ix * 73856093u ^ (uint32_t)(iy + 1) * 19349663u ^ (uint32_t)iz * 83492791u);
    float c110 = hash_to_unit(seed ^ (uint32_t)(ix + 1) * 73856093u ^ (uint32_t)(iy + 1) * 19349663u ^ (uint32_t)iz * 83492791u);
    float c001 = hash_to_unit(seed ^ (uint32_t)ix * 73856093u ^ (uint32_t)iy * 19349663u ^ (uint32_t)(iz + 1) * 83492791u);
    float c101 = hash_to_unit(seed ^ (uint32_t)(ix + 1) * 73856093u ^ (uint32_t)iy * 19349663u ^ (uint32_t)(iz + 1) * 83492791u);
    float c011 = hash_to_unit(seed ^ (uint32_t)ix * 73856093u ^ (uint32_t)(iy + 1) * 19349663u ^ (uint32_t)(iz + 1) * 83492791u);
    float c111 = hash_to_unit(seed ^ (uint32_t)(ix + 1) * 73856093u ^ (uint32_t)(iy + 1) * 19349663u ^ (uint32_t)(iz + 1) * 83492791u);

    float x00 = lerpf_local(c000, c100, ux);
    float x10 = lerpf_local(c010, c110, ux);
    float x01 = lerpf_local(c001, c101, ux);
    float x11 = lerpf_local(c011, c111, ux);
    float y0 = lerpf_local(x00, x10, uy);
    float y1 = lerpf_local(x01, x11, uy);
    return lerpf_local(y0, y1, uz);
}

static float fbm_noise(float x, float y, float z, uint32_t seed) {
    float sum = 0.0f;
    float amp = 0.5f;
    float freq = 1.0f;
    for (int octave = 0; octave < 4; ++octave) {
        sum += amp * value_noise_3d(x * freq, y * freq, z * freq, seed + (uint32_t)octave * 1013u);
        freq *= 2.0f;
        amp *= 0.5f;
    }
    return clampf_local(sum, 0.0f, 1.0f);
}

static float smooth_band(float y, float min_y, float max_y, float edge) {
    min_y = clampf_local(min_y, 0.0f, 1.0f);
    max_y = clampf_local(max_y, 0.0f, 1.0f);
    if (max_y < min_y) {
        float tmp = min_y;
        min_y = max_y;
        max_y = tmp;
    }
    edge = clampf_local(edge, 0.001f, 0.5f);
    float rise = clampf_local((y - min_y) / edge, 0.0f, 1.0f);
    float fall = clampf_local((max_y - y) / edge, 0.0f, 1.0f);
    return fade(rise) * fade(fall);
}

static float region_weight(const AtmosphericDensityRegion *region,
                           float x,
                           float y,
                           float z,
                           bool use_z) {
    if (!region || !region->enabled) return 0.0f;
    float sx = fmaxf(sane_or(region->size_x, 0.1f), 0.001f);
    float sy = fmaxf(sane_or(region->size_y, 0.1f), 0.001f);
    float sz = fmaxf(sane_or(region->size_z, 0.1f), 0.001f);
    float dx = fabsf(x - sane_or(region->center_x, 0.5f)) / sx;
    float dy = fabsf(y - sane_or(region->center_y, 0.5f)) / sy;
    float dz = use_z ? fabsf(z - sane_or(region->center_z, 0.5f)) / sz : 0.0f;
    float dist = 0.0f;
    if (region->shape == ATMOSPHERIC_REGION_ELLIPSE) {
        dist = sqrtf(dx * dx + dy * dy + dz * dz);
    } else {
        dist = fmaxf(fmaxf(dx, dy), dz);
    }
    float falloff = clampf_local(sane_or(region->falloff, 0.2f), 0.001f, 1.0f);
    float t = clampf_local((1.0f - dist) / falloff, 0.0f, 1.0f);
    return fade(t);
}

AtmosphericPresetSettings atmospheric_preset_default_settings(void) {
    AtmosphericPresetSettings settings = {0};
    settings.enabled = true;
    settings.seed = 1337u;
    settings.base_density = 0.0f;
    settings.density_scale = 7.0f;
    settings.density_threshold = 0.54f;
    settings.base_wind_x = 4.0f;
    settings.base_wind_y = 0.25f;
    settings.base_wind_z = 1.0f;
    settings.turbulence_strength = 2.0f;
    settings.noise_scale = 3.0f;
    settings.detail_scale = 10.0f;
    settings.band_min_y = 0.22f;
    settings.band_max_y = 0.78f;
    settings.band_edge_falloff = 0.16f;
    settings.region_count = 2;
    settings.regions[0] = (AtmosphericDensityRegion){
        .enabled = true,
        .shape = ATMOSPHERIC_REGION_ELLIPSE,
        .center_x = 0.38f,
        .center_y = 0.50f,
        .center_z = 0.48f,
        .size_x = 0.28f,
        .size_y = 0.16f,
        .size_z = 0.22f,
        .density = 2.0f,
        .falloff = 0.35f,
    };
    settings.regions[1] = (AtmosphericDensityRegion){
        .enabled = true,
        .shape = ATMOSPHERIC_REGION_RECT,
        .center_x = 0.70f,
        .center_y = 0.62f,
        .center_z = 0.55f,
        .size_x = 0.18f,
        .size_y = 0.12f,
        .size_z = 0.18f,
        .density = 1.25f,
        .falloff = 0.45f,
    };
    return settings;
}

void atmospheric_preset_sanitize(AtmosphericPresetSettings *settings) {
    if (!settings) return;
    AtmosphericPresetSettings defaults = atmospheric_preset_default_settings();
    settings->enabled = settings->enabled ? true : false;
    if (settings->seed == 0u) settings->seed = defaults.seed;
    settings->base_density = clampf_local(sane_or(settings->base_density, defaults.base_density), 0.0f, 50.0f);
    settings->density_scale = clampf_local(sane_or(settings->density_scale, defaults.density_scale), 0.0f, 100.0f);
    settings->density_threshold = clampf_local(sane_or(settings->density_threshold, defaults.density_threshold), 0.0f, 1.0f);
    settings->base_wind_x = clampf_local(sane_or(settings->base_wind_x, defaults.base_wind_x), -200.0f, 200.0f);
    settings->base_wind_y = clampf_local(sane_or(settings->base_wind_y, defaults.base_wind_y), -200.0f, 200.0f);
    settings->base_wind_z = clampf_local(sane_or(settings->base_wind_z, defaults.base_wind_z), -200.0f, 200.0f);
    settings->turbulence_strength = clampf_local(sane_or(settings->turbulence_strength, defaults.turbulence_strength), 0.0f, 100.0f);
    settings->noise_scale = clampf_local(sane_or(settings->noise_scale, defaults.noise_scale), 0.01f, 128.0f);
    settings->detail_scale = clampf_local(sane_or(settings->detail_scale, defaults.detail_scale), 0.01f, 256.0f);
    settings->band_min_y = clampf_local(sane_or(settings->band_min_y, defaults.band_min_y), 0.0f, 1.0f);
    settings->band_max_y = clampf_local(sane_or(settings->band_max_y, defaults.band_max_y), 0.0f, 1.0f);
    settings->band_edge_falloff = clampf_local(sane_or(settings->band_edge_falloff, defaults.band_edge_falloff), 0.001f, 1.0f);
    if (settings->region_count > MAX_ATMOSPHERIC_DENSITY_REGIONS) {
        settings->region_count = MAX_ATMOSPHERIC_DENSITY_REGIONS;
    }
    for (size_t i = 0; i < settings->region_count; ++i) {
        AtmosphericDensityRegion *region = &settings->regions[i];
        region->enabled = region->enabled ? true : false;
        if (region->shape != ATMOSPHERIC_REGION_RECT &&
            region->shape != ATMOSPHERIC_REGION_ELLIPSE) {
            region->shape = ATMOSPHERIC_REGION_RECT;
        }
        region->center_x = clampf_local(sane_or(region->center_x, 0.5f), 0.0f, 1.0f);
        region->center_y = clampf_local(sane_or(region->center_y, 0.5f), 0.0f, 1.0f);
        region->center_z = clampf_local(sane_or(region->center_z, 0.5f), 0.0f, 1.0f);
        region->size_x = clampf_local(sane_or(region->size_x, 0.1f), 0.001f, 1.0f);
        region->size_y = clampf_local(sane_or(region->size_y, 0.1f), 0.001f, 1.0f);
        region->size_z = clampf_local(sane_or(region->size_z, 0.1f), 0.001f, 1.0f);
        region->density = clampf_local(sane_or(region->density, 0.0f), -50.0f, 100.0f);
        region->falloff = clampf_local(sane_or(region->falloff, 0.2f), 0.001f, 1.0f);
    }
}

bool atmospheric_preset_enabled(const FluidScenePreset *preset) {
    return preset && preset->domain == SCENE_DOMAIN_ATMOSPHERIC && preset->atmosphere.enabled;
}

static AtmosphericFieldSample sample_field(const AtmosphericPresetSettings *settings,
                                           float x,
                                           float y,
                                           float z,
                                           bool use_z) {
    AtmosphericPresetSettings local = {0};
    AtmosphericFieldSample sample = {0};
    if (!settings || !settings->enabled) return sample;
    local = *settings;
    atmospheric_preset_sanitize(&local);
    if (!local.enabled) return sample;

    x = clampf_local(x, 0.0f, 1.0f);
    y = clampf_local(y, 0.0f, 1.0f);
    z = clampf_local(z, 0.0f, 1.0f);

    float band = smooth_band(y, local.band_min_y, local.band_max_y, local.band_edge_falloff);
    float base = fbm_noise(x * local.noise_scale, y * local.noise_scale, z * local.noise_scale, local.seed);
    float detail = fbm_noise(x * local.detail_scale + 17.0f,
                             y * local.detail_scale + 31.0f,
                             z * local.detail_scale + 47.0f,
                             local.seed ^ 0x9e3779b9u);
    float shaped = fmaxf(0.0f, base + detail * 0.35f - local.density_threshold);
    sample.density = local.base_density + shaped * local.density_scale * band;
    for (size_t i = 0; i < local.region_count; ++i) {
        const AtmosphericDensityRegion *region = &local.regions[i];
        sample.density += region_weight(region, x, y, z, use_z) * region->density;
    }
    if (sample.density < ATMOSPHERIC_DENSITY_EPSILON) {
        sample.density = 0.0f;
    }

    const float e = 0.015f;
    if (use_z) {
        float ax_y1 = fbm_noise((x + 11.0f) * local.noise_scale,
                                (y + e + 13.0f) * local.noise_scale,
                                (z + 17.0f) * local.noise_scale,
                                local.seed + 53u);
        float ax_y0 = fbm_noise((x + 11.0f) * local.noise_scale,
                                (y - e + 13.0f) * local.noise_scale,
                                (z + 17.0f) * local.noise_scale,
                                local.seed + 53u);
        float ay_z1 = fbm_noise((x + 19.0f) * local.noise_scale,
                                (y + 23.0f) * local.noise_scale,
                                (z + e + 29.0f) * local.noise_scale,
                                local.seed + 97u);
        float ay_z0 = fbm_noise((x + 19.0f) * local.noise_scale,
                                (y + 23.0f) * local.noise_scale,
                                (z - e + 29.0f) * local.noise_scale,
                                local.seed + 97u);
        float az_x1 = fbm_noise((x + e + 31.0f) * local.noise_scale,
                                (y + 37.0f) * local.noise_scale,
                                (z + 41.0f) * local.noise_scale,
                                local.seed + 193u);
        float az_x0 = fbm_noise((x - e + 31.0f) * local.noise_scale,
                                (y + 37.0f) * local.noise_scale,
                                (z + 41.0f) * local.noise_scale,
                                local.seed + 193u);
        float az_y1 = fbm_noise((x + 31.0f) * local.noise_scale,
                                (y + e + 37.0f) * local.noise_scale,
                                (z + 41.0f) * local.noise_scale,
                                local.seed + 193u);
        float az_y0 = fbm_noise((x + 31.0f) * local.noise_scale,
                                (y - e + 37.0f) * local.noise_scale,
                                (z + 41.0f) * local.noise_scale,
                                local.seed + 193u);
        float ax_z1 = fbm_noise((x + 11.0f) * local.noise_scale,
                                (y + 13.0f) * local.noise_scale,
                                (z + e + 17.0f) * local.noise_scale,
                                local.seed + 53u);
        float ax_z0 = fbm_noise((x + 11.0f) * local.noise_scale,
                                (y + 13.0f) * local.noise_scale,
                                (z - e + 17.0f) * local.noise_scale,
                                local.seed + 53u);
        float ay_x1 = fbm_noise((x + e + 19.0f) * local.noise_scale,
                                (y + 23.0f) * local.noise_scale,
                                (z + 29.0f) * local.noise_scale,
                                local.seed + 97u);
        float ay_x0 = fbm_noise((x - e + 19.0f) * local.noise_scale,
                                (y + 23.0f) * local.noise_scale,
                                (z + 29.0f) * local.noise_scale,
                                local.seed + 97u);
        float inv = 0.5f / e;
        float curl_x = (az_y1 - az_y0 - (ay_z1 - ay_z0)) * inv;
        float curl_y = (ax_z1 - ax_z0 - (az_x1 - az_x0)) * inv;
        float curl_z = (ay_x1 - ay_x0 - (ax_y1 - ax_y0)) * inv;
        sample.velocity_x = local.base_wind_x + curl_x * local.turbulence_strength;
        sample.velocity_y = local.base_wind_y + curl_y * local.turbulence_strength;
        sample.velocity_z = local.base_wind_z + curl_z * local.turbulence_strength;
    } else {
        float n_y1 = fbm_noise(x * local.noise_scale,
                               (y + e) * local.noise_scale,
                               0.5f * local.noise_scale,
                               local.seed + 211u);
        float n_y0 = fbm_noise(x * local.noise_scale,
                               (y - e) * local.noise_scale,
                               0.5f * local.noise_scale,
                               local.seed + 211u);
        float n_x1 = fbm_noise((x + e) * local.noise_scale,
                               y * local.noise_scale,
                               0.5f * local.noise_scale,
                               local.seed + 211u);
        float n_x0 = fbm_noise((x - e) * local.noise_scale,
                               y * local.noise_scale,
                               0.5f * local.noise_scale,
                               local.seed + 211u);
        float inv = 0.5f / e;
        sample.velocity_x = local.base_wind_x + (n_y1 - n_y0) * inv * local.turbulence_strength;
        sample.velocity_y = local.base_wind_y - (n_x1 - n_x0) * inv * local.turbulence_strength;
        sample.velocity_z = 0.0f;
    }
    return sample;
}

AtmosphericFieldSample atmospheric_field_sample_2d(const AtmosphericPresetSettings *settings,
                                                   float x,
                                                   float y) {
    return sample_field(settings, x, y, 0.5f, false);
}

AtmosphericFieldSample atmospheric_field_sample_3d(const AtmosphericPresetSettings *settings,
                                                   float x,
                                                   float y,
                                                   float z) {
    return sample_field(settings, x, y, z, true);
}

size_t atmospheric_field_seed_2d(const AtmosphericPresetSettings *settings,
                                 int width,
                                 int height,
                                 float *density,
                                 float *velocity_x,
                                 float *velocity_y) {
    if (!settings || !settings->enabled || width <= 0 || height <= 0 ||
        !density || !velocity_x || !velocity_y) {
        return 0;
    }
    size_t seeded = 0;
    float inv_w = (width > 1) ? 1.0f / (float)(width - 1) : 0.0f;
    float inv_h = (height > 1) ? 1.0f / (float)(height - 1) : 0.0f;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            size_t idx = (size_t)y * (size_t)width + (size_t)x;
            AtmosphericFieldSample sample = atmospheric_field_sample_2d(settings,
                                                                        (float)x * inv_w,
                                                                        (float)y * inv_h);
            density[idx] = sample.density;
            velocity_x[idx] = sample.velocity_x;
            velocity_y[idx] = sample.velocity_y;
            if (sample.density > ATMOSPHERIC_DENSITY_EPSILON) {
                seeded++;
            }
        }
    }
    return seeded;
}
