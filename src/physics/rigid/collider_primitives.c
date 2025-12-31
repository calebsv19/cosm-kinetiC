#include "physics/rigid/collider_primitives.h"

#include <math.h>
#include "physics/math/math2d.h"
#include <stdio.h>
#include "physics/rigid/collider_utils.h"
#include "physics/rigid/collider_geom.h"
#include "physics/rigid/collider_tagging.h"
#include "physics/rigid/collider_prim_geom.h"

static int collect_segment_points(const HullPoint *pts,
                                  int count,
                                  int start_idx,
                                  int end_idx,
                                  HullPoint *out,
                                  int max_out) {
    if (!pts || !out || max_out <= 0 || count <= 0) return 0;
    if (end_idx < start_idx) return 0;
    int written = 0;
    for (int i = start_idx; i <= end_idx && written < max_out; ++i) {
        out[written++] = pts[i];
    }
    // Always include the vertex after end_idx (wrap) so spans have at least two edges (>=3 verts).
    if (written < max_out) {
        int next = (end_idx + 1) % count;
        out[written++] = pts[next];
    }
    return written;
}

static bool span_has_signed_curvature_flip(const HullPoint *span, int n) {
    if (!span || n < 3) return false;
    float prev_sign = 0.0f;
    for (int i = 0; i < n - 2; ++i) {
        HullPoint a = span[i];
        HullPoint b = span[i + 1];
        HullPoint c = span[i + 2];
        float cross = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
        float sign = (cross > 1e-5f) ? 1.0f : (cross < -1e-5f ? -1.0f : 0.0f);
        if (sign == 0.0f) continue;
        if (prev_sign == 0.0f) {
            prev_sign = sign;
            continue;
        }
        if (sign != prev_sign) return true;
    }
    return false;
}

static int split_span_on_curvature(const HullPoint *span,
                                   int span_count,
                                   HullPoint *out_a,
                                   int *out_a_count,
                                   HullPoint *out_b,
                                   int *out_b_count) {
    if (!span || span_count < 3 || !out_a || !out_b || !out_a_count || !out_b_count) return 0;
    int mid = span_count / 2;
    if (mid < 2) mid = 2;
    int a_count = 0;
    for (int i = 0; i <= mid && a_count < 512; ++i) out_a[a_count++] = span[i];
    int b_count = 0;
    for (int i = mid; i < span_count && b_count < 512; ++i) out_b[b_count++] = span[i];
    *out_a_count = a_count;
    *out_b_count = b_count;
    return (a_count >= 2 && b_count >= 2) ? 2 : 0;
}

static float max_axis_deviation(const HullPoint *pts,
                                int n,
                                Vec2 axis_unit,
                                HullPoint a) {
    float ax = axis_unit.x;
    float ay = axis_unit.y;
    float perp_x = -ay;
    float perp_y = ax;
    float base_proj = a.x * perp_x + a.y * perp_y;
    float max_dev = 0.0f;
    for (int i = 0; i < n; ++i) {
        float proj = pts[i].x * perp_x + pts[i].y * perp_y;
        float dev = fabsf(proj - base_proj);
        if (dev > max_dev) max_dev = dev;
    }
    return max_dev;
}

