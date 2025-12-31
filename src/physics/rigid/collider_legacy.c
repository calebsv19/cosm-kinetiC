#include "physics/rigid/collider_legacy.h"

#include <math.h>
#include <string.h>

#include "physics/rigid/collider_utils.h"
#include "physics/rigid/collider_geom.h"
#include "physics/rigid/collider_tagging.h"

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

int collider_resample_path(const ShapeAssetPath *path,
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

int collider_decompose_to_convex(const HullPoint *pts,
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
    int c = collider_collapse_collinear(pts, count, cleaned, 128);
    if (c < 3) return 0;
    float area = polygon_area_signed(cleaned, c);
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
            ccount = collider_simplify_poly(polys[i], ccount, simplified, vert_cap, 0.01f);
            if (ccount < 3) ccount = vert_cap;
            for (int k = 0; k < ccount; ++k) polys[i][k] = simplified[k];
        }
        counts[i] = ccount;
        for (int k = 0; k < ccount; ++k) parts[i][k] = polys[i][k];
    }
    return poly_count;
}
