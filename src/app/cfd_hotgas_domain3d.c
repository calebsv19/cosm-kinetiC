#include "app/cfd_hotgas_domain3d.h"
#include "app/cfd_hotgas_flux3d.h"
#include "app/cfd_hotgas_reconstruction3d.h"
#include <math.h>
#include <string.h>

bool cfd_hotgas_domain3d_validate(const CfdHotGas3d *s, const CfdHotGasDomain *d) {
    if (!s || !s->state || !d || !isfinite(d->conductivity_w_m_k) || d->conductivity_w_m_k < 0 ||
        d->conductivity_w_m_k > 100 || d->contact_flux < 0 || d->contact_flux > 3)
        return false;
    for (int a = 0; a < 3; ++a) {
        if (!isfinite(d->gravity_m_s2[a]) || fabs(d->gravity_m_s2[a]) > 1000)
            return false;
        bool lo = d->face[2 * a].kind == CFD_HOTGAS_PERIODIC;
        bool hi = d->face[2 * a + 1].kind == CFD_HOTGAS_PERIODIC;
        if (lo != hi || (lo && d->gravity_m_s2[a] != 0))
            return false; /* periodic potential would be multivalued */
    }
    for (int f = 0; f < 6; ++f) {
        const CfdHotGasFace *b = d->face + f;
        if (b->kind < CFD_HOTGAS_PERIODIC || b->kind > CFD_HOTGAS_RESERVOIR ||
            !isfinite(b->wall_temperature_k) || b->wall_temperature_k < 0 ||
            (b->kind != CFD_HOTGAS_SLIP_WALL && b->wall_temperature_k != 0))
            return false;
        if (s->caloric.active &&
            ((b->wall_temperature_k>0 && (b->wall_temperature_k<200 || b->wall_temperature_k>1500)) ||
             (b->kind==CFD_HOTGAS_RESERVOIR &&
              (b->reservoir_temperature_k<200 || b->reservoir_temperature_k>1500)))) return false;
        if (b->kind == CFD_HOTGAS_RESERVOIR) {
            if (!isfinite(b->reservoir_pressure_origin_pa) ||
                b->reservoir_pressure_origin_pa <= 0 || !isfinite(b->reservoir_temperature_k) ||
                b->reservoir_temperature_k <= 0)
                return false;
            for (int a = 0; a < 3; ++a)
                if (!isfinite(b->reservoir_velocity_m_s[a]))
                    return false;
        }
    }
    return true;
}

static bool reconstruct(const CfdHotGas3d *s, const CfdHotGasDomain *d, const double *u, int axis,
                        int sign, double out[5]) {
    double t, p, c;
    if (!cfd_hotgas3d_primitive(s, u, &t, &p, &c))
        return false;
    double half = d->gravity_m_s2[axis] * s->grid.h[axis] / (2 * s->gas_constant_j_kg_k * t);
    if (!isfinite(half) || fabs(half) > 50)
        return false;
    /* Stored isothermal equilibrium values are cell averages. Dividing by
     * sinh(half)/half maps the average to the cell-center equilibrium value. */
    double average = fabs(half) < 1e-6 ? 1 + half * half / 6 : sinh(half) / half;
    double factor = exp(sign * half) / average;
    for (int i = 0; i < 5; ++i)
        out[i] = u[i] * factor;
    return cfd_hotgas3d_primitive(s, out, &t, &p, &c);
}

