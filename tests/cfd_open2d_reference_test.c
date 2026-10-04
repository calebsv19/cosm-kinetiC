#include "app/cfd_open2d_force_check.h"
#include "app/cfd_open2d_budget.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int argc, char **argv) {
    int n = argc > 1 ? atoi(argv[1]) : 32;
    assert(n == 16 || n == 32 || n == 64 || n == 128);
    double mean = argc > 2 ? atof(argv[2]) : .002;
    assert(mean > 0 && mean <= .002);
    double length = argc > 3 ? atof(argv[3]) : 4;
    assert(length == 4 || length == 8);
    int nx = argc>5?atoi(argv[5]):(int)lround(n * length / 4);
    assert(nx <= 128);
    CfdOpen2D c;
    assert(cfd_open2d_init(&c, nx, n, length, 2, .5, 1, .1, mean));
    assert(cfd_open2d_set_obstacle(&c, (int)lround(1.5*nx/length), 3*n/8, (int)lround(2.5*nx/length), 5*n/8));
    int nu = (nx + 1) * n, nv = nx * (n + 1);
    double *old = calloc(nu + nv, sizeof(double));
    assert(old);
    double dt = n >= 64 ? .001*64*64/(n*n) : .004;
    dt=fmin(dt,.32/(.2*(pow(nx/length,2)+pow(n/2.,2))+1.5*mean*nx/length));
    double dt_scale=argc>4?atof(argv[4]):1;assert(dt_scale>0&&dt_scale<=1);dt*=dt_scale;
    int steps = (int)ceil(1 / dt-1e-9), quiet = 0;dt=1./steps;
    CfdMac2DForceCheck before = {0}, after = {0};
    CfdOpen2DEnergy energy_before={0},energy_after={0};
    for (int second = 1; second <= 40; second++) {
        memcpy(old, c.u, nu * sizeof(double));
        memcpy(old + nu, c.v, nv * sizeof(double));
        for (int t = 0; t < steps; t++) {
            if (t == steps - 1)
                assert(cfd_open2d_force_check(&c, (int)lround(.5*nx/length), n/8, (int)lround(3.5*nx/length), 7*n/8, &before));
            if(t==steps-1)assert(cfd_open2d_energy(&c,&energy_before));
            assert(cfd_open2d_step(&c, dt));
        }
        double change = 0;
        for (int k = 0; k < nu; k++)
            change = fmax(change, fabs(c.u[k] - old[k]));
        for (int k = 0; k < nv; k++)
            change = fmax(change, fabs(c.v[k] - old[nu + k]));
        assert(cfd_open2d_force_check(&c, (int)lround(.5*nx/length), n/8, (int)lround(3.5*nx/length), 7*n/8, &after));
        double cv = after.cv_pressure_x_n + after.cv_viscous_x_n - after.cv_advective_x_n -
                    (after.cv_momentum_x_kg_m_s - before.cv_momentum_x_kg_m_s) / dt;
        double surface = after.surface_pressure_x_n + after.surface_viscous_x_n;
        printf("n=%d nx=%d length=%.9g time=%.9g inlet_mean=%.12g velocity_change_relative=%.12g "
               "surface_force=%.12g "
               "pressure=%.12g viscous=%.12g higher_order_force=%.12g cv_force=%.12g "
               "divergence=%.12g mass_error=%.12g\n",
               n, nx, length, c.time, mean, change / mean, surface, after.surface_pressure_x_n,
               after.surface_viscous_x_n, after.quadratic_pressure_x_n + after.cubic_viscous_x_n,
               cv, c.divergence, fabs(c.inlet_flux - c.outlet_flux));
        assert(cfd_open2d_energy(&c,&energy_after));
        double power=energy_after.pressure_work_w+energy_after.viscous_work_w-energy_after.outward_kinetic_flux_w;
        double energy_rate=(energy_after.kinetic_energy_j-energy_before.kinetic_energy_j)/dt;
        printf("energy_n=%d time=%.9g input_power=%.12g dissipation=%.12g rate=%.12g relative_energy_residual=%.12g\n",n,c.time,power,energy_after.dissipation_w,energy_rate,(power-energy_after.dissipation_w-energy_rate)/fabs(power));
        fflush(stdout);
        quiet = second >= 5 && change / mean < 1e-6 ? quiet + 1 : 0;
        if (quiet >= 2)
            break;
    }
    assert(quiet >= 2);
    free(old);
    cfd_open2d_destroy(&c);
}
