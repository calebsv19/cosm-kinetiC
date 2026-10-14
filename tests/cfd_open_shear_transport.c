/* Native evolving shear: u_x(z,t)=0.3*cos(pi*z)*exp(-nu*pi^2*t).
 * No velocity is prescribed after initialization. Momentum advection vanishes
 * analytically, viscosity changes the shear, pressure and normal flow are zero.
 * A localized hot packet follows X(z,t)=0.3*cos(pi*z)*(1-exp(-nu*pi^2*t))/(nu*pi^2).
 * Reference cell averages use analytic X/Y integrals and 16-point Gauss Z
 * integration. 32-point integration independently bounds reference error.
 * Synthetic verification only; no combustion or material calibration. */
#include "app/cfd_open_atmosphere3d.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
static double integral(double x) {
    double k=4*acos(-1.);x=fmax(-.125,fmin(.125,x));
    return .375*x+sin(2*k*x)/(4*k)+sin(4*k*x)/(32*k);
}
static double bump(double x) {return fabs(x)<.125?pow(cos(4*acos(-1.)*x),4):0;}
static void gauss(int n,double *x,double *w) {
    double pi=acos(-1.);
    for(int i=0;i<(n+1)/2;i++){
        double z=cos(pi*(i+.75)/(n+.5)),derivative=0;
        for(int iteration=0;iteration<30;iteration++){
            double p=1,prior=0;
            for(int j=1;j<=n;j++){double old=p;p=((2*j-1)*z*p-(j-1)*prior)/j;prior=old;}
            derivative=n*(z*p-prior)/(z*z-1);double next=z-p/derivative;
            if(fabs(next-z)<1e-15){z=next;break;}z=next;
        }
        x[i]=-z;x[n-1-i]=z;w[i]=w[n-1-i]=2/((1-z*z)*derivative*derivative);
    }
}
static double average(int side,int i,int j,int k,double t,int points,const double *gx,const double *gw) {
    double result=0,pi=acos(-1.),nu=.01;
    for(int r=0;r<points;r++){
        double z=(k+.5+.5*gx[r])/side;
        double shift=.3*cos(pi*z)*(-expm1(-nu*pi*pi*t))/(nu*pi*pi);
        double ax=integral((i+1.)/side-.5-shift)-integral((double)i/side-.5-shift);
        result+=.5*gw[r]*bump(z-.5)*side*ax;
    }
    return result*8*(integral((j+1.)/8-.5)-integral((double)j/8-.5));
}
static double run(int side) {
    int grid[3]={side,8,side};double l[3]={1,1,1},zero[2]={0,0},ambient[2]={300,300};CfdOpenAtmosphere3d s;
    assert(cfd_open_atmosphere3d_init(&s,grid,l,1.2,.012,1005,300,0,0,.0005,zero,ambient,zero,false,9.81,1./300,.1));
    s.cache_projection=true;s.muscl=true;
    int count=s.grid.count;double *source=calloc((size_t)count,sizeof(double));assert(source);double pi=acos(-1.);
    for(int q=0;q<count;q++){
        int i=q%side,j=q/side%8,k=q/(side*8);
        double value=side*(integral((i+1.)/side-.5)-integral((double)i/side-.5))*
            8*(integral((j+1.)/8-.5)-integral((double)j/8-.5))*
            side*(integral((k+1.)/side-.5)-integral((double)k/side-.5));
        s.energy[q]=1206*s.grid.volume*value;s.smoke[q]=s.energy[q]*1e-6;
        s.initial_j+=s.energy[q];s.initial_kg+=s.smoke[q];
        s.velocity[q]=.3*cos(pi*(k+.5)/side);
    }
    for(int tick=0;tick<1000;tick++)assert(cfd_open_atmosphere3d_step(&s,source,source));
    double gx[32],gw[32],hx[16],hw[16];gauss(32,gx,gw);gauss(16,hx,hw);
    double error=0,scale=0,reference_error=0,velocity_error=0,total=0;
    double lambda=4*side*side*pow(sin(pi/(2*side)),2),discrete_decay=pow(1-.01*lambda*s.dt,1000);
    for(int q=0;q<count;q++){
        int i=q%side,j=q/side%8,k=q/(side*8);
        double exact=1206*s.grid.volume*average(side,i,j,k,.5,32,gx,gw);
        double lower=1206*s.grid.volume*average(side,i,j,k,.5,16,hx,hw);
        error+=fabs(s.energy[q]-exact);scale+=exact;reference_error+=fabs(exact-lower);total+=s.energy[q];
        double shape=.3*cos(pi*(k+.5)/side);
        assert(fabs(s.velocity[q]-shape*discrete_decay)<1e-12);
        velocity_error=fmax(velocity_error,fabs(s.velocity[q]-shape*exp(-.01*pi*pi*.5)));
        assert(s.energy[q]>=0 && fabs(s.smoke[q]-s.energy[q]*1e-6)<1e-14);
    }
    assert(reference_error/scale<1e-6 && fabs(total-s.initial_j)<1e-10 && velocity_error<1e-4);
    printf("{\"check\":\"native_evolving_shear_heat_transport\",\"side_xz\":%d,\"dt_s\":%.17g,\"relative_l1\":%.17g,\"reference_quadrature_difference_l1\":%.17g,\"velocity_max_continuum_error_m_s\":%.17g,\"heat_balance_j\":%.17g}\n",side,s.dt,error/scale,reference_error/scale,velocity_error,total-s.initial_j);
    free(source);cfd_open_atmosphere3d_destroy(&s);return error/scale;
}
void cfd_open_shear_transport_checks(void) {
    double previous=1;
    for(int n=16;n<=64;n*=2){double error=run(n);assert(error<previous);previous=error;}
    assert(previous<.1);
}
