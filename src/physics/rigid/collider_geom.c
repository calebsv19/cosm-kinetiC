#include "physics/rigid/collider_geom.h"

#include <math.h>
#include "physics/rigid/collider_types.h"
#include "physics/rigid/collider_utils.h"

float polygon_area_signed(const HullPoint *pts, int count) {
    if (!pts || count < 3) return 0.0f;
    float area = 0.0f;
    for (int i = 0; i < count; ++i) {
        HullPoint a = pts[i];
        HullPoint b = pts[(i + 1) % count];
        area += (a.x * b.y - b.x * a.y);
    }
    return 0.5f * area;
}

float polygon_area(const HullPoint *pts, int count) {
    return fabsf(polygon_area_signed(pts, count));
}

HullPoint polygon_centroid(const HullPoint *pts, int count) {
    HullPoint c = (HullPoint){0.0f, 0.0f};
    if (!pts || count < 3) return c;
    float a = 0.0f;
    for (int i = 0; i < count; ++i) {
        HullPoint p0 = pts[i];
        HullPoint p1 = pts[(i + 1) % count];
        float cross = p0.x * p1.y - p1.x * p0.y;
        a += cross;
        c.x += (p0.x + p1.x) * cross;
        c.y += (p0.y + p1.y) * cross;
    }
    if (fabsf(a) < 1e-6f) return c;
    a *= 0.5f;
    float inv = 1.0f / (6.0f * a);
    c.x *= inv;
    c.y *= inv;
    return c;
}

bool point_in_polygon(const HullPoint *poly, int count, HullPoint p) {
    if (!poly || count < 3) return false;
    bool inside = false;
    for (int i = 0, j = count - 1; i < count; j = i++) {
        float xi = poly[i].x, yi = poly[i].y;
        float xj = poly[j].x, yj = poly[j].y;
        bool intersect = ((yi > p.y) != (yj > p.y)) &&
                         (p.x < (xj - xi) * (p.y - yi) / (yj - yi + 1e-8f) + xi);
        if (intersect) inside = !inside;
    }
    return inside;
}

bool point_in_triangle(const HullPoint *p,
                       const HullPoint *a,
                       const HullPoint *b,
                       const HullPoint *c) {
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

bool polygon_convex(const HullPoint *pts, int count) {
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

// Region helpers
bool region_contains_point_mask(const bool mask[128][128],
                                const bool visited[128][128],
                                int res,
                                float minx,
                                float spanx,
                                float miny,
                                float spany,
                                float gx,
                                float gy) {
    int rx = (int)(((gx) - minx) / spanx * (float)res);
    int ry = (int)(((gy) - miny) / spany * (float)res);
    if (rx < 0) rx = 0;
    if (rx >= res) rx = res - 1;
    if (ry < 0) ry = 0;
    if (ry >= res) ry = res - 1;
    bool visited_block = visited ? visited[ry][rx] : false;
    return mask[ry][rx] && !visited_block;
}

// Build a simple region representation: one solid region with all boundary indices.
int collider_build_regions(const HullPoint *pts,
                           int count,
                           ColliderRegion *regions,
                           int max_regions) {
    if (!pts || count < 3 || !regions || max_regions <= 0) return 0;
    int n = count;
    if (collider_nearly_equal(pts[0], pts[count - 1], 1e-4f)) n = count - 1;
    if (n < 3) return 0;
    int rcount = 0;
    ColliderRegion reg;
    reg.boundary_count = 0;
    reg.is_solid = true;
    for (int i = 0; i < n && reg.boundary_count < 512; ++i) {
        reg.boundary_indices[reg.boundary_count++] = i;
    }
    regions[rcount++] = reg;
    return rcount;
}
