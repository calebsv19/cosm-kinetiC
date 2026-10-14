/* Independent SI heating, analytic acceleration and Fourier transport checks.
 * These isolate numerical ingredients; they are not physical fire calibration. */
#include "app/cfd_open_atmosphere3d.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
static void init(CfdOpenAtmosphere3d *s,int nx,int nz,double dt,bool buoyancy) {
    int n[3]={nx,4,nz};double l[3]={1,1,1},p[2]={0,0},t[2]={300,300},sm[2]={0,0};
    assert(cfd_open_atmosphere3d_init(s,n,l,1.2,1.8e-5,1005,300,0,0,dt,p,t,sm,buoyancy,9.81,1./300,.1));s->cache_projection=true;s->muscl=true;
}
static void heating(int nz,bool buoyant) {
    CfdOpenAtmosphere3d s;init(&s,4,nz,.005,buoyant);s.ground=true;
    int n=s.grid.count;double *src=calloc((size_t)2*n,sizeof(double));assert(src);
    for(int q=0;q<n;q++)if(q/s.plane<nz/8){src[q]=1206*s.grid.volume*s.dt;src[n+q]=.001*s.grid.volume*s.dt;}
    for(int tick=0;tick<200;tick++)assert(cfd_open_atmosphere3d_step(&s,src,src+n));
    double error=0;for(int q=0;q<n;q++){
        double expected=q/s.plane<nz/8?1206*s.grid.volume:0;
        error=fmax(error,fabs(s.energy[q]-expected));
        assert(fabs(s.smoke[q]-(q/s.plane<nz/8?.001*s.grid.volume:0))<1e-15);
        assert(fabs(300+s.energy[q]/(1206*s.grid.volume)-(q/s.plane<nz/8?301:300))<1e-12);
        /* Uniform horizontal layer is hydrostatic, not an accelerating plume.
         * Exact cell-centre pressure integrates the piecewise constant force.
         * Euler evaluates buoyancy at the old temperature, t=1-dt. */
        if(buoyant){double z=(q/s.plane+.5)/nz;
            double pressure=z<.125?1.2*9.81/300*(1-s.dt)*(z-.125):0;
            assert(fabs(s.pressure[q]-pressure)<1e-10);
        }
    }
    for(int q=0;q<s.velocity_count;q++)assert(fabs(s.velocity[q])<1e-10);
    assert(error<1e-12);printf("{\"check\":\"source_layer%s\",\"nz\":%d,\"max_heat_error_j\":%.17g}\n",buoyant?"_hydrostatic":"",nz,error);
    free(src);cfd_open_atmosphere3d_destroy(&s);
}
static double buoyancy(double dt) {
    CfdOpenAtmosphere3d s;init(&s,4,8,dt,true);int n=s.grid.count;
    double *src=calloc((size_t)2*n,sizeof(double));assert(src);for(int q=0;q<n;q++)src[q]=1206*s.grid.volume*dt;
    int ticks=(int)round(1/dt);for(int tick=0;tick<ticks;tick++){
        /* Exact uniform inflow matches the pre-step transported field. */
        s.ambient_k[0]=s.ambient_k[1]=300+tick*dt;
        assert(cfd_open_atmosphere3d_step(&s,src,src+n));
    }
    double exact=.5*9.81/300,actual=s.velocity[2*n],error=fabs(actual-exact);
    assert(fabs(actual-exact*(1-dt))<1e-12);
    for(int q=0;q<n+s.plane;q++)assert(fabs(s.velocity[2*n+q]-actual)<1e-12);
    printf("{\"check\":\"heating_buoyancy\",\"dt_s\":%.17g,\"velocity_m_s\":%.17g,\"exact_velocity_m_s\":%.17g,\"absolute_error_m_s\":%.17g}\n",dt,actual,exact,error);
    free(src);cfd_open_atmosphere3d_destroy(&s);return error;
}
static double transport(int nx,double dt,bool diffusion) {
    CfdOpenAtmosphere3d s;init(&s,nx,4,dt,false);int n=s.grid.count;double pi=acos(-1.),D=diffusion?.01:0;
    s.diffusivity=D;s.conductivity=D*1206;
    double sinc=sin(pi/nx)/(pi/nx);
    for(int q=0;q<n;q++){
        double profile=1+.4*sinc*cos(2*pi*((q%nx+.5)/nx));s.energy[q]=profile*s.grid.volume;s.smoke[q]=profile*.001*s.grid.volume;
        if(!diffusion)s.velocity[q]=1;
    }
    s.initial_j=1;s.initial_kg=.001;double *zero=calloc((size_t)n,sizeof(double));assert(zero);
    int ticks=(int)round(.5/dt);for(int tick=0;tick<ticks;tick++)assert(cfd_open_atmosphere3d_step(&s,zero,zero));
    double error=0,scale=0,smoke_error=0;
    for(int q=0;q<n;q++){
        double exact=(1+.4*sinc*exp(-4*pi*pi*D*.5)*cos(2*pi*((q%nx+.5)/nx-(diffusion?0:.5))))*s.grid.volume;
        error+=fabs(s.energy[q]-exact);scale+=exact;smoke_error+=fabs(s.smoke[q]-.001*exact);
    }
    assert(fabs(error-smoke_error*1000)<1e-12);
    printf("{\"check\":\"%s\",\"nx\":%d,\"dt_s\":%.17g,\"heat_smoke_relative_l1\":%.17g}\n",diffusion?"fourier_diffusion":"cell_average_translation",nx,dt,error/scale);
    free(zero);cfd_open_atmosphere3d_destroy(&s);return error/scale;
}
/* Localized 3D buoyancy with an independent continuum projection solution.
 * theta=sin(pi*x)sin(pi*y)sin(pi*z/2), all lengths one metre. From rest,
 * p=-rho*b*kz/(kx^2+ky^2+kz^2)*sin(kx*x)sin(ky*y)cos(kz*z).
 * This satisfies zero side/top pressure and zero ground pressure derivative.
 * Temperature inputs are exact cell averages; velocity/pressure comparisons
 * use the continuum values at MAC faces and pressure cell centres. */
