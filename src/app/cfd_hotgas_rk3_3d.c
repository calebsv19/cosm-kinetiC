#include "app/cfd_hotgas_reconstruction3d.h"
#include <string.h>

static bool blend(CfdHotGas3d *trial, const CfdHotGas3d *original, double trial_weight) {
    for (int q = 0; q < trial->grid.count; ++q) {
        for (int i = 0; i < 5; ++i)
            trial->state[5 * q + i] = original->state[5 * q + i] + trial_weight *
                                     (trial->state[5 * q + i] - original->state[5 * q + i]);
        double t, p, c;
        if (!cfd_hotgas3d_primitive(trial, trial->state + 5 * q, &t, &p, &c))
            return false;
    }
    return true;
}

bool cfd_hotgas_domain3d_rk3(CfdHotGas3d *s, const CfdHotGasDomain *d,
                             const double *source, double dt, CfdHotGasDomainBudget *output) {
    if (!output || !cfd_hotgas_domain3d_validate(s, d))
        return false;
    CfdHotGas3d trial;
    if (!cfd_hotgas3d_init(&trial, s->grid.n, s->grid.length, s->gamma, s->gas_constant_j_kg_k))
        return false;
    trial.caloric = s->caloric;
    size_t size = (size_t)5 * s->grid.count * sizeof(double);
    memcpy(trial.state, s->state, size);
    CfdHotGasDomainBudget stages[3];
    bool okay = cfd_hotgas_domain3d_euler(&trial, d, source, dt, stages) &&
                cfd_hotgas_domain3d_euler(&trial, d, source, dt, stages + 1) &&
                blend(&trial, s, .25) &&
                cfd_hotgas_domain3d_euler(&trial, d, source, dt, stages + 2) &&
                blend(&trial, s, 2. / 3);
    if (okay) {
        CfdHotGasDomainBudget budget = {0};
        const double weights[3] = {1. / 6, 1. / 6, 2. / 3};
        for (int k = 0; k < 3; ++k) {
            for (int f = 0; f < 6; ++f) {
                for (int i = 0; i < 5; ++i)
                    budget.outward[f][i] += weights[k] * stages[k].outward[f][i];
                budget.potential_outward_j[f] += weights[k] * stages[k].potential_outward_j[f];
                budget.conducted_outward_j[f] += weights[k] * stages[k].conducted_outward_j[f];
            }
            for (int a = 0; a < 3; ++a)
                budget.gravity_impulse_kg_m_s[a] += weights[k] * stages[k].gravity_impulse_kg_m_s[a];
            budget.gravity_work_j += weights[k] * stages[k].gravity_work_j;
        }
        memcpy(s->state, trial.state, size);
        *output = budget;
    }
    cfd_hotgas3d_destroy(&trial);
    return okay;
}
