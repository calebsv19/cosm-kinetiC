#include "physics/rigid/collider_builder.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "render/import_project.h"
#include "physics/math/math2d.h"
#include "app/shape_lookup.h"
#include "physics/rigid/collider_types.h"
#include "physics/rigid/collider_geom.h"
#include "physics/rigid/collider_utils.h"
#include "physics/rigid/collider_classify.h"
#include "physics/rigid/collider_prim_geom.h"
#include "physics/rigid/collider_tagging.h"
#include "physics/rigid/collider_primitives.h"
#include "physics/rigid/collider_legacy.h"
#include "physics/rigid/collider_debug.h"
#include "physics/rigid/collider_partition.h"

static inline float import_pos_to_unit(float pos, float span) {
    float min = 0.5f - span;
    float max = 0.5f + span;
    float t = (pos - min) / (max - min);
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return t;
}

// Lightweight wrapper to test if a point lies in a solid region (inside mask and not void).
static bool region_contains_point(const bool inside[128][128],
                                  const bool void_mask[128][128],
                                  int res,
                                  float minx,
                                  float spanx,
                                  float miny,
                                  float spany,
                                  float gx,
                                  float gy) {
    return region_contains_point_mask(inside, void_mask, res, minx, spanx, miny, spany, gx, gy);
}

// Classify edges into segments of similar curvature/aspect.
// Stub: tag all closed paths for the upcoming primitive fitter.
static void collider_tag_asset_paths(const ShapeAsset *asset,
                                     const AppConfig *cfg) {
    if (!asset || !cfg || !cfg->collider_primitives_enabled) return;
    const float thresh = (cfg->collider_corner_angle_deg > 0.0f)
                             ? cfg->collider_corner_angle_deg
                             : 22.5f;
    for (size_t pi = 0; pi < asset->path_count; ++pi) {
        const ShapeAssetPath *path = &asset->paths[pi];
        if (!path || !path->closed || path->point_count < 3) continue;
        int corner_count = 0;
        HullPoint simplified[512];
        int count = collider_simplify_loop_preserve_corners(path,
                                                            thresh,
                                                            cfg->collider_corner_simplify_eps > 0.0f
                                                                ? cfg->collider_corner_simplify_eps
                                                                : 0.75f,
                                                            simplified,
                                                            512,
                                                            &corner_count);
        bool corner_flags[512] = {0};
        bool concave_flags[512] = {0};
        TaggedPoint tagged[512];
        int tagged_corners = 0;
        int tc = collider_tag_closed_points(simplified,
                                            count,
                                            thresh,
                                            tagged,
                                            512,
                                            &tagged_corners);
        int usable = tc;
        if (tc >= 2 && collider_nearly_equal((HullPoint){tagged[0].pos.x, tagged[0].pos.y},
                                             (HullPoint){tagged[tc - 1].pos.x, tagged[tc - 1].pos.y},
                                             1e-4f)) usable = tc - 1;
        for (int i = 0; i < usable; ++i) {
            if (tagged[i].is_corner) corner_flags[i] = true;
            if (tagged[i].is_concave) concave_flags[i] = true;
        }
        ColliderSegment segments[256];
        int seg_count = classify_segments(simplified, count, corner_flags, concave_flags, segments, 256, false);
        // Debug/logging hook: corner and segment counts help validate tagging; keep quiet otherwise.
        ColliderPrimitive prims[64];
        int prim_count = collider_fit_primitives(simplified,
                                                 count,
                                                 segments,
                                                 seg_count,
                                                 cfg,
                                                 prims,
                                                 cfg->collider_max_primitives < 64 ? cfg->collider_max_primitives : 64,
                                                 NULL, NULL, 0, 0.0f, 0.0f, 0.0f, 0.0f);
        if (count > 0 && corner_count > 0) {
            fprintf(stderr,
                    "[collider] tag path=%zu verts=%d corners=%d segments=%d prims=%d thresh=%.1f eps=%.2f\n",
                    pi, count, corner_count, seg_count, prim_count, thresh, cfg->collider_corner_simplify_eps);
        }
        // For now we discard the primitives; future steps will reuse them.
    }
}