static bool reservoir(const CfdHotGas3d *s, const CfdHotGasDomain *d, int f, const double xyz[3],
                      double out[5]) {
    const CfdHotGasFace *b = d->face + f;
    double argument = 0;
    for (int a = 0; a < 3; ++a)
        argument +=
            d->gravity_m_s2[a] * xyz[a] / (s->gas_constant_j_kg_k * b->reservoir_temperature_k);
    if (!isfinite(argument) || fabs(argument) > 50)
        return false;
    double average = 1;
    for (int a = 0; a < 3; ++a)
        if (a != f / 2) {
            double half = d->gravity_m_s2[a] * s->grid.h[a] /
                          (2 * s->gas_constant_j_kg_k * b->reservoir_temperature_k);
            if (!isfinite(half) || fabs(half) > 50)
                return false;
            average *= fabs(half) < 1e-6 ? 1 + half * half / 6 : sinh(half) / half;
        }
    double p = b->reservoir_pressure_origin_pa * exp(argument) * average,
           rho = p / (s->gas_constant_j_kg_k * b->reservoir_temperature_k);
    out[0] = rho;
    out[4] = p / (s->gamma - 1);
    if (s->caloric.active) {
        double e, cv;
        if (!cfd_hotgas3d_energy(s,b->reservoir_temperature_k,&e,&cv)) return false;
        out[4]=rho*e;
    }
    for (int a = 0; a < 3; ++a) {
        double v = b->reservoir_velocity_m_s[a];
        out[a + 1] = rho * v;
        out[4] += .5 * rho * v * v;
    }
    return true;
}

static bool riemann(const CfdHotGas3d *s, const double left[5], const double right[5], int axis,
                    double output[5], double *speed) {
    double t, pl, pr, cl, cr;
    if (!cfd_hotgas3d_primitive(s, left, &t, &pl, &cl) ||
        !cfd_hotgas3d_primitive(s, right, &t, &pr, &cr))
        return false;
    double vl = left[axis + 1] / left[0], vr = right[axis + 1] / right[0];
    *speed = fmax(fabs(vl) + cl, fabs(vr) + cr);
    double fl[5] = {left[axis + 1], left[1] * vl, left[2] * vl, left[3] * vl, (left[4] + pl) * vl};
    double fr[5] = {right[axis + 1], right[1] * vr, right[2] * vr, right[3] * vr,
                    (right[4] + pr) * vr};
    fl[axis + 1] += pl;
    fr[axis + 1] += pr;
    for (int i = 0; i < 5; ++i)
        output[i] = .5 * (fl[i] + fr[i]) - .5 * (*speed) * (right[i] - left[i]);
    return true;
}

bool cfd_hotgas_domain3d_conduction(const CfdHotGas3d *s, const CfdHotGasDomain *d, int q, int axis,
                                    int boundary, double *flux) {
    if (!s || !d || !flux || q < 0 || q >= s->grid.count || axis < 0 || axis > 2 || boundary < -1 ||
        boundary > 5)
        return false;
    double t, p, c, other;
    if (!cfd_hotgas3d_primitive(s, s->state + 5 * q, &t, &p, &c))
        return false;
    double distance = s->grid.h[axis];
    int direction = 1;
    if (boundary < 0) {
        int neighbor = cfd_cartesian3d_neighbor(&s->grid, q, axis, 1);
        if (!cfd_hotgas3d_primitive(s, s->state + 5 * neighbor, &other, &p, &c))
            return false;
    } else {
        if (boundary / 2 != axis)
            return false;
        const CfdHotGasFace *b = d->face + boundary;
        if (b->kind == CFD_HOTGAS_PERIODIC)
            return false;
        if (b->kind == CFD_HOTGAS_SLIP_WALL && b->wall_temperature_k == 0) {
            *flux = 0;
            return true;
        }
        other =
            b->kind == CFD_HOTGAS_SLIP_WALL ? b->wall_temperature_k : b->reservoir_temperature_k;
        distance *= .5;
        direction = boundary % 2 ? 1 : -1;
    }
    *flux = -d->conductivity_w_m_k * (other - t) / distance * direction;
    return isfinite(*flux);
}

