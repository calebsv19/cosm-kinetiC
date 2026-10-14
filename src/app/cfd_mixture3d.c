#include "app/cfd_mixture3d.h"
#include <math.h>
#include <string.h>

bool cfd_mixture3d_init(CfdMixture3d *s,const int n[3],const double length[3],double diffusion,double conductivity) {
    if(!s || !isfinite(diffusion) || diffusion<0 || !isfinite(conductivity) || conductivity<0) return false;
    memset(s,0,sizeof(*s));
    if(!cfd_cartesian3d_init(&s->grid,n,length) || s->grid.count>65536 ||
       !cfd_mixture_thermo_init(&s->thermo)) return false;
    s->state=cfd_memory_calloc((size_t)20*s->grid.count,sizeof(double));
    if(!s->state) return false;
    s->candidate=s->state+10*s->grid.count;
    s->diffusion_m2_s=diffusion;s->conductivity_w_m_k=conductivity;return true;
}
void cfd_mixture3d_destroy(CfdMixture3d *s) {
    if(s) {cfd_memory_free(s->state);s->state=NULL;s->candidate=NULL;}
}
bool cfd_mixture3d_step(CfdMixture3d *s,const double *source,double dt) {
    if(!s || !s->state || !source || !isfinite(dt) || dt<=0) return false;
    int n=s->grid.count;double maximum[3]={0},alpha=s->diffusion_m2_s;
    for(int q=0;q<n;++q) {
        double trial[10];
        for(int i=0;i<10;++i) {
            if(!isfinite(source[10*q+i])) return false;
            trial[i]=s->state[10*q+i]+source[10*q+i];s->candidate[10*q+i]=trial[i];
        }
        for(int stage=0;stage<2;++stage) {
            const double *u=stage?trial:s->state+10*q;double t,p,c,cv;
            if(!cfd_mixture_primitive(&s->thermo,u,&t,&p,&c,&cv)) return false;
            alpha=fmax(alpha,s->conductivity_w_m_k/(u[0]*cv));
            for(int a=0;a<3;++a) maximum[a]=fmax(maximum[a],fabs(u[a+1]/u[0])+c);
        }
    }
    double courant=0;
    for(int a=0;a<3;++a) courant+=dt*(maximum[a]/s->grid.h[a]+2*alpha/(s->grid.h[a]*s->grid.h[a]));
    if(!isfinite(courant) || courant>.4) return false;
    for(int q=0;q<n;++q) for(int a=0;a<3;++a) {
        int right=cfd_cartesian3d_neighbor(&s->grid,q,a,1);double f[10],d[10],speed;
        if(!cfd_mixture_flux(&s->thermo,s->state+10*q,s->state+10*right,a,
            s->diffusion_m2_s,s->conductivity_w_m_k,s->grid.h[a],f,d,&speed)) return false;
        for(int i=0;i<10;++i) {
            double transfer=dt/s->grid.h[a]*f[i];
            s->candidate[10*q+i]-=transfer;s->candidate[10*right+i]+=transfer;
        }
    }
    for(int q=0;q<n;++q) {
        double t,p,c,cv;
        if(!cfd_mixture_primitive(&s->thermo,s->candidate+10*q,&t,&p,&c,&cv)) return false;
    }
    memcpy(s->state,s->candidate,(size_t)10*n*sizeof(double));return true;
}
