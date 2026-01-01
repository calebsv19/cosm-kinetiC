#include "physics/rigid/collider_tagging.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "physics/math/math2d.h"
#include "physics/rigid/collider_utils.h"

int collider_tag_closed_path(const ShapeAssetPath *path,
                             float corner_thresh_deg,
                             TaggedPoint *out,
                             int max_out,
                             int *out_corner_count) {
    if (!path || !out || max_out <= 0 || !path->closed || path->point_count < 3) return 0;
    int pc = (int)path->point_count;
    if (pc > max_out) pc = max_out;
    if (out_corner_count) *out_corner_count = 0;

    int written = 0;
    for (int i = 0; i < pc && written < max_out; ++i) {
        ShapeAssetPoint p_prev = path->points[(i + pc - 1) % pc];
        ShapeAssetPoint p_cur  = path->points[i];
        ShapeAssetPoint p_next = path->points[(i + 1) % pc];
        Vec2 v0 = vec2(p_cur.x - p_prev.x, p_cur.y - p_prev.y);
        Vec2 v1 = vec2(p_next.x - p_cur.x, p_next.y - p_cur.y);
        float len0 = vec2_len(v0);
        float len1 = vec2_len(v1);
        float angle_deg = 0.0f;
        bool is_corner = false;
        bool is_concave = false;
        if (len0 > 1e-5f && len1 > 1e-5f) {
            float dot = vec2_dot(v0, v1) / (len0 * len1);
            dot = collider_clamp_float(dot, -1.0f, 1.0f);
            float ang = acosf(dot) * (180.0f / (float)M_PI);
            angle_deg = ang;
            is_corner = (ang > corner_thresh_deg);
            float cross = v0.x * v1.y - v0.y * v1.x;
            is_concave = (cross < 0.0f);
        }
        if (is_corner && out_corner_count) {
            (*out_corner_count)++;
        }
        out[written++] = (TaggedPoint){
            .pos = vec2(p_cur.x, p_cur.y),
            .angle_deg = angle_deg,
            .is_corner = is_corner,
            .is_concave = is_concave
        };
    }
    return written;
}

int collider_tag_closed_points(const HullPoint *pts,
                               int count,
                               float corner_thresh_deg,
                               TaggedPoint *out,
                               int max_out,
                               int *out_corner_count) {
    if (!pts || !out || max_out <= 0 || count < 3) return 0;
    if (count > max_out) count = max_out;
    if (out_corner_count) *out_corner_count = 0;
    int written = 0;
    for (int i = 0; i < count; ++i) {
        HullPoint p_prev = pts[(i + count - 1) % count];
        HullPoint p_cur  = pts[i];
        HullPoint p_next = pts[(i + 1) % count];
        Vec2 v0 = vec2(p_cur.x - p_prev.x, p_cur.y - p_prev.y);
        Vec2 v1 = vec2(p_next.x - p_cur.x, p_next.y - p_cur.y);
        float len0 = vec2_len(v0);
        float len1 = vec2_len(v1);
        float angle_deg = 0.0f;
        bool is_corner = false;
        bool is_concave = false;
        if (len0 > 1e-5f && len1 > 1e-5f) {
            float dot = vec2_dot(v0, v1) / (len0 * len1);
            dot = collider_clamp_float(dot, -1.0f, 1.0f);
            float ang = acosf(dot) * (180.0f / (float)M_PI);
            angle_deg = ang;
            is_corner = (ang > corner_thresh_deg);
            float cross = v0.x * v1.y - v0.y * v1.x;
            is_concave = (cross < 0.0f);
        }
        if (is_corner && out_corner_count) (*out_corner_count)++;
        out[written++] = (TaggedPoint){
            .pos = vec2(p_cur.x, p_cur.y),
            .angle_deg = angle_deg,
            .is_corner = is_corner,
            .is_concave = is_concave
        };
    }
    return written;
}

// Corner-preserving simplification: split loop into spans between corners,
// simplify each span (keeping endpoints), and stitch back together closed.
int collider_simplify_loop_preserve_corners(const ShapeAssetPath *path,
                                            float corner_thresh_deg,
                                            float eps,
                                            HullPoint *out,
                                            int max_out,
                                            int *out_corner_count) {
    if (!path || !out || max_out <= 0 || !path->closed || path->point_count < 3) return 0;
    TaggedPoint tagged[512];
    int corner_count = 0;
    int tc = collider_tag_closed_path(path, corner_thresh_deg, tagged, 512, &corner_count);
    if (out_corner_count) *out_corner_count = corner_count;
    if (tc < 3) return 0;

    int corner_indices[512];
    int corner_total = 0;
    for (int i = 0; i < tc && corner_total < 512; ++i) {
        if (tagged[i].is_corner) corner_indices[corner_total++] = i;
    }
    if (corner_total == 0) {
        corner_indices[corner_total++] = 0;
    }

    int written = 0;
    for (int ci = 0; ci < corner_total; ++ci) {
        int start = corner_indices[ci];
        int end = corner_indices[(ci + 1) % corner_total];
        HullPoint span[512];
        int span_count = 0;
        int idx = start;
        while (true) {
            if (span_count < 512) {
                span[span_count++] = (HullPoint){tagged[idx].pos.x, tagged[idx].pos.y};
            }
            if (idx == end) break;
            idx = (idx + 1) % tc;
        }
        if (span_count < 2) continue;

        HullPoint span_simpl[512];
        int simp_count = collider_simplify_poly(span, span_count, span_simpl, 512, eps);
        if (simp_count < 2) {
            simp_count = 2;
            span_simpl[0] = span[0];
            span_simpl[1] = span[span_count - 1];
        }

        for (int k = 0; k < simp_count && written < max_out; ++k) {
            if (written > 0 && collider_nearly_equal((HullPoint){tagged[start].pos.x, tagged[start].pos.y},
                                                     span_simpl[k], 1e-4f)) {
                if (k == 0) continue;
            }
            out[written++] = span_simpl[k];
        }
    }

    if (written >= 2 && written < max_out) {
        if (!collider_nearly_equal(out[0], out[written - 1], 1e-4f)) {
            out[written++] = out[0];
        }
    }
    return written;
}

