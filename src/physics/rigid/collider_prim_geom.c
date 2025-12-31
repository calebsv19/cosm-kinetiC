#include "physics/rigid/collider_prim_geom.h"

#include "physics/math/math2d.h"

int collider_capsule_vertices(const ColliderPrimitive *p, Vec2 *out, int max_out) {
    if (!p || !out || max_out < 6) return 0;
    Vec2 u = vec2_normalize(p->axis);
    if (vec2_len(u) < 1e-6f) u = vec2(1.0f, 0.0f);
    Vec2 v = vec2(-u.y, u.x);
    float r = p->radius;
    float hl = p->half_extents.x; // stored half-length along axis
    Vec2 a = vec2_add(p->center, vec2_scale(u, -hl));
    Vec2 b = vec2_add(p->center, vec2_scale(u, hl));
    Vec2 u45 = vec2_scale(u, r * 0.70710678f);
    Vec2 v45 = vec2_scale(v, r * 0.70710678f);
    int w = 0;
    Vec2 candidates[10] = {
        vec2_add(b, vec2_scale(v, r)),
        vec2_add(b, vec2_add(u45, v45)),
        vec2_add(b, vec2_scale(u, r)),
        vec2_sub(b, vec2_add(u45, v45)),
        vec2_sub(b, vec2_scale(v, r)),
        vec2_sub(a, vec2_scale(v, r)),
        vec2_sub(a, vec2_add(u45, v45)),
        vec2_sub(a, vec2_scale(u, r)),
        vec2_add(a, vec2_add(u45, v45)),
        vec2_add(a, vec2_scale(v, r))
    };
    for (int i = 0; i < 10 && w < max_out; ++i) {
        out[w++] = candidates[i];
    }
    return w;
}

int collider_box_vertices(const ColliderPrimitive *p, Vec2 *out, int max_out) {
    if (!p || !out || max_out < 4) return 0;
    Vec2 u = vec2_normalize(p->axis);
    if (vec2_len(u) < 1e-6f) u = vec2(1.0f, 0.0f);
    Vec2 v = vec2(-u.y, u.x);
    Vec2 hx = vec2_scale(u, p->half_extents.x);
    Vec2 hy = vec2_scale(v, p->half_extents.y);
    Vec2 c = p->center;
    out[0] = vec2_add(c, vec2_add(hx, hy));
    out[1] = vec2_add(c, vec2_sub(hx, hy));
    out[2] = vec2_sub(c, vec2_add(hx, hy));
    out[3] = vec2_sub(c, vec2_sub(hx, hy));
    return 4;
}

int collider_hull_vertices(const ColliderPrimitive *p, Vec2 *out, int max_out) {
    if (!p || !out || max_out <= 0 || p->hull_count <= 0) return 0;
    int n = p->hull_count;
    if (n > max_out) n = max_out;
    for (int i = 0; i < n; ++i) out[i] = p->hull[i];
    return n;
}

int collider_primitive_to_vertices(const ColliderPrimitive *prim,
                                   Vec2 *out,
                                   int max_out) {
    if (!prim || !out || max_out <= 0) return 0;
    switch (prim->type) {
        case COLLIDER_PRIM_BOX:
            return collider_box_vertices(prim, out, max_out);
        case COLLIDER_PRIM_CAPSULE:
            return collider_capsule_vertices(prim, out, max_out);
        case COLLIDER_PRIM_HULL:
        default:
            return collider_hull_vertices(prim, out, max_out);
    }
}
