#include "cfd_lowmach_room_internal.h"
#include "app/cfd_memory.h"
#include <math.h>
#include <string.h>

static double cell_velocity(const CfdLmRoom *s,int q,int a) {
    int lo=s->cell_face[6*q+2*a],hi=s->cell_face[6*q+2*a+1];
    return .5*((lo<0?0:s->velocity[lo])+(hi<0?0:s->velocity[hi]));
}
void cfd_lm_predict(const CfdLmRoom *s,double dt,double *u) {
    double ambient=s->p0/(CFD_LM_R*CFD_LM_T0);
    for(int f=0;f<s->faces;f++) {
        CfdLmFace v=s->face[f];double adv=0,uf=s->velocity[f];
        for(int a=0;a<3;a++) {
            double transport=a==v.axis?uf:cell_velocity(s,v.left,a);
            if(a!=v.axis&&v.right>=0)transport=.5*(transport+cell_velocity(s,v.right,a));
            int p[3]={v.xyz[0],v.xyz[1],v.xyz[2]};p[a]+=transport>=0?-1:1;
            int other=cfd_lm_lookup(s,v.axis,p);
            /* Normal wall velocity zero; tangential wall derivative zero.
             * A reservoir uses zero-gradient velocity extrapolation. */
            bool outside=p[v.axis]<0||p[v.axis]>s->n;
            double neighbor=other<0?(a==v.axis&&!outside?0:uf):s->velocity[other];
            adv+=fabs(transport)*(uf-neighbor)*s->n;
        }
        double rho=v.right<0?s->rho[v.left]:.5*(s->rho[v.left]+s->rho[v.right]);
        double buoy=v.axis==2?s->gravity*(1-ambient/rho):0;
        u[f]=uf+dt*(buoy-adv);
    }
}
bool cfd_lm_step(CfdLmRoom *s,double dt,const double *heat,CfdLmReceipt *out) {
    if(!out||!heat||!cfd_lm_valid(s)||!isfinite(dt)||dt<=0||dt>.01||s->time+dt>.200000000001||s->steps>=4000)return false;
    int n=s->cells;double dv=1./n,area=1./(s->n*s->n),total=0;
    for(int q=0;q<n;q++) {
        if(!isfinite(heat[q])||heat[q]<0||heat[q]>1e7||(s->solid[q]&&heat[q]!=0))return false;
        total+=heat[q]*dv;
    }
    double *work=cfd_memory_calloc((size_t)n*4+s->faces,sizeof(double));if(!work)return false;
    double *rho=work,*h=rho+n,*p=h+n,*target=p+n,*u=target+n;
    memcpy(rho,s->rho,(size_t)n*sizeof(double));memcpy(h,s->enthalpy,(size_t)n*sizeof(double));
    CfdLmReceipt r={0};double volume=s->fluid_cells*dv;
    double pdot=s->vent?0:CFD_LM_R/(CFD_LM_CP-CFD_LM_R)*total/volume;
    double nextp=s->p0+dt*pdot;
    double before_mass=0,before_h=0;
    for(int q=0;q<n;q++)if(!s->solid[q]) {
        before_mass+=rho[q]*dv;before_h+=h[q]*dv;
        target[q]=(CFD_LM_R/CFD_LM_CP*heat[q]-(1-CFD_LM_R/CFD_LM_CP)*pdot)/s->p0;
        h[q]+=dt*(heat[q]+pdot);
    }
    cfd_lm_predict(s,dt,u);
    bool ok=cfd_lm_project(s,dt,target,u,p,&r);
    if(!ok)goto done;
    memset(target,0,(size_t)n*sizeof(double));
    for(int f=0;f<s->faces;f++) {
        CfdLmFace v=s->face[f];int donor=u[f]>=0?v.left:v.right;
        double rd=donor<0?s->p0/(CFD_LM_R*CFD_LM_T0):s->rho[donor];
        double hd=donor<0?CFD_LM_CP/CFD_LM_R*s->p0:s->enthalpy[donor];
        double mass=dt*area*u[f]*rd,energy=dt*area*u[f]*hd;
        rho[v.left]-=mass/dv;h[v.left]-=energy/dv;
        if(v.right>=0) {rho[v.right]+=mass/dv;h[v.right]+=energy/dv;}
        else {r.outward_mass_kg+=mass;r.outward_enthalpy_j+=energy;}
        if(u[f]>0)target[v.left]+=dt*s->n*u[f];
        else if(v.right>=0)target[v.right]-=dt*s->n*u[f];
        double temperature=hd/(CFD_LM_CP*rd),sound=sqrt(CFD_LM_CP/(CFD_LM_CP-CFD_LM_R)*CFD_LM_R*temperature);
        r.max_mach=fmax(r.max_mach,fabs(u[f])/sound);
    }
    r.heat_j=total*dt;r.pressure_work_j=pdot*volume*dt;
    double after_mass=0,after_h=0;
    for(int q=0;q<n;q++)if(!s->solid[q]) {
        after_mass+=rho[q]*dv;after_h+=h[q]*dv;
        r.advective_courant=fmax(r.advective_courant,target[q]);
        r.eos_relative_error=fmax(r.eos_relative_error,fabs(h[q]*CFD_LM_R/CFD_LM_CP/nextp-1));
    }
    r.mass_residual_kg=after_mass-before_mass+r.outward_mass_kg;
    r.enthalpy_residual_j=after_h-before_h+r.outward_enthalpy_j-r.heat_j-r.pressure_work_j;
    CfdLmRoom candidate=*s;candidate.rho=rho;candidate.enthalpy=h;candidate.velocity=u;candidate.pressure=p;
    candidate.p0=nextp;candidate.time+=dt;candidate.steps++;
    ok=r.advective_courant<=.4&&r.max_mach<=.05&&fabs(r.mass_residual_kg)<1e-10&&
        fabs(r.enthalpy_residual_j)<1e-6&&cfd_lm_valid(&candidate);
    if(ok) {
        memcpy(s->rho,rho,(size_t)n*sizeof(double));memcpy(s->enthalpy,h,(size_t)n*sizeof(double));
        memcpy(s->velocity,u,(size_t)s->faces*sizeof(double));memcpy(s->pressure,p,(size_t)n*sizeof(double));
        s->p0=nextp;s->time=candidate.time;s->steps=candidate.steps;*out=r;
    }
done:
    cfd_memory_free(work);return ok;
}
