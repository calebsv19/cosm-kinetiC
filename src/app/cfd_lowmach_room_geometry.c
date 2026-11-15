#include "cfd_lowmach_room_internal.h"
#include "app/cfd_memory.h"
#include <math.h>
#include <string.h>

static int key(int n, int a, const int p[3]) {
    int m=n+1;
    return ((a*m+p[2])*m+p[1])*m+p[0];
}
int cfd_lm_lookup(const CfdLmRoom *s, int a, const int p[3]) {
    for(int d=0;d<3;d++) if(p[d]<0 || p[d]>=s->n+(a==d)) return -1;
    return s->lookup[key(s->n,a,p)];
}
void cfd_lm_destroy(CfdLmRoom *s) {
    if(!s) return;
    cfd_memory_free(s->solid); cfd_memory_free(s->cell_face);
    cfd_memory_free(s->lookup); cfd_memory_free(s->face);
    cfd_memory_free(s->rho); cfd_memory_free(s->enthalpy);
    cfd_memory_free(s->velocity); cfd_memory_free(s->pressure);
    memset(s,0,sizeof(*s));
}
bool cfd_lm_create(CfdLmRoom *s,int n,bool vent,bool obstacle,double gravity) {
    if(!s || (n!=16 && n!=32) || !isfinite(gravity) || gravity>0 || gravity< -9.81) return false;
    CfdLmRoom t={0}; t.n=n;t.cells=n*n*n;t.vent=vent;t.obstacle=obstacle;
    t.gravity=gravity;t.p0=CFD_LM_P0;
    t.solid=cfd_memory_calloc((size_t)t.cells,1);
    t.cell_face=cfd_memory_malloc((size_t)t.cells*6*sizeof(int));
    t.lookup=cfd_memory_malloc((size_t)3*(n+1)*(n+1)*(n+1)*sizeof(int));
    t.face=cfd_memory_calloc((size_t)3*t.cells,sizeof(*t.face));
    t.rho=cfd_memory_calloc((size_t)t.cells,sizeof(double));
    t.enthalpy=cfd_memory_calloc((size_t)t.cells,sizeof(double));
    t.pressure=cfd_memory_calloc((size_t)t.cells,sizeof(double));
    t.velocity=cfd_memory_calloc((size_t)3*t.cells,sizeof(double));
    if(!t.solid||!t.cell_face||!t.lookup||!t.face||!t.rho||!t.enthalpy||!t.pressure||!t.velocity) {
        cfd_lm_destroy(&t);return false;
    }
    for(int q=0;q<t.cells*6;q++)t.cell_face[q]=-1;
    for(int q=0;q<3*(n+1)*(n+1)*(n+1);q++)t.lookup[q]=-1;
    /* Same SI surfaces as the existing small single-window room. The closed
     * variant closes the outer aperture plane but retains its gas volume. */
    for(int k=0;k<n;k++)for(int j=0;j<n;j++)for(int i=0;i<n;i++) {
        int q=(k*n+j)*n+i;
        bool opening=j>=3*n/8 && j<5*n/8 && k>=3*n/8 && k<7*n/8;
        bool block=obstacle && i>=n/2 && i<5*n/8 && j>=n/4 && j<3*n/8 && k<n/4;
        t.solid[q]=(unsigned char)(i<n/16||j<n/16||j>=15*n/16||k>=15*n/16||(i>=15*n/16&&!opening)||block);
        if(!t.solid[q]) {
            t.fluid_cells++;t.rho[q]=CFD_LM_P0/(CFD_LM_R*CFD_LM_T0);
            t.enthalpy[q]=CFD_LM_CP/CFD_LM_R*CFD_LM_P0;
        }
    }
    for(int k=0;k<n;k++)for(int j=0;j<n;j++)for(int i=0;i<n;i++) {
        int q=(k*n+j)*n+i,p[3]={i,j,k},stride[3]={1,n,n*n};
        if(t.solid[q])continue;
        for(int a=0;a<3;a++) {
            int other=p[a]+1<n ? q+stride[a] : -1;
            if((other>=0&&t.solid[other]) || (other<0&&!(vent&&a==0)))continue;
            int f=t.faces++;t.face[f]=(CfdLmFace){.left=q,.right=other,.axis=a,.xyz={i,j,k}};
            t.face[f].xyz[a]++;
            t.lookup[key(n,a,t.face[f].xyz)]=f;t.cell_face[6*q+2*a+1]=f;
            if(other>=0)t.cell_face[6*other+2*a]=f;
        }
    }
    *s=t;return true;
}
bool cfd_lm_valid(const CfdLmRoom *s) {
    if(!s || (s->n!=16&&s->n!=32) || s->cells!=s->n*s->n*s->n ||
       !s->rho||!s->enthalpy||!s->velocity||!s->pressure||!s->solid||!s->face||
       !isfinite(s->p0)||s->p0<50000||s->p0>200000||!isfinite(s->gravity)||
       s->gravity>0||s->gravity< -9.81||!isfinite(s->time)||s->time<0||s->time>.200000000001||s->steps>4000) return false;
    if(s->vent && s->p0!=CFD_LM_P0)return false;
    for(int q=0;q<s->cells;q++) {
        if(!isfinite(s->pressure[q]))return false;
        if(s->solid[q]) {if(s->rho[q]!=0||s->enthalpy[q]!=0||s->pressure[q]!=0)return false;continue;}
        double t=s->enthalpy[q]/(CFD_LM_CP*s->rho[q]);
        if(!isfinite(s->rho[q])||s->rho[q]<=0||!isfinite(t)||t<250||t>1200||
           fabs(s->enthalpy[q]*CFD_LM_R/CFD_LM_CP/s->p0-1)>1e-8)return false;
    }
    for(int f=0;f<s->faces;f++)if(!isfinite(s->velocity[f])||fabs(s->velocity[f])>30)return false;
    return true;
}
void cfd_lm_heater(const CfdLmRoom *s,double strength,double *heat) {
    int n=s->n;
    for(int q=0;q<s->cells;q++) {
        int i=q%n,j=q/n%n,k=q/(n*n);
        heat[q]=!s->solid[q]&&i>=3*n/8&&i<5*n/8&&j>=3*n/8&&j<5*n/8&&k>=n/8&&k<n/4 ? strength : 0;
    }
}
