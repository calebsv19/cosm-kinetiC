#include "app/cfd_memory.h"
#include "app/cfd_refined_channel.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifndef CFD_REFINED_PRESSURE_CORRECTORS
#define CFD_REFINED_PRESSURE_CORRECTORS 1
#endif
static double profile(const CfdRefinedChannel *c, double y) {
    double h = c->mesh->ny * c->mesh->dy, eta = y / h;
    return 6 * c->mean * eta * (1 - eta);
}
#ifdef CFD_REFINED_VERIFY_SPLIT
static bool outlet(const CfdRefinedMesh *m, const CfdRefinedFace *f) {
    return f->axis == 0 && f->hi < 0 && fabs(f->cx - m->nx * m->dx) < 1e-10 * m->dx;
}
static void gradient(const CfdRefinedMesh *m, const double *pf, double *gx, double *gy) {
    memset(gx, 0, (size_t)m->cell_count * sizeof(double));
    memset(gy, 0, (size_t)m->cell_count * sizeof(double));
    for (int fi = 0; fi < m->face_count; fi++) {
        const CfdRefinedFace *f = &m->faces[fi];
        double *g = f->axis == 0 ? gx : gy;
        if (f->lo >= 0)
            g[f->lo] += f->area * pf[fi] / m->cells[f->lo].volume;
        if (f->hi >= 0)
            g[f->hi] -= f->area * pf[fi] / m->cells[f->hi].volume;
    }
}
#endif
void cfd_refined_channel_destroy(CfdRefinedChannel *c) {
    if (!c)
        return;
    cfd_refined_diffusion_destroy(c->velocity_operator);
    cfd_refined_diffusion_destroy(c->pressure_operator);
    cfd_refined_transport_destroy(c->transport);
    cfd_refined_mixed_destroy(c->mixed_operator);
    cfd_memory_free(c->storage);
    cfd_memory_free(c->face_storage);
    memset(c, 0, sizeof(*c));
}
bool cfd_refined_channel_init(CfdRefinedChannel *c, const CfdRefinedMesh *m, double rho, double mu,
                              double mean) {
    if (!c)
        return false;
    memset(c, 0, sizeof(*c));
    if (!m || !m->cells || !m->faces || !isfinite(rho) || rho <= 0 || !isfinite(mu) || mu <= 0 ||
        !isfinite(mean) || mean <= 0)
        return false;
    c->mesh = m;
    c->rho = rho;
    c->nu = mu / rho;
    if (!isfinite(c->nu) || c->nu <= 0) {
        memset(c, 0, sizeof(*c));
        return false;
    }
    c->mean = mean;
    c->max_correctors = 64;
    c->coupling_tolerance = 1e-6;
    int nc = m->cell_count, nf = m->face_count;
    c->storage = cfd_memory_calloc((size_t)16 * nc, sizeof(double));
    c->face_storage = cfd_memory_calloc((size_t)8 * nf, sizeof(double));
    unsigned char *mask = cfd_memory_calloc(nf, 1);
    if (!c->storage || !c->face_storage || !mask) {
        cfd_memory_free(mask);
        cfd_refined_channel_destroy(c);
        return false;
    }
    double **arrays[] = {&c->u,         &c->v,         &c->old_u,  &c->old_v,  &c->adv_u, &c->adv_v,
                         &c->old_adv_u, &c->old_adv_v, &c->next_u, &c->next_v, &c->p,     &c->dp,
                         &c->gx,        &c->gy,        &c->rhs,    &c->balance};
    for (int i = 0; i < 16; i++)
        *arrays[i] = c->storage + (size_t)i * nc;
    double **faces[] = {&c->face_u,  &c->face_v, &c->normal, &c->face_p,
                        &c->face_dp, &c->flux,   &c->bc,     &c->bc_adv};
    for (int i = 0; i < 8; i++)
        *faces[i] = c->face_storage + (size_t)i * nf;
#ifdef CFD_REFINED_VERIFY_SPLIT
    for (int f = 0; f < nf; f++)
        mask[f] = !outlet(m, &m->faces[f]);
    c->velocity_operator = cfd_refined_diffusion_create(m, mask);
    for (int f = 0; f < nf; f++)
        mask[f] = outlet(m, &m->faces[f]);
    c->pressure_operator = cfd_refined_diffusion_create(m, mask);
#else
    c->mixed_operator = cfd_refined_mixed_create(m, c->nu);
#endif
    cfd_memory_free(mask);
    c->transport = cfd_refined_transport_create(m);
    bool operator_ready = false;
#ifdef CFD_REFINED_VERIFY_SPLIT
    operator_ready = c->velocity_operator && c->pressure_operator;
#else
    operator_ready = c->mixed_operator != NULL;
#endif
    if (!operator_ready || !c->transport) {
        cfd_refined_channel_destroy(c);
        return false;
    }
    double h = m->ny * m->dy, length = m->nx * m->dx, dpdx = -12 * mu * mean / (h * h);
    for (int i = 0; i < nc; i++) {
        c->u[i] = c->old_u[i] = profile(c, m->cells[i].cy);
        c->p[i] = dpdx * (m->cells[i].cx - length);
    }
    for (int f = 0; f < nf; f++) {
        c->face_u[f] = m->faces[f].boundary == CFD_REFINED_SOLID ? 0 : profile(c, m->faces[f].cy);
        c->normal[f] = m->faces[f].axis == 0 ? c->face_u[f] : 0;
        c->face_p[f] = dpdx * (m->faces[f].cx - length);
    }
    return true;
}
bool cfd_refined_channel_step_forced(CfdRefinedChannel *c, double dt, const double *acceleration_x,
                                     const double *acceleration_y) {
    if (!c || !c->mesh || c->failed || !isfinite(dt) || dt <= 0 ||
        (c->tick && fabs(dt - c->dt) > 1e-12 * c->dt))
        return false;
    const CfdRefinedMesh *m = c->mesh;
    int nc = m->cell_count, nf = m->face_count;
    for (int i = 0; i < nc; i++)
        if ((acceleration_x && !isfinite(acceleration_x[i])) ||
            (acceleration_y && !isfinite(acceleration_y[i])))
            return false;
    clock_t transport_begin = clock();
    c->transport_cpu_ms = c->mixed_solve_cpu_ms = 0;
    double rate, flow;
#ifdef CFD_REFINED_VERIFY_SPLIT
    CfdRefinedSolve solve;
#endif
    for (int f = 0; f < nf; f++)
        c->bc_adv[f] = m->faces[f].boundary == CFD_REFINED_SOLID ? 0 : profile(c, m->faces[f].cy);
    if (!cfd_refined_transport_rhs(c->transport, c->u, c->normal, c->bc_adv, c->adv_u, &rate,
                                   &flow))
        return false;
    c->transport_cfl = dt * rate;
    if (c->transport_cfl > .25)
        return false;
    memset(c->bc_adv, 0, (size_t)nf * sizeof(double));
    if (!cfd_refined_transport_rhs(c->transport, c->v, c->normal, c->bc_adv, c->adv_v, &rate,
                                   &flow))
        return false;
    c->transport_cpu_ms = 1000.0 * (clock() - transport_begin) / CLOCKS_PER_SEC;
#ifdef CFD_REFINED_VERIFY_SPLIT
    double mass = (c->tick ? 1.5 : 1) / dt;
    c->failed = true;
    c->velocity_iterations = 0;
    c->pressure_iterations = 0;
    int maximum =
        CFD_REFINED_PRESSURE_CORRECTORS > 0 ? CFD_REFINED_PRESSURE_CORRECTORS : c->max_correctors;
    bool coupling_converged = false;
    for (int correction = 0; correction < maximum; correction++) {
        c->correctors = correction + 1;
        gradient(m, c->face_p, c->gx, c->gy);
        for (int axis = 0; axis < 2; axis++) {
            const double *q = axis ? c->v : c->u, *old = axis ? c->old_v : c->old_u;
            const double *adv = axis ? c->adv_v : c->adv_u,
                         *old_adv = axis ? c->old_adv_v : c->old_adv_u;
            const double *g = axis ? c->gy : c->gx;
            const double *force = axis ? acceleration_y : acceleration_x;
            for (int i = 0; i < nc; i++)
                c->rhs[i] = (c->tick ? (2 * q[i] - .5 * old[i]) / dt : q[i] / dt) +
                            (c->tick ? 2 * adv[i] - old_adv[i] : adv[i]) - g[i] / c->rho +
                            (force ? force[i] : 0);
            for (int f = 0; f < nf; f++)
                c->bc[f] = axis || outlet(m, &m->faces[f]) ? 0 : profile(c, m->faces[f].cy);
            if (!cfd_refined_helmholtz_solve(c->velocity_operator, mass, c->nu, c->rhs, c->bc,
                                             axis ? c->next_v : c->next_u,
                                             axis ? c->face_v : c->face_u, c->flux, &solve))
                return false;
            c->velocity_iterations += solve.iterations;
        }
        for (int f = 0; f < nf; f++)
            c->normal[f] = m->faces[f].axis == 0 ? c->face_u[f] : c->face_v[f];
        cfd_refined_flux_balance(m, c->normal, c->balance);
        c->coupling_residual = 0;
        for (int i = 0; i < nc; i++)
            c->coupling_residual =
                fmax(c->coupling_residual,
                     fabs(c->balance[i]) / m->cells[i].volume * (m->ny * m->dy) / c->mean);
        coupling_converged = c->coupling_residual <= c->coupling_tolerance;
        for (int i = 0; i < nc; i++)
            c->rhs[i] = -c->rho * mass * c->balance[i] / m->cells[i].volume;
        memset(c->bc, 0, (size_t)nf * sizeof(double));
        if (!cfd_refined_diffusion_solve(c->pressure_operator, c->rhs, c->bc, c->dp, c->face_dp,
                                         c->flux, &solve))
            return false;
        c->pressure_iterations += solve.iterations;
        for (int f = 0; f < nf; f++) {
            c->normal[f] += c->flux[f] / (c->rho * mass);
            c->face_p[f] += c->face_dp[f];
        }
        for (int i = 0; i < nc; i++)
            c->p[i] += c->dp[i];
        if (CFD_REFINED_PRESSURE_CORRECTORS == 0 && coupling_converged)
            break;
    }
    if (CFD_REFINED_PRESSURE_CORRECTORS == 0 && !coupling_converged)
        return false;
    gradient(m, c->face_dp, c->gx, c->gy);
    for (int i = 0; i < nc; i++) {
        c->old_u[i] = c->u[i];
        c->old_v[i] = c->v[i];
        c->u[i] = c->next_u[i] - c->gx[i] / (c->rho * mass);
        c->v[i] = c->next_v[i] - c->gy[i] / (c->rho * mass);
        c->old_adv_u[i] = c->adv_u[i];
        c->old_adv_v[i] = c->adv_v[i];
    }
#else
    double mass = (c->tick ? 1.5 : 1) / dt;
    c->failed = true;
    for (int i = 0; i < nc; i++) {
        c->next_u[i] = (c->tick ? (2 * c->u[i] - .5 * c->old_u[i]) / dt : c->u[i] / dt) +
                       (c->tick ? 2 * c->adv_u[i] - c->old_adv_u[i] : c->adv_u[i]) +
                       (acceleration_x ? acceleration_x[i] : 0);
        c->next_v[i] = (c->tick ? (2 * c->v[i] - .5 * c->old_v[i]) / dt : c->v[i] / dt) +
                       (c->tick ? 2 * c->adv_v[i] - c->old_adv_v[i] : c->adv_v[i]) +
                       (acceleration_y ? acceleration_y[i] : 0);
    }
    for (int f = 0; f < nf; f++)
        c->bc[f] = m->faces[f].boundary == CFD_REFINED_SOLID ? 0 : profile(c, m->faces[f].cy);
    memset(c->bc_adv, 0, (size_t)nf * sizeof(double));
    CfdRefinedMixedReport report;
    clock_t mixed_begin = clock();
    if (!cfd_refined_mixed_solve(c->mixed_operator, mass, c->next_u, c->next_v, c->bc, c->bc_adv,
                                 c->next_u, c->next_v, c->face_u, c->face_v, c->p, &report))
        return false;
    c->mixed_solve_cpu_ms = 1000.0 * (clock() - mixed_begin) / CLOCKS_PER_SEC;
    c->mixed_iterations = report.iterations;
    c->mixed_relative_residual = report.relative_residual;
    c->mixed_storage_bytes = report.storage_bytes;
    c->coupling_residual = report.divergence * (m->ny * m->dy) / c->mean;
    for (int i = 0; i < nc; i++) {
        c->old_u[i] = c->u[i];
        c->old_v[i] = c->v[i];
        c->u[i] = c->next_u[i];
        c->v[i] = c->next_v[i];
        c->p[i] *= c->rho;
        c->old_adv_u[i] = c->adv_u[i];
        c->old_adv_v[i] = c->adv_v[i];
    }
    for (int f = 0; f < nf; f++) {
        c->normal[f] = m->faces[f].axis == 0 ? c->face_u[f] : c->face_v[f];
        c->face_p[f] = NAN; /* Cell pressure only; do not invent a wall trace. */
    }
#endif
    /* Native mixed face velocities satisfy momentum and continuity together.
     * Split verification builds retain predictor traces; no wall stress or
     * pressure trace is qualified by either representation at this stage. */
    cfd_refined_flux_balance(m, c->normal, c->balance);
    c->divergence = 0;
    for (int i = 0; i < nc; i++)
        c->divergence = fmax(c->divergence, fabs(c->balance[i]) / m->cells[i].volume);
    c->time += dt;
    c->dt = dt;
    c->tick++;
    c->failed = false;
    return true;
}

bool cfd_refined_channel_step(CfdRefinedChannel *c, double dt) {
    return cfd_refined_channel_step_forced(c, dt, NULL, NULL);
}
