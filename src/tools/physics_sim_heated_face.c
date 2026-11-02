#include "physics_sim_heated_face.h"
#include <math.h>

static bool support(const CfdOpenAtmosphere3d *s,const PhysicsSimHeatedFace *f,
                    double depth,int *layers,int *count) {
    if(!s || !f || !s->solid_box || f->axis<0 || f->axis>2 ||
       (f->side!=-1 && f->side!=1) || !isfinite(depth) || depth<=0)return false;
    double cells=depth/s->grid.h[f->axis];
    if(!isfinite(cells) || cells<1 || cells>s->grid.n[f->axis] ||
       fabs(cells-round(cells))>1e-10)return false;
    *layers=(int)round(cells);
    int available=f->side<0?s->solid_lo[f->axis]:s->grid.n[f->axis]-s->solid_hi[f->axis];
    if(*layers>available)return false;
    int a=(f->axis+1)%3,b=(f->axis+2)%3;
    *count=*layers*(s->solid_hi[a]-s->solid_lo[a])*(s->solid_hi[b]-s->solid_lo[b]);
    return *count>0;
}

static int cell(const CfdOpenAtmosphere3d *s, const PhysicsSimHeatedFace *f, int t) {
    int a=(f->axis+1)%3, b=(f->axis+2)%3;
    int width=s->solid_hi[a]-s->solid_lo[a];
    int plane=width*(s->solid_hi[b]-s->solid_lo[b]),layer=t/plane;t%=plane;
    int p[3];
    p[f->axis]=f->side<0?s->solid_lo[f->axis]-1-layer:s->solid_hi[f->axis]+layer;
    p[a]=s->solid_lo[a]+t%width;p[b]=s->solid_lo[b]+t/width;
    return (p[2]*s->grid.n[1]+p[1])*s->grid.n[0]+p[0];
}

bool physics_sim_heated_face_sample(const CfdOpenAtmosphere3d *s,
    const PhysicsSimHeatedFace *f,double depth,double *rise) {
    int layers,n;
    if(!rise || !support(s,f,depth,&layers,&n) || !cfd_open_atmosphere3d_valid(s))return false;
    double sum=0,correction=0;
    for(int i=0;i<n;i++) {
        double y=s->energy[cell(s,f,i)]-correction,next=sum+y;
        correction=(next-sum)-y;sum=next;
    }
    double value=sum/(n*s->rho*s->cp*s->grid.volume);
    if(!isfinite(value))return false;
    *rise=value;return true;
}

bool physics_sim_heated_face_exchange(CfdOpenAtmosphere3d *s,
                                     PhysicsSimHeatedFace *f, double dt) {
    if(!s || !f || f->axis<0 || f->axis>2)return false;
    return physics_sim_heated_face_exchange_depth(s,f,dt,s->grid.h[f->axis]);
}

static bool exchange_cells(CfdOpenAtmosphere3d *s,PhysicsSimHeatedFace *f,
    double dt,int n,int first,int stride,double conductance,bool validated) {
    if(!s || !f || (!validated && !cfd_open_atmosphere3d_valid(s)) ||
       f->axis<0 || f->axis>2 || (f->side!=-1 && f->side!=1) ||
       !isfinite(dt) || dt<=0 || !isfinite(f->temperature_k) ||
       f->temperature_k<s->reference_k || !isfinite(f->capacity_j_k) ||
       f->capacity_j_k<=0 || !isfinite(f->h_w_m2_k) || f->h_w_m2_k<0 ||
       !isfinite(f->gas_to_surface_j) || !isfinite(f->temperature_correction_k))return false;
    double cg=s->rho*s->cp*s->grid.volume, cs=f->capacity_j_k;
    /* Exact pair transfer: Cg*Cs/(Cg+Cs) (Tg-Ts) (1-exp(-k dt)).
     * Avoid a capacity product and preserve small-exponent precision. */
    double rate=conductance*(1/cg+1/cs), reduced=1/(1/cg+1/cs);
    if(!isfinite(rate) || !isfinite(reduced) || reduced<=0)return false;
    double factor=-expm1(-rate*dt*.5);
    if(!isfinite(factor))return false;
    double *work=cfd_memory_calloc((size_t)n,sizeof(double));
    if(!work)return false;
    for(int t=0;t<n;t++)work[t]=s->energy[(first>=0?first+t*stride:cell(s,f,t))];
    double ts=(f->temperature_k-s->reference_k)+f->temperature_correction_k, transfer=0;
    if(!isfinite(ts) || ts<0){cfd_memory_free(work);return false;}
    bool ok=true;
    for(int pass=0;pass<2 && ok;pass++)for(int i=0;i<n;i++) {
        int t=pass?n-1-i:i;
        double tg=work[t]/cg;
        double q=reduced*(tg-ts)*factor;
        double next=work[t]-q, next_ts=ts+q/cs;
        if(!isfinite(next) || next<0 || !isfinite(next_ts) ||
           next_ts<0 || !isfinite(transfer+q) ||
           (s->buoyancy && next/cg/s->reference_k>s->max_contrast)) {ok=false;break;}
        work[t]=next;ts=next_ts;transfer+=q;
    }
    if(!isfinite(f->gas_to_surface_j+transfer) || !isfinite(s->reference_k+ts))ok=false;
    if(ok) {
        for(int t=0;t<n;t++)s->energy[(first>=0?first+t*stride:cell(s,f,t))]=work[t];
        f->temperature_k=s->reference_k+ts;
        f->temperature_correction_k=ts-(f->temperature_k-s->reference_k);
        f->gas_to_surface_j+=transfer;
    }
    cfd_memory_free(work);
    return ok;
}

