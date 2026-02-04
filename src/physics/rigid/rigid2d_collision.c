#include "physics/rigid/rigid2d_collision.h"

#include <math.h>
#include <string.h>

typedef struct {
    Vec2 normal;
    float penetration;
    int  hit;
    int  from_b; // 0 if axis from A, 1 if from B
} Contact;

static Vec2 rotate_vec(Vec2 v, float angle) {
    float c = cosf(angle);
    float s = sinf(angle);
    return vec2(v.x * c - v.y * s, v.x * s + v.y * c);
}

static float poly_area_local(const Vec2 *verts, int count) {
    if (!verts || count < 3) return 0.0f;
    float a = 0.0f;
    for (int i = 0, j = count - 1; i < count; j = i++) {
        a += verts[j].x * verts[i].y - verts[i].x * verts[j].y;
    }
    return 0.5f * a;
}

static int poly_world_verts(const RigidBody2D *b, Vec2 *out, int cap) {
    if (!b || !out || cap <= 0 || b->poly.count <= 0) return 0;
    int n = b->poly.count;
    if (n > cap) n = cap;
    float c = cosf(b->angle);
    float s = sinf(b->angle);
    for (int i = 0; i < n; ++i) {
        float lx = b->poly.verts[i].x;
        float ly = b->poly.verts[i].y;
        float wx = lx * c - ly * s + b->position.x;
        float wy = lx * s + ly * c + b->position.y;
        out[i] = vec2(wx, wy);
    }
    return n;
}

int rigid2d_poly_world(const RigidBody2D *b, Vec2 *out, int cap) {
    return poly_world_verts(b, out, cap);
}

static Vec2 edge_normal(Vec2 a, Vec2 b) {
    Vec2 e = vec2_sub(b, a);
    Vec2 n = vec2(-e.y, e.x);
    float len = vec2_len(n);
    if (len < 1e-6f) return vec2(0.0f, 0.0f);
    return vec2_scale(n, 1.0f / len);
}

static int reference_face_index(const Vec2 *verts, int count, Vec2 normal) {
    int idx = 0;
    float max_dot = -1e9f;
    for (int i = 0; i < count; ++i) {
        int j = (i + 1) % count;
        Vec2 n = edge_normal(verts[i], verts[j]);
        float d = vec2_dot(n, normal);
        if (d > max_dot) {
            max_dot = d;
            idx = i;
        }
    }
    return idx;
}

static int incident_face_index(const Vec2 *verts, int count, Vec2 normal) {
    int idx = 0;
    float min_dot = 1e9f;
    for (int i = 0; i < count; ++i) {
        int j = (i + 1) % count;
        Vec2 n = edge_normal(verts[i], verts[j]);
        float d = vec2_dot(n, normal);
        if (d < min_dot) {
            min_dot = d;
            idx = i;
        }
    }
    return idx;
}

static int clip_segment(Vec2 in0, Vec2 in1, Vec2 n, float c, Vec2 *out0, Vec2 *out1) {
    float d0 = vec2_dot(n, in0) - c;
    float d1 = vec2_dot(n, in1) - c;
    int count = 0;
    if (d0 <= 0.0f) out0[count++] = in0;
    if (d1 <= 0.0f) out0[count++] = in1;
    if (d0 * d1 < 0.0f) {
        float t = d0 / (d0 - d1);
        Vec2 p = vec2_add(in0, vec2_scale(vec2_sub(in1, in0), t));
        out0[count++] = p;
    }
    if (count == 2 && out1) {
        out1[0] = out0[0];
        out1[1] = out0[1];
    }
    return count;
}