static int fit_capsule_from_span(const HullPoint *span,
                                 int span_count,
                                 float max_len_ratio,
                                 ColliderPrimitive *out,
                                 int max_out) {
    if (!span || !out || span_count < 2 || max_out <= 0) return 0;
    HullPoint a = span[0];
    HullPoint b = span[span_count - 1];
    Vec2 axis = vec2(b.x - a.x, b.y - a.y);
    float len = vec2_len(axis);
    if (len < 1e-5f) return 0;
    Vec2 axis_unit = vec2_scale(axis, 1.0f / len);
    float radius = max_axis_deviation(span, span_count, axis_unit, a) * 0.5f;
    float half_len = len * 0.5f;
    if (radius < 1e-4f) radius = 1e-4f;

    int written = 0;
    float ratio = half_len / radius;
    if (max_len_ratio > 0.0f && ratio > max_len_ratio && max_out >= 2) {
        HullPoint mid = {(a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f};
        ColliderPrimitive p0 = {
            .type = COLLIDER_PRIM_CAPSULE,
            .center = vec2((a.x + mid.x) * 0.5f, (a.y + mid.y) * 0.5f),
            .axis = axis_unit,
            .half_extents = vec2(vec2_len(vec2(mid.x - a.x, mid.y - a.y)) * 0.5f, 0.0f),
            .radius = radius
        };
        ColliderPrimitive p1 = {
            .type = COLLIDER_PRIM_CAPSULE,
            .center = vec2((mid.x + b.x) * 0.5f, (mid.y + b.y) * 0.5f),
            .axis = axis_unit,
            .half_extents = vec2(vec2_len(vec2(b.x - mid.x, b.y - mid.y)) * 0.5f, 0.0f),
            .radius = radius
        };
        out[written++] = p0;
        if (written < max_out) out[written++] = p1;
        return written;
    }

    ColliderPrimitive prim = {
        .type = COLLIDER_PRIM_CAPSULE,
        .center = vec2((a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f),
        .axis = axis_unit,
        .half_extents = vec2(half_len, 0.0f),
        .radius = radius
    };
    out[written++] = prim;
    return written;
}

static ColliderPrimitive fit_box_from_span(const HullPoint *span,
                                           int span_count) {
    HullPoint a = span[0];
    HullPoint b = span[span_count - 1];
    Vec2 axis = vec2(b.x - a.x, b.y - a.y);
    float len = vec2_len(axis);
    Vec2 axis_unit = (len > 1e-5f) ? vec2_scale(axis, 1.0f / len) : vec2(1.0f, 0.0f);
    Vec2 perp = vec2(-axis_unit.y, axis_unit.x);
    float min_t = 1e9f, max_t = -1e9f;
    float min_p = 1e9f, max_p = -1e9f;
    for (int i = 0; i < span_count; ++i) {
        Vec2 v = vec2(span[i].x, span[i].y);
        float t = vec2_dot(v, axis_unit);
        float p = vec2_dot(v, perp);
        if (t < min_t) min_t = t;
        if (t > max_t) max_t = t;
        if (p < min_p) min_p = p;
        if (p > max_p) max_p = p;
    }
    float cx = 0.5f * (min_t + max_t);
    float cy = 0.5f * (min_p + max_p);
    Vec2 center = vec2_add(vec2_scale(axis_unit, cx), vec2_scale(perp, cy));
    Vec2 half_ext = vec2(fmaxf(0.5f * (max_t - min_t), 1e-4f),
                         fmaxf(0.5f * (max_p - min_p), 1e-4f));
    ColliderPrimitive prim = {
        .type = COLLIDER_PRIM_BOX,
        .center = center,
        .axis = axis_unit,
        .half_extents = half_ext,
        .radius = 0.0f,
        .hull_count = 0
    };
    return prim;
}

static int fit_hull_from_span(const HullPoint *span,
                              int span_count,
                              int max_hull_verts,
                              ColliderPrimitive *out,
                              int max_out) {
    if (!span || span_count < 3 || max_out <= 0) return 0;
    HullPoint hull[32];
    int hcount = collider_compute_convex_hull(span, span_count, hull, 32);
    if (hcount < 3) return 0;
    if (hcount > max_hull_verts) hcount = max_hull_verts;
    ColliderPrimitive prim = {
        .type = COLLIDER_PRIM_HULL,
        .center = vec2(0.0f, 0.0f),
        .axis = vec2(1.0f, 0.0f),
        .half_extents = vec2(0.0f, 0.0f),
        .radius = 0.0f,
        .hull_count = hcount
    };
    for (int i = 0; i < hcount; ++i) {
        prim.hull[i] = vec2(hull[i].x, hull[i].y);
    }
    out[0] = prim;
    return 1;
}

int collider_fit_primitives(const HullPoint *pts,
                            int count,
                            const ColliderSegment *segments,
                            int seg_count,
                            const AppConfig *cfg,
                            ColliderPrimitive *out,
                            int max_out,
                            const bool region_mask[128][128],
                            const bool region_visited[128][128],
                            int region_res,
                            float minx, float spanx,
                            float miny, float spany) {
    if (!pts || count < 3 || !segments || seg_count <= 0 || !cfg || !out || max_out <= 0) return 0;
    int written = 0;
    int reject_open = 0;
    int reject_region = 0;
    int reject_span = 0;
    for (int si = 0; si < seg_count && written < max_out; ++si) {
        const ColliderSegment *seg = &segments[si];
        if (!seg->solid_facing) {
            reject_open++;
            continue;
        }
        HullPoint span[512];
        int span_count = collect_segment_points(pts, count, seg->start_idx, seg->end_idx, span, 512);
        if (span_count < 2) {
            reject_span++;
            continue;
        }

        // Optional curvature split to improve hourglass-like shapes.
        if (span_has_signed_curvature_flip(span, span_count) && (max_out - written) > 1) {
            HullPoint sa[512], sb[512];
            int ca = 0, cb = 0;
            if (split_span_on_curvature(span, span_count, sa, &ca, sb, &cb) == 2) {
                ColliderSegment tmpSegs[2] = {*seg, *seg};
                tmpSegs[0].start_idx = 0; tmpSegs[0].end_idx = ca - 1;
                tmpSegs[1].start_idx = 0; tmpSegs[1].end_idx = cb - 1;
                const HullPoint *tmpPts[2] = {sa, sb};
                const int tmpCount[2] = {ca, cb};
                for (int ti = 0; ti < 2 && written < max_out; ++ti) {
                    const ColliderSegment *ts = &tmpSegs[ti];
                    const HullPoint *sp = tmpPts[ti];
                    int sc = tmpCount[ti];
                    if (sc < 2) continue;
                    ColliderPrimitive candidate;
                    int produced = 0;
                    switch (ts->cls) {
                        case SEG_STRAIGHT:
                            candidate = fit_box_from_span(sp, sc);
                            produced = 1;
                            break;
                        case SEG_GENTLE:
                            produced = fit_capsule_from_span(sp,
                                                             sc,
                                                             cfg->collider_capsule_max_len_ratio,
                                                             &candidate,
                                                             1);
                            break;
                        case SEG_TIGHT:
                        case SEG_SHORT:
                        default:
                            produced = fit_hull_from_span(sp,
                                                          sc,
                                                          cfg->collider_max_hull_vertices,
                                                          &candidate,
                                                          1);
                            break;
                    }
                    if (produced > 0) {
                        bool ok = true;
                        Vec2 verts[20];
                        int vc = collider_primitive_to_vertices(&candidate, verts, 20);
                        if (vc >= 3) {
                            for (int v = 0; v < vc; ++v) {
                                if (!region_contains_point_mask(region_mask, region_visited, region_res, minx, spanx, miny, spany, verts[v].x, verts[v].y)) {
                                    ok = false;
                                    reject_region++;
                                    break;
                                }
                            }
                        }
                        if (ok) out[written++] = candidate;
                    }
                    if (written >= cfg->collider_max_primitives) break;
                }
                continue;
            }
        }

        switch (seg->cls) {
            case SEG_STRAIGHT: {
                ColliderPrimitive cand = fit_box_from_span(span, span_count);
                bool ok = true;
                Vec2 verts[16];
                int vc = collider_box_vertices(&cand, verts, 16);
                if (vc >= 3 && region_mask) {
                    float cx = 0.0f, cy = 0.0f;
                    for (int v = 0; v < vc; ++v) { cx += verts[v].x; cy += verts[v].y; }
                    cx /= (float)vc; cy /= (float)vc;
                    if (!region_contains_point_mask(region_mask, region_visited, region_res, minx, spanx, miny, spany, cx, cy)) {
                        ok = false;
                        reject_region++;
                    }
                }
                if (ok) out[written++] = cand;
            } break;
            case SEG_GENTLE: {
                ColliderPrimitive cand[2];
                int produced = fit_capsule_from_span(span,
                                                     span_count,
                                                     cfg->collider_capsule_max_len_ratio,
                                                     cand,
                                                     2);
                for (int i = 0; i < produced && written < max_out; ++i) {
                    bool ok = true;
                    Vec2 verts[20];
                    int vc = collider_capsule_vertices(&cand[i], verts, 20);
                    if (vc >= 3 && region_mask) {
                        float cx = 0.0f, cy = 0.0f;
                        for (int v = 0; v < vc; ++v) { cx += verts[v].x; cy += verts[v].y; }
                        cx /= (float)vc; cy /= (float)vc;
                        if (!region_contains_point_mask(region_mask, region_visited, region_res, minx, spanx, miny, spany, cx, cy)) {
                            ok = false;
                            reject_region++;
                        }
                    }
                    if (ok) out[written++] = cand[i];
                }
            } break;
            case SEG_TIGHT:
            case SEG_SHORT:
            default: {
                ColliderPrimitive cand;
                int produced = fit_hull_from_span(span,
                                                  span_count,
                                                  cfg->collider_max_hull_vertices,
                                                  &cand,
                                                  1);
                if (produced > 0) {
                    bool ok = true;
                    Vec2 verts[16];
                    int vc = collider_hull_vertices(&cand, verts, 16);
                    if (vc >= 3 && region_mask) {
                        float cx = 0.0f, cy = 0.0f;
                        for (int v = 0; v < vc; ++v) { cx += verts[v].x; cy += verts[v].y; }
                        cx /= (float)vc; cy /= (float)vc;
                        if (!region_contains_point_mask(region_mask, region_visited, region_res, minx, spanx, miny, spany, cx, cy)) {
                            ok = false;
                            reject_region++;
                        }
                    }
                    if (ok) out[written++] = cand;
                }
            } break;
        }
        if (written >= cfg->collider_max_primitives) {
            written = cfg->collider_max_primitives;
            break;
        }
    }
    if (written > max_out) written = max_out;
    if (cfg->collider_debug_logs) {
        fprintf(stderr,
                "[collider] primgen seg=%d kept=%d rej_open=%d rej_span=%d rej_region=%d out=%d\n",
                seg_count, seg_count - (reject_open + reject_span), reject_open, reject_span, reject_region, written);
    }
    return written;
}
