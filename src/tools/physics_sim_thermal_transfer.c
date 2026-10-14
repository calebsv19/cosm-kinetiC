/* Qualification-only nested thermal transfer, independent of source mapping.
 * MC slopes in coarse-cell extensive energy; symmetric child offsets conserve
 * the parent mean. A common limiter bounds all eight children by the six face
 * neighbours plus the parent. Physical domain-edge slopes are zero. */
#include "physics_sim_prescribed_flow.h"
#include <math.h>
static double mc(double a,double b) {
    if(a*b<=0)return 0;
    return copysign(fmin(.5*fabs(a+b),fmin(2*fabs(a),2*fabs(b))),a);
}
bool prescribed_flow_reconstruct(const CfdOpenAtmosphere3d *c,const CfdOpenAtmosphere3d *f,double *out) {
    if(!c || !f || !out || !c->storage || !f->storage || c->solid_box || f->solid_box ||
       out==c->energy || out==f->energy || c->rho!=f->rho || c->cp!=f->cp || c->reference_k!=f->reference_k)return false;
    for(int a=0;a<3;a++)if(f->grid.n[a]!=2*c->grid.n[a] || f->grid.length[a]!=c->grid.length[a])return false;
    for(int q=0;q<c->grid.count;q++)if(!isfinite(c->energy[q]) || c->energy[q]<0)return false;
    int nx=c->grid.n[0],ny=c->grid.n[1],nz=c->grid.n[2],stride[3]={1,nx,nx*ny};
    for(int k=0;k<nz;k++)for(int j=0;j<ny;j++)for(int i=0;i<nx;i++) {
        int q=(k*ny+j)*nx+i,pos[3]={i,j,k};double v=c->energy[q],lo=v,hi=v,d[3]={0};
        for(int a=0;a<3;a++) {
            bool lower=pos[a]>0,upper=pos[a]+1<c->grid.n[a];
            if(lower){lo=fmin(lo,c->energy[q-stride[a]]);hi=fmax(hi,c->energy[q-stride[a]]);}
            if(upper){lo=fmin(lo,c->energy[q+stride[a]]);hi=fmax(hi,c->energy[q+stride[a]]);}
            if(lower && upper)d[a]=mc(v-c->energy[q-stride[a]],c->energy[q+stride[a]]-v);
        }
        double excursion=.25*(fabs(d[0])+fabs(d[1])+fabs(d[2]));
        double limit=excursion>0?fmin(1,fmin(v-lo,hi-v)/excursion):1;
        for(int z=0;z<2;z++)for(int y=0;y<2;y++)for(int x=0;x<2;x++) {
            double delta=.25*limit*((2*x-1)*d[0]+(2*y-1)*d[1]+(2*z-1)*d[2]);
            /* Enforce the analytic bounds after roundoff in the shared limiter.
             * Only roundoff excursions are clipped; mean closure is tested. */
            out[(((2*k+z)*2*ny+2*j+y)*2*nx+2*i+x)]=fmax(lo,fmin(hi,v+delta))/8;
        }
    }
    return true;
}
