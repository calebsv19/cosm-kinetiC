#include "app/cfd_periodic3d.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static const double pi = 3.14159265358979323846;
/* Independent continuous ABC/Beltrami solution: curl(u) = k*u,
 * p = -rho*(|u|^2 - its mean)/2, a(t) = a0*exp(-nu*k^2*t).
 * No solver forcing or prescribed final field enters the numerical solve. */
static void exact(const double x[3], double amplitude, double v[3], double *p) {
    double sn[3], cs[3];
    for (int a = 0; a < 3; a++) {
        sn[a] = sin(pi * x[a]);
        cs[a] = cos(pi * x[a]);
    }
    v[0] = amplitude * (sn[2] + cs[1]);
    v[1] = amplitude * (sn[0] + cs[2]);
    v[2] = amplitude * (sn[1] + cs[0]);
    if (p)
        *p = -amplitude * amplitude *
             (sn[2]*cs[1] + sn[0]*cs[2] + sn[1]*cs[0]);
}
static double run(int n, double dt, const char *kind) {
    CfdMemoryBudget budget = {.limit_bytes = 256*1024*1024};
    CfdMemoryBudget *previous = cfd_memory_scope(&budget);
    int counts[3] = {n,n,n};
    double length[3] = {2,2,2};
    CfdCartesian3d grid;
    assert(cfd_cartesian3d_init(&grid, counts, length));
    size_t values = (size_t)3*grid.count;
    double *initial = cfd_memory_calloc(values, sizeof(double));
    assert(initial);
    for (int q = 0; q < grid.count; q++)
        for (int a = 0; a < 3; a++) {
            double x[3], v[3];
            cfd_cartesian3d_position(&grid,q,a,x);
            exact(x,.2,v,NULL);
            initial[a*grid.count+q] = v[a];
        }
    CfdPeriodic3d s;
    assert(!cfd_periodic3d_init_unforced(&s,counts,length,1,.05,dt,initial,values-1));
    double saved = initial[0]; initial[0] = NAN;
    assert(!cfd_periodic3d_init_unforced(&s,counts,length,1,.05,dt,initial,values));
    initial[0] = saved + 1;
    assert(!cfd_periodic3d_init_unforced(&s,counts,length,1,.05,dt,initial,values));
    initial[0] = saved;
    assert(cfd_periodic3d_init_unforced(&s,counts,length,1,.05,dt,initial,values));
    cfd_memory_free(initial);
    assert(s.unforced && isnan(s.velocity_l2_error) && isnan(s.pressure_l2_error));
    double previous_energy = s.kinetic_j, maximum_residual = 0, maximum_divergence = 0;
    for (int step = 0; step < (int)lround(.2/dt); step++) {
        size_t allocations = budget.successful_allocations;
        assert(cfd_periodic3d_step(&s));
        if (step >= 2)
            assert(budget.successful_allocations == allocations);
        assert(s.kinetic_j < previous_energy);
        previous_energy = s.kinetic_j;
        assert(s.forcing_power_w == 0);
        for (size_t q = 0; q < values; q++)
            assert(s.forcing[q] == 0);
        maximum_residual = fmax(maximum_residual,s.true_residual);
        maximum_divergence = fmax(maximum_divergence,s.max_divergence);
    }
    double amplitude = .2*exp(-.05*pi*pi*s.time);
    double lambda_h = 4*pow(sin(pi/n)/(2./n),2);
    double discrete_amplitude = .2*exp(-.05*lambda_h*s.time);
    double velocity_error = 0, velocity_norm = 0, pressure_error = 0, pressure_norm = 0;
    double temporal_error = 0, temporal_norm = 0, pressure_mean = 0, momentum[3] = {0};
    for (int q = 0; q < grid.count; q++) {
        for (int a = 0; a < 3; a++) {
            double x[3], v[3], discrete[3];
            cfd_cartesian3d_position(&grid,q,a,x);
            exact(x,amplitude,v,NULL);exact(x,discrete_amplitude,discrete,NULL);
            double value = s.velocity[a*grid.count+q];
            velocity_error += pow(value-v[a],2);velocity_norm += v[a]*v[a];
            temporal_error += pow(value-discrete[a],2);temporal_norm += discrete[a]*discrete[a];
            momentum[a] += value*grid.volume;
        }
        double x[3], v[3], p;
        cfd_cartesian3d_position(&grid,q,-1,x);exact(x,amplitude,v,&p);
        pressure_error += pow(s.pressure[q]-p,2);pressure_norm += p*p;
        pressure_mean += s.pressure[q]/grid.count;
    }
    cfd_periodic3d_transport(&grid,s.velocity,s.transport);
    double work = 0, sum[3] = {0};
    for (int a = 0; a < 3; a++)
        for (int q = 0; q < grid.count; q++) {
            sum[a] += s.transport[a*grid.count+q]*grid.volume;
            work += s.velocity[a*grid.count+q]*s.transport[a*grid.count+q]*grid.volume;
        }
    assert(fabs(pressure_mean)<1e-10 && fabs(work)<1e-10);
    for (int a=0;a<3;a++)
        assert(fabs(sum[a])<1e-10 && fabs(momentum[a])<1e-10);
    double vr=sqrt(velocity_error/velocity_norm), pr=sqrt(pressure_error/pressure_norm);
    double tr=sqrt(temporal_error/temporal_norm);
    double exact_energy=1.5*amplitude*amplitude*8;
    double exact_dissipation=3*.05*pi*pi*amplitude*amplitude*8;
    printf("{\"kind\":\"%s\",\"n\":%d,\"dt_s\":%.17g,\"velocity_relative_error\":%.17g,"
           "\"pressure_relative_error\":%.17g,\"temporal_velocity_relative_error\":%.17g,"
           "\"energy_relative_error\":%.17g,\"dissipation_relative_error\":%.17g,"
           "\"maximum_true_residual\":%.17g,\"maximum_divergence\":%.17g,"
           "\"forcing_power_w\":%.17g,\"advective_work_w\":%.17g,\"peak_owned_bytes\":%zu}\n",
           kind,n,dt,vr,pr,tr,fabs(s.kinetic_j/exact_energy-1),
           fabs(s.physical_dissipation_w/exact_dissipation-1),maximum_residual,
           maximum_divergence,s.forcing_power_w,work,budget.peak_bytes);
    fflush(stdout);
    assert(maximum_residual <= 1e-11 && maximum_divergence < 1e-8);
    cfd_periodic3d_destroy(&s);assert(budget.live_bytes==0);cfd_memory_scope(previous);
    return tr;
}
static void failure_controls(void) {
    int n[3] = {8,8,8};
    double length[3] = {2,2,2};
    double initial[3*8*8*8];
    for (size_t q = 0; q < sizeof(initial)/sizeof(initial[0]); q++)
        initial[q] = 100;
    CfdMemoryBudget budget = {.limit_bytes = 1024};
    CfdMemoryBudget *previous = cfd_memory_scope(&budget);
    CfdPeriodic3d s;
    assert(!cfd_periodic3d_init_unforced(&s,n,length,1,.05,.1,initial,3*8*8*8));
    cfd_periodic3d_destroy(&s);
    assert(budget.live_bytes == 0 && budget.last_failure == CFD_MEMORY_LIMIT);
    budget.limit_bytes = 16*1024*1024;
    assert(cfd_periodic3d_init_unforced(&s,n,length,1,.05,.1,initial,3*8*8*8));
    assert(!cfd_periodic3d_step(&s));
    assert(s.failed && s.time == 0 && s.steps == 0);
    for (int q = 0; q < 3*8*8*8; q++)
        assert(s.velocity[q] == 100 && s.previous[q] == 100);
    for (int q = 0; q < 8*8*8; q++)
        assert(s.pressure[q] == 0);
    cfd_periodic3d_destroy(&s);
    assert(budget.live_bytes == 0);
    cfd_memory_scope(previous);
}
int main(int argc, char **argv) {
    (void)argv;
    failure_controls();
    if (argc > 1) {
        run(8,.01,"sanitizer");
        return 0;
    }
    run(8,.001,"spatial");run(16,.001,"spatial");run(32,.001,"spatial");
    double e1=run(16,.02,"temporal"),e2=run(16,.01,"temporal"),e3=run(16,.005,"temporal");
    assert(e1/e2 > 3 && e2/e3 > 3);
    return 0;
}
