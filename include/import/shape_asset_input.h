#ifndef PHYSICS_SIM_SHAPE_ASSET_INPUT_H
#define PHYSICS_SIM_SHAPE_ASSET_INPUT_H
#include "geo/shape_asset.h"
#define PHYSICS_SIM_ASSET_MAX_BYTES (16u * 1024u * 1024u)
#define PHYSICS_SIM_ASSET_MAX_PATHS 1024u
#define PHYSICS_SIM_ASSET_MAX_POINTS 10000u
#define PHYSICS_SIM_ASSET_MAX_NAME_BYTES 1048576u
/* Shared host producer/consumer contract; neither function performs file I/O. */
bool physics_sim_shape_asset_admitted(const ShapeAsset *asset);
bool physics_sim_shape_asset_text_admitted(const char *text, size_t size);
/* Trusted-local strict bounded input; caller supplies empty output storage. */
bool physics_sim_shape_asset_load(const char *path, ShapeAsset *out_asset);
#endif
