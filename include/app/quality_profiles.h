#ifndef QUALITY_PROFILES_H
#define QUALITY_PROFILES_H

#include "app/app_config.h"

typedef enum QualityProfileCatalogId {
    QUALITY_PROFILE_CATALOG_2D = 0,
    QUALITY_PROFILE_CATALOG_3D
} QualityProfileCatalogId;

typedef struct QualityProfileDef {
    const char *name;
    int grid_w;
    int grid_h;
    int solver_iterations;
    int physics_substeps;
    bool enable_blur;
} QualityProfileDef;

QualityProfileCatalogId quality_profile_catalog_for_space_mode(SpaceMode mode);
int  quality_profile_count_for_catalog(QualityProfileCatalogId catalog);
const QualityProfileDef *quality_profile_get_for_catalog(QualityProfileCatalogId catalog,
                                                         int index);
const char *quality_profile_name_for_catalog(QualityProfileCatalogId catalog, int index);
void quality_profile_apply_for_catalog(AppConfig *cfg,
                                       QualityProfileCatalogId catalog,
                                       int index);
int  quality_profile_count_for_space_mode(SpaceMode mode);
const QualityProfileDef *quality_profile_get_for_space_mode(SpaceMode mode, int index);
const char *quality_profile_name_for_space_mode(SpaceMode mode, int index);
void quality_profile_apply_for_space_mode(AppConfig *cfg, SpaceMode mode, int index);

int  quality_profile_count(void);
const QualityProfileDef *quality_profile_get(int index);
const char *quality_profile_name(int index);
void quality_profile_apply(AppConfig *cfg, int index);

#endif // QUALITY_PROFILES_H
