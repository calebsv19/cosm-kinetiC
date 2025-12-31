#include "physics/rigid2d/collider_builder.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "render/import_project.h"
#include "physics/math/math2d.h"
#include "app/shape_lookup.h"

typedef struct HullPoint {
    float x, y;
} HullPoint;

static inline float import_pos_to_unit(float pos, float span) {
    float min = 0.5f - span;
    float max = 0.5f + span;
    float t = (pos - min) / (max - min);
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return t;
}

static inline int clamp_int(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static float polygon_area(const HullPoint *pts, int count) {
    if (!pts || count < 3) return 0.0f;
    float area = 0.0f;
    for (int i = 0; i < count; ++i) {
        HullPoint a = pts[i];
        HullPoint b = pts[(i + 1) % count];
        area += (a.x * b.y - b.x * a.y);
    }
    return 0.5f * area;
}

static int collapse_collinear(const HullPoint *in, int in_count, HullPoint *out, int max_out) {
    if (!in || !out || in_count <= 0 || max_out <= 0) return 0;
    int out_count = 0;
    for (int i = 0; i < in_count; ++i) {
        HullPoint prev = in[(i + in_count - 1) % in_count];
        HullPoint curr = in[i];
        HullPoint next = in[(i + 1) % in_count];
        float cx = (next.x - curr.x) * (curr.y - prev.y) - (next.y - curr.y) * (curr.x - prev.x);
        if (fabsf(cx) < 1e-5f && out_count > 0) continue;
        if (out_count < max_out) {
            out[out_count++] = curr;
        }
    }
    return out_count;
}

// Simple monotonic-chain convex hull; returns CCW hull.
static int compute_convex_hull(const HullPoint *pts, int count, HullPoint *out, int max_out) {
    if (!pts || !out || count < 3 || max_out < 3) return 0;
    HullPoint tmp[512];
    int n = count < 512 ? count : 512;
    for (int i = 0; i < n; ++i) tmp[i] = pts[i];
    // sort by x, then y
    for (int i = 0; i < n; ++i) {
        for (int j = i + 1; j < n; ++j) {
            if (tmp[j].x < tmp[i].x || (tmp[j].x == tmp[i].x && tmp[j].y < tmp[i].y)) {
                HullPoint t = tmp[i]; tmp[i] = tmp[j]; tmp[j] = t;
            }
        }
    }
    HullPoint hull[1024];
    int h = 0;
    for (int i = 0; i < n; ++i) {
        while (h >= 2) {
            HullPoint a = hull[h - 2], b = hull[h - 1], c = tmp[i];
            if ((b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x) <= 0.0f) h--;
            else break;
        }
        hull[h++] = tmp[i];
    }
    int lower = h;
    for (int i = n - 2; i >= 0; --i) {
        while (h >= lower + 1) {
            HullPoint a = hull[h - 2], b = hull[h - 1], c = tmp[i];
            if ((b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x) <= 0.0f) h--;
            else break;
        }
        hull[h++] = tmp[i];
    }
    if (h > 1) h--;
    int out_count = (h < max_out) ? h : max_out;
    for (int i = 0; i < out_count; ++i) out[i] = hull[i];
    return out_count;
}

static int simplify_poly(const HullPoint *pts, int n, HullPoint *out, int max_out, float epsilon) {
    if (!pts || !out || n <= 0 || max_out <= 0) return 0;
    if (n <= 2) {
        int c = (n < max_out) ? n : max_out;
        memcpy(out, pts, (size_t)c * sizeof(HullPoint));
        return c;
    }
    int stack[512];
    int top = 0;
    stack[top++] = 0;
    stack[top++] = n - 1;
    char *keep = (char *)calloc((size_t)n, 1);
    if (!keep) return 0;
    keep[0] = keep[n - 1] = 1;
    while (top > 0) {
        int end = stack[--top];
        int start = stack[--top];
        float maxd = 0.0f;
        int idx = -1;
        HullPoint a = pts[start];
        HullPoint b = pts[end];
        float bxax = b.x - a.x;
        float byay = b.y - a.y;
        float len2 = bxax * bxax + byay * byay;
        for (int i = start + 1; i < end; ++i) {
            float px = pts[i].x - a.x;
            float py = pts[i].y - a.y;
            float t = (len2 > 0.0f) ? (px * bxax + py * byay) / len2 : 0.0f;
            float projx = a.x + t * bxax;
            float projy = a.y + t * byay;
            float dx = pts[i].x - projx;
            float dy = pts[i].y - projy;
            float dist2 = dx * dx + dy * dy;
            if (dist2 > maxd) {
                maxd = dist2;
                idx = i;
            }
        }
        if (idx != -1 && maxd > epsilon * epsilon) {
            keep[idx] = 1;
            stack[top++] = start;
            stack[top++] = idx;
            stack[top++] = idx;
            stack[top++] = end;
        }
    }
    int out_count = 0;
    for (int i = 0; i < n && out_count < max_out; ++i) {
        if (keep[i]) out[out_count++] = pts[i];
    }
    free(keep);
    return out_count;
}

static bool point_in_triangle(const HullPoint *p, const HullPoint *a, const HullPoint *b, const HullPoint *c) {
    float v0x = c->x - a->x, v0y = c->y - a->y;
    float v1x = b->x - a->x, v1y = b->y - a->y;
    float v2x = p->x - a->x, v2y = p->y - a->y;
    float dot00 = v0x * v0x + v0y * v0y;
    float dot01 = v0x * v1x + v0y * v1y;
    float dot02 = v0x * v2x + v0y * v2y;
    float dot11 = v1x * v1x + v1y * v1y;
    float dot12 = v1x * v2x + v1y * v2y;
    float denom = dot00 * dot11 - dot01 * dot01;
    if (fabsf(denom) < 1e-8f) return false;
    float inv = 1.0f / denom;
    float u = (dot11 * dot02 - dot01 * dot12) * inv;
    float v = (dot00 * dot12 - dot01 * dot02) * inv;
    return (u >= -1e-5f) && (v >= -1e-5f) && (u + v <= 1.0f + 1e-5f);
}

static bool polygon_convex(const HullPoint *pts, int count) {
    if (!pts || count < 3) return false;
    float sign = 0.0f;
    for (int i = 0; i < count; ++i) {
        HullPoint a = pts[i];
        HullPoint b = pts[(i + 1) % count];
        HullPoint c = pts[(i + 2) % count];
        float cross = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
        if (fabsf(cross) < 1e-5f) continue;
        if (sign == 0.0f) sign = (cross > 0.0f) ? 1.0f : -1.0f;
        else if ((cross > 0.0f && sign < 0.0f) || (cross < 0.0f && sign > 0.0f)) return false;
    }
    return true;
}

static int ear_clip_triangles(const HullPoint *poly, int n, HullPoint tris[][3], int max_tris) {
    if (!poly || n < 3 || max_tris <= 0) return 0;
    HullPoint tmp[128];
    if (n > 128) n = 128;
    for (int i = 0; i < n; ++i) tmp[i] = poly[i];
    int idx[128];
    for (int i = 0; i < n; ++i) idx[i] = i;
    int vert_count = n;
    int tri_count = 0;
    int guard = 0;
    while (vert_count >= 3 && tri_count < max_tris && guard < 4096) {
        guard++;
        bool ear_found = false;
        for (int i = 0; i < vert_count; ++i) {
            int i0 = idx[(i + vert_count - 1) % vert_count];
            int i1 = idx[i];
            int i2 = idx[(i + 1) % vert_count];
            HullPoint a = tmp[i0], b = tmp[i1], c = tmp[i2];
            float z = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
            if (z <= 0.0f) continue; // reflex for CCW
            bool contains = false;
            for (int j = 0; j < vert_count; ++j) {
                int ij = idx[j];
                if (ij == i0 || ij == i1 || ij == i2) continue;
                if (point_in_triangle(&tmp[ij], &a, &b, &c)) {
                    contains = true;
                    break;
                }
            }
            if (contains) continue;
            tris[tri_count][0] = a;
            tris[tri_count][1] = b;
            tris[tri_count][2] = c;
            tri_count++;
            for (int k = i; k < vert_count - 1; ++k) {
                idx[k] = idx[k + 1];
            }
            vert_count--;
            ear_found = true;
            break;
        }
        if (!ear_found) break;
    }
    return tri_count;
}

static bool polygons_share_edge(const HullPoint *a, int na, const HullPoint *b, int nb, int *out_ai, int *out_bi) {
    if (!a || !b || na < 2 || nb < 2) return false;
    for (int i = 0; i < na; ++i) {
        int ia1 = i;
        int ia2 = (i + 1) % na;
        for (int j = 0; j < nb; ++j) {
            int jb1 = j;
            int jb2 = (j + 1) % nb;
            if (fabsf(a[ia1].x - b[jb2].x) < 1e-4f &&
                fabsf(a[ia1].y - b[jb2].y) < 1e-4f &&
                fabsf(a[ia2].x - b[jb1].x) < 1e-4f &&
                fabsf(a[ia2].y - b[jb1].y) < 1e-4f) {
                if (out_ai) *out_ai = ia1;
                if (out_bi) *out_bi = jb1;
                return true;
            }
        }
    }
    return false;
}

static int merge_polygons(HullPoint polys[][32], int *counts, int poly_count, int max_count, int vert_cap) {
    bool merged = true;
    while (merged && poly_count > 1) {
        merged = false;
        for (int i = 0; i < poly_count && !merged; ++i) {
            for (int j = i + 1; j < poly_count && !merged; ++j) {
                int ai = 0, bi = 0;
                if (!polygons_share_edge(polys[i], counts[i], polys[j], counts[j], &ai, &bi)) continue;
                HullPoint merged_pts[32];
                int mcount = 0;
                int a_next = (ai + 1) % counts[i];
                for (int k = a_next; k != ai; k = (k + 1) % counts[i]) {
                    if (mcount < vert_cap) merged_pts[mcount++] = polys[i][k];
                }
                int b_next = (bi + 1) % counts[j];
                for (int k = b_next; k != bi; k = (k + 1) % counts[j]) {
                    if (mcount < vert_cap) merged_pts[mcount++] = polys[j][k];
                }
                if (mcount < 3 || mcount > vert_cap) continue;
                if (!polygon_convex(merged_pts, mcount)) continue;
                for (int k = 0; k < mcount; ++k) polys[i][k] = merged_pts[k];
                counts[i] = mcount;
                for (int k = j; k < poly_count - 1; ++k) {
                    counts[k] = counts[k + 1];
                    for (int v = 0; v < vert_cap; ++v) polys[k][v] = polys[k + 1][v];
                }
                poly_count--;
                merged = true;
            }
        }
    }
    if (poly_count > max_count) poly_count = max_count;
    return poly_count;
}

static int decompose_to_convex(const HullPoint *pts,
                               int count,
                               HullPoint parts[][32],
                               int *counts,
                               int max_parts,
                               int vert_cap) {
    if (!pts || count < 3 || max_parts < 1) return 0;
    if (vert_cap < 3) vert_cap = 3;
    if (vert_cap > 32) vert_cap = 32;
    HullPoint cleaned[128];
    if (count > 128) count = 128;
    int c = collapse_collinear(pts, count, cleaned, 128);
    if (c < 3) return 0;
    float area = polygon_area(cleaned, c);
    if (area < 0.0f) {
        for (int i = 0; i < c / 2; ++i) {
            HullPoint tmp = cleaned[i];
            cleaned[i] = cleaned[c - 1 - i];
            cleaned[c - 1 - i] = tmp;
        }
    }
    const int TRI_CAP = 32;
    HullPoint tris[TRI_CAP][3];
    int tri_count = ear_clip_triangles(cleaned, c, tris, TRI_CAP);
    if (tri_count <= 0) return 0;

    HullPoint polys[TRI_CAP][32];
    int poly_counts[TRI_CAP];
    int poly_count = 0;
    for (int i = 0; i < tri_count && poly_count < TRI_CAP; ++i) {
        polys[poly_count][0] = tris[i][0];
        polys[poly_count][1] = tris[i][1];
        polys[poly_count][2] = tris[i][2];
        poly_counts[poly_count] = 3;
        poly_count++;
    }
    poly_count = merge_polygons(polys, poly_counts, poly_count, max_parts, vert_cap);

    if (poly_count > max_parts) poly_count = max_parts;
    for (int i = 0; i < poly_count; ++i) {
        int ccount = poly_counts[i];
        if (ccount > vert_cap) {
            HullPoint simplified[32];
            ccount = simplify_poly(polys[i], ccount, simplified, vert_cap, 0.01f);
            if (ccount < 3) ccount = vert_cap;
            for (int k = 0; k < ccount; ++k) polys[i][k] = simplified[k];
        }
        counts[i] = ccount;
        for (int k = 0; k < ccount; ++k) parts[i][k] = polys[i][k];
    }
    return poly_count;
}

// Resample a closed path to even spacing based on desired sample density.
static int resample_path(const ShapeAssetPath *path,
                         float cx,
                         float cy,
                         float norm,
                         const ImportProjectParams *proj,
                         HullPoint *out,
                         int max_out,
                         float samples_per_100) {
    if (!path || !out || !proj || max_out <= 0 || path->point_count < 2) return 0;
    int pc = (int)path->point_count;
    if (pc > 512) pc = 512;

    float total = 0.0f;
    float seg_len[512];
    for (int i = 0; i < pc; ++i) {
        ShapeAssetPoint a = path->points[i];
        ShapeAssetPoint b = path->points[(i + 1) % pc];
        float dx = (b.x - a.x) * norm;
        float dy = (b.y - a.y) * norm;
        float len = sqrtf(dx * dx + dy * dy);
        seg_len[i] = len;
        total += len;
    }
    if (total <= 1e-5f) return 0;

    if (samples_per_100 <= 0.0f) samples_per_100 = 24.0f;
    float desired = total * (samples_per_100 / 100.0f);
    int target = (int)ceilf(desired);
    if (target < 3) target = 3;
    if (target > max_out) target = max_out;

    float step = total / (float)target;
    float dist = 0.0f;
    int out_count = 0;
    for (int k = 0; k < target && out_count < max_out; ++k) {
        float d = dist;
        int seg = 0;
        while (seg < pc) {
            if (d <= seg_len[seg]) break;
            d -= seg_len[seg];
            seg++;
        }
        if (seg >= pc) seg = pc - 1;
        ShapeAssetPoint a = path->points[seg];
        ShapeAssetPoint b = path->points[(seg + 1) % pc];
        float t = (seg_len[seg] > 1e-5f) ? d / seg_len[seg] : 0.0f;
        float sx = a.x + t * (b.x - a.x);
        float sy = a.y + t * (b.y - a.y);
        float dx = (sx - cx) * norm;
        float dy = (sy - cy) * norm;
        ImportProjectPoint pp = import_project_point(proj, dx, dy);
        if (!pp.valid) {
            dist += step;
            continue;
        }
        float gx = pp.screen_x / (float)proj->window_w * (float)(proj->grid_w - 1);
        float gy = pp.screen_y / (float)proj->window_h * (float)(proj->grid_h - 1);
        out[out_count++] = (HullPoint){gx, gy};
        dist += step;
    }
    return out_count;
}

bool collider_build_import(const AppConfig *cfg,
                           const ShapeAssetLibrary *lib,
                           ImportedShape *imp) {
    if (!cfg || !lib || !imp) return false;
    const ShapeAsset *asset = shape_lookup_from_path(lib, imp->path);
    if (!asset) return false;
    ShapeAssetBounds b;
    if (!shape_asset_bounds(asset, &b) || !b.valid) return false;

    int w = cfg->grid_w;
    int h = cfg->grid_h;
    if (w <= 1 || h <= 1) return false;

    // Config caps
    int loop_cap = clamp_int(cfg->collider_max_loops, 1, 16);
    int loop_vert_cap = clamp_int(cfg->collider_max_loop_vertices, 8, 256);
    int part_cap = clamp_int(cfg->collider_max_parts, 1, 8);
    int part_vert_cap = clamp_int(cfg->collider_max_part_vertices, 3, 32);
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
        int lc = resample_path(path, cx, cy, norm, &proj, loops[loop_total], loop_vert_cap, samples_per_100);
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

    // Legacy verts (single loop) for compatibility
    HullPoint legacy_simplified[32];
    int legacy_count = simplify_poly(outer, outer_count, legacy_simplified, 32, simplify_eps);
    if (legacy_count < 3) {
        legacy_count = outer_count < 32 ? outer_count : 32;
        memcpy(legacy_simplified, outer, (size_t)legacy_count * sizeof(HullPoint));
    }
    imp->collider_vert_count = legacy_count;
    // Map to local-space coords (window units, centered at import position)
    float center_grid_x = import_pos_to_unit(imp->position_x, span_x_cfg) * (float)(w - 1);
    float center_grid_y = import_pos_to_unit(imp->position_y, span_y_cfg) * (float)(h - 1);
    float ww = (float)(cfg->window_w > 0 ? cfg->window_w : w);
    float wh = (float)(cfg->window_h > 0 ? cfg->window_h : h);
    for (int i = 0; i < imp->collider_vert_count; ++i) {
        float wx = (legacy_simplified[i].x / (float)(w - 1)) * ww;
        float wy = (legacy_simplified[i].y / (float)(h - 1)) * wh;
        float cxw = (center_grid_x / (float)(w - 1)) * ww;
        float cyw = (center_grid_y / (float)(h - 1)) * wh;
        imp->collider_verts[i] = vec2(wx - cxw, wy - cyw);
    }

    // Physics contour: lightly simplified outer loop
    HullPoint part_src[128];
    int part_src_count = simplify_poly(outer, outer_count, part_src, 128, simplify_eps * 0.2f);
    if (part_src_count < 3) {
        part_src_count = collapse_collinear(outer, outer_count, part_src, 128);
    }
    if (part_src_count < 3) {
        part_src_count = outer_count < 128 ? outer_count : 128;
        memcpy(part_src, outer, (size_t)part_src_count * sizeof(HullPoint));
    }

    HullPoint part_polys[8][32];
    int part_counts[8] = {0};
    int part_count = (part_src_count >= 3)
                         ? decompose_to_convex(part_src, part_src_count, part_polys, part_counts, part_cap, part_vert_cap)
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

    // Hull fallback
    HullPoint hull_tmp[64];
    int hull_tmp_count = 0;
    if (part_count <= 0 && part_src_count >= 3) {
        hull_tmp_count = compute_convex_hull(part_src, part_src_count, hull_tmp, 64);
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

    // Fallback single part from legacy simplified
    if (written_parts == 0 && legacy_count >= 3) {
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
