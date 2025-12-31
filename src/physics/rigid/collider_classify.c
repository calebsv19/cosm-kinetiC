#include "physics/rigid/collider_classify.h"

#include <math.h>
#include <stdio.h>
#include "physics/math/math2d.h"
#include "physics/rigid/collider_utils.h"

static inline ColliderSegmentClass classify_edge_simple(float len,
                                                        float turn_deg,
                                                        float short_len_thresh,
                                                        float straight_thresh,
                                                        float gentle_thresh) {
    if (len < short_len_thresh) return SEG_SHORT;
    if (turn_deg < straight_thresh) return SEG_STRAIGHT;
    if (turn_deg < gentle_thresh) return SEG_GENTLE;
    return SEG_TIGHT;
}

ColliderSegmentClass classify_span_basic(const HullPoint *span, int count) {
    if (!span || count < 2) return SEG_SHORT;
    float length = 0.0f;
    float max_dev = 0.0f;
    HullPoint a = span[0];
    HullPoint b = span[count - 1];
    Vec2 chord = vec2(b.x - a.x, b.y - a.y);
    float chord_len = vec2_len(chord);
    Vec2 chord_unit = (chord_len > 1e-5f) ? vec2_scale(chord, 1.0f / chord_len) : vec2(1.0f, 0.0f);
    Vec2 perp = vec2(-chord_unit.y, chord_unit.x);
    float base_proj = a.x * perp.x + a.y * perp.y;
    for (int i = 0; i < count - 1; ++i) {
        float dx = span[i + 1].x - span[i].x;
        float dy = span[i + 1].y - span[i].y;
        length += sqrtf(dx * dx + dy * dy);
    }
    for (int i = 0; i < count; ++i) {
        float proj = span[i].x * perp.x + span[i].y * perp.y;
        float dev = fabsf(proj - base_proj);
        if (dev > max_dev) max_dev = dev;
    }
    float short_thresh = 0.02f * (length > 1e-4f ? length : 1.0f);
    if (length < short_thresh) return SEG_SHORT;
    float dev_ratio = (length > 1e-4f) ? (max_dev / length) : 0.0f;
    if (dev_ratio < 0.01f) return SEG_STRAIGHT;
    if (dev_ratio < 0.08f) return SEG_GENTLE;
    return SEG_TIGHT;
}

int classify_segments(const HullPoint *pts,
                      int count,
                      const bool *corner_flags,
                      const bool *concave_flags,
                      ColliderSegment *out,
                      int max_out,
                      bool debug) {
    if (!pts || !out || max_out <= 0 || count < 3) return 0;
    int n = count;
    bool has_close = collider_nearly_equal(pts[0], pts[count - 1], 1e-4f);
    if (has_close) {
        n = count - 1;
    }
    if (n < 3) return 0;
    (void)concave_flags;

    float perim = 0.0f;
    float edge_len[512];
    float edge_turn_deg[512];
    HullPoint edges[512];
    if (n > 512) n = 512;
    for (int i = 0; i < n; ++i) {
        HullPoint a = pts[i];
        HullPoint b = pts[(i + 1) % n];
        float dx = b.x - a.x;
        float dy = b.y - a.y;
        edges[i].x = dx;
        edges[i].y = dy;
        float len = sqrtf(dx * dx + dy * dy);
        edge_len[i] = len;
        perim += len;
    }
    for (int i = 0; i < n; ++i) {
        HullPoint e0 = edges[i];
        HullPoint e1 = edges[(i + 1) % n];
        float len0 = edge_len[i];
        float len1 = edge_len[(i + 1) % n];
        float ang = 0.0f;
        if (len0 > 1e-5f && len1 > 1e-5f) {
            float dot = (e0.x * e1.x + e0.y * e1.y) / (len0 * len1);
            dot = collider_clamp_float(dot, -1.0f, 1.0f);
            ang = acosf(dot) * (180.0f / (float)M_PI);
        }
        edge_turn_deg[i] = fabsf(ang);
    }

    float straight_thresh = 5.0f;
    float gentle_thresh = 25.0f;
    float short_len_thresh = fmaxf(0.05f * perim, 1e-3f);

    ColliderSegment segs[256];
    int seg_count = 0;
    int run_start = 0;
    ColliderSegmentClass run_cls = classify_edge_simple(edge_len[0],
                                                        edge_turn_deg[0],
                                                        short_len_thresh,
                                                        straight_thresh,
                                                        gentle_thresh);
    for (int i = 0; i < n && seg_count < 256; ++i) {
        int next_edge = (i + 1) % n;
        bool corner_split = false;
        if (corner_flags) {
            int corner_idx = (i + 1) % n;
            corner_split = corner_flags[corner_idx];
        }
        ColliderSegmentClass cls_next = classify_edge_simple(edge_len[next_edge],
                                                             edge_turn_deg[next_edge],
                                                             short_len_thresh,
                                                             straight_thresh,
                                                             gentle_thresh);
        bool class_split = (cls_next != run_cls);
        if (corner_split || class_split || next_edge == 0) {
            int end = i;
            float len_sum = 0.0f;
            float turn_sum = 0.0f;
            float minx = pts[run_start].x, maxx = pts[run_start].x;
            float miny = pts[run_start].y, maxy = pts[run_start].y;
            for (int e = run_start; e <= end; ++e) {
                len_sum += edge_len[e];
                turn_sum += edge_turn_deg[e];
                int vnext = (e + 1);
                if (vnext >= n) vnext -= n;
                HullPoint v = pts[vnext];
                if (v.x < minx) minx = v.x;
                if (v.x > maxx) maxx = v.x;
                if (v.y < miny) miny = v.y;
                if (v.y > maxy) maxy = v.y;
            }
            float dx = maxx - minx;
            float dy = maxy - miny;
            float aspect = (fmaxf(dx, dy)) / fmaxf(fminf(dx, dy), 1e-4f);
            float mean_turn = (len_sum > 1e-4f) ? turn_sum / len_sum : 0.0f;
            int end_v = end + 1;
            if (end_v >= n) end_v = n - 1;
            if (end_v < run_start) end_v = run_start; // clamp if something went wrong
            if (end_v == run_start && run_start + 1 < n) end_v = run_start + 1;
            segs[seg_count++] = (ColliderSegment){
                .start_idx = run_start,
                .end_idx = end_v,
                .length = len_sum,
                .turn = turn_sum,
                .mean_turn = mean_turn,
                .aspect = aspect,
                .cls = run_cls,
                .solid_facing = true // initialize; builder may refine, but never leave uninitialized
            };
            run_start = next_edge;
            run_cls = cls_next;
        }
    }

    if (seg_count > max_out) seg_count = max_out;
    for (int i = 0; i < seg_count; ++i) {
        out[i] = segs[i];
        if (debug) {
            fprintf(stderr,
                    "[classify] seg[%d] start=%d end=%d cls=%d len=%.2f turn=%.2f\n",
                    i, out[i].start_idx, out[i].end_idx, out[i].cls, out[i].length, out[i].turn);
        }
    }
    if (debug) {
        fprintf(stderr, "[classify] segments=%d\n", seg_count);
    }
    return seg_count;
}
