#include "render/import_project.h"

#include <math.h>

bool import_compute_span_from_window(int cfg_w, int cfg_h, float *out_span_x, float *out_span_y) {
    if (!out_span_x || !out_span_y) return false;
    if (cfg_w <= 0 || cfg_h <= 0) return false;
    float min_dim = (float)((cfg_w < cfg_h) ? cfg_w : cfg_h);
    if (min_dim <= 0.0f) min_dim = 1.0f;
    *out_span_x = 0.5f * ((float)cfg_w / min_dim);
    *out_span_y = 0.5f * ((float)cfg_h / min_dim);
    return true;
}

ImportProjectPoint import_project_point(const ImportProjectParams *p, float delta_x, float delta_y) {
    ImportProjectPoint out = {0};
    if (!p || !p->bounds || !p->bounds->valid || p->window_w <= 0 || p->window_h <= 0) {
        return out;
    }
    float span_x = p->span_x_cfg;
    float span_y = p->span_y_cfg;
    if (span_x <= 0.0f) span_x = 1.0f;
    if (span_y <= 0.0f) span_y = 1.0f;

    // Normalize stored position into square space.
    float min_x = 0.5f - span_x;
    float max_x = 0.5f + span_x;
    float min_y = 0.5f - span_y;
    float max_y = 0.5f + span_y;
    float pos_x_sq = (max_x > min_x) ? ((p->pos_x - min_x) / (max_x - min_x)) : p->pos_x;
    float pos_y_sq = (max_y > min_y) ? ((p->pos_y - min_y) / (max_y - min_y)) : p->pos_y;
    if (pos_x_sq < 0.0f) pos_x_sq = 0.0f;
    if (pos_x_sq > 1.0f) pos_x_sq = 1.0f;
    if (pos_y_sq < 0.0f) pos_y_sq = 0.0f;
    if (pos_y_sq > 1.0f) pos_y_sq = 1.0f;

    // Apply rotation/scale to the delta.
    float cos_a = cosf(p->rotation_deg * (float)M_PI / 180.0f);
    float sin_a = sinf(p->rotation_deg * (float)M_PI / 180.0f);
    float rx = delta_x * cos_a - delta_y * sin_a;
    float ry = delta_x * sin_a + delta_y * cos_a;
    float ra_x = rx + pos_x_sq;
    float ra_y = ry + pos_y_sq;

    // Screen projection uses min-dim to preserve aspect (same as editor visual).
    float screen_min_dim = (float)((p->window_w < p->window_h) ? p->window_w : p->window_h);
    if (screen_min_dim <= 0.0f) screen_min_dim = 1.0f;
    float pos_px_x = pos_x_sq * (float)p->window_w;
    float pos_px_y = pos_y_sq * (float)p->window_h;
    float delta_px_x = (ra_x - pos_x_sq) * screen_min_dim;
    float delta_px_y = (ra_y - pos_y_sq) * screen_min_dim;
    out.screen_x = pos_px_x + delta_px_x;
    out.screen_y = pos_px_y + delta_px_y;

    // Grid projection: map square space back to configured spans, then to grid cells.
    float world_x = ra_x * (max_x - min_x) + min_x; // still normalized 0..1 in editor square
    float world_y = ra_y * (max_y - min_y) + min_y;
    out.grid_x = world_x * (float)(p->grid_w > 0 ? p->grid_w : 1);
    out.grid_y = world_y * (float)(p->grid_h > 0 ? p->grid_h : 1);
    out.valid = true;
    return out;
}
