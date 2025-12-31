#include "physics/rigid/collider_partition.h"

#include <math.h>
#include <string.h>
#include <stdio.h>
#include "physics/rigid/collider_utils.h"

// Compute intersections of the polygon with a vertical line x = xv.
static int intersect_vertical(const HullPoint *pts, int n, float xv, float *out_y, int max_y) {
    int count = 0;
    for (int i = 0; i < n; ++i) {
        HullPoint a = pts[i];
        HullPoint b = pts[(i + 1) % n];
        float minx = fminf(a.x, b.x);
        float maxx = fmaxf(a.x, b.x);
        if (xv < minx || xv > maxx) continue;
        float dx = b.x - a.x;
        float dy = b.y - a.y;
        float t = (fabsf(dx) > 1e-6f) ? ((xv - a.x) / dx) : 0.0f;
        if (t < 0.0f || t > 1.0f) continue;
        float y = a.y + dy * t;
        if (count < max_y) out_y[count++] = y;
    }
    // sort
    for (int i = 0; i < count; ++i) {
        for (int j = i + 1; j < count; ++j) {
            if (out_y[j] < out_y[i]) {
                float tmp = out_y[i]; out_y[i] = out_y[j]; out_y[j] = tmp;
            }
        }
    }
    // ensure even count
    if (count % 2 == 1) count--;
    return count;
}

// Simple vertical sweep partition into trapezoids between x-splits.
int collider_partition_trapezoids(const HullPoint *pts,
                                  int count,
                                  const bool *concave_flags,
                                  HullPoint parts[][8],
                                  int *part_counts,
                                  int max_parts,
                                  bool debug_logs) {
    if (!pts || count < 3 || !parts || !part_counts || max_parts <= 0) return 0;
    int n = count;
    if (collider_nearly_equal(pts[0], pts[count - 1], 1e-4f)) {
        n = count - 1;
    }
    float xs[520];
    int xcount = 0;
    float minx = pts[0].x, maxx = pts[0].x;
    for (int i = 0; i < n; ++i) {
        if (pts[i].x < minx) minx = pts[i].x;
        if (pts[i].x > maxx) maxx = pts[i].x;
    }
    xs[xcount++] = minx;
    xs[xcount++] = maxx;
    if (concave_flags) {
        for (int i = 0; i < n && xcount < 520; ++i) {
            if (concave_flags[i]) xs[xcount++] = pts[i].x;
        }
    }
    // unique + sort
    for (int i = 0; i < xcount; ++i) {
        for (int j = i + 1; j < xcount; ++j) {
            if (xs[j] < xs[i]) {
                float t = xs[i]; xs[i] = xs[j]; xs[j] = t;
            }
        }
    }
    int uniq = 0;
    for (int i = 0; i < xcount; ++i) {
        if (i == 0 || fabsf(xs[i] - xs[uniq - 1]) > 1e-4f) {
            xs[uniq++] = xs[i];
        }
    }
    xcount = uniq;
    if (xcount < 2) return 0;

    int part_total = 0;
    for (int xi = 0; xi < xcount - 1 && part_total < max_parts; ++xi) {
        float x0 = xs[xi];
        float x1 = xs[xi + 1];
        if (fabsf(x1 - x0) < 1e-4f) continue;
        float y0[512], y1[512];
        int c0 = intersect_vertical(pts, n, x0, y0, 512);
        int c1 = intersect_vertical(pts, n, x1, y1, 512);
        int pairs = (c0 < c1 ? c0 : c1);
        if (pairs % 2 == 1) pairs--;
        for (int k = 0; k + 1 < pairs && part_total < max_parts; k += 2) {
            float y0b = y0[k];
            float y0t = y0[k + 1];
            float y1b = y1[k];
            float y1t = y1[k + 1];
            parts[part_total][0] = (HullPoint){x0, y0b};
            parts[part_total][1] = (HullPoint){x1, y1b};
            parts[part_total][2] = (HullPoint){x1, y1t};
            parts[part_total][3] = (HullPoint){x0, y0t};
            part_counts[part_total] = 4;
            part_total++;
        }
    }
    if (debug_logs) {
        fprintf(stderr, "[partition] parts=%d\n", part_total);
    }
    return part_total;
}