void rigid2d_update_body_aabb(RigidBody2D *b) {
    if (!b) return;
    const float margin = 1.0f; // small expansion to avoid missing near contacts
    if (b->shape == RIGID2D_SHAPE_POLY) {
        float minx = 1e9f, miny = 1e9f, maxx = -1e9f, maxy = -1e9f;
        float c = cosf(b->angle);
        float s = sinf(b->angle);
        for (int i = 0; i < b->poly.count; ++i) {
            float lx = b->poly.verts[i].x;
            float ly = b->poly.verts[i].y;
            float wx = lx * c - ly * s + b->position.x;
            float wy = lx * s + ly * c + b->position.y;
            if (wx < minx) minx = wx;
            if (wx > maxx) maxx = wx;
            if (wy < miny) miny = wy;
            if (wy > maxy) maxy = wy;
        }
        b->poly.aabb_min_x = minx - margin;
        b->poly.aabb_min_y = miny - margin;
        b->poly.aabb_max_x = maxx + margin;
        b->poly.aabb_max_y = maxy + margin;
    } else if (b->shape == RIGID2D_SHAPE_AABB) {
        float hx = b->half_extents.x;
        float hy = b->half_extents.y;
        b->poly.aabb_min_x = b->position.x - hx - margin;
        b->poly.aabb_max_x = b->position.x + hx + margin;
        b->poly.aabb_min_y = b->position.y - hy - margin;
        b->poly.aabb_max_y = b->position.y + hy + margin;
    } else if (b->shape == RIGID2D_SHAPE_CIRCLE) {
        float r = b->radius;
        b->poly.aabb_min_x = b->position.x - r - margin;
        b->poly.aabb_max_x = b->position.x + r + margin;
        b->poly.aabb_min_y = b->position.y - r - margin;
        b->poly.aabb_max_y = b->position.y + r + margin;
    }
}

int rigid2d_aabb_overlap(const RigidBody2D *a, const RigidBody2D *b) {
    if (!a || !b) return 0;
    float aminx = a->poly.aabb_min_x;
    float amaxx = a->poly.aabb_max_x;
    float aminy = a->poly.aabb_min_y;
    float amaxy = a->poly.aabb_max_y;
    float bminx = b->poly.aabb_min_x;
    float bmaxx = b->poly.aabb_max_x;
    float bminy = b->poly.aabb_min_y;
    float bmaxy = b->poly.aabb_max_y;
    return (amaxx >= bminx && bmaxx >= aminx &&
            amaxy >= bminy && bmaxy >= aminy);
}

static Contact sat_poly_poly(const RigidBody2D *a, const RigidBody2D *b) {
    Contact c = {.hit = 0, .penetration = 0.0f, .normal = vec2(0.0f, 0.0f), .from_b = 0};
    if (!a || !b || a->poly.count < 3 || b->poly.count < 3) return c;
    if (fabsf(poly_area_local(a->poly.verts, a->poly.count)) < 1e-6f) return c;
    if (fabsf(poly_area_local(b->poly.verts, b->poly.count)) < 1e-6f) return c;

    const RigidBody2D *polys[2] = {a, b};
    float min_pen = 1e9f;
    Vec2 best_normal = vec2(0.0f, 0.0f);
    int normal_from_b = 0;

    for (int idx = 0; idx < 2; ++idx) {
        const RigidBody2D *p = polys[idx];
        for (int i = 0; i < p->poly.count; ++i) {
            Vec2 v0 = rotate_vec(p->poly.verts[i], p->angle);
            Vec2 v1 = rotate_vec(p->poly.verts[(i + 1) % p->poly.count], p->angle);
            v0 = vec2_add(v0, p->position);
            v1 = vec2_add(v1, p->position);
            Vec2 edge = vec2_sub(v1, v0);
            Vec2 axis = vec2(-edge.y, edge.x);
            float len = vec2_len(axis);
            if (len < 1e-6f) continue;
            axis = vec2_scale(axis, 1.0f / len);

            float minA = 1e9f, maxA = -1e9f;
            for (int va = 0; va < a->poly.count; ++va) {
                Vec2 wp = vec2_add(rotate_vec(a->poly.verts[va], a->angle), a->position);
                float proj = vec2_dot(wp, axis);
                if (proj < minA) minA = proj;
                if (proj > maxA) maxA = proj;
            }
            float minB = 1e9f, maxB = -1e9f;
            for (int vb = 0; vb < b->poly.count; ++vb) {
                Vec2 wp = vec2_add(rotate_vec(b->poly.verts[vb], b->angle), b->position);
                float proj = vec2_dot(wp, axis);
                if (proj < minB) minB = proj;
                if (proj > maxB) maxB = proj;
            }
            float overlap = fminf(maxA, maxB) - fmaxf(minA, minB);
            if (overlap <= 0.0f) {
                return c; // separating axis found
            }
            if (overlap < min_pen) {
                min_pen = overlap;
                best_normal = axis;
                normal_from_b = (idx == 1);
                Vec2 delta = vec2_sub(b->position, a->position);
                if (vec2_dot(delta, best_normal) < 0.0f) {
                    best_normal = vec2_scale(best_normal, -1.0f);
                }
            }
        }
    }

    c.hit = 1;
    c.penetration = min_pen;
    c.normal = best_normal;
    c.from_b = normal_from_b;
    return c;
}

