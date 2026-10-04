/* Whole-field physical scaling for the existing stationary steady-Stokes model. */
#include "app/cfd_obstacle3d.h"
#include "app/cfd_obstacle3d_pressure_trace.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

typedef struct { double mu, flow, rho; } MaterialCase;
static bool bounded(void *opaque) {
    (void)opaque;
    return true;
}
static double relative(double observed, double expected) {
    assert(isfinite(observed) && isfinite(expected) && expected > 0);
    return fabs(observed / expected - 1);
}
static double field_error(const double *a, const double *b, int count, double scale) {
    double error = 0, norm = 0;
    for (int q = 0; q < count; q++) {
        assert(isfinite(a[q]) && isfinite(b[q]));
        double expected = scale * b[q];
        error += (a[q] - expected) * (a[q] - expected);
        norm += expected * expected;
    }
    assert(norm > 0);
    return sqrt(error / norm);
}
int main(void) {
    const MaterialCase cases[] = {{.1,.008,1}, {.05,.008,1}, {.2,.008,1},
        {.1,.004,1}, {.1,.016,1}, {.05,.004,.5}, {.2,.016,2}, {.1,.008,.5}, {.1,.008,2}};
    int dims[3] = {32,16,16}; double lengths[3] = {4,2,2};
    CfdMemoryBudget budget = {.limit_bytes=64*1024*1024};
    CfdMemoryBudget *previous = cfd_memory_scope(&budget);
    CfdObstacle3d base;
    assert(cfd_obstacle3d_init(&base,dims,lengths,cases[0].rho,cases[0].mu,cases[0].flow,2));
    cfd_obstacle_mixed3d_checkpoint(base.mixed,bounded,NULL);
    assert(cfd_obstacle3d_solve(&base));
    CfdObstacle3dPressureTrace base_trace;
    assert(cfd_obstacle3d_pressure_trace_diagnostic(&base,base.p,(size_t)base.cells,&base_trace));
    double maximum = 0;
    for (size_t i = 0; i < sizeof(cases)/sizeof(cases[0]); i++) {
        CfdObstacle3d s;
        assert(cfd_obstacle3d_init(&s,dims,lengths,cases[i].rho,cases[i].mu,cases[i].flow,2));
        cfd_obstacle_mixed3d_checkpoint(s.mixed,bounded,NULL);
        assert(cfd_obstacle3d_solve(&s));
        assert(s.count==base.count && s.cells==base.cells);
        assert(s.relative_residual<=1e-11 && s.max_divergence<1e-8 && s.flux_error<1e-9);
        assert(s.discrete_energy_imbalance<1e-9);
        assert(fabs(s.discrete_momentum_residual[0])/(4*s.inlet_pressure)<1e-9);
        CfdObstacle3dPressureTrace trace;
        assert(cfd_obstacle3d_pressure_trace_diagnostic(&s,s.p,(size_t)s.cells,&trace));
        double qscale=cases[i].flow/cases[0].flow;
        double muscale=cases[i].mu/cases[0].mu;
        double pscale=qscale*muscale, dscale=qscale*qscale*muscale;
        double errors[] = {
            field_error(s.u,base.u,s.count,qscale), field_error(s.p,base.p,s.cells,pscale),
            relative(s.inlet_pressure,base.inlet_pressure*pscale),
            relative(s.pressure_force[0],base.pressure_force[0]*pscale),
            relative(s.viscous_force[0],base.viscous_force[0]*pscale),
            relative(trace.force_n[0],base_trace.force_n[0]*pscale),
            relative(s.physical_dissipation,base.physical_dissipation*dscale),
            relative(s.physical_power,base.physical_power*dscale),
            relative(s.inlet_flow,cases[i].flow), relative(s.outlet_flow,cases[i].flow)};
        double worst=0;
        for(size_t j=0;j<sizeof(errors)/sizeof(errors[0]);j++) worst=fmax(worst,errors[j]);
        assert(worst<1e-8); maximum=fmax(maximum,worst);
        printf("{\"case\":%zu,\"mu_pa_s\":%.17g,\"flow_m3_s\":%.17g,\"density_kg_m3\":%.17g,"
               "\"velocity_field_error\":%.17g,\"pressure_field_error\":%.17g,"
               "\"maximum_scaling_relative_error\":%.17g,\"physical_energy_imbalance\":%.17g}\n",
               i,cases[i].mu,cases[i].flow,cases[i].rho,errors[0],errors[1],worst,s.physical_energy_imbalance);
        cfd_obstacle3d_destroy(&s);
    }
    cfd_obstacle3d_destroy(&base);assert(budget.live_bytes==0);cfd_memory_scope(previous);
    printf("{\"status\":\"passed_steady_stokes_material_scaling\",\"cases\":9,"
           "\"maximum_relative_error\":%.17g,\"peak_owned_bytes\":%zu,"
           "\"density_role\":\"no inertia term in this steady Stokes model\","
           "\"physical_cube_force_certification\":false}\n",maximum,budget.peak_bytes);
    return 0;
}