static bool face(CfdHotGas3d *s, const CfdHotGasDomain *d, int q, int axis, int boundary, double dt,
                 double maximum[3], CfdHotGasDomainBudget *budget) {
    double left[5], right[5], f[5], xyz[3], speed, heat;
    int sign = boundary < 0 || boundary % 2 ? 1 : -1;
    int neighbor = boundary < 0 ? cfd_cartesian3d_neighbor(&s->grid, q, axis, 1) : -1;
    cfd_cartesian3d_position(&s->grid, q, -1, xyz);
    xyz[axis] += sign * .5 * s->grid.h[axis];
    double *inside = sign > 0 ? left : right, *outside = sign > 0 ? right : left;
    if (!(d->contact_flux >= 2 ? cfd_hotgas_muscl_face(s, d, q, axis, sign, inside) :
                              reconstruct(s, d, s->state + 5 * q, axis, sign, inside)))
        return false;
    if (boundary < 0) {
        if (!(d->contact_flux >= 2 ? cfd_hotgas_muscl_face(s, d, neighbor, axis, -1, outside) :
                                  reconstruct(s, d, s->state + 5 * neighbor, axis, -1, outside)))
            return false;
    } else if (d->face[boundary].kind == CFD_HOTGAS_SLIP_WALL) {
        memcpy(outside, inside, sizeof(left));
        outside[axis + 1] = -outside[axis + 1];
    } else if (!reservoir(s, d, boundary, xyz, outside))
        return false;
    /* Physical reservoir/wall Riemann problems retain the acoustic impedance.
     * Low-Mach antidiffusion is confined to interior (including periodic) faces. */
    bool resolved = d->contact_flux == 3 && boundary < 0 ? cfd_hotgas_lhllc3d(s, left, right, axis,
        fmax(cfd_hotgas_compression3d(s, d, q), cfd_hotgas_compression3d(s, d, neighbor)), f, &speed) :
                    d->contact_flux ? cfd_hotgas_hllc3d(s, left, right, axis, f, &speed) :
                                    riemann(s, left, right, axis, f, &speed);
    if (!resolved ||
        !cfd_hotgas_domain3d_conduction(s, d, q, axis, boundary, &heat))
        return false;
    if (boundary >= 0 && d->face[boundary].kind == CFD_HOTGAS_SLIP_WALL) {
        /* Enforce the exact impermeable wall constraint, including roundoff
         * in HLLC star-state subtraction. Only normal pressure force remains. */
        f[0] = f[4] = 0;
        for (int a = 0; a < 3; ++a)
            if (a != axis)
                f[a + 1] = 0;
    }
    maximum[axis] = fmax(maximum[axis], speed);
    f[4] += heat;
    double amount[5];
    for (int i = 0; i < 5; ++i) {
        amount[i] = dt / s->grid.h[axis] * f[i];
        s->candidate[5 * q + i] -= sign * amount[i];
        if (neighbor >= 0)
            s->candidate[5 * neighbor + i] += amount[i];
    }
    /* Work is derived from the same mass face flux and face potential as mass
     * transport. This conserves gas plus gravitational potential energy. */
    double work = .5 * d->gravity_m_s2[axis] * s->grid.h[axis] * amount[0];
    s->candidate[5 * q + 4] += work;
    if (neighbor >= 0)
        s->candidate[5 * neighbor + 4] += work;
    budget->gravity_work_j += work * s->grid.volume * (neighbor >= 0 ? 2 : 1);
    if (boundary >= 0) {
        double potential = 0;
        for (int a = 0; a < 3; ++a)
            potential -= d->gravity_m_s2[a] * xyz[a];
        for (int i = 0; i < 5; ++i)
            budget->outward[boundary][i] += sign * dt * s->grid.area[axis] * f[i];
        budget->potential_outward_j[boundary] += potential * sign * dt * s->grid.area[axis] * f[0];
        budget->conducted_outward_j[boundary] += sign * dt * s->grid.area[axis] * heat;
    }
    return true;
}