static Contact sat_circle_poly(const RigidBody2D *circle, const RigidBody2D *poly) {
    Contact c = {.hit = 0, .penetration = 0.0f, .normal = vec2(0.0f, 0.0f)};
    if (!circle || !poly || poly->poly.count <= 0) return c;
    Vec2 center = circle->position;
    float radius = circle->radius;

    float min_pen = 1e9f;
    Vec2 best_normal = vec2(0.0f, 0.0f);
    int hit = 0;

    // Test polygon edges
    for (int i = 0; i < poly->poly.count; ++i) {
        Vec2 v0 = vec2_add(rotate_vec(poly->poly.verts[i], poly->angle), poly->position);
        Vec2 v1 = vec2_add(rotate_vec(poly->poly.verts[(i + 1) % poly->poly.count], poly->angle), poly->position);
        Vec2 edge = vec2_sub(v1, v0);
        Vec2 axis = vec2(-edge.y, edge.x);
        float len = vec2_len(axis);
        if (len < 1e-6f) continue;
        axis = vec2_scale(axis, 1.0f / len);

        float minP = 1e9f, maxP = -1e9f;
        for (int vp = 0; vp < poly->poly.count; ++vp) {
            Vec2 wp = vec2_add(rotate_vec(poly->poly.verts[vp], poly->angle), poly->position);
            float proj = vec2_dot(wp, axis);
            if (proj < minP) minP = proj;
            if (proj > maxP) maxP = proj;
        }
        float projC = vec2_dot(center, axis);
        float minC = projC - radius;
        float maxC = projC + radius;
        float overlap = fminf(maxP, maxC) - fmaxf(minP, minC);
        if (overlap <= 0.0f) {
            return c; // separating axis
        }
        if (overlap < min_pen) {
            min_pen = overlap;
            best_normal = axis;
            Vec2 delta = vec2_sub(center, poly->position);
            if (vec2_dot(delta, best_normal) < 0.0f) {
                best_normal = vec2_scale(best_normal, -1.0f);
            }
        }
        hit = 1;
    }

    if (!hit) return c;
    c.hit = 1;
    c.penetration = min_pen;
    c.normal = best_normal;
    return c;
}

static int build_manifold_poly_poly(const RigidBody2D *a,
                                    const RigidBody2D *b,
                                    const Contact *sep,
                                    RigidManifold *m) {
    if (!a || !b || !sep || !m || !sep->hit) return 0;
    Vec2 va[32], vb[32];
    int ca = poly_world_verts(a, va, 32);
    int cb = poly_world_verts(b, vb, 32);
    if (ca < 3 || cb < 3) return 0;
    int ref_idx = reference_face_index(va, ca, sep->normal);
    int ref_next = (ref_idx + 1) % ca;
    Vec2 ref_v0 = va[ref_idx];
    Vec2 ref_v1 = va[ref_next];
    Vec2 ref_n = edge_normal(ref_v0, ref_v1);

    int inc_idx = incident_face_index(vb, cb, sep->normal);
    int inc_next = (inc_idx + 1) % cb;
    Vec2 inc_v0 = vb[inc_idx];
    Vec2 inc_v1 = vb[inc_next];

    Vec2 ref_edge = vec2_sub(ref_v1, ref_v0);
    float ref_len = vec2_len(ref_edge);
    if (ref_len < 1e-6f) return 0;
    Vec2 ref_tan = vec2_scale(ref_edge, 1.0f / ref_len);
    Vec2 ref_left_n = vec2(-ref_tan.y, ref_tan.x);
    Vec2 ref_right_n = vec2(ref_tan.y, -ref_tan.x);
    float ref_left_c = vec2_dot(ref_left_n, ref_v0);
    float ref_right_c = vec2_dot(ref_right_n, ref_v1);
    float ref_front_c = vec2_dot(ref_n, ref_v0);

    Vec2 clip1[2];
    Vec2 clip2[2];
    int c1 = clip_segment(inc_v0, inc_v1, ref_left_n, ref_left_c, clip1, NULL);
    if (c1 < 2) return 0;
    int c2 = clip_segment(clip1[0], clip1[1], ref_right_n, ref_right_c, clip2, NULL);
    if (c2 < 2) return 0;

    int contact_count = 0;
    for (int i = 0; i < 2; ++i) {
        float sep_d = vec2_dot(ref_n, clip2[i]) - ref_front_c;
        float pen = -sep_d;
        if (pen >= 0.0f) {
            m->contacts[contact_count].position = clip2[i];
            m->contacts[contact_count].penetration = pen;
            m->contacts[contact_count].normal_impulse = 0.0f;
            m->contacts[contact_count].tangent_impulse = 0.0f;
            contact_count++;
        }
    }
    if (contact_count == 0) return 0;
    m->contact_count = contact_count;
    m->normal = sep->normal;
    m->depth = sep->penetration;
    return 1;
}