bool physics_sim_heated_face_exchange_depth(CfdOpenAtmosphere3d *s,
    PhysicsSimHeatedFace *f,double dt,double depth) {
    int layers,n;
    if(!support(s,f,depth,&layers,&n))return false;
    return exchange_cells(s,f,dt,n,-1,1,f->h_w_m2_k*s->grid.area[f->axis]/layers,false);
}

/* Prescribed-flow diagnostic: a nonlocal full-cross-section exchange slab.
 * No wall or obstacle is added. Uses the same pair-transfer kernel as the box. */
bool physics_sim_heated_slab_exchange(CfdOpenAtmosphere3d *s,
    PhysicsSimHeatedFace *f,double dt,double lo,double hi) {
    if(!s||!f||s->solid_box||f->axis!=2||f->side!=-1||!isfinite(lo)||!isfinite(hi)||
       lo<0||hi<=lo||hi>s->grid.length[2])return false;
    double a=lo/s->grid.h[2],b=hi/s->grid.h[2];
    if(fabs(a-round(a))>1e-10||fabs(b-round(b))>1e-10)return false;
    int first=(int)round(a)*s->plane,n=((int)round(b)-(int)round(a))*s->plane;
    if(n<=0)return false;
    double area=s->grid.length[0]*s->grid.length[1];
    return exchange_cells(s,f,dt,n,first,1,f->h_w_m2_k*area/n,false);
}

/* One bottom-layer strip per x cell, spanning y. Atomic across all columns.
 * Validate the full grid once, then reuse the unchanged exact pair kernel. */
bool physics_sim_heated_columns_exchange(CfdOpenAtmosphere3d *s,
    PhysicsSimHeatedFace *faces,int count,double dt) {
    if(!s||!faces||!s->ground||s->solid_box||count!=s->grid.n[0]||
       !cfd_open_atmosphere3d_valid(s))return false;
    for(int i=0;i<count;i++)if(faces[i].axis!=2||faces[i].side!=-1)return false;
    double *saved=cfd_memory_calloc((size_t)s->plane,sizeof(double));
    PhysicsSimHeatedFace *copy=cfd_memory_calloc((size_t)count,sizeof(*copy));
    if(!saved||!copy){cfd_memory_free(saved);cfd_memory_free(copy);return false;}
    for(int i=0;i<s->plane;i++)saved[i]=s->energy[i];
    for(int i=0;i<count;i++)copy[i]=faces[i];
    bool ok=true;
    for(int i=0;i<count&&ok;i++)ok=exchange_cells(s,&copy[i],dt,s->grid.n[1],i,
        count,copy[i].h_w_m2_k*s->grid.area[2],true);
    if(ok)for(int i=0;i<count;i++)faces[i]=copy[i];
    else for(int i=0;i<s->plane;i++)s->energy[i]=saved[i];
    cfd_memory_free(saved);cfd_memory_free(copy);return ok;
}
