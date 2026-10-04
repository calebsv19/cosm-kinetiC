#include "app/cfd_channel.h"
#include <math.h>
#include <string.h>

static void measure(CfdChannel *c) {
    double h=c->height/c->n, area=c->length*c->width;
    c->tau[0]=2*c->mu*(c->u[0]-c->wall_bottom)/h;
    for(int j=1;j<c->n;j++) c->tau[j]=c->mu*(c->u[j]-c->u[j-1])/h;
    c->tau[c->n]=2*c->mu*(c->wall_top-c->u[c->n-1])/h;
    c->volume_flux=c->kinetic_energy=c->dissipation=0;
    for(int j=0;j<c->n;j++) {
        c->volume_flux+=c->u[j]*h*c->width;
        c->kinetic_energy+=.5*c->rho*c->u[j]*c->u[j]*h*area;
    }
    for(int j=0;j<=c->n;j++)
        c->dissipation+=c->tau[j]*c->tau[j]/c->mu*h*area*((j==0||j==c->n)?.5:1);
    c->pressure_power=c->gradient*c->length*c->volume_flux;
    c->wall_power=area*(c->tau[c->n]*c->wall_top-c->tau[0]*c->wall_bottom);
}

bool cfd_channel_init(CfdChannel *c, int cells, const double d[3],
        double rho, double mu, double gradient, double bottom, double top) {
    if(!c||!d||cells<4||cells>CFD_CHANNEL_MAX_CELLS) return false;
    for(int a=0;a<3;a++) if(!isfinite(d[a])||d[a]<.1||d[a]>100) return false;
    if(!isfinite(rho)||rho<.001||rho>50000||!isfinite(mu)||mu<1e-9||mu>1000||
       !isfinite(gradient)||fabs(gradient)>10000||!isfinite(bottom)||fabs(bottom)>100||
       !isfinite(top)||fabs(top)>100) return false;
    double speed=fmax(fabs(bottom),fabs(top))+fabs(gradient)*d[1]*d[1]/(8*mu);
    if(rho*d[1]*speed/mu>100) return false;
    memset(c,0,sizeof(*c));
    c->n=cells;c->length=d[0];c->height=d[1];c->width=d[2];
    c->rho=rho;c->mu=mu;c->gradient=gradient;c->wall_bottom=bottom;c->wall_top=top;
    measure(c);
    return true;
}

bool cfd_channel_step(CfdChannel *c, double dt) {
    if(!c||c->n<4||c->n>CFD_CHANNEL_MAX_CELLS||!isfinite(dt)||dt<=0||dt>.1) return false;
    double h=c->height/c->n, r=c->mu*dt/(c->rho*h*h);
    if(!isfinite(r)||r>1e8) return false;
    double upper[CFD_CHANNEL_MAX_CELLS], rhs[CFD_CHANNEL_MAX_CELLS];
    double old[CFD_CHANNEL_MAX_CELLS], energy_before=c->kinetic_energy;
    memcpy(old,c->u,c->n*sizeof(double));
    /* Backward Euler with shared face stresses; half-cell wall distances.
     * Each interior shear flux appears once with each sign. */
    for(int j=0;j<c->n;j++) {
        double diag=1+((j==0||j==c->n-1)?3:2)*r;
        double b=old[j]+dt*c->gradient/c->rho;
        if(j==0) b+=2*r*c->wall_bottom;
        if(j==c->n-1) b+=2*r*c->wall_top;
        if(j>0) { diag+=r*upper[j-1]; b+=r*rhs[j-1]; }
        upper[j]=j==c->n-1?0:-r/diag;
        rhs[j]=b/diag;
    }
    for(int j=c->n-1;j>=0;j--) c->u[j]=rhs[j]-(j+1<c->n?upper[j]*c->u[j+1]:0);
    measure(c);
    double rate=0, area=c->length*c->width;
    c->max_acceleration=c->local_residual=c->temporal_dissipation=0;
    for(int j=0;j<c->n;j++) {
        double du=c->u[j]-old[j], accel=du/dt;
        double residual=c->rho*accel-c->gradient-(c->tau[j+1]-c->tau[j])/h;
        if(!isfinite(c->u[j])||!isfinite(residual)) return false;
        rate+=c->rho*accel*h*area;
        c->local_residual=fmax(c->local_residual,fabs(residual));
        c->max_acceleration=fmax(c->max_acceleration,fabs(accel));
        c->temporal_dissipation+=.5*c->rho*du*du*h*area/dt;
    }
    double wall_force=(c->tau[c->n]-c->tau[0])*area;
    c->momentum_residual=rate-c->gradient*c->height*area-wall_force;
    c->recovered_pressure_drop=(rate-wall_force)/(c->height*area)*c->length;
    c->energy_residual=(c->kinetic_energy-energy_before)/dt-c->pressure_power-c->wall_power+
        c->dissipation+c->temporal_dissipation;
    c->time+=dt;
    return isfinite(c->energy_residual)&&isfinite(c->momentum_residual);
}

double cfd_channel_velocity(const CfdChannel *c, double y) {
    double h=c->height/c->n;
    if(y<=h*.5) return c->wall_bottom+(c->u[0]-c->wall_bottom)*fmax(0,y)/(h*.5);
    if(y>=c->height-h*.5) return c->wall_top+(c->u[c->n-1]-c->wall_top)*fmax(0,c->height-y)/(h*.5);
    double q=y/h-.5;int j=(int)floor(q);double f=q-j;
    return c->u[j]*(1-f)+c->u[j+1]*f;
}
double cfd_channel_shear(const CfdChannel *c, double y) {
    double q=fmin(c->n,fmax(0,y*c->n/c->height));int j=(int)floor(q);
    return j==c->n?c->tau[j]:c->tau[j]*(1-(q-j))+c->tau[j+1]*(q-j);
}
