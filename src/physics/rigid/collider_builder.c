#include "physics/rigid/collider_builder.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "render/import_project.h"
#include "physics/math/math2d.h"
#include "app/shape_lookup.h"
#include "physics/rigid/collider_geom.h"
#include "physics/rigid/collider_utils.h"
#include "physics/rigid/collider_tagging.h"
#include "physics/rigid/collider_legacy.h"

static inline float import_pos_to_unit(float pos, float span) {
    float min = 0.5f - span;
    float max = 0.5f + span;
    float t = (pos - min) / (max - min);
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return t;
}

// Convex decomposition pipeline: normalize polygon, ear-clip to convex parts, emit to physics.
bool collider_build_import(const AppConfig *cfg,
                           const ShapeAssetLibrary *lib,
                           ImportedShape *imp) {
    if (!cfg || !lib || !imp) return false;
    if (cfg->collider_debug_logs) {
        fprintf(stderr, "[collider] build start path=%s\n",
                (imp->path[0] != '\0') ? imp->path : "(null)");
    }

    const ShapeAsset *asset = shape_lookup_from_path(lib, imp->path);
    if (!asset) return false;

    ShapeAssetBounds b;
    if (!shape_asset_bounds(asset, &b) || !b.valid) return false;

    const int w = cfg->grid_w;
    const int h = cfg->grid_h;
    if (w <= 1 || h <= 1) return false;

    // Config caps
    int loop_cap = collider_clamp_int(cfg->collider_max_loops, 1, 16);
    int loop_vert_cap = collider_clamp_int(cfg->collider_max_loop_vertices, 8, 256);
    int part_cap = collider_clamp_int(cfg->collider_max_parts, 1, 16);
    int part_vert_cap = collider_clamp_int(cfg->collider_max_part_vertices, 3, 32);
    const int pooled_part_cap = 128;
    if (part_vert_cap * part_cap > pooled_part_cap) {
        part_vert_cap = pooled_part_cap / part_cap;
        if (part_vert_cap < 3) part_vert_cap = 3;
    }

    const float simplify_eps = (cfg->collider_simplify_epsilon > 0.0f) ? cfg->collider_simplify_epsilon : 1.5f;

    float span_x_cfg = 1.0f, span_y_cfg = 1.0f;
    import_compute_span_from_window(cfg->window_w, cfg->window_h, &span_x_cfg, &span_y_cfg);

    float max_dim = fmaxf(b.max_x - b.min_x, b.max_y - b.min_y);
    if (max_dim <= 0.0001f) return false;
    const float desired_fit = 0.25f;
    float norm = (imp->scale * desired_fit) / max_dim;
    float cx = 0.5f * (b.min_x + b.max_x);
    float cy = 0.5f * (b.min_y + b.max_y);

    float center_grid_x = import_pos_to_unit(imp->position_x, span_x_cfg) * (float)(w - 1);
    float center_grid_y = import_pos_to_unit(imp->position_y, span_y_cfg) * (float)(h - 1);
    float ww = (float)(cfg->window_w > 0 ? cfg->window_w : w);
    float wh = (float)(cfg->window_h > 0 ? cfg->window_h : h);

    // ------------------------------------------------------------
    // 1) Project and normalize closed paths
    HullPoint loops[16][256];
    int loop_counts[16] = {0};
    int loop_total = 0;
    for (size_t pi = 0; pi < asset->path_count && loop_total < loop_cap; ++pi) {
        const ShapeAssetPath *path = &asset->paths[pi];
        if (!path || path->point_count < 2 || !path->closed) continue;
        ImportProjectParams proj = {
            .grid_w = w,
            .grid_h = h,
            .window_w = cfg->window_w,
            .window_h = cfg->window_h,
            .span_x_cfg = span_x_cfg,
            .span_y_cfg = span_y_cfg,
            .pos_x = imp->position_x,
            .pos_y = imp->position_y,
            .rotation_deg = imp->rotation_deg,
            .scale = imp->scale,
            .bounds = &b
        };
        // Directly project original path vertices to preserve corners; cap to loop_vert_cap.
        int lc = 0;
        int pc = (int)path->point_count;
        if (pc > 512) pc = 512;
        for (int k = 0; k < pc && lc < loop_vert_cap; ++k) {
            ShapeAssetPoint p = path->points[k];
            float dx = (p.x - cx) * norm;
            float dy = (p.y - cy) * norm;
            ImportProjectPoint pp = import_project_point(&proj, dx, dy);
            if (!pp.valid) continue;
            float gx = pp.screen_x / (float)proj.window_w * (float)(proj.grid_w - 1);
            float gy = pp.screen_y / (float)proj.window_h * (float)(proj.grid_h - 1);
            loops[loop_total][lc++] = (HullPoint){gx, gy};
        }
        if (lc >= 3) {
            HullPoint first = loops[loop_total][0];
            HullPoint last = loops[loop_total][lc - 1];
            if ((fabsf(first.x - last.x) > 1e-4f || fabsf(first.y - last.y) > 1e-4f) && lc < loop_vert_cap) {
                loops[loop_total][lc++] = first;
            }
        }
        if (lc >= 3) {
            loop_counts[loop_total] = lc;
            loop_total++;
        }
        if (cfg->collider_debug_logs) {
            fprintf(stderr, "[collider] path=%zu resampled=%d\n", pi, lc);
        }
    }
    if (loop_total <= 0) return false;

    // Pick largest area loop as the solid outline (simple polygon assumption).
    float best_area = 0.0f;
    int best_idx = 0;
    for (int i = 0; i < loop_total; ++i) {
        float a = fabsf(polygon_area(loops[i], loop_counts[i]));
        if (a > best_area) {
            best_area = a;
            best_idx = i;
        }
    }

    if (cfg->collider_debug_logs) {
        fprintf(stderr, "[collider] loops=%d best=%d area=%.3f\n", loop_total, best_idx, best_area);
    }

    HullPoint outer[256];
    int outer_count = loop_counts[best_idx];
    if (outer_count > 256) outer_count = 256;
    memcpy(outer, loops[best_idx], (size_t)outer_count * sizeof(HullPoint));

    // Normalize: collapse collinear, ensure CCW.
    HullPoint cleaned[256];
    int cleaned_count = outer_count < 256 ? outer_count : 256;
    memcpy(cleaned, outer, (size_t)cleaned_count * sizeof(HullPoint));
    if (cfg->collider_debug_logs) {
        fprintf(stderr, "[collider] before_collapse=%d after_collapse=%d\n", outer_count, cleaned_count);
        int dump = cleaned_count < 16 ? cleaned_count : 16;
        for (int i = 0; i < dump; ++i) {
            fprintf(stderr, "  v%d: %.2f %.2f\n", i, cleaned[i].x, cleaned[i].y);
        }
    }
    if (cleaned_count < 3) return false;
    float signed_area = polygon_area_signed(cleaned, cleaned_count);
    if (signed_area < 0.0f) {
        for (int i = 0; i < cleaned_count / 2; ++i) {
            HullPoint tmp = cleaned[i];
            cleaned[i] = cleaned[cleaned_count - 1 - i];
            cleaned[cleaned_count - 1 - i] = tmp;
        }
        signed_area = -signed_area;
    }
    if (signed_area < 1e-4f) return false;
    if (cfg->collider_debug_logs) {
        fprintf(stderr, "[collider] cleaned=%d area=%.3f convex=%d\n",
                cleaned_count, signed_area, polygon_convex(cleaned, cleaned_count) ? 1 : 0);
    }

    // Debug outline (non-physics): lightly simplify for render/debug overlay.
    HullPoint dbg_loop[32];
    int dbg_count = collider_simplify_poly(cleaned, cleaned_count, dbg_loop, 32, simplify_eps);
    if (dbg_count < 3) {
        dbg_count = cleaned_count < 32 ? cleaned_count : 32;
        memcpy(dbg_loop, cleaned, (size_t)dbg_count * sizeof(HullPoint));
    }
    imp->collider_vert_count = dbg_count;
    for (int i = 0; i < dbg_count; ++i) {
        float wx = (dbg_loop[i].x / (float)(w - 1)) * ww;
        float wy = (dbg_loop[i].y / (float)(h - 1)) * wh;
        float cxw = (center_grid_x / (float)(w - 1)) * ww;
        float cyw = (center_grid_y / (float)(h - 1)) * wh;
        imp->collider_verts[i] = vec2(wx - cxw, wy - cyw);
    }

    // ------------------------------------------------------------
    // 2) Convex decomposition (ear clip + merge)
    HullPoint part_polys[8][32];
    int part_counts[8] = {0};
    int part_count = 0;

    if (polygon_convex(cleaned, cleaned_count)) {
        part_count = 1;
        int vc = (cleaned_count > part_vert_cap) ? part_vert_cap : cleaned_count;
        part_counts[0] = vc;
        for (int i = 0; i < vc; ++i) part_polys[0][i] = cleaned[i];
    } else {
        part_count = collider_decompose_to_convex(cleaned,
                                                  cleaned_count,
                                                  part_polys,
                                                  part_counts,
                                                  part_cap,
                                                  part_vert_cap);
        if (part_count <= 0) {
            HullPoint hull[64];
            int hc = collider_compute_convex_hull(cleaned, cleaned_count, hull, 64);
            if (hc >= 3) {
                part_count = 1;
                part_counts[0] = (hc > part_vert_cap) ? part_vert_cap : hc;
                for (int i = 0; i < part_counts[0]; ++i) part_polys[0][i] = hull[i];
            }
        }
    }
    if (cfg->collider_debug_logs) {
        fprintf(stderr, "[collider] part_cap=%d vert_cap=%d part_count_raw=%d\n",
                part_cap, part_vert_cap, part_count);
    }

    // Validate and emit parts
    int written_parts = 0;
    int vert_cursor = 0;
    float emitted_area = 0.0f;
    for (int pi = 0; pi < part_count && written_parts < part_cap; ++pi) {
        int c = part_counts[pi];
        if (c < 3) continue;
        float area = fabsf(polygon_area(part_polys[pi], c));
        if (area < 1e-5f) continue;
        if (vert_cursor + c > 128) break;

        imp->collider_part_offsets[written_parts] = vert_cursor;
        imp->collider_part_counts[written_parts] = c;
        for (int v = 0; v < c; ++v) {
            float wx = (part_polys[pi][v].x / (float)(w - 1)) * ww;
            float wy = (part_polys[pi][v].y / (float)(h - 1)) * wh;
            float cxw = (center_grid_x / (float)(w - 1)) * ww;
            float cyw = (center_grid_y / (float)(h - 1)) * wh;
            imp->collider_parts_verts[vert_cursor++] = vec2(wx - cxw, wy - cyw);
        }
        if (cfg->collider_debug_logs) {
            fprintf(stderr, "[emit] part=%d verts=%d area=%.2f\n", written_parts, c, area);
        }
        written_parts++;
        emitted_area += area;
    }
    if (cfg->collider_debug_logs) {
        float outer_area = fabsf(polygon_area(cleaned, cleaned_count));
        fprintf(stderr, "[collider] emitted=%d total_area=%.2f cover=%.2f\n",
                written_parts, emitted_area, outer_area > 1e-5f ? emitted_area / outer_area : 0.0f);
    }

    imp->collider_part_count = written_parts;
    return written_parts > 0;
}
