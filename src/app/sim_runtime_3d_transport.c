/* Qualification velocity transport. App-owned discretization, no UI policy. */
#include "app/sim_runtime_3d_solver.h"
#include <float.h>
#include <math.h>

/* Cell-centred voxel traversal. Tied crossings visit each side conservatively,
 * so a trajectory cannot jump across a one-cell wall or its corner. */
static bool clear_segment(const SimRuntime3DDomainDesc *d, const uint8_t *solid,
                          const float start[3], const float end[3]) {
    if (!solid) return true;
    int q[3], step[3], dims[3] = {d->grid_w, d->grid_h, d->grid_d};
    float next[3], delta[3];
    for (int a = 0; a < 3; ++a) {
        q[a] = (int)floorf(start[a] + .5f);
        float v = end[a] - start[a];
        step[a] = v > 0 ? 1 : -1;
        delta[a] = v == 0 ? INFINITY : fabsf(1 / v);
        next[a] = v == 0 ? INFINITY : (q[a] + .5f * step[a] - start[a]) / v;
    }
    for (;;) {
        int axis = next[0] < next[1] ? 0 : 1;
        if (next[2] < next[axis]) axis = 2;
        if (next[axis] > 1) return true;
        int tied = 0;
        for (int a = 0; a < 3; ++a)
            if (fabsf(next[a] - next[axis]) < 1e-6f) tied |= 1 << a;
        for (int subset = tied; subset; subset = (subset - 1) & tied) {
            int n[3] = {q[0], q[1], q[2]};
            for (int a = 0; a < 3; ++a) if (subset & (1 << a)) n[a] += step[a];
            for (int a = 0; a < 3; ++a) if (n[a] < 0 || n[a] >= dims[a]) return false;
            if (solid[sim_runtime_3d_volume_index(d, n[0], n[1], n[2])]) return false;
        }
        for (int a = 0; a < 3; ++a) if (tied & (1 << a)) {
            q[a] += step[a]; next[a] += delta[a];
        }
    }
}

typedef struct TransportSample {
    float value, low, high;
    bool regular;
} TransportSample;

/* Ignore solid/invisible donors, renormalize remaining convex weights. Never
 * invent a solid-cell value by averaging unrelated fluid across an obstacle. */
static TransportSample sample(const float *field, const float *valid,
                              const SimRuntime3DDomainDesc *d, const uint8_t *solid,
                              const float origin[3], const float target[3], float fallback) {
    TransportSample s = {.value = fallback, .low = FLT_MAX, .high = -FLT_MAX, .regular = true};
    int dims[3] = {d->grid_w, d->grid_h, d->grid_d}, lo[3], hi[3];
    float p[3], f[3], weight = 0, sum = 0;
    for (int a = 0; a < 3; ++a) {
        p[a] = fminf(fmaxf(target[a], 0), dims[a] - 1);
        if (p[a] != target[a]) s.regular = false;
        lo[a] = (int)floorf(p[a]);
        hi[a] = lo[a] + 1 < dims[a] ? lo[a] + 1 : lo[a];
        f[a] = p[a] - lo[a];
    }
    if (!clear_segment(d, solid, origin, p)) {
        s.regular = false;
        s.low = s.high = fallback;
        return s;
    }
    for (int z = 0; z < 2; ++z) for (int y = 0; y < 2; ++y) for (int x = 0; x < 2; ++x) {
        float w = (x ? f[0] : 1-f[0]) * (y ? f[1] : 1-f[1]) * (z ? f[2] : 1-f[2]);
        if (w <= 0) continue;
        int ix = x ? hi[0] : lo[0], iy = y ? hi[1] : lo[1], iz = z ? hi[2] : lo[2];
        size_t i = sim_runtime_3d_volume_index(d, ix, iy, iz);
        float donor[3] = {ix, iy, iz};
        if ((solid && solid[i]) || !clear_segment(d, solid, origin, donor)) {
            s.regular = false;
            continue;
        }
        if (valid && valid[i] == 0) s.regular = false;
        sum += w * field[i]; weight += w;
        s.low = fminf(s.low, field[i]); s.high = fmaxf(s.high, field[i]);
    }
    if (weight > 0) s.value = sum / weight;
    else { s.regular = false; s.low = s.high = fallback; }
    return s;
}

void sim_runtime_3d_advect_velocity_bounded(SimRuntime3DVolume *v,
        SimRuntime3DSolverScratch *s, const uint8_t *solid, float dt_cells, SimRuntime3DSolverStepMetrics *metrics) {
    const SimRuntime3DDomainDesc *d = &v->desc;
    const float *old[3] = {s->velocity_x_prev, s->velocity_y_prev, s->velocity_z_prev};
    float *out[3] = {v->velocity_x, v->velocity_y, v->velocity_z};
    /* Reuse pressure workspace before projection. No allocation per step. */
    float *predicted = s->divergence, *regular = s->pressure_prev;
    for (int c = 0; c < 3; ++c) {
        for (int z = 0; z < d->grid_d; ++z) for (int y = 0; y < d->grid_h; ++y) for (int x = 0; x < d->grid_w; ++x) {
            size_t i = sim_runtime_3d_volume_index(d, x, y, z);
            if (solid && solid[i]) { predicted[i] = regular[i] = 0; continue; }
            float p[3] = {x, y, z}, back[3];
            for (int a = 0; a < 3; ++a) back[a] = p[a] - dt_cells * old[a][i];
            TransportSample f = sample(old[c], NULL, d, solid, p, back, old[c][i]);
            predicted[i] = f.value; regular[i] = f.regular;
        }
        for (int z = 0; z < d->grid_d; ++z) for (int y = 0; y < d->grid_h; ++y) for (int x = 0; x < d->grid_w; ++x) {
            size_t i = sim_runtime_3d_volume_index(d, x, y, z);
            if (solid && solid[i]) { out[c][i] = 0; continue; }
            out[c][i] = predicted[i];
            if (!regular[i]) {
                if (metrics) metrics->transport_fallback_components++;
                continue;
            }
            float p[3] = {x, y, z}, back[3], forward[3];
            for (int a = 0; a < 3; ++a) {
                back[a] = p[a] - dt_cells * old[a][i];
                forward[a] = p[a] + dt_cells * old[a][i];
            }
            TransportSample r = sample(predicted, regular, d, solid, p, forward, predicted[i]);
            if (!r.regular) {
                if (metrics) metrics->transport_fallback_components++;
                continue;
            }
            TransportSample f = sample(old[c], NULL, d, solid, p, back, old[c][i]);
            float corrected = predicted[i] + .5f * (old[c][i] - r.value);
            out[c][i] = fminf(f.high, fmaxf(f.low, corrected));
            if (metrics) {
                metrics->transport_corrected_components++;
                if (corrected < f.low || corrected > f.high) metrics->transport_limited_components++;
            }
        }
    }
}