int collider_project_simplified_to_grid(const HullPoint *in,
                                        int in_count,
                                        float cx,
                                        float cy,
                                        float norm,
                                        const ImportProjectParams *proj,
                                        HullPoint *out,
                                        int max_out) {
    if (!in || !proj || !out || max_out <= 0 || in_count <= 0) return 0;
    int written = 0;
    for (int i = 0; i < in_count && written < max_out; ++i) {
        float dx = (in[i].x - cx) * norm;
        float dy = (in[i].y - cy) * norm;
        ImportProjectPoint pp = import_project_point(proj, dx, dy);
        if (!pp.valid) continue;
        float gx = pp.screen_x / (float)proj->window_w * (float)(proj->grid_w - 1);
        float gy = pp.screen_y / (float)proj->window_h * (float)(proj->grid_h - 1);
        out[written++] = (HullPoint){gx, gy};
    }
    if (written >= 2 && written < max_out) {
        if (!collider_nearly_equal(out[0], out[written - 1], 1e-4f)) {
            out[written++] = out[0];
        }
    }
    return written;
}

int collider_collapse_collinear(const HullPoint *in, int in_count, HullPoint *out, int max_out) {
    if (!in || !out || in_count <= 0 || max_out <= 0) return 0;
    int out_count = 0;
    for (int i = 0; i < in_count; ++i) {
        HullPoint prev = in[(i + in_count - 1) % in_count];
        HullPoint curr = in[i];
        HullPoint next = in[(i + 1) % in_count];
        float cx = (next.x - curr.x) * (curr.y - prev.y) - (next.y - curr.y) * (curr.x - prev.x);
        // Keep a much tighter tolerance so we do not accidentally collapse real corners.
        if (fabsf(cx) < 1e-7f && out_count > 0) continue;
        if (out_count < max_out) {
            out[out_count++] = curr;
        }
    }
    return out_count;
}

int collider_compute_convex_hull(const HullPoint *pts, int count, HullPoint *out, int max_out) {
    if (!pts || !out || count < 3 || max_out < 3) return 0;
    HullPoint tmp[512];
    int n = count < 512 ? count : 512;
    for (int i = 0; i < n; ++i) tmp[i] = pts[i];
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

int collider_simplify_poly(const HullPoint *pts, int n, HullPoint *out, int max_out, float epsilon) {
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

int collider_simplify_intent(const HullPoint *pts,
                             int n,
                             HullPoint *out,
                             int max_out,
                             float min_angle_deg,
                             float min_edge_len) {
    if (!pts || !out || n < 3 || max_out <= 0) return 0;
    float min_edge2 = min_edge_len * min_edge_len;
    float cos_thresh = cosf(min_angle_deg * (float)M_PI / 180.0f);
    if (cos_thresh < -1.0f) cos_thresh = -1.0f;
    if (cos_thresh > 1.0f) cos_thresh = 1.0f;

    int out_count = 0;
    for (int i = 0; i < n; ++i) {
        int ip = (i + n - 1) % n;
        int inext = (i + 1) % n;
        HullPoint p0 = pts[ip];
        HullPoint p1 = pts[i];
        HullPoint p2 = pts[inext];

        float v1x = p1.x - p0.x;
        float v1y = p1.y - p0.y;
        float v2x = p2.x - p1.x;
        float v2y = p2.y - p1.y;
        float len1 = v1x * v1x + v1y * v1y;
        float len2 = v2x * v2x + v2y * v2y;

        // Keep if edge too short check fails (we keep endpoints), or angle is meaningful.
        bool keep = false;
        if (len1 < min_edge2 || len2 < min_edge2) {
            keep = true;
        } else {
            float inv1 = 1.0f / sqrtf(len1);
            float inv2 = 1.0f / sqrtf(len2);
            float dot = (v1x * v2x + v1y * v2y) * inv1 * inv2;
            if (dot < cos_thresh) keep = true; // significant turn
        }

        if (keep && out_count < max_out) {
            out[out_count++] = p1;
        }
    }

    // Ensure closed loop by repeating first if needed.
    if (out_count >= 3) {
        HullPoint first = out[0];
        HullPoint last = out[out_count - 1];
        if (fabsf(first.x - last.x) > 1e-4f || fabsf(first.y - last.y) > 1e-4f) {
            if (out_count < max_out) {
                out[out_count++] = first;
            }
        }
    }
    return out_count;
}
