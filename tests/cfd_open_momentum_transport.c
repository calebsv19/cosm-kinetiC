/* Independent momentum balance: a periodic discrete streamfunction curl is
 * divergence free. With no forcing and zero initial vertical velocity, one
 * step has no vertical momentum transfer. Periodic pressure gradients and
 * viscosity sum to zero, so both horizontal momentum integrals are unchanged.
 * This checks nonlinear transport, not a manufactured copy of its flux code. */
#include "app/cfd_open_atmosphere3d.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
static double psi(double x,double y) {
    double p=2*acos(-1.);
    return .01*sin(p*x+.3)*sin(2*p*y+.7)+.005*cos(3*p*x-.2)*sin(p*y+.5)
        +.003*sin(p*(x+y)+.8);
}
/* A transverse carrier transports a viscously decaying shear. This is an
 * independent Navier-Stokes solution with nonzero momentum advection, zero
 * pressure gradient and no prescribed velocity after initialization. */
static double traveling_shear(int side,double dt) {
    int grid[3]={4,side,4};double length[3]={1,1,1},zero[2]={0,0},ambient[2]={300,300};
    CfdOpenAtmosphere3d s;
    assert(cfd_open_atmosphere3d_init(&s,grid,length,1.2,.0012,1005,300,0,0,dt,zero,ambient,zero,false,9.81,1./300,.1));
    s.muscl=true;s.cache_projection=true;double pi=acos(-1.);
    for(int q=0;q<s.grid.count;q++) {
        double y=(q/4%side+.5)/side;
        s.velocity[q]=.2+.15*sin(2*pi*y+.3);s.velocity[s.grid.count+q]=.1;
    }
    double *source=calloc((size_t)s.grid.count,sizeof(double));assert(source);
    int ticks=(int)round(.5/dt);
    for(int tick=0;tick<ticks;tick++)assert(cfd_open_atmosphere3d_step(&s,source,source));
    double error=0,scale=0;
    for(int q=0;q<s.grid.count;q++) {
        double y=(q/4%side+.5)/side;
        double exact=.2+.15*exp(-.001*4*pi*pi*.5)*sin(2*pi*(y-.1*.5)+.3);
        error+=fabs(s.velocity[q]-exact);scale+=fabs(exact-.2);
        assert(fabs(s.velocity[s.grid.count+q]-.1)<1e-12);
    }
    printf("{\"check\":\"native_traveling_shear_momentum\",\"side_y\":%d,\"dt_s\":%.17g,\"perturbation_relative_l1\":%.17g}\n",side,dt,error/scale);
    free(source);cfd_open_atmosphere3d_destroy(&s);return error/scale;
}
void cfd_open_momentum_transport_checks(void) {
    for(int side=16;side<=64;side*=2) {
        int grid[3]={side,side,4};double length[3]={1,1,1},zero[2]={0,0},ambient[2]={300,300};
        CfdOpenAtmosphere3d s;
        assert(cfd_open_atmosphere3d_init(&s,grid,length,1.2,1.8e-5,1005,300,0,0,.001,zero,ambient,zero,false,9.81,1./300,.1));
        s.muscl=true;s.cache_projection=true;
        double before[2]={0,0},after[2]={0,0};
        for(int q=0;q<s.grid.count;q++) {
            double x=(q%side)/(double)side,y=(q/side%side)/(double)side,h=1./side;
            s.velocity[q]=.2+(psi(x,y+h)-psi(x,y))/h;
            s.velocity[s.grid.count+q]=-.1-(psi(x+h,y)-psi(x,y))/h;
            for(int a=0;a<2;a++)before[a]+=s.rho*s.grid.volume*s.velocity[a*s.grid.count+q];
        }
        double *source=calloc((size_t)s.grid.count,sizeof(double));assert(source);
        assert(cfd_open_atmosphere3d_valid(&s));
        assert(cfd_open_atmosphere3d_step(&s,source,source));
        for(int q=0;q<s.grid.count;q++)for(int a=0;a<2;a++)after[a]+=s.rho*s.grid.volume*s.velocity[a*s.grid.count+q];
        printf("{\"check\":\"nonlinear_periodic_momentum_balance\",\"side_xy\":%d,\"x_drift_kg_m_s\":%.17g,\"y_drift_kg_m_s\":%.17g}\n",side,after[0]-before[0],after[1]-before[1]);fflush(stdout);
        assert(fabs(after[0]-before[0])<1e-12 && fabs(after[1]-before[1])<1e-12);
        free(source);cfd_open_atmosphere3d_destroy(&s);
    }
    double previous=1;
    for(int side=16;side<=64;side*=2){double error=traveling_shear(side,.0005);assert(error<previous);previous=error;}
    assert(previous<.01);
    /* Time refinement uses the same grid and analytic reference, independently
     * of the grid sequence above; report it without asserting error cancellation
     * is monotonic in the total spatial-plus-temporal error. */
    for(double dt=.002;dt>=.0005;dt*=.5)traveling_shear(32,dt);
}
#ifdef MOMENTUM_TEST_MAIN
int main(void) {cfd_open_momentum_transport_checks();return 0;}
#endif
