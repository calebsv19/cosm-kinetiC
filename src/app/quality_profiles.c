#include "app/quality_profiles.h"

#include <stddef.h>

static const QualityProfileDef QUALITY_PROFILES[] = {
    { "Preview", 96, 96, 12, 1, false },
    { "Balanced", 128, 128, 16, 2, true },
    { "High", 256, 256, 24, 3, true },
    { "Deep", 384, 384, 32, 4, true }
};

int quality_profile_count(void) {
    return (int)(sizeof(QUALITY_PROFILES) / sizeof(QUALITY_PROFILES[0]));
}

const QualityProfileDef *quality_profile_get(int index) {
    if (index < 0 || index >= quality_profile_count()) return NULL;
    return &QUALITY_PROFILES[index];
}

const char *quality_profile_name(int index) {
    if (index < 0) return "Custom";
    const QualityProfileDef *profile = quality_profile_get(index);
    return profile ? profile->name : "Custom";
}

void quality_profile_apply(AppConfig *cfg, int index) {
    if (!cfg) return;
    const QualityProfileDef *profile = quality_profile_get(index);
    if (!profile) {
        cfg->quality_index = -1;
        return;
    }
    cfg->grid_w = profile->grid_w;
    cfg->grid_h = profile->grid_h;
    cfg->fluid_solver_iterations = profile->solver_iterations;
    cfg->physics_substeps = profile->physics_substeps;
    cfg->enable_render_blur = profile->enable_blur;
    cfg->quality_index = index;
}
