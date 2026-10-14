/* Exact vertical source-edge verification. Constant upward velocity transports
 * a stationary, sharply bounded volumetric source. Its cell averages follow
 * the characteristic integral of an indicator, including off-grid edges.
 * No buoyancy, altered diffusion or fitted plume reference is involved. */
#include "app/cfd_open_atmosphere3d.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
static double ramp_integral(double x) {return .5*pow(fmax(x,0),2);}
static double heat_integral(double z,double lower,double t) {
    double upper=lower+.125,shift=.2*t;
    return (ramp_integral(z-lower)-ramp_integral(z-upper)
        -ramp_integral(z-lower-shift)+ramp_integral(z-upper-shift))/.2;
}
static double run(int nz,double dt,double lower) {
    int grid[3]={4,4,nz};double length[3]={1,1,1},zero[2]={0,0},ambient[2]={300,300};
    CfdOpenAtmosphere3d s;
    assert(cfd_open_atmosphere3d_init(&s,grid,length,1.2,1.8e-5,1005,300,0,0,dt,zero,ambient,zero,false,9.81,1./300,.1));
    s.muscl=true;s.cache_projection=true;
    int n=s.grid.count;double *source=calloc((size_t)2*n,sizeof(double));assert(source);
    for(int q=0;q<n;q++) {
        double z=(q/s.plane)/(double)nz,h=1./nz;
        double overlap=fmax(0.,fmin(z+h,lower+.125)-fmax(z,lower));
        source[q]=1206*s.grid.volume*dt*nz*overlap;source[n+q]=source[q]*1e-6;
    }
    for(int q=0;q<n+s.plane;q++)s.velocity[2*n+q]=.2;
    for(int tick=0;tick<(int)round(.5/dt);tick++)assert(cfd_open_atmosphere3d_step(&s,source,source+n));
    double error=0,scale=0,total=0,peak=0;
    for(int q=0;q<n;q++) {
        double z=(q/s.plane)/(double)nz;
        double exact=1206*s.grid.volume*nz*(heat_integral(z+1./nz,lower,.5)-heat_integral(z,lower,.5));
        error+=fabs(s.energy[q]-exact);scale+=exact;total+=s.energy[q];peak=fmax(peak,s.energy[q]/(1206*s.grid.volume));
        assert(s.energy[q]>=0 && fabs(s.smoke[q]-1e-6*s.energy[q])<1e-14);
    }
    double out=0,in=0;
    for(int q=0;q<s.boundary_faces;q++){in+=s.flux[q];out+=s.flux[s.boundary_faces+q];}
    assert(fabs(total-s.input_j-in+out)<1e-10 && peak<=.5+1e-12);
    printf("{\"check\":\"vertical_stationary_source_edges\",\"nz\":%d,\"dt_s\":%.17g,\"source_lower_m\":%.17g,\"relative_l1\":%.17g,\"heat_balance_j\":%.17g}\n",nz,dt,lower,error/scale,total-s.input_j-in+out);fflush(stdout);
    free(source);cfd_open_atmosphere3d_destroy(&s);return error/scale;
}
void cfd_open_source_edge_transport_checks(void) {
    for(int edge=0;edge<2;edge++) {
        double previous=1;
        for(int nz=16;nz<=128;nz*=2){double e=run(nz,.0005,edge?.213:.25);assert(e<previous);previous=e;}
        assert(previous<.02);
        for(double dt=.005;dt>=.00125;dt*=.5)run(32,dt,edge?.213:.25);
    }
}
#ifdef SOURCE_EDGE_TEST_MAIN
int main(void) {cfd_open_source_edge_transport_checks();return 0;}
#endif
