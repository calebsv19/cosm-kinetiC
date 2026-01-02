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

static const float kCoverageMin = 0.65f;     // minimum area coverage to accept collider
static const float kDupEps2     = 1e-6f;     // reject near-duplicate points (squared)

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

    HullPoint outer_raw[256];
    int outer_count = loop_counts[best_idx];
    if (outer_count > 256) outer_count = 256;
    memcpy(outer_raw, loops[best_idx], (size_t)outer_count * sizeof(HullPoint));

    // Remove duplicates/zero-length edges while preserving order.
    HullPoint outer[256];
    int dedup_count = 0;
    for (int i = 0; i < outer_count; ++i) {
        HullPoint p = outer_raw[i];
        if (dedup_count > 0) {
            float dx = p.x - outer[dedup_count - 1].x;
            float dy = p.y - outer[dedup_count - 1].y;
            if ((dx * dx + dy * dy) < kDupEps2) continue;
        }
        outer[dedup_count++] = p;
    }
    // Ensure closure if last != first.
    if (dedup_count >= 3) {
        HullPoint first = outer[0];
        HullPoint last  = outer[dedup_count - 1];
        float dx = first.x - last.x;
        float dy = first.y - last.y;
        if ((dx * dx + dy * dy) >= kDupEps2 && dedup_count < 256) {
            outer[dedup_count++] = first;
        }
    }
    outer_count = dedup_count;

    // Try accepting the raw (clean) loop first.
    HullPoint cleaned[256];
    int cleaned_count = outer_count < 256 ? outer_count : 256;
    memcpy(cleaned, outer, (size_t)cleaned_count * sizeof(HullPoint));
    // Drop duplicate closing vertex if present.
    if (cleaned_count >= 2 && collider_nearly_equal(cleaned[0], cleaned[cleaned_count - 1], 1e-6f)) {
        cleaned_count--;
    }
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

    bool is_convex_raw = polygon_convex(cleaned, cleaned_count);
    // ------------------------------------------------------------
    // Convex fast-path: accept untouched if within vertex cap.
    if (is_convex_raw && cleaned_count <= part_vert_cap) {
        int vc = cleaned_count;
        if (vc > 128) vc = 128;
        imp->collider_part_offsets[0] = 0;
        imp->collider_part_counts[0] = vc;
        int vcur = 0;
        for (int i = 0; i < vc; ++i) {
            float wx = (cleaned[i].x / (float)(w - 1)) * ww;
            float wy = (cleaned[i].y / (float)(h - 1)) * wh;
            float cxw = (center_grid_x / (float)(w - 1)) * ww;
            float cyw = (center_grid_y / (float)(h - 1)) * wh;
            imp->collider_parts_verts[vcur++] = vec2(wx - cxw, wy - cyw);
        }
        imp->collider_part_count = 1;
        imp->collider_vert_count = (vc < 32) ? vc : 32;
        for (int i = 0; i < imp->collider_vert_count; ++i) {
            float wx = (cleaned[i].x / (float)(w - 1)) * ww;
            float wy = (cleaned[i].y / (float)(h - 1)) * wh;
            float cxw = (center_grid_x / (float)(w - 1)) * ww;
            float cyw = (center_grid_y / (float)(h - 1)) * wh;
            imp->collider_verts[i] = vec2(wx - cxw, wy - cyw);
        }
        if (cfg->collider_debug_logs) {
            fprintf(stderr, "[collider] convex passthrough verts=%d\n", cleaned_count);
        }
        return true;
    }

    // Do NOT simplify convex shapes. For concave, keep full boundary for decomposition.

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
    HullPoint part_polys[16][32];
    int part_counts[16] = {0};
    int part_count = 0;

    bool is_convex = polygon_convex(cleaned, cleaned_count);
    if (is_convex) {
        int n = cleaned_count;
        // Remove duplicate last if present for partitioning.
        if (n >= 2 && collider_nearly_equal(cleaned[0], cleaned[n - 1], 1e-6f)) n--;
        if (n <= part_vert_cap) {
            part_count = 1;
            part_counts[0] = n;
            for (int i = 0; i < n; ++i) part_polys[0][i] = cleaned[i];
        } else {
            // Split convex polygon into contiguous boundary strips (no radial fan)
            int chunk = part_vert_cap;
            if (chunk < 3) chunk = 3;
            int needed = (int)ceilf((float)n / (float)(chunk - 1)); // overlap one vertex to keep continuity
            if (needed > part_cap) {
                chunk = (int)ceilf((float)n / (float)part_cap) + 1;
                if (chunk < 3) chunk = 3;
                needed = (int)ceilf((float)n / (float)(chunk - 1));
            }
            if (needed > part_cap) needed = part_cap;
            part_count = 0;
            int start = 0;
            for (int p = 0; p < needed && start < n && part_count < part_cap; ++p) {
                int take = chunk;
                if (start + take > n) take = n - start;
                int count = take;
                if (part_count == needed - 1 && start + take < n) {
                    // final piece but vertices remain; extend to cover all
                    count = n - start;
                }
                if (count < 3) break;
                if (count > part_vert_cap) count = part_vert_cap;
                part_counts[part_count] = count;
                for (int k = 0; k < count; ++k) {
                    int idx = start + k;
                    if (idx >= n) idx = n - 1;
                    part_polys[part_count][k] = cleaned[idx];
                }
                part_count++;
                start += (chunk - 1); // overlap one vertex between strips
            }
        }
    } else {
        // Mild simplification for concave shapes to reduce oversampling on curves.
        HullPoint simp[256];
        int sc = collider_simplify_poly(cleaned, cleaned_count, simp, 256, 0.5f);
        if (sc >= 3) {
            memcpy(cleaned, simp, (size_t)sc * sizeof(HullPoint));
            cleaned_count = sc;
        }
        // Concave: try passes with increasingly strict diagonal deviation.
        const float scales[3] = {1.0f, 0.5f, 0.3f};
        bool accepted = false;
        float outer_area = fabsf(polygon_area(cleaned, cleaned_count));
        for (int pass = 0; pass < 3 && !accepted; ++pass) {
            part_count = collider_decompose_to_convex(cleaned,
                                                      cleaned_count,
                                                      part_polys,
                                                      part_counts,
                                                      part_cap,
                                                      part_vert_cap,
                                                      scales[pass]);
            if (part_count <= 0) continue;
            float emitted_area = 0.0f;
            for (int pi = 0; pi < part_count; ++pi) {
                int c = part_counts[pi];
                if (c < 3) continue;
                emitted_area += fabsf(polygon_area(part_polys[pi], c));
            }
            float coverage = (outer_area > 1e-5f) ? emitted_area / outer_area : 0.0f;
            if (cfg->collider_debug_logs) {
                fprintf(stderr, "[collider] pass=%d scale=%.2f parts=%d area=%.2f cover=%.2f\n",
                        pass, scales[pass], part_count, emitted_area, coverage);
            }
            if (coverage >= kCoverageMin) {
                int written_parts = 0;
                int vert_cursor = 0;
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
                }
                imp->collider_part_count = written_parts;
                if (cfg->collider_debug_logs) {
                    fprintf(stderr, "[collider] emitted=%d cover=%.2f (pass=%d)\n",
                            written_parts, coverage, pass);
                }
                if (written_parts > 0) return true;
            }
        }
        // Coverage failed after retries: hull fallback or reject.
        HullPoint hull[64];
        int hc = collider_compute_convex_hull(cleaned, cleaned_count, hull, 64);
        if (hc >= 3) {
            int vc = (hc > part_vert_cap) ? part_vert_cap : hc;
            if (vc > 128) vc = 128;
            imp->collider_part_offsets[0] = 0;
            imp->collider_part_counts[0] = vc;
            int vcur = 0;
            for (int i = 0; i < vc; ++i) {
                float wx = (hull[i].x / (float)(w - 1)) * ww;
                float wy = (hull[i].y / (float)(h - 1)) * wh;
                float cxw = (center_grid_x / (float)(w - 1)) * ww;
                float cyw = (center_grid_y / (float)(h - 1)) * wh;
                imp->collider_parts_verts[vcur++] = vec2(wx - cxw, wy - cyw);
            }
            imp->collider_part_count = 1;
            if (cfg->collider_debug_logs) {
                fprintf(stderr, "[collider] coverage retry failed -> hull fallback\n");
            }
            return true;
        }
        if (cfg->collider_debug_logs) {
            fprintf(stderr, "[collider] coverage retry failed and no hull; rejecting\n");
        }
        return false;
    }

    if (cfg->collider_debug_logs) {
        fprintf(stderr, "[collider] part_cap=%d vert_cap=%d part_count_raw=%d\n",
                part_cap, part_vert_cap, part_count);
    }

    // Validate and emit parts (convex or split-convex path)
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
    float outer_area = fabsf(polygon_area(cleaned, cleaned_count));
    float coverage = (outer_area > 1e-5f) ? emitted_area / outer_area : 0.0f;
    if (cfg->collider_debug_logs) {
        fprintf(stderr, "[collider] emitted=%d total_area=%.2f cover=%.2f\n",
                written_parts, emitted_area, coverage);
    }

    imp->collider_part_count = written_parts;
    return written_parts > 0;
}
