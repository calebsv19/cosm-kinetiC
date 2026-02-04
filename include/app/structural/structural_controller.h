#ifndef STRUCTURAL_CONTROLLER_H
#define STRUCTURAL_CONTROLLER_H

#include "app/app_config.h"
#include "geo/shape_library.h"

int structural_controller_run(const AppConfig *cfg,
                              const ShapeAssetLibrary *shape_library,
                              const char *preset_path);

#endif // STRUCTURAL_CONTROLLER_H
