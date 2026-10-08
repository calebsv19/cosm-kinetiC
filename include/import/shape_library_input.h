#ifndef PHYSICS_SIM_SHAPE_LIBRARY_INPUT_H
#define PHYSICS_SIM_SHAPE_LIBRARY_INPUT_H
#include "geo/shape_library.h"
/* Empty output required. False leaves it empty; selected failures never partially publish. */
bool physics_sim_shape_library_load(const char *path, ShapeAssetLibrary *out_lib);
#endif
