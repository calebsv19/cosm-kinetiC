#include "app/app_config.h"
#include "app/quality_profiles.h"

#include <stdbool.h>
#include <stdio.h>

static bool test_space_mode_catalog_split(void) {
    const QualityProfileDef *profile_2d = quality_profile_get_for_space_mode(SPACE_MODE_2D, 1);
    const QualityProfileDef *profile_3d = quality_profile_get_for_space_mode(SPACE_MODE_3D, 1);
    if (!profile_2d || !profile_3d) return false;
    if (quality_profile_catalog_for_space_mode(SPACE_MODE_2D) != QUALITY_PROFILE_CATALOG_2D) return false;
    if (quality_profile_catalog_for_space_mode(SPACE_MODE_3D) != QUALITY_PROFILE_CATALOG_3D) return false;
    if (quality_profile_count_for_space_mode(SPACE_MODE_3D) != 4) return false;
    if (quality_profile_count_for_space_mode(SPACE_MODE_2D) < 4) return false;
    return profile_2d->name && profile_3d->name &&
           profile_2d->name[0] != '\0' &&
           profile_3d->name[0] != '\0' &&
           profile_2d->solver_iterations == 16 &&
           profile_3d->solver_iterations == 18 &&
           profile_2d->grid_w == 128 &&
           profile_3d->grid_w == 160;
}

static bool test_3d_quality_presets_preserve_physical_coefficients(void) {
    static const struct {
        int index;
        int grid;
        int substeps;
        int solver;
        bool blur;
    } expectations[] = {
        {0, 96, 1, 10, false},
        {1, 160, 2, 18, true},
        {2, 224, 3, 28, true},
        {3, 256, 4, 40, true},
    };
    for (size_t i = 0; i < sizeof(expectations) / sizeof(expectations[0]); ++i) {
        AppConfig cfg = app_config_default();
        cfg.space_mode = SPACE_MODE_3D;
        cfg.grid_w = 320;
        cfg.grid_h = 320;
        cfg.grid_d = 72;
        cfg.physics_substeps = 7;
        cfg.fluid_solver_iterations = 33;
        cfg.density_diffusion = 0.00037f;
        cfg.density_decay = 0.022f;
        cfg.fluid_buoyancy_force = 2.75f;
        cfg.velocity_damping = 0.000013f;
        cfg.emitter_density_multiplier = 1.35f;
        cfg.emitter_velocity_multiplier = 0.82f;
        cfg.emitter_sink_multiplier = 1.14f;
        cfg.enable_render_blur = false;

        quality_profile_apply_for_space_mode(&cfg, SPACE_MODE_3D, expectations[i].index);

        if (!(cfg.grid_w == expectations[i].grid &&
              cfg.grid_h == expectations[i].grid &&
              cfg.grid_d == 72 &&
              cfg.physics_substeps == expectations[i].substeps &&
              cfg.fluid_solver_iterations == expectations[i].solver &&
              cfg.enable_render_blur == expectations[i].blur &&
              cfg.quality_index == expectations[i].index &&
              cfg.density_diffusion == 0.00037f &&
              cfg.density_decay == 0.022f &&
              cfg.fluid_buoyancy_force == 2.75f &&
              cfg.velocity_damping == 0.000013f &&
              cfg.emitter_density_multiplier == 1.35f &&
              cfg.emitter_velocity_multiplier == 0.82f &&
              cfg.emitter_sink_multiplier == 1.14f)) {
            return false;
        }
    }
    return true;
}

static bool test_2d_quality_presets_preserve_physical_coefficients(void) {
    AppConfig cfg = app_config_default();
    cfg.space_mode = SPACE_MODE_2D;
    cfg.grid_w = 300;
    cfg.grid_h = 300;
    cfg.physics_substeps = 5;
    cfg.fluid_solver_iterations = 31;
    cfg.density_diffusion = 0.00022f;
    cfg.density_decay = 0.081f;
    cfg.fluid_buoyancy_force = 0.91f;
    cfg.velocity_damping = 0.000009f;
    cfg.emitter_density_multiplier = 1.27f;
    cfg.emitter_velocity_multiplier = 1.08f;
    cfg.emitter_sink_multiplier = 0.73f;
    cfg.enable_render_blur = true;

    quality_profile_apply_for_space_mode(&cfg, SPACE_MODE_2D, 0);

    return cfg.grid_w == 96 &&
           cfg.grid_h == 96 &&
           cfg.physics_substeps == 1 &&
           cfg.fluid_solver_iterations == 12 &&
           cfg.enable_render_blur == false &&
           cfg.quality_index == 0 &&
           cfg.density_diffusion == 0.00022f &&
           cfg.density_decay == 0.081f &&
           cfg.fluid_buoyancy_force == 0.91f &&
           cfg.velocity_damping == 0.000009f &&
           cfg.emitter_density_multiplier == 1.27f &&
           cfg.emitter_velocity_multiplier == 1.08f &&
           cfg.emitter_sink_multiplier == 0.73f;
}

int main(void) {
    if (!test_space_mode_catalog_split()) {
        fprintf(stderr, "quality_profiles_contract_test: space-mode catalog split failed\n");
        return 1;
    }
    if (!test_3d_quality_presets_preserve_physical_coefficients()) {
        fprintf(stderr, "quality_profiles_contract_test: 3d fidelity-only preset contract failed\n");
        return 1;
    }
    if (!test_2d_quality_presets_preserve_physical_coefficients()) {
        fprintf(stderr, "quality_profiles_contract_test: 2d fidelity-only preset contract failed\n");
        return 1;
    }
    fprintf(stdout, "quality_profiles_contract_test: success\n");
    return 0;
}
