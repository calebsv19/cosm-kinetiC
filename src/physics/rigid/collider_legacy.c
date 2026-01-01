#include "physics/rigid/collider_legacy.h"

#include <math.h>
#include <string.h>

#include "physics/rigid/collider_utils.h"
#include "physics/rigid/collider_geom.h"
#include "physics/rigid/collider_tagging.h"

static void build_current_poly(const HullPoint *pts,
                               const int *idx,
                               int vert_count,
                               HullPoint *out) {
    for (int k = 0; k < vert_count; ++k) out[k] = pts[idx[k]];
}

static float segment_distance(HullPoint a, HullPoint b, HullPoint p) {
    float vx = b.x - a.x, vy = b.y - a.y;
    float wx = p.x - a.x, wy = p.y - a.y;
    float len2 = vx * vx + vy * vy;
    float t = (len2 > 1e-8f) ? (wx * vx + wy * vy) / len2 : 0.0f;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    float px = a.x + t * vx;
    float py = a.y + t * vy;
    float dx = p.x - px, dy = p.y - py;
    return sqrtf(dx * dx + dy * dy);
}

static bool diagonal_valid(const HullPoint *pts,
                           const int *idx,
                           int vert_count,
                           HullPoint a,
                           HullPoint c,
                           float dev_tol) {
    HullPoint cur_poly[128];
    if (vert_count > 128) vert_count = 128;
    build_current_poly(pts, idx, vert_count, cur_poly);
    HullPoint mids[3] = {
        {(a.x + c.x) * 0.5f, (a.y + c.y) * 0.5f},
        {(2.0f * a.x + c.x) / 3.0f, (2.0f * a.y + c.y) / 3.0f},
        {(a.x + 2.0f * c.x) / 3.0f, (a.y + 2.0f * c.y) / 3.0f},
    };
    for (int m = 0; m < 3; ++m) {
        HullPoint p = mids[m];
        if (!point_in_polygon(cur_poly, vert_count, p)) return false;
        float best = 1e9f;
        for (int e = 0; e < vert_count; ++e) {
            HullPoint p0 = cur_poly[e];
            HullPoint p1 = cur_poly[(e + 1) % vert_count];
            float d = segment_distance(p0, p1, p);
            if (d < best) best = d;
        }
        if (best > dev_tol) return false;
    }
    return true;
}

static int ear_clip_triangles(const HullPoint *poly, int n, HullPoint tris[][3], int max_tris) {
    if (!poly || n < 3 || max_tris <= 0) return 0;
    HullPoint tmp[128];
    if (n > 128) n = 128;
    for (int i = 0; i < n; ++i) tmp[i] = poly[i];
    // Bounding box to set deviation tolerance.
    float minx = tmp[0].x, maxx = tmp[0].x, miny = tmp[0].y, maxy = tmp[0].y;
    for (int i = 1; i < n; ++i) {
        if (tmp[i].x < minx) minx = tmp[i].x;
        if (tmp[i].x > maxx) maxx = tmp[i].x;
        if (tmp[i].y < miny) miny = tmp[i].y;
        if (tmp[i].y > maxy) maxy = tmp[i].y;
    }
    float span = fmaxf(maxx - minx, maxy - miny);
    float dev_tol = fmaxf(1.5f, span * 0.02f);

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
            // Boundary-aware diagonal validity.
            if (!diagonal_valid(tmp, idx, vert_count, a, c, dev_tol)) continue;
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

    if (target < pc) target = pc;
    if (target < 8) target = 8;
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
    if (poly_count <= 1) return poly_count;

    bool merged = true;
    while (merged) {
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
        if (poly_count <= max_count) break;
    }

    if (poly_count > max_count) {
        // Try to reduce count by merging smallest polys with neighbors first.
        int guard = 0;
        while (poly_count > max_count && guard < 256) {
            guard++;
            // Find smallest polygon by area.
            int smallest = 0;
            float smallest_area = fabsf(polygon_area(polys[0], counts[0]));
            for (int i = 1; i < poly_count; ++i) {
                float a = fabsf(polygon_area(polys[i], counts[i]));
                if (a < smallest_area) {
                    smallest_area = a;
                    smallest = i;
                }
            }
            bool merged_one = false;
            for (int j = 0; j < poly_count && !merged_one; ++j) {
                if (j == smallest) continue;
                int ai = 0, bi = 0;
                if (!polygons_share_edge(polys[smallest], counts[smallest], polys[j], counts[j], &ai, &bi)) continue;
                HullPoint merged_pts[32];
                int mcount = 0;
                int a_next = (ai + 1) % counts[smallest];
                for (int k = a_next; k != ai; k = (k + 1) % counts[smallest]) {
                    if (mcount < vert_cap) merged_pts[mcount++] = polys[smallest][k];
                }
                int b_next = (bi + 1) % counts[j];
                for (int k = b_next; k != bi; k = (k + 1) % counts[j]) {
                    if (mcount < vert_cap) merged_pts[mcount++] = polys[j][k];
                }
                if (mcount < 3 || mcount > vert_cap) continue;
                if (!polygon_convex(merged_pts, mcount)) continue;
                // Commit merge into smallest slot.
                counts[smallest] = mcount;
                for (int k = 0; k < mcount; ++k) polys[smallest][k] = merged_pts[k];
                for (int k = j; k < poly_count - 1; ++k) {
                    counts[k] = counts[k + 1];
                    for (int v = 0; v < vert_cap; ++v) polys[k][v] = polys[k + 1][v];
                }
                poly_count--;
                merged_one = true;
            }
            if (!merged_one) break; // cannot reduce further
        }
        // If still too many, keep the largest by area.
        if (poly_count > max_count) {
            float areas[128];
            for (int i = 0; i < poly_count; ++i) {
                areas[i] = fabsf(polygon_area(polys[i], counts[i]));
            }
            for (int i = 0; i < poly_count - 1; ++i) {
                for (int j = i + 1; j < poly_count; ++j) {
                    if (areas[j] > areas[i]) {
                        float ta = areas[i]; areas[i] = areas[j]; areas[j] = ta;
                        int tc = counts[i]; counts[i] = counts[j]; counts[j] = tc;
                        HullPoint tmp[32];
                        memcpy(tmp, polys[i], sizeof(tmp));
                        memcpy(polys[i], polys[j], sizeof(tmp));
                        memcpy(polys[j], tmp, sizeof(tmp));
                    }
                }
            }
            poly_count = max_count;
        }
    }
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