// Simple monotonic-chain convex hull; returns CCW hull.
bool collider_build_import(const AppConfig *cfg,
                           const ShapeAssetLibrary *lib,
                           ImportedShape *imp) {
    if (!cfg || !lib || !imp) return false;
    if (cfg->collider_debug_logs) {
        fprintf(stderr,
                "[collider] build start path=%s prim=%d logs=%d\n",
                (imp->path[0] != '\0') ? imp->path : "(null)",
                cfg->collider_primitives_enabled ? 1 : 0,
                cfg->collider_debug_logs ? 1 : 0);
    }
    const ShapeAsset *asset = shape_lookup_from_path(lib, imp->path);
    if (!asset) return false;
    ShapeAssetBounds b;
    if (!shape_asset_bounds(asset, &b) || !b.valid) return false;

    // Stub for the upcoming primitive pipeline: tag vertices when enabled.
    if (cfg->collider_primitives_enabled) {
        collider_tag_asset_paths(asset, cfg);
        // For now, fall through to the legacy path.
    }

    int w = cfg->grid_w;
    int h = cfg->grid_h;
    if (w <= 1 || h <= 1) return false;

    // Config caps
    int loop_cap = collider_clamp_int(cfg->collider_max_loops, 1, 16);
    int loop_vert_cap = collider_clamp_int(cfg->collider_max_loop_vertices, 8, 256);
    int part_cap = collider_clamp_int(cfg->collider_max_parts, 1, 8);
    int part_vert_cap = collider_clamp_int(cfg->collider_max_part_vertices, 3, 32);
    const int pooled_part_cap = 64;
    if (part_vert_cap * part_cap > pooled_part_cap) {
        part_vert_cap = pooled_part_cap / part_cap;
        if (part_vert_cap < 3) part_vert_cap = 3;
    }
    float simplify_eps = (cfg->collider_simplify_epsilon > 0.0f) ? cfg->collider_simplify_epsilon : 1.5f;
    float samples_per_100 = (cfg->collider_curve_sample_rate > 0.0f) ? cfg->collider_curve_sample_rate : 24.0f;

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

    if (cfg->collider_primitives_enabled) {
        // Project and simplify each closed path, fit primitives, then assemble collider parts.
    const float corner_thresh = (cfg->collider_corner_angle_deg > 0.0f)
                                        ? cfg->collider_corner_angle_deg
                                        : 22.5f;
    const float corner_eps = (cfg->collider_corner_simplify_eps > 0.0f)
                                     ? cfg->collider_corner_simplify_eps
                                     : 0.75f;
    const float boundary_offset = (cfg->collider_region_offset_eps > 0.0f)
                                      ? cfg->collider_region_offset_eps
                                      : 1.5f;

        HullPoint debug_loop[64];
        int debug_loop_count = 0;

        for (size_t pi = 0; pi < asset->path_count; ++pi) {
            const ShapeAssetPath *path = &asset->paths[pi];
            if (!path || !path->closed || path->point_count < 3) continue;
            HullPoint simplified_asset[512];
            int corner_count = 0;
            int simp_count = collider_simplify_loop_preserve_corners(path,
                                                                     corner_thresh,
                                                                     corner_eps,
                                                                     simplified_asset,
                                                                     512,
                                                                     &corner_count);
            if (simp_count < 3) continue;

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
            HullPoint simplified_grid[512];
            int simp_grid_count = collider_project_simplified_to_grid(simplified_asset,
                                                                      simp_count,
                                                                      cx,
                                                                      cy,
                                                                      norm,
                                                                      &proj,
                                                                      simplified_grid,
                                                                      512);
            if (simp_grid_count < 3) continue;
            float signed_area = polygon_area_signed(simplified_grid, simp_grid_count);
            if (fabsf(signed_area) < 1e-5f) continue;
            if (signed_area < 0.0f) {
                // Treat negative-winding loops as holes; skip to avoid filling cavities.
                continue;
            }
            bool loop_convex = polygon_convex(simplified_grid, simp_grid_count);

            // Regions (single solid for now)
            ColliderRegion regions_arr[4];
            int region_count = collider_build_regions(simplified_grid, simp_grid_count, regions_arr, 4);
            if (cfg->collider_debug_logs) {
                fprintf(stderr, "[region] count=%d\n", region_count);
                for (int ri = 0; ri < region_count; ++ri) {
                    fprintf(stderr, "[region] %d: boundary=%d is_solid=%d\n",
                            ri, regions_arr[ri].boundary_count, regions_arr[ri].is_solid ? 1 : 0);
                }
            }

            // Fast path: convex outline -> single hull, no overlapping per-edge prims.
            if (loop_convex) {
                HullPoint hull[64];
                int hull_count = collider_compute_convex_hull(simplified_grid, simp_grid_count, hull, 64);
                if (hull_count >= 3) {
                    int vc = hull_count;
                    int max_v = cfg->collider_max_part_vertices > 0 ? cfg->collider_max_part_vertices : 32;
                    if (vc > max_v) vc = max_v;
                    if (vc > 64) vc = 64;
                    int vert_cursor = 0;
                    imp->collider_part_offsets[0] = 0;
                    imp->collider_part_counts[0] = vc;
                    for (int v = 0; v < vc; ++v) {
                        float wx = (hull[v].x / (float)(w - 1)) * ww;
                        float wy = (hull[v].y / (float)(h - 1)) * wh;
                        float cxw = (center_grid_x / (float)(w - 1)) * ww;
                        float cyw = (center_grid_y / (float)(h - 1)) * wh;
                        imp->collider_parts_verts[vert_cursor++] = vec2(wx - cxw, wy - cyw);
                    }
                    imp->collider_part_count = 1;

                    // Debug outline: reuse simplified loop (clamped)
                    int dbg_count = (simp_grid_count < 32) ? simp_grid_count : 32;
                    imp->collider_vert_count = dbg_count;
                    for (int i = 0; i < dbg_count; ++i) {
                        float wx = (simplified_grid[i].x / (float)(w - 1)) * ww;
                        float wy = (simplified_grid[i].y / (float)(h - 1)) * wh;
                        float cxw = (center_grid_x / (float)(w - 1)) * ww;
                        float cyw = (center_grid_y / (float)(h - 1)) * wh;
                        imp->collider_verts[i] = vec2(wx - cxw, wy - cyw);
                    }
                    if (cfg->collider_debug_logs) {
                        fprintf(stderr, "[collider] convex fastpath hull verts=%d parts=1\n", vc);
                    }
                    return true;
                }
            }

            // --- Region detection (coarse grid flood fill) ---
            int region_res = cfg->collider_region_grid_res > 0 ? cfg->collider_region_grid_res : 64;
            if (region_res < 8) region_res = 8;
            if (region_res > 128) region_res = 128;
            bool inside_mask[128][128];
            bool visited[128][128];
            bool void_mask[128][128];
            memset(inside_mask, 0, sizeof(inside_mask));
            memset(visited, 0, sizeof(visited));
            memset(void_mask, 0, sizeof(void_mask));

            float minx = simplified_grid[0].x, maxx = simplified_grid[0].x;
            float miny = simplified_grid[0].y, maxy = simplified_grid[0].y;
            for (int i = 1; i < simp_grid_count; ++i) {
                if (simplified_grid[i].x < minx) minx = simplified_grid[i].x;
                if (simplified_grid[i].x > maxx) maxx = simplified_grid[i].x;
                if (simplified_grid[i].y < miny) miny = simplified_grid[i].y;
                if (simplified_grid[i].y > maxy) maxy = simplified_grid[i].y;
            }
            float spanx = fmaxf(maxx - minx, 1e-3f);
            float spany = fmaxf(maxy - miny, 1e-3f);

            for (int iy = 0; iy < region_res; ++iy) {
                for (int ix = 0; ix < region_res; ++ix) {
                    float gx = minx + (ix + 0.5f) / (float)region_res * spanx;
                    float gy = miny + (iy + 0.5f) / (float)region_res * spany;
                    inside_mask[iy][ix] = point_in_polygon(simplified_grid, simp_grid_count, (HullPoint){gx, gy});
                }
            }

            // Flood fill outside to mark void.
            int qx[region_res * region_res];
            int qy[region_res * region_res];
            int qh = 0, qt = 0;
            for (int i = 0; i < region_res; ++i) {
                int edges[4][2] = {{0, i}, {region_res - 1, i}, {i, 0}, {i, region_res - 1}};
                for (int e = 0; e < 4; ++e) {
                    int ex = edges[e][0], ey = edges[e][1];
                    if (!visited[ey][ex] && !inside_mask[ey][ex]) {
                        visited[ey][ex] = true;
                        qx[qt] = ex; qy[qt] = ey; qt++;
                    }
                }
            }
            const int dx4[4] = {1, -1, 0, 0};
            const int dy4[4] = {0, 0, 1, -1};
            while (qh < qt) {
                int cxm = qx[qh];
                int cym = qy[qh];
                qh++;
                for (int k = 0; k < 4; ++k) {
                    int nx = cxm + dx4[k];
                    int ny = cym + dy4[k];
                    if (nx < 0 || nx >= region_res || ny < 0 || ny >= region_res) continue;
                    if (visited[ny][nx]) continue;
                    if (inside_mask[ny][nx]) continue;
                    visited[ny][nx] = true;
                    qx[qt] = nx; qy[qt] = ny; qt++;
                }
            }

            // Preserve void mask and reset visited for solid extraction.
            memcpy(void_mask, visited, sizeof(void_mask));
            memset(visited, 0, sizeof(visited));

            // Extract solid regions (connected components of inside_mask not void).
            typedef struct {
                int cells;
                int minx, miny, maxx, maxy;
                float cx, cy;
            } RegionInfo;
            RegionInfo regions[16];
            int region_mask_count = 0;

            for (int iy = 0; iy < region_res; ++iy) {
                for (int ix = 0; ix < region_res; ++ix) {
                    if (!inside_mask[iy][ix] || visited[iy][ix] || void_mask[iy][ix]) continue;
                    if (region_mask_count >= 16) break;
                    RegionInfo reg = {.cells = 0, .minx = ix, .maxx = ix, .miny = iy, .maxy = iy, .cx = 0.0f, .cy = 0.0f};
                    qh = qt = 0;
                    qx[qt] = ix; qy[qt] = iy; qt++;
                    visited[iy][ix] = true;
                    while (qh < qt) {
                        int cxm = qx[qh];
                        int cym = qy[qh];
                        qh++;
                        reg.cells++;
                        if (cxm < reg.minx) reg.minx = cxm;
                        if (cxm > reg.maxx) reg.maxx = cxm;
                        if (cym < reg.miny) reg.miny = cym;
                        if (cym > reg.maxy) reg.maxy = cym;
                        reg.cx += cxm;
                        reg.cy += cym;
                        for (int k = 0; k < 4; ++k) {
                            int nx = cxm + dx4[k];
                            int ny = cym + dy4[k];
                            if (nx < 0 || nx >= region_res || ny < 0 || ny >= region_res) continue;
                            if (visited[ny][nx]) continue;
                            if (!inside_mask[ny][nx] || void_mask[ny][nx]) continue;
                            visited[ny][nx] = true;
                            qx[qt] = nx; qy[qt] = ny; qt++;
                        }
                    }
                    if (reg.cells > 0) {
                        reg.cx /= (float)reg.cells;
                        reg.cy /= (float)reg.cells;
                    }
                    regions[region_mask_count++] = reg;
                }
                if (region_mask_count >= 16) break;
            }

            int solid_regions = 0;
            for (int r = 0; r < region_count; ++r) {
                if (regions[r].cells >= 4) solid_regions++;
            }
            if (cfg->collider_debug_logs) {
                fprintf(stderr, "[collider] path=%zu regions=solid:%d void:%d\n",
                        pi, solid_regions, 1); // void count not tracked separately here
            }

            // Derive feature spans between corners and fit per-span primitives.
            TaggedPoint corner_pts[512];
            int corner_count_grid = 0;
            int tagged_count = collider_tag_closed_points(simplified_grid,
                                                          simp_grid_count,
                                                          corner_thresh,
                                                          corner_pts,
                                                          512,
                                                          &corner_count_grid);
            int corner_indices[512];
            int corner_total = 0;
            int concave_count = 0;
            bool corner_flags[512] = {0};
            bool concave_flags[512] = {0};
            int usable_count = tagged_count;
            if (tagged_count >= 2 && collider_nearly_equal((HullPoint){corner_pts[0].pos.x, corner_pts[0].pos.y},
                                                           (HullPoint){corner_pts[tagged_count - 1].pos.x, corner_pts[tagged_count - 1].pos.y},
                                                           1e-4f)) {
                usable_count = tagged_count - 1;
            }
            for (int ci = 0; ci < usable_count && corner_total < 512; ++ci) {
                if (corner_pts[ci].is_corner) {
                    corner_indices[corner_total++] = ci;
                    corner_flags[ci] = true;
                    if (corner_pts[ci].is_concave) concave_count++;
                }
                if (corner_pts[ci].is_concave) concave_flags[ci] = true;
            }
            if (corner_total == 0) {
                corner_indices[corner_total++] = 0;
                corner_flags[0] = true;
            }
            if (cfg->collider_debug_logs) {
                fprintf(stderr, "[tag] loop verts=%d concave=%d convex=%d\n",
                        simp_grid_count, concave_count, corner_total - concave_count);
                for (int ci = 0; ci < usable_count; ++ci) {
                    if (corner_pts[ci].is_corner) {
                        fprintf(stderr, "[tag] i=%d angle=%.2f concave=%d\n",
                                ci, corner_pts[ci].angle_deg, corner_pts[ci].is_concave ? 1 : 0);
                    }
                }
            }

            ColliderSegment segments[256];
            int seg_count = classify_segments(simplified_grid,
                                              simp_grid_count,
                                              corner_flags,
                                              concave_flags,
                                              segments,
                                              256,
                                              cfg->collider_debug_logs);

            // Mark spans that face outward only; avoid sealing voids.
            float area_sign = (signed_area > 0.0f) ? 1.0f : -1.0f; // CCW positive
            for (int si = 0; si < seg_count; ++si) {
                ColliderSegment *seg = &segments[si];
                if (loop_convex) {
                    seg->solid_facing = true;
                    continue;
                }
                if (seg->start_idx < 0 || seg->end_idx < 0 || seg->start_idx >= simp_grid_count || seg->end_idx >= simp_grid_count) {
                    seg->solid_facing = false;
                    continue;
                }
                HullPoint a = simplified_grid[seg->start_idx];
                HullPoint b = simplified_grid[seg->end_idx];
                float dx = b.x - a.x;
                float dy = b.y - a.y;
                float len = sqrtf(dx * dx + dy * dy);
                if (len < 1e-5f) {
                    seg->solid_facing = false;
                    continue;
                }
                float nx = (area_sign > 0.0f) ? dy : -dy;
                float ny = (area_sign > 0.0f) ? -dx : dx;
                float inv = 1.0f / len;
                nx *= inv;
                ny *= inv;
                float mx = 0.5f * (a.x + b.x);
                float my = 0.5f * (a.y + b.y);
                // Two-offset check to avoid over-rejecting due to coarse masks.
                HullPoint test1 = {mx + nx * boundary_offset, my + ny * boundary_offset};
                HullPoint test2 = {mx + nx * boundary_offset * 2.0f, my + ny * boundary_offset * 2.0f};
                bool out1 = !point_in_polygon(simplified_grid, simp_grid_count, test1);
                bool out2 = !point_in_polygon(simplified_grid, simp_grid_count, test2);
                seg->solid_facing = out1 || out2;
            }
            // Build colliders per solid region
            int total_parts = 0;
            int vert_cursor = 0;

            // Partition path into trapezoids for concave cases; emit directly if succeeds.
            if (!loop_convex) {
                HullPoint part_polys[32][8];
                int part_counts_local[32];
                int part_count_local = collider_partition_trapezoids(simplified_grid,
                                                                     simp_grid_count,
                                                                     concave_flags,
                                                                     part_polys,
                                                                     part_counts_local,
                                                                     32,
                                                                     cfg->collider_debug_logs);
                if (cfg->collider_debug_logs) {
                    fprintf(stderr, "[partition] region=0 parts=%d\n", part_count_local);
                }
                for (int pi2 = 0; pi2 < part_count_local && total_parts < cfg->collider_max_primitives; ++pi2) {
                    int c = part_counts_local[pi2];
                    if (c < 3) continue;
                    if (c > cfg->collider_max_part_vertices) c = cfg->collider_max_part_vertices;
                    if (vert_cursor + c > 64) break;
                    imp->collider_part_offsets[total_parts] = vert_cursor;
                    imp->collider_part_counts[total_parts] = c;
                    for (int v = 0; v < c; ++v) {
                        float wx = (part_polys[pi2][v].x / (float)(w - 1)) * ww;
                        float wy = (part_polys[pi2][v].y / (float)(h - 1)) * wh;
                        float cxw = (center_grid_x / (float)(w - 1)) * ww;
                        float cyw = (center_grid_y / (float)(h - 1)) * wh;
                        imp->collider_parts_verts[vert_cursor++] = vec2(wx - cxw, wy - cyw);
                    }
                    if (cfg->collider_debug_logs) {
                        float area = polygon_area(part_polys[pi2], c);
                        fprintf(stderr, "[emit] part=%d verts=%d area=%.2f\n", total_parts, c, area);
                    }
                    total_parts++;
                }
                if (total_parts > 0) {
                    imp->collider_part_count = total_parts;
                    // Debug outline: keep simplified loop.
                    int dbg_count = (simp_grid_count < 32) ? simp_grid_count : 32;
                    imp->collider_vert_count = dbg_count;
                    for (int i = 0; i < dbg_count; ++i) {
                        float wx = (simplified_grid[i].x / (float)(w - 1)) * ww;
                        float wy = (simplified_grid[i].y / (float)(h - 1)) * wh;
                        float cxw = (center_grid_x / (float)(w - 1)) * ww;
                        float cyw = (center_grid_y / (float)(h - 1)) * wh;
                        imp->collider_verts[i] = vec2(wx - cxw, wy - cyw);
                    }
                    return true;
                }
            }

            for (int r = 0; r < region_count && total_parts < cfg->collider_max_primitives; ++r) {
                if (regions[r].cells < 4) continue;

                ColliderSegment region_segs[256];
                int region_seg_count = 0;
                int region_seg_reject_out = 0;
                int region_seg_reject_face = 0;
                if (loop_convex) {
                    // For convex shapes, keep all segments and skip region-facing culling.
                    for (int si = 0; si < seg_count && region_seg_count < 256; ++si) {
                        region_segs[region_seg_count++] = segments[si];
                        region_segs[region_seg_count - 1].solid_facing = true;
                    }
                } else {
                    for (int si = 0; si < seg_count && region_seg_count < 256; ++si) {
                        const ColliderSegment *seg = &segments[si];
                        float mx = 0.5f * (simplified_grid[seg->start_idx].x + simplified_grid[seg->end_idx].x);
                        float my = 0.5f * (simplified_grid[seg->start_idx].y + simplified_grid[seg->end_idx].y);
                        // Be permissive: keep all segments; we will still apply facing to avoid sealing voids.
                        region_segs[region_seg_count] = *seg;
                        // Re-evaluate solid_facing against this region mask (except convex fast-path handled earlier).
                        HullPoint a = simplified_grid[seg->start_idx];
                        HullPoint b = simplified_grid[seg->end_idx];
                        float dx = b.x - a.x;
                        float dy = b.y - a.y;
                        float len = sqrtf(dx * dx + dy * dy);
                        if (len < 1e-5f) {
                            region_segs[region_seg_count].solid_facing = false;
                        } else {
                            float nx = dy / len;
                            float ny = -dx / len;
                            float tx1 = mx + nx * boundary_offset;
                            float ty1 = my + ny * boundary_offset;
                            float tx2 = mx + nx * boundary_offset * 2.0f;
                            float ty2 = my + ny * boundary_offset * 2.0f;
                            bool out1 = !region_contains_point(inside_mask, void_mask, region_res, minx, spanx, miny, spany, tx1, ty1);
                            bool out2 = !region_contains_point(inside_mask, void_mask, region_res, minx, spanx, miny, spany, tx2, ty2);
                            region_segs[region_seg_count].solid_facing = out1 || out2;
                            if (!region_segs[region_seg_count].solid_facing) region_seg_reject_face++;
                        }
                        region_seg_count++;
                    }
                }
                if (region_seg_count == 0) continue;

                ColliderPrimitive prims[32];
                int added = collider_fit_primitives(simplified_grid,
                                                    simp_grid_count,
                                                    region_segs,
                                                    region_seg_count,
                                                    cfg,
                                                    prims,
                                                    32,
                                                    loop_convex ? NULL : inside_mask,
                                                    loop_convex ? NULL : void_mask,
                                                    region_res,
                                                    minx, spanx,
                                                    miny, spany);
                if (added == 0 && !loop_convex) {
                    // Relax region mask if nothing survived; still honor facing.
                    added = collider_fit_primitives(simplified_grid,
                                                    simp_grid_count,
                                                    region_segs,
                                                    region_seg_count,
                                                    cfg,
                                                    prims,
                                                    32,
                                                    NULL,
                                                    NULL,
                                                    region_res,
                                                    minx, spanx,
                                                    miny, spany);
                }
                int parts_before = total_parts;
                for (int pi2 = 0; pi2 < added && total_parts < cfg->collider_max_primitives; ++pi2) {
                    Vec2 verts[32];
                    int vc = collider_primitive_to_vertices(&prims[pi2], verts, 32);
                    if (vc < 3) continue;
                    HullPoint hp[32];
                    for (int v = 0; v < vc; ++v) hp[v] = (HullPoint){verts[v].x, verts[v].y};
                    HullPoint c = polygon_centroid(hp, vc);
                    if (!loop_convex) {
                        if (!region_contains_point(inside_mask, void_mask, region_res, minx, spanx, miny, spany, c.x, c.y)) continue;
                    }
                    if (vert_cursor + vc > 64) break;
                    imp->collider_part_offsets[total_parts] = vert_cursor;
                    imp->collider_part_counts[total_parts] = vc;
                    for (int v = 0; v < vc; ++v) {
                        float wx = (verts[v].x / (float)(w - 1)) * ww;
                        float wy = (verts[v].y / (float)(h - 1)) * wh;
                        float cxw = (center_grid_x / (float)(w - 1)) * ww;
                        float cyw = (center_grid_y / (float)(h - 1)) * wh;
                        imp->collider_parts_verts[vert_cursor++] = vec2(wx - cxw, wy - cyw);
                    }
                    total_parts++;
                }
                if (cfg->collider_debug_logs) {
                    fprintf(stderr,
                            "[collider] path=%zu region=%d cells=%d segs=%d keep=%d rej_out=%d rej_face=%d prims=%d parts=%d\n",
                            pi, r, regions[r].cells, seg_count, region_seg_count,
                            region_seg_reject_out, region_seg_reject_face, added, total_parts - parts_before);
                }
            }

            imp->collider_part_count = total_parts;
            // Debug outline (non-physics): store simplified loop in legacy verts.
            imp->collider_vert_count = 0;
            if (debug_loop_count == 0) {
                debug_loop_count = (simp_grid_count < 64) ? simp_grid_count : 64;
                memcpy(debug_loop, simplified_grid, (size_t)debug_loop_count * sizeof(HullPoint));
            }
            if (debug_loop_count >= 3) {
                int dbg_count = (debug_loop_count < 32) ? debug_loop_count : 32;
                imp->collider_vert_count = dbg_count;
                for (int i = 0; i < dbg_count; ++i) {
                    float wx = (debug_loop[i].x / (float)(w - 1)) * ww;
                    float wy = (debug_loop[i].y / (float)(h - 1)) * wh;
                    float cxw = (center_grid_x / (float)(w - 1)) * ww;
                    float cyw = (center_grid_y / (float)(h - 1)) * wh;
                    imp->collider_verts[i] = vec2(wx - cxw, wy - cyw);
                }
            }
            if (imp->collider_part_count > 0) {
                return true;
            }
            // If primitive path failed, stop here to surface the issue (no legacy fallback for concave cases).
            return false;
        }
    }

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
        // Legacy path: uniform resampling per path.
        int lc = collider_resample_path(path, cx, cy, norm, &proj, loops[loop_total], loop_vert_cap, samples_per_100);
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
    }
    if (loop_total <= 0) return false;

    // Sort loops by area descending and pick largest as outer
    float loop_areas[16];
    for (int i = 0; i < loop_total; ++i) {
        loop_areas[i] = fabsf(polygon_area(loops[i], loop_counts[i]));
    }
    for (int i = 0; i < loop_total; ++i) {
        for (int j = i + 1; j < loop_total; ++j) {
            if (loop_areas[j] > loop_areas[i]) {
                float ta = loop_areas[i]; loop_areas[i] = loop_areas[j]; loop_areas[j] = ta;
                int tc = loop_counts[i]; loop_counts[i] = loop_counts[j]; loop_counts[j] = tc;
                HullPoint tmp[256];
                memcpy(tmp, loops[i], sizeof(tmp));
                memcpy(loops[i], loops[j], sizeof(tmp));
                memcpy(loops[j], tmp, sizeof(tmp));
            }
        }
    }

    HullPoint outer[256];
    int outer_count = loop_counts[0];
    if (outer_count > 256) outer_count = 256;
    memcpy(outer, loops[0], (size_t)outer_count * sizeof(HullPoint));
    // Ensure CCW orientation for outer.
    float outer_area = polygon_area_signed(outer, outer_count);
    if (outer_area < 0.0f) {
        for (int i = 0; i < outer_count / 2; ++i) {
            HullPoint tmp = outer[i];
            outer[i] = outer[outer_count - 1 - i];
            outer[outer_count - 1 - i] = tmp;
        }
    }

    // Legacy verts (single loop) for compatibility
    HullPoint legacy_simplified[32];
    int legacy_count = collider_simplify_poly(outer, outer_count, legacy_simplified, 32, simplify_eps);
    if (legacy_count < 3) {
        legacy_count = outer_count < 32 ? outer_count : 32;
        memcpy(legacy_simplified, outer, (size_t)legacy_count * sizeof(HullPoint));
    }
    imp->collider_vert_count = legacy_count;
    // Map to local-space coords (window units, centered at import position)
    for (int i = 0; i < imp->collider_vert_count; ++i) {
        float wx = (legacy_simplified[i].x / (float)(w - 1)) * ww;
        float wy = (legacy_simplified[i].y / (float)(h - 1)) * wh;
        float cxw = (center_grid_x / (float)(w - 1)) * ww;
        float cyw = (center_grid_y / (float)(h - 1)) * wh;
        imp->collider_verts[i] = vec2(wx - cxw, wy - cyw);
    }

    // Physics contour: lightly simplified outer loop
    HullPoint part_src[128];
    int part_src_count = collider_simplify_poly(outer, outer_count, part_src, 128, simplify_eps * 0.2f);
    if (part_src_count < 3) {
        part_src_count = collider_collapse_collinear(outer, outer_count, part_src, 128);
    }
    if (part_src_count < 3) {
        part_src_count = outer_count < 128 ? outer_count : 128;
        memcpy(part_src, outer, (size_t)part_src_count * sizeof(HullPoint));
    }

    HullPoint part_polys[8][32];
    int part_counts[8] = {0};
    int part_count = (part_src_count >= 3)
                         ? collider_decompose_to_convex(part_src, part_src_count, part_polys, part_counts, part_cap, part_vert_cap)
                         : 0;

    // Filter degenerates
    int filtered = 0;
    for (int pi = 0; pi < part_count && pi < part_cap; ++pi) {
        int c = part_counts[pi];
        if (c < 3) continue;
        float area = fabsf(polygon_area(part_polys[pi], c));
        if (area < 1e-4f) continue;
        part_counts[filtered] = c;
        for (int vi = 0; vi < c; ++vi) {
            part_polys[filtered][vi] = part_polys[pi][vi];
        }
        filtered++;
    }
    part_count = filtered;

    // Hull fallback only for convex outlines (avoid sealing concave shapes).
    HullPoint hull_tmp[64];
    int hull_tmp_count = 0;
    if (part_count <= 0 && part_src_count >= 3 && polygon_convex(part_src, part_src_count)) {
        hull_tmp_count = collider_compute_convex_hull(part_src, part_src_count, hull_tmp, 64);
        if (hull_tmp_count >= 3) {
            part_count = 1;
            part_counts[0] = (hull_tmp_count > part_vert_cap) ? part_vert_cap : hull_tmp_count;
            for (int vi = 0; vi < part_counts[0]; ++vi) {
                part_polys[0][vi] = hull_tmp[vi];
            }
        }
    }

    int vert_cursor = 0;
    int written_parts = 0;
    for (int pi = 0; pi < part_count && written_parts < part_cap; ++pi) {
        int c = part_counts[pi];
        if (c < 3) continue;
        if (c > part_vert_cap) c = part_vert_cap;
        if (vert_cursor + c > 64) break;
        // Interior guard using legacy outer loop.
        HullPoint hp[32];
        for (int v = 0; v < c; ++v) hp[v] = part_polys[pi][v];
        HullPoint cc = polygon_centroid(hp, c);
        if (!point_in_polygon(outer, outer_count, cc)) continue;

        imp->collider_part_offsets[written_parts] = vert_cursor;
        imp->collider_part_counts[written_parts] = c;
        for (int vi = 0; vi < c; ++vi) {
            float wx = (part_polys[pi][vi].x / (float)(w - 1)) * ww;
            float wy = (part_polys[pi][vi].y / (float)(h - 1)) * wh;
            float cxw = (center_grid_x / (float)(w - 1)) * ww;
            float cyw = (center_grid_y / (float)(h - 1)) * wh;
            imp->collider_parts_verts[vert_cursor++] = vec2(wx - cxw, wy - cyw);
        }
        written_parts++;
    }

    // Fallback single part from legacy simplified (only if convex to avoid sealing cavities).
    if (written_parts == 0 && legacy_count >= 3 && polygon_convex(legacy_simplified, legacy_count)) {
        int vc_single = legacy_count;
        if (vc_single > part_vert_cap) vc_single = part_vert_cap;
        if (vc_single > 64) vc_single = 64;
        imp->collider_part_offsets[0] = 0;
        imp->collider_part_counts[0] = vc_single;
        vert_cursor = 0;
        for (int vi = 0; vi < vc_single; ++vi) {
            float wx = (legacy_simplified[vi].x / (float)(w - 1)) * ww;
            float wy = (legacy_simplified[vi].y / (float)(h - 1)) * wh;
            float cxw = (center_grid_x / (float)(w - 1)) * ww;
            float cyw = (center_grid_y / (float)(h - 1)) * wh;
            imp->collider_parts_verts[vert_cursor++] = vec2(wx - cxw, wy - cyw);
        }
        written_parts = 1;
    }
    imp->collider_part_count = (written_parts > 0) ? written_parts : 0;

    return imp->collider_part_count > 0;
}
