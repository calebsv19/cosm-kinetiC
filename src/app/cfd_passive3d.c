#include "app/cfd_passive3d.h"
#include <math.h>
#include <string.h>
#include <stdint.h>

double cfd_passive3d_total(const double *v, int n) {
    double sum=0, correction=0;
    for (int q=0;q<n;q++) {
        double y=v[q]-correction, next=sum+y;
        correction=(next-sum)-y; sum=next;
    }
    return sum;
}
void cfd_passive3d_destroy(CfdPassive3d *s) {
    if (s) { cfd_memory_free(s->storage); s->storage=NULL; }
}
bool cfd_passive3d_init(CfdPassive3d *s, const int n[3], const double length[3],
    double rho, double cp, double reference_k, double conductivity, double diffusivity) {
    if (!s) return false;
    memset(s,0,sizeof(*s));
    if (!cfd_cartesian3d_init(&s->grid,n,length) || s->grid.count>262144 ||
        !isfinite(rho) || rho<=0 || !isfinite(cp) || cp<=0 ||
        !isfinite(reference_k) || reference_k<=0 || !isfinite(conductivity) || conductivity<0 ||
        !isfinite(diffusivity) || diffusivity<0) return false;
    s->rho=rho;s->cp=cp;s->reference_k=reference_k;
    s->alpha=conductivity/(rho*cp);s->tracer_diffusivity=diffusivity;
    if (!isfinite(rho*cp*s->grid.volume) || rho*cp*s->grid.volume<=0 || !isfinite(s->alpha)) return false;
    s->storage=cfd_memory_calloc((size_t)6*s->grid.count,sizeof(double));
    if (!s->storage) return false;
    s->energy=s->storage;s->smoke=s->energy+s->grid.count;
    s->candidate=s->smoke+s->grid.count;s->work=s->candidate+2*s->grid.count;
    return true;
}
static bool advance(const CfdCartesian3d *g, const double *v, const double *old,
    double *out, const double *source, double dt, double fraction, double diffusion) {
    int n=g->count;
    for (int q=0;q<n;q++) out[q]=old[q]+source[q]*fraction;
    for (int a=0;a<3;a++) for (int q=0;q<n;q++) {
        int left=cfd_cartesian3d_neighbor(g,q,a,-1);
        double u=v[a*n+q];
        double flux=dt*(u*(u>=0?old[left]:old[q])/g->h[a]
            -diffusion*(old[q]-old[left])/(g->h[a]*g->h[a]));
        out[q]+=flux;out[left]-=flux;
    }
    for (int q=0;q<n;q++) if (!isfinite(out[q]) || out[q]<0) return false;
    return true;
}
static bool overlaps(const double *p,size_t values,const double *base,size_t owned) {
    uintptr_t start=(uintptr_t)p, owner=(uintptr_t)base;
    if (start>UINTPTR_MAX-values*sizeof(double) || owner>UINTPTR_MAX-owned*sizeof(double)) return true;
    return start<owner+owned*sizeof(double) && owner<start+values*sizeof(double);
}
bool cfd_passive3d_step(CfdPassive3d *s, const double *v,
    const double *source_j, const double *source_kg, double dt) {
    if (!s || !s->storage || !v || !source_j || !source_kg ||
        !isfinite(dt) || dt<=0 || dt>1 || !isfinite(s->time+dt) || s->time+dt<=s->time ||
        s->steps>=1000000) return false;
    int n=s->grid.count;double maximum=0;
    if (overlaps(v,(size_t)3*n,s->storage,(size_t)6*n) ||
        overlaps(source_j,n,s->storage,(size_t)6*n) || overlaps(source_kg,n,s->storage,(size_t)6*n)) return false;
    for (int q=0;q<n;q++) {
        if (!isfinite(s->energy[q]) || s->energy[q]<0 || !isfinite(s->smoke[q]) || s->smoke[q]<0 ||
            !isfinite(source_j[q]) || source_j[q]<0 || !isfinite(source_kg[q]) || source_kg[q]<0) return false;
        double rate=0,divergence=0;
        for (int a=0;a<3;a++) {
            double lo=v[a*n+q],hi=v[a*n+cfd_cartesian3d_neighbor(&s->grid,q,a,1)],h=s->grid.h[a];
            if (!isfinite(lo) || !isfinite(hi)) return false;
            rate+=(fmax(hi,0)+fmax(-lo,0))/h+2*fmax(s->alpha,s->tracer_diffusivity)/(h*h);
            divergence+=(hi-lo)/h;
        }
        if (!isfinite(rate) || !isfinite(divergence) || fabs(divergence)>=1e-8) return false;
        maximum=fmax(maximum,rate);
    }
    double required=ceil(dt*maximum/.8);
    if (!isfinite(required) || required>4096) return false;
    unsigned cycles=(unsigned)fmax(1,required);
    if (s->work_cells>100000000 || (unsigned long long)cycles*n>100000000-s->work_cells) return false;
    double initial_j=cfd_passive3d_total(s->energy,n), initial_kg=cfd_passive3d_total(s->smoke,n);
    double input_j=cfd_passive3d_total(source_j,n),input_kg=cfd_passive3d_total(source_kg,n);
    if (!isfinite(initial_j+input_j) || !isfinite(initial_kg+input_kg) ||
        !isfinite(s->input_j+input_j) || !isfinite(s->input_kg+input_kg)) return false;
    memcpy(s->candidate,s->energy,(size_t)n*sizeof(double));
    memcpy(s->candidate+n,s->smoke,(size_t)n*sizeof(double));
    for (unsigned i=0;i<cycles;i++) {
        if (!advance(&s->grid,v,s->candidate,s->work,source_j,dt/cycles,1.0/cycles,s->alpha) ||
            !advance(&s->grid,v,s->candidate+n,s->work+n,source_kg,dt/cycles,1.0/cycles,s->tracer_diffusivity)) return false;
        memcpy(s->candidate,s->work,(size_t)2*n*sizeof(double));
    }
    double balance_j=cfd_passive3d_total(s->candidate,n)-initial_j-input_j;
    double balance_kg=cfd_passive3d_total(s->candidate+n,n)-initial_kg-input_kg;
    if (!isfinite(balance_j) || !isfinite(balance_kg) ||
        fabs(balance_j)>1e-11*fmax(initial_j+input_j,1e-12) ||
        fabs(balance_kg)>1e-11*fmax(initial_kg+input_kg,1e-12)) return false;
    for (int q=0;q<n;q++) if (!isfinite(s->reference_k+s->candidate[q]/(s->rho*s->cp*s->grid.volume))) return false;
    memcpy(s->energy,s->candidate,(size_t)n*sizeof(double));
    memcpy(s->smoke,s->candidate+n,(size_t)n*sizeof(double));
    s->time+=dt;s->steps++;s->substeps=cycles;s->work_cells+=(unsigned long long)cycles*n;s->input_j+=input_j;s->input_kg+=input_kg;
    s->last_balance_j=balance_j;s->last_balance_kg=balance_kg;
    return true;
}