int rigid2d_collision_manifold(const RigidBody2D *a,
                               const RigidBody2D *b,
                               RigidManifold *m) {
    if (!a || !b || !m) return 0;
    memset(m, 0, sizeof(*m));
    m->body_a = -1;
    m->body_b = -1;
    m->restitution = 0.5f * (a->restitution + b->restitution);
    m->friction = 0.5f * (a->friction + b->friction);
    // Keep friction modest to avoid spin explosions on off-center contacts.
    m->friction = fminf(m->friction, 0.15f);

    // Treat AABB as a simple 4-vert poly so the SAT path can handle it.
    RigidBody2D pa = *a;
    RigidBody2D pb = *b;
    Vec2 aabb_verts[4];
    Vec2 bbb_verts[4];
    if (a->shape == RIGID2D_SHAPE_AABB) {
        pa.shape = RIGID2D_SHAPE_POLY;
        pa.poly.count = 4;
        pa.poly.verts = aabb_verts;
        float hx = a->half_extents.x;
        float hy = a->half_extents.y;
        aabb_verts[0] = vec2(-hx, -hy);
        aabb_verts[1] = vec2( hx, -hy);
        aabb_verts[2] = vec2( hx,  hy);
        aabb_verts[3] = vec2(-hx,  hy);
    }
    if (b->shape == RIGID2D_SHAPE_AABB) {
        pb.shape = RIGID2D_SHAPE_POLY;
        pb.poly.count = 4;
        pb.poly.verts = bbb_verts;
        float hx = b->half_extents.x;
        float hy = b->half_extents.y;
        bbb_verts[0] = vec2(-hx, -hy);
        bbb_verts[1] = vec2( hx, -hy);
        bbb_verts[2] = vec2( hx,  hy);
        bbb_verts[3] = vec2(-hx,  hy);
    }
    const RigidBody2D *aa = &pa;
    const RigidBody2D *bb = &pb;

    Contact contact = {.hit = 0};
    int built = 0;
    if (aa->shape == RIGID2D_SHAPE_CIRCLE && bb->shape == RIGID2D_SHAPE_POLY) {
        contact = sat_circle_poly(aa, bb);
        if (contact.hit && contact.penetration > 0.0f) {
            m->normal = contact.normal;
            m->depth = contact.penetration;
            m->contact_count = 1;
            m->contacts[0].position =
                vec2_sub(aa->position, vec2_scale(contact.normal, aa->radius));
            m->contacts[0].penetration = contact.penetration;
            m->contacts[0].normal_impulse = 0.0f;
            m->contacts[0].tangent_impulse = 0.0f;
            built = 1;
        }
    } else if (aa->shape == RIGID2D_SHAPE_POLY && bb->shape == RIGID2D_SHAPE_CIRCLE) {
        contact = sat_circle_poly(bb, aa);
        if (contact.hit && contact.penetration > 0.0f) {
            m->normal = vec2_scale(contact.normal, -1.0f);
            m->depth = contact.penetration;
            m->contact_count = 1;
            m->contacts[0].position =
                vec2_add(bb->position, vec2_scale(contact.normal, bb->radius));
            m->contacts[0].penetration = contact.penetration;
            m->contacts[0].normal_impulse = 0.0f;
            m->contacts[0].tangent_impulse = 0.0f;
            built = 1;
        }
    } else if (aa->shape == RIGID2D_SHAPE_POLY && bb->shape == RIGID2D_SHAPE_POLY) {
        contact = sat_poly_poly(aa, bb);
        if (contact.hit && contact.penetration > 0.0f) {
            // Ensure normal always points from reference (A) to incident (B).
            const RigidBody2D *ref = aa;
            const RigidBody2D *inc = bb;
            Vec2 normal = contact.normal;
            if (contact.from_b) {
                // Swap roles so reference face comes from the polygon that supplied the axis.
                ref = bb;
                inc = aa;
                normal = vec2_scale(normal, -1.0f);
            }
            m->depth = contact.penetration;
            m->normal = normal;
            built = build_manifold_poly_poly(ref, inc, &contact, m);
            if (!built) {
                // Fallback: place contact using support points along the SAT normal to reduce lever-arm torque.
                Vec2 va[32], vb[32];
                int ca = poly_world_verts(ref, va, 32);
                int cb = poly_world_verts(inc, vb, 32);
                float maxA = -1e9f, maxB = -1e9f;
                Vec2 supA = ref->position, supB = inc->position;
                for (int i = 0; i < ca; ++i) {
                    float d = vec2_dot(va[i], normal);
                    if (d > maxA) { maxA = d; supA = va[i]; }
                }
                for (int i = 0; i < cb; ++i) {
                    float d = vec2_dot(vb[i], vec2_scale(normal, -1.0f));
                    if (d > maxB) { maxB = d; supB = vb[i]; }
                }
                float overlap = contact.penetration;
                Vec2 mid = vec2_scale(vec2_add(supA, supB), 0.5f);
                m->normal = normal;
                m->depth = overlap;
                m->contact_count = 1;
                m->contacts[0].position = mid;
                m->contacts[0].penetration = overlap;
                m->contacts[0].normal_impulse = 0.0f;
                m->contacts[0].tangent_impulse = 0.0f;
                built = 1;
            }
        }
    }
    if (built) {
        // Build tangent for friction
        m->tangent = vec2(-m->normal.y, m->normal.x);
    }
    return built;
}

