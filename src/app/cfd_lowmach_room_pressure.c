#include "cfd_lowmach_room_internal.h"
#include "app/cfd_memory.h"
#include <math.h>
#include <string.h>

static double beta(const CfdLmRoom *s,int f) {
    CfdLmFace v=s->face[f];
    return v.right<0 ? 1/s->rho[v.left] : 2/(s->rho[v.left]+s->rho[v.right]);
}
static void apply(const CfdLmRoom *s,const double *x,double *y) {
    memset(y,0,(size_t)s->cells*sizeof(double));
    for(int f=0;f<s->faces;f++) {
        CfdLmFace v=s->face[f];
        double a=beta(s,f)*s->n*s->n*(v.right<0?2:1);
        double d=a*(x[v.left]-(v.right<0?0:x[v.right]));
        y[v.left]+=d;if(v.right>=0)y[v.right]-=d;
    }
}
static double dot(const CfdLmRoom *s,const double *a,const double *b) {
    double v=0;for(int q=0;q<s->cells;q++)if(!s->solid[q])v+=a[q]*b[q];return v;
}
static void mean_free(const CfdLmRoom *s,double *x) {
    if(s->vent)return;
    double mean=0;for(int q=0;q<s->cells;q++)if(!s->solid[q])mean+=x[q];
    mean/=s->fluid_cells;for(int q=0;q<s->cells;q++)if(!s->solid[q])x[q]-=mean;
}
bool cfd_lm_project(const CfdLmRoom *s,double dt,const double *target,
                    double *u,double *p,CfdLmReceipt *receipt) {
    int n=s->cells;
    double *work=cfd_memory_calloc((size_t)n*6,sizeof(double));if(!work)return false;
    double *b=work,*r=b+n,*z=r+n,*d=z+n,*ad=d+n,*diag=ad+n;
    for(int q=0;q<n;q++)b[q]=target[q]/dt;
    for(int f=0;f<s->faces;f++) {
        CfdLmFace v=s->face[f];double flux=u[f]*s->n/dt;
        b[v.left]-=flux;if(v.right>=0)b[v.right]+=flux;
        double a=beta(s,f)*s->n*s->n*(v.right<0?2:1);
        diag[v.left]+=a;if(v.right>=0)diag[v.right]+=a;
    }
    mean_free(s,b);memcpy(r,b,(size_t)n*sizeof(double));memset(p,0,(size_t)n*sizeof(double));
    for(int q=0;q<n;q++)if(!s->solid[q])z[q]=r[q]/diag[q];
    mean_free(s,z);memcpy(d,z,(size_t)n*sizeof(double));
    double rz=dot(s,r,z),norm=sqrt(dot(s,b,b)),tol=1e-11*fmax(norm,1.);
    bool ok=false;
    for(int it=0;it<2000;it++) {
        if(sqrt(dot(s,r,r))<=tol) {ok=true;break;}
        apply(s,d,ad);double den=dot(s,d,ad);
        if(!isfinite(den)||den<=0||!isfinite(rz))break;
        double alpha=rz/den;
        for(int q=0;q<n;q++) {p[q]+=alpha*d[q];r[q]-=alpha*ad[q];}
        mean_free(s,r);
        for(int q=0;q<n;q++)if(!s->solid[q])z[q]=r[q]/diag[q];
        mean_free(s,z);double next=dot(s,r,z),ratio=next/rz;
        for(int q=0;q<n;q++)d[q]=z[q]+ratio*d[q];
        rz=next;receipt->pressure_iterations=it+1;
    }
    /* Independently recompute the actual operator residual, including the
     * unpinned sealed-cell compatibility equation. No hidden RHS correction. */
    if(ok) {
        mean_free(s,p);apply(s,p,ad);
        for(int q=0;q<n;q++)if(!s->solid[q]&&fabs(ad[q]-b[q])>5*tol)ok=false;
    }
    if(ok) {
        memset(r,0,(size_t)n*sizeof(double));
        for(int f=0;f<s->faces;f++) {
            CfdLmFace v=s->face[f];
            u[f]+=dt*beta(s,f)*s->n*(v.right<0?2:1)*(p[v.left]-(v.right<0?0:p[v.right]));
            r[v.left]+=s->n*u[f];if(v.right>=0)r[v.right]-=s->n*u[f];
        }
        for(int q=0;q<n;q++)if(!s->solid[q])receipt->divergence_error=fmax(receipt->divergence_error,fabs(r[q]-target[q]));
        ok=isfinite(receipt->divergence_error)&&receipt->divergence_error<1e-8;
    }
    cfd_memory_free(work);return ok;
}
