#include "app/quality_profiles.h"

#include <stddef.h>

static const QualityProfileDef QUALITY_PROFILES_2D[] = {
    { "Preview", 96, 96, 12, 1, 0.00010f, 0.05f, 1.50f, 0.000006f, 1.00f, 1.00f, 1.00f, false },
    { "Balanced", 128, 128, 16, 2, 0.00010f, 0.05f, 1.50f, 0.000006f, 1.00f, 1.00f, 1.00f, true },
    { "High", 256, 256, 24, 3, 0.00010f, 0.05f, 1.50f, 0.000006f, 1.00f, 1.00f, 1.00f, true },
    { "Deep", 384, 384, 32, 4, 0.00010f, 0.05f, 1.50f, 0.000006f, 1.00f, 1.00f, 1.00f, true },
    { "Karman", 256, 256, 40, 3, 0.00010f, 0.05f, 1.50f, 0.000006f, 1.00f, 1.00f, 1.00f, false },
    { "Tiny3D", 64, 64, 8, 1, 0.00010f, 0.05f, 1.50f, 0.000006f, 1.00f, 1.00f, 1.00f, false }
};

static const QualityProfileDef QUALITY_PROFILES_3D[] = {
    { "Preview", 96, 96, 10, 1, 0.00008f, 0.06f, 1.20f, 0.000004f, 0.90f, 0.90f, 1.00f, false },
    { "Basic", 160, 160, 18, 2, 0.00010f, 0.05f, 1.40f, 0.000005f, 1.00f, 1.00f, 1.00f, true },
    { "High", 224, 224, 28, 3, 0.00012f, 0.04f, 1.60f, 0.000006f, 1.10f, 1.05f, 1.00f, true },
    { "Deep", 256, 256, 40, 4, 0.00015f, 0.03f, 1.80f, 0.000007f, 1.20f, 1.10f, 1.00f, true }
};

static int profile_count_for_catalog(QualityProfileCatalogId catalog) {
    switch (catalog) {
    case QUALITY_PROFILE_CATALOG_3D:
        return (int)(sizeof(QUALITY_PROFILES_3D) / sizeof(QUALITY_PROFILES_3D[0]));
    case QUALITY_PROFILE_CATALOG_2D:
    default:
        return (int)(sizeof(QUALITY_PROFILES_2D) / sizeof(QUALITY_PROFILES_2D[0]));
    }
}

QualityProfileCatalogId quality_profile_catalog_for_space_mode(SpaceMode mode) {
    return mode == SPACE_MODE_3D ? QUALITY_PROFILE_CATALOG_3D : QUALITY_PROFILE_CATALOG_2D;
}

int quality_profile_count_for_catalog(QualityProfileCatalogId catalog) {
    return profile_count_for_catalog(catalog);
}

const QualityProfileDef *quality_profile_get_for_catalog(QualityProfileCatalogId catalog,
                                                         int index) {
    if (index < 0 || index >= profile_count_for_catalog(catalog)) return NULL;
    switch (catalog) {
    case QUALITY_PROFILE_CATALOG_3D:
        return &QUALITY_PROFILES_3D[index];
    case QUALITY_PROFILE_CATALOG_2D:
    default:
        return &QUALITY_PROFILES_2D[index];
    }
}

const char *quality_profile_name_for_catalog(QualityProfileCatalogId catalog, int index) {
    if (index < 0) return "Custom";
    const QualityProfileDef *profile = quality_profile_get_for_catalog(catalog, index);
    return profile ? profile->name : "Custom";
}

void quality_profile_apply_for_catalog(AppConfig *cfg,
                                       QualityProfileCatalogId catalog,
                                       int index) {
    if (!cfg) return;
    const QualityProfileDef *profile = quality_profile_get_for_catalog(catalog, index);
    if (!profile) {
        cfg->quality_index = -1;
        return;
    }
    cfg->grid_w = profile->grid_w;
    cfg->grid_h = profile->grid_h;
    cfg->fluid_solver_iterations = profile->solver_iterations;
    cfg->physics_substeps = profile->physics_substeps;
    cfg->density_diffusion = profile->density_diffusion;
    cfg->density_decay = profile->density_decay;
    cfg->fluid_buoyancy_force = profile->fluid_buoyancy_force;
    cfg->velocity_damping = profile->velocity_damping;
    cfg->emitter_density_multiplier = profile->emitter_density_multiplier;
    cfg->emitter_velocity_multiplier = profile->emitter_velocity_multiplier;
    cfg->emitter_sink_multiplier = profile->emitter_sink_multiplier;
    cfg->enable_render_blur = profile->enable_blur;
    cfg->quality_index = index;
}

int quality_profile_count_for_space_mode(SpaceMode mode) {
    return quality_profile_count_for_catalog(quality_profile_catalog_for_space_mode(mode));
}

const QualityProfileDef *quality_profile_get_for_space_mode(SpaceMode mode, int index) {
    return quality_profile_get_for_catalog(quality_profile_catalog_for_space_mode(mode), index);
}

const char *quality_profile_name_for_space_mode(SpaceMode mode, int index) {
    return quality_profile_name_for_catalog(quality_profile_catalog_for_space_mode(mode), index);
}

void quality_profile_apply_for_space_mode(AppConfig *cfg, SpaceMode mode, int index) {
    quality_profile_apply_for_catalog(cfg, quality_profile_catalog_for_space_mode(mode), index);
}

int quality_profile_count(void) {
    return quality_profile_count_for_catalog(QUALITY_PROFILE_CATALOG_2D);
}

const QualityProfileDef *quality_profile_get(int index) {
    return quality_profile_get_for_catalog(QUALITY_PROFILE_CATALOG_2D, index);
}

const char *quality_profile_name(int index) {
    return quality_profile_name_for_catalog(QUALITY_PROFILE_CATALOG_2D, index);
}

void quality_profile_apply(AppConfig *cfg, int index) {
    quality_profile_apply_for_catalog(cfg, QUALITY_PROFILE_CATALOG_2D, index);
}
