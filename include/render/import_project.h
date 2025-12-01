#ifndef IMPORT_PROJECT_H
#define IMPORT_PROJECT_H

#include <stdbool.h>
#include "app/scene_presets.h"
#include "geo/shape_asset.h"

// Projects an import asset point (already relative to asset center) to screen pixels and grid coords.
// The helper applies the same aspect-aware span that mask rasterization uses.
typedef struct ImportProjectParams {
    int grid_w;
    int grid_h;
    int window_w;
    int window_h;
    float span_x_cfg;
    float span_y_cfg;
    float pos_x;   // import position as stored (editor space)
    float pos_y;   // import position as stored (editor space)
    float rotation_deg;
    float scale;   // import scale multiplier
    const ShapeAssetBounds *bounds; // must be valid
} ImportProjectParams;

typedef struct ImportProjectPoint {
    float screen_x;
    float screen_y;
    float grid_x;
    float grid_y;
    bool  valid;
} ImportProjectPoint;

// delta_x/y are centered asset-space positions (normalized with desired_fit scaling outside).
ImportProjectPoint import_project_point(const ImportProjectParams *p, float delta_x, float delta_y);

// Convenience: compute span_x/span_y from configured window size (same as mask raster).
bool import_compute_span_from_window(int cfg_w, int cfg_h, float *out_span_x, float *out_span_y);

#endif // IMPORT_PROJECT_H
