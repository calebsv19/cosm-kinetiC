#include "app/cfd_hotgas_reconstruction3d.h"
#include <string.h>

bool cfd_hotgas_domain3d_rk2(CfdHotGas3d *s, const CfdHotGasDomain *d,
                             const double *source, double dt, CfdHotGasDomainBudget *output) {
    if (!output || !cfd_hotgas_domain3d_validate(s, d))
        return false;
    CfdHotGas3d trial;
    if (!cfd_hotgas3d_init(&trial, s->grid.n, s->grid.length, s->gamma, s->gas_constant_j_kg_k))
        return false;
    trial.caloric = s->caloric;
    size_t size = (size_t)5 * s->grid.count * sizeof(double);
    memcpy(trial.state, s->state, size);
    CfdHotGasDomainBudget first, second;
    bool okay = cfd_hotgas_domain3d_euler(&trial, d, source, dt, &first) &&
                cfd_hotgas_domain3d_euler(&trial, d, source, dt, &second);
    if (okay) {
        for (int q = 0; q < s->grid.count; ++q) {
            for (int i = 0; i < 5; ++i)
                trial.state[5 * q + i] = .5 * (s->state[5 * q + i] + trial.state[5 * q + i]);
            double t, p, c;
            if (!cfd_hotgas3d_primitive(s, trial.state + 5 * q, &t, &p, &c)) {
                okay = false;
                break;
            }
        }
    }
    if (okay) {
        CfdHotGasDomainBudget budget = {0};
        for (int f = 0; f < 6; ++f) {
            for (int i = 0; i < 5; ++i)
                budget.outward[f][i] = .5 * (first.outward[f][i] + second.outward[f][i]);
            budget.potential_outward_j[f] = .5 * (first.potential_outward_j[f] + second.potential_outward_j[f]);
            budget.conducted_outward_j[f] = .5 * (first.conducted_outward_j[f] + second.conducted_outward_j[f]);
        }
        for (int a = 0; a < 3; ++a)
            budget.gravity_impulse_kg_m_s[a] = .5 * (first.gravity_impulse_kg_m_s[a] + second.gravity_impulse_kg_m_s[a]);
        budget.gravity_work_j = .5 * (first.gravity_work_j + second.gravity_work_j);
        memcpy(s->state, trial.state, size);
        *output = budget;
    }
    cfd_hotgas3d_destroy(&trial);
    return okay;
}