void rigid2d_positional_correction(RigidBody2D *a,
                                   RigidBody2D *b,
                                   const RigidManifold *m) {
    if (!a || !b || !m || m->contact_count <= 0) return;
    const float percent = 0.20f; // slightly stronger push-out to clear initial sticking
    const float slop = 2.0f;     // tolerate tiny overlaps
    float inv_mass_sum = a->inv_mass + b->inv_mass;
    if (inv_mass_sum <= 0.0f) return;
    float corr_mag = fmaxf(m->depth - slop, 0.0f) * percent / inv_mass_sum;
    Vec2 correction = vec2_scale(m->normal, corr_mag);
    if (!a->is_static) a->position = vec2_sub(a->position, vec2_scale(correction, a->inv_mass));
    if (!b->is_static) b->position = vec2_add(b->position, vec2_scale(correction, b->inv_mass));
}

void rigid2d_resolve_impulse_basic(RigidBody2D *a,
                                   RigidBody2D *b,
                                   RigidManifold *m,
                                   float dt) {
    if (!a || !b || !m || m->contact_count <= 0) return;
    Vec2 normal = m->normal;
    const float baumgarte = 0.25f;
    const float bias_slop = 2.0f;
    const float inv_dt = (dt > 1e-5f) ? (1.0f / dt) : 60.0f;
    // First pass: normal impulses
    for (int ci = 0; ci < m->contact_count; ++ci) {
        Vec2 cp = m->contacts[ci].position;
        Vec2 rA = vec2_sub(cp, a->position);
        Vec2 rB = vec2_sub(cp, b->position);
        // Relative velocity at contact
        Vec2 rv = vec2_sub(vec2_add(b->velocity, vec2(-b->angular_velocity * rB.y,
                                                      b->angular_velocity * rB.x)),
                           vec2_add(a->velocity, vec2(-a->angular_velocity * rA.y,
                                                      a->angular_velocity * rA.x)));
        float vel_along_normal = vec2_dot(rv, normal);
        if (vel_along_normal > 0.0f) continue;

        float raN = rA.x * normal.y - rA.y * normal.x; // cross(rA, n)
        float rbN = rB.x * normal.y - rB.y * normal.x; // cross(rB, n)
        float inv_mass_norm = a->inv_mass + b->inv_mass +
                              (raN * raN) * a->inv_inertia + (rbN * rbN) * b->inv_inertia;
        if (inv_mass_norm <= 0.0f) continue;

        const float rest_thresh = 0.5f;
        float e = (vel_along_normal < -rest_thresh) ? m->restitution : 0.0f;
        float bias = 0.0f;
        float pen = m->contacts[ci].penetration;
        if (pen > bias_slop) {
            bias = -baumgarte * (pen - bias_slop) * inv_dt;
        }
        float j = -(1.0f + e) * vel_along_normal + bias;
        j /= inv_mass_norm;
        // Clamp angular contribution to avoid huge spin from shallow contacts.
        const float max_dw = 0.3f; // rad/s per iteration
        float dwA = raN * j * a->inv_inertia;
        float dwB = rbN * j * b->inv_inertia;
        float scale = 1.0f;
        if (fabsf(dwA) > max_dw) scale = fminf(scale, max_dw / fabsf(dwA));
        if (fabsf(dwB) > max_dw) scale = fminf(scale, max_dw / fabsf(dwB));
        j *= scale;

        Vec2 impulse = vec2_scale(normal, j);
        if (!a->is_static) {
            a->velocity = vec2_sub(a->velocity, vec2_scale(impulse, a->inv_mass));
            a->angular_velocity -= raN * j * a->inv_inertia;
        }
        if (!b->is_static) {
            b->velocity = vec2_add(b->velocity, vec2_scale(impulse, b->inv_mass));
            b->angular_velocity += rbN * j * b->inv_inertia;
        }
        m->contacts[ci].normal_impulse = j;
    }
    // Second pass: friction using updated velocities
    for (int ci = 0; ci < m->contact_count; ++ci) {
        Vec2 cp = m->contacts[ci].position;
        Vec2 rA = vec2_sub(cp, a->position);
        Vec2 rB = vec2_sub(cp, b->position);
        Vec2 rv = vec2_sub(vec2_add(b->velocity, vec2(-b->angular_velocity * rB.y,
                                                      b->angular_velocity * rB.x)),
                           vec2_add(a->velocity, vec2(-a->angular_velocity * rA.y,
                                                      a->angular_velocity * rA.x)));
        Vec2 tdir = vec2_sub(rv, vec2_scale(normal, vec2_dot(rv, normal)));
        float tlen = vec2_len(tdir);
        if (tlen < 1e-5f) continue;
        Vec2 tnorm = vec2_scale(tdir, 1.0f / tlen);
        float vel_along_tangent = vec2_dot(rv, tnorm);
        float raT = rA.x * tnorm.y - rA.y * tnorm.x;
        float rbT = rB.x * tnorm.y - rB.y * tnorm.x;
        float inv_mass_tan = a->inv_mass + b->inv_mass +
                             (raT * raT) * a->inv_inertia + (rbT * rbT) * b->inv_inertia;
        if (inv_mass_tan <= 0.0f) continue;
        float jt = -vel_along_tangent / inv_mass_tan;
        float friction = m->friction;
        float jn_cap = fabsf(m->contacts[ci].normal_impulse);
        float max_fric_cap = jn_cap * friction;
        if (jt > max_fric_cap) jt = max_fric_cap;
        else if (jt < -max_fric_cap) jt = -max_fric_cap;
        Vec2 ft = vec2_scale(tnorm, jt);
        if (!a->is_static) {
            a->velocity = vec2_sub(a->velocity, vec2_scale(ft, a->inv_mass));
            a->angular_velocity -= raT * jt * a->inv_inertia;
        }
        if (!b->is_static) {
            b->velocity = vec2_add(b->velocity, vec2_scale(ft, b->inv_mass));
            b->angular_velocity += rbT * jt * b->inv_inertia;
        }
    }
}
