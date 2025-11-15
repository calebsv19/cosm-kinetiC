#ifndef QUALITY_PROFILES_H
#define QUALITY_PROFILES_H

#include "app/app_config.h"

typedef struct QualityProfileDef {
    const char *name;
    int grid_w;
    int grid_h;
    int solver_iterations;
    int physics_substeps;
    bool enable_blur;
} QualityProfileDef;

int  quality_profile_count(void);
const QualityProfileDef *quality_profile_get(int index);
const char *quality_profile_name(int index);
void quality_profile_apply(AppConfig *cfg, int index);

#endif // QUALITY_PROFILES_H