bool cfd_hotgas_domain3d_euler(CfdHotGas3d *s, const CfdHotGasDomain *d, const double *source,
                              double dt, CfdHotGasDomainBudget *output) {
    if (!output || !source || !isfinite(dt) || dt <= 0 || !cfd_hotgas_domain3d_validate(s, d))
        return false;
    double maximum[3] = {0}, diffusivity = 0;
    CfdHotGasDomainBudget budget = {0};
    for (int q = 0; q < s->grid.count; ++q) {
        double t, p, c;
        if (!cfd_hotgas3d_primitive(s, s->state + 5 * q, &t, &p, &c))
            return false;
        for (int a = 0; a < 3; ++a)
            maximum[a] = fmax(maximum[a], fabs(s->state[5 * q + a + 1] / s->state[5 * q]) + c);
        double *trial = s->candidate + 5 * q;
        for (int i = 0; i < 5; ++i) {
            if (!isfinite(source[5 * q + i]))
                return false;
            trial[i] = s->state[5 * q + i] + source[5 * q + i];
        }
        for (int a = 0; a < 3; ++a) {
            double impulse = s->state[5 * q] * d->gravity_m_s2[a] * dt;
            trial[a + 1] += impulse;
            budget.gravity_impulse_kg_m_s[a] += impulse * s->grid.volume;
        }
        if (!cfd_hotgas3d_primitive(s, trial, &t, &p, &c))
            return false;
        for (int a = 0; a < 3; ++a)
            maximum[a] = fmax(maximum[a], fabs(trial[a + 1] / trial[0]) + c);
        double density = fmin(s->state[5 * q], trial[0]);
        double cv=s->gas_constant_j_kg_k/(s->gamma-1);
        if (s->caloric.active) {
            double e, own_t, own_cv;
            if (!cfd_hotgas3d_energy(s,t,&e,&cv) ||
                !cfd_hotgas3d_primitive(s,s->state+5*q,&own_t,&p,&c) ||
                !cfd_hotgas3d_energy(s,own_t,&e,&own_cv)) return false;
            cv=fmin(cv,own_cv);
        }
        diffusivity = fmax(diffusivity, d->conductivity_w_m_k /
                                            (s->caloric.active ? density * cv : density * s->gas_constant_j_kg_k / (s->gamma - 1)));
    }
    for (int q = 0; q < s->grid.count; ++q)
        for (int a = 0, stride = 1; a < 3; ++a) {
            int coordinate = q / stride % s->grid.n[a];
            if (coordinate < s->grid.n[a] - 1 || d->face[2 * a + 1].kind == CFD_HOTGAS_PERIODIC) {
                if (!face(s, d, q, a, -1, dt, maximum, &budget))
                    return false;
            } else if (!face(s, d, q, a, 2 * a + 1, dt, maximum, &budget))
                return false;
            if (coordinate == 0 && d->face[2 * a].kind != CFD_HOTGAS_PERIODIC)
                if (!face(s, d, q, a, 2 * a, dt, maximum, &budget))
                    return false;
            stride *= s->grid.n[a];
        }
    double courant = 0;
    for (int a = 0; a < 3; ++a)
        courant +=
            dt * (maximum[a] / s->grid.h[a] + 2 * diffusivity / (s->grid.h[a] * s->grid.h[a]));
    if (!isfinite(courant) || courant > .4)
        return false;
    for (int q = 0; q < s->grid.count; ++q) {
        double t, p, c;
        if (!cfd_hotgas3d_primitive(s, s->candidate + 5 * q, &t, &p, &c))
            return false;
    }
    memcpy(s->state, s->candidate, (size_t)5 * s->grid.count * sizeof(double));
    *output = budget;
    return true;
}

bool cfd_hotgas_domain3d_step(CfdHotGas3d *s, const CfdHotGasDomain *d, const double *source,
                              double dt, CfdHotGasDomainBudget *budget) {
    if (!d)
        return false;
    return d->contact_flux == 3 ? cfd_hotgas_domain3d_rk3(s, d, source, dt, budget) :
           d->contact_flux == 2 ? cfd_hotgas_domain3d_rk2(s, d, source, dt, budget) :
                                 cfd_hotgas_domain3d_euler(s, d, source, dt, budget);
}
