#include "app/atmospheric/atmospheric_field.h"

#include <math.h>
#include <stdbool.h>
#include <stdio.h>

static bool finite_sample(AtmosphericFieldSample sample) {
    return isfinite(sample.density) &&
           isfinite(sample.velocity_x) &&
           isfinite(sample.velocity_y) &&
           isfinite(sample.velocity_z);
}

static bool nearly_equal(float a, float b) {
    return fabsf(a - b) <= 0.0001f;
}

static bool test_samples_are_deterministic_and_finite(void) {
    AtmosphericPresetSettings settings = atmospheric_preset_default_settings();
    AtmosphericFieldSample a = atmospheric_field_sample_3d(&settings, 0.37f, 0.48f, 0.59f);
    AtmosphericFieldSample b = atmospheric_field_sample_3d(&settings, 0.37f, 0.48f, 0.59f);
    if (!finite_sample(a) || !finite_sample(b)) return false;
    if (!nearly_equal(a.density, b.density)) return false;
    if (!nearly_equal(a.velocity_x, b.velocity_x)) return false;
    if (!nearly_equal(a.velocity_y, b.velocity_y)) return false;
    if (!nearly_equal(a.velocity_z, b.velocity_z)) return false;
    return true;
}

static bool test_height_band_shapes_density(void) {
    AtmosphericPresetSettings settings = atmospheric_preset_default_settings();
    settings.region_count = 0;
    settings.base_density = 0.0f;
    settings.density_threshold = 0.0f;
    settings.band_min_y = 0.35f;
    settings.band_max_y = 0.65f;
    settings.band_edge_falloff = 0.08f;
    AtmosphericFieldSample low = atmospheric_field_sample_3d(&settings, 0.5f, 0.05f, 0.5f);
    AtmosphericFieldSample mid = atmospheric_field_sample_3d(&settings, 0.5f, 0.5f, 0.5f);
    if (!finite_sample(low) || !finite_sample(mid)) return false;
    if (mid.density <= low.density) return false;
    return true;
}

static bool test_2d_seed_populates_density_and_planar_velocity(void) {
    float density[16] = {0};
    float vel_x[16] = {0};
    float vel_y[16] = {0};
    AtmosphericPresetSettings settings = atmospheric_preset_default_settings();
    settings.base_density = 0.25f;
    size_t seeded = atmospheric_field_seed_2d(&settings, 4, 4, density, vel_x, vel_y);
    if (seeded == 0) return false;
    for (size_t i = 0; i < 16; ++i) {
        if (!isfinite(density[i]) || !isfinite(vel_x[i]) || !isfinite(vel_y[i])) {
            return false;
        }
    }
    return true;
}

int main(void) {
    if (!test_samples_are_deterministic_and_finite()) {
        fprintf(stderr, "atmospheric_field_contract_test: deterministic finite sample failed\n");
        return 1;
    }
    if (!test_height_band_shapes_density()) {
        fprintf(stderr, "atmospheric_field_contract_test: height band contract failed\n");
        return 1;
    }
    if (!test_2d_seed_populates_density_and_planar_velocity()) {
        fprintf(stderr, "atmospheric_field_contract_test: 2d seed contract failed\n");
        return 1;
    }
    fprintf(stdout, "atmospheric_field_contract_test: success\n");
    return 0;
}