static double localized_projection(int side) {
    int grid[3]={side,side,side};double length[3]={1,1,1},zero[2]={0,0},ambient[2]={300,300};
    CfdOpenAtmosphere3d s;
    assert(cfd_open_atmosphere3d_init(&s,grid,length,1.2,1.8e-5,1005,300,0,0,.005,zero,ambient,zero,true,9.81,1./300,.1));
    s.ground=true;s.muscl=true;s.cache_projection=true;assert(cfd_open_atmosphere3d_lateral(&s));
    double pi=acos(-1.),kz=.5*pi,K=2*pi*pi+kz*kz,b=9.81/300;
    double sx=sin(pi/(2*side))/(pi/(2*side)),sz=sin(kz/(2*side))/(kz/(2*side));
    int n=s.grid.count;double *src=calloc((size_t)n,sizeof(double));assert(src);
    for(int q=0;q<n;q++){
        double x=(q%side+.5)/side,y=(q/side%side+.5)/side,z=(q/(side*side)+.5)/side;
        s.energy[q]=1206*s.grid.volume*sx*sx*sz*sin(pi*x)*sin(pi*y)*sin(kz*z);s.initial_j+=s.energy[q];
    }
    assert(cfd_open_atmosphere3d_step(&s,src,src));
    double error=0,scale=0,pe=0,ps=0;
    for(int a=0;a<3;a++)for(int k=0;k<side+(a==2);k++)for(int j=0;j<side+(a==1);j++)for(int i=0;i<side+(a==0);i++){
        double x=(i+(a==0?0:.5))/side,y=(j+(a==1?0:.5))/side,z=(k+(a==2?0:.5))/side;
        double exact=a==0?s.dt*b*kz*pi/K*cos(pi*x)*sin(pi*y)*cos(kz*z):
            a==1?s.dt*b*kz*pi/K*sin(pi*x)*cos(pi*y)*cos(kz*z):
            s.dt*b*(1-kz*kz/K)*sin(pi*x)*sin(pi*y)*sin(kz*z);
        double difference=s.velocity[cfd_open_atmosphere3d_face(&s,a,i,j,k)]-exact;
        error+=difference*difference;scale+=exact*exact;
    }
    for(int q=0;q<n;q++){
        double x=(q%side+.5)/side,y=(q/side%side+.5)/side,z=(q/(side*side)+.5)/side;
        double exact=-s.rho*b*kz/K*sin(pi*x)*sin(pi*y)*cos(kz*z),difference=s.pressure[q]-exact;
        pe+=difference*difference;ps+=exact*exact;
    }
    double relative=sqrt(error/scale),pressure=sqrt(pe/ps);
    assert(s.divergence<1e-8 && s.residual<1e-11);
    printf("{\"check\":\"localized_buoyancy_projection\",\"side\":%d,\"velocity_relative_l2\":%.17g,\"pressure_relative_l2\":%.17g}\n",side,relative,pressure);
    if(side==64)assert(relative<.01 && pressure<.01);
    free(src);cfd_open_atmosphere3d_destroy(&s);return relative;
}
void cfd_open_heated_transport_checks(void);
void cfd_open_shear_transport_checks(void);
void cfd_open_momentum_transport_checks(void);
void cfd_open_source_edge_transport_checks(void);
int main(void) {
    cfd_open_source_edge_transport_checks();
    cfd_open_momentum_transport_checks();
    cfd_open_heated_transport_checks();
    cfd_open_shear_transport_checks();
    double projection_error=1;
    for(int n=8;n<=64;n*=2){double e=localized_projection(n);assert(e<.4*projection_error);projection_error=e;}
    for(int n=8;n<=64;n*=2){heating(n,false);heating(n,true);}
    double a=buoyancy(.01),b=buoyancy(.005),c=buoyancy(.0025);assert(fabs(a/b-2)<1e-8 && fabs(b/c-2)<1e-8);
    for(int d=0;d<2;d++){
        double old=1;
        for(int nx=16;nx<=64;nx*=2){double e=transport(nx,.0005,d);assert(e<old);old=e;}
        assert(old<.02);
        /* Fixed mesh time refinement is deliberately separate. Total spatial
         * error need not fall monotonically: Euler antidiffusion can cancel it. */
        for(double dt=.005;dt>=.00125;dt*=.5)transport(32,dt,d);
    }
    return 0;
}
