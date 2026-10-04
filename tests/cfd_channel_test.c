#include "app/cfd_channel.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
static const double pi=3.14159265358979323846;
static CfdChannel channel(int n,double g,double bottom,double top) {
    CfdChannel c;double d[3]={2,1,.5};
    assert(cfd_channel_init(&c,n,d,1,.1,g,bottom,top));return c;
}
static void advance(CfdChannel *c,double dt,int steps) {
    for(int i=0;i<steps;i++) {
        assert(cfd_channel_step(c,dt));
        assert(c->local_residual<1e-9);
        assert(fabs(c->momentum_residual)<1e-9);
        assert(fabs(c->energy_residual)<1e-9);
        assert(c->dissipation>=0&&c->temporal_dissipation>=0);
    }
}
static double steady(int n,double g,double bottom,double top) {
    CfdChannel c=channel(n,g,bottom,top);advance(&c,.05,800);
    double error=0, h=c.height/n;
    for(int j=0;j<n;j++) {
        double y=(j+.5)*h;
        double exact=bottom+(top-bottom)*y/c.height+g*y*(c.height-y)/(2*c.mu);
        error=fmax(error,fabs(c.u[j]-exact));
    }
    double bottom_tau=c.mu*(top-bottom)/c.height+g*c.height/2;
    double top_tau=c.mu*(top-bottom)/c.height-g*c.height/2;
    double flow=c.width*((bottom+top)*c.height/2+g*pow(c.height,3)/(12*c.mu));
    assert(fabs(c.tau[0]-bottom_tau)<1e-10);assert(fabs(c.tau[n]-top_tau)<1e-10);
    assert(fabs(c.recovered_pressure_drop-g*c.length)<1e-10);
    assert(c.max_acceleration<1e-10);
    printf("steady n=%d G=%g walls=(%g,%g) max_velocity_error=%g flow_error=%g bottom_tau=%g top_tau=%g\n",
        n,g,bottom,top,error,fabs(c.volume_flux-flow),c.tau[0],c.tau[n]);
    return error;
}
static double transient(double dt) {
    CfdChannel c=channel(256,.1,0,0);int steps=(int)lround(.5/dt);advance(&c,dt,steps);
    double error=0;
    for(int j=0;j<c.n;j++) {
        double y=(j+.5)/c.n;
        double exact=.1*y*(1-y)/(2*.1);
        for(int m=1;m<400;m+=2) exact-=4*.1/(.1*pi*pi*pi*m*m*m)*sin(m*pi*y)*exp(-.1*m*m*pi*pi*.5);
        error+=pow(c.u[j]-exact,2)/c.n;
    }
    error=sqrt(error);printf("startup dt=%g L2_error=%g\n",dt,error);return error;
}
int main(void) {
    double a=steady(16,.1,0,0),b=steady(32,.1,0,0),c=steady(64,.1,0,0);
    assert(a/b>3.9&&a/b<4.1&&b/c>3.9&&b/c<4.1);
    assert(c/.125<.0003);
    assert(steady(32,0,-.2,.5)<1e-11);
    steady(32,-.1,0,0);steady(32,.1,-.2,.5);
    a=transient(.1);b=transient(.05);c=transient(.025);
    assert(a/b>1.8&&b/c>1.8&&c<.0005);
    CfdChannel original=channel(32,.1,0,0),scaled;double d[3]={2,1,.5};
    assert(cfd_channel_init(&scaled,32,d,10,1,1,0,0));
    advance(&original,.01,50);advance(&scaled,.01,50);
    for(int j=0;j<32;j++)assert(fabs(original.u[j]-scaled.u[j])<1e-12);
    assert(fabs(scaled.tau[0]-10*original.tau[0])<1e-11);
    CfdChannel rest=channel(16,0,0,0);advance(&rest,.1,10);assert(rest.volume_flux==0&&rest.kinetic_energy==0);
    assert(!cfd_channel_init(&rest,3,d,1,.1,.1,0,0));
    assert(!cfd_channel_init(&rest,16,d,1,.001,1,0,0));
    assert(!cfd_channel_init(&rest,16,d,NAN,.1,.1,0,0));
    assert(!cfd_channel_step(&original,NAN));
    puts("CFD channel verification passed: second-order steady profiles, first-order startup, shear, units, momentum and energy budgets");
}
