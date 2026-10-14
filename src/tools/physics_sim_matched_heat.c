/* Compile the accepted implementation into this diagnostic translation unit
 * to call its private scalar operator without copying it or changing the
 * product API. The ordinary solver object is excluded from this tool's link. */
#include "physics_sim_matched_heat.h"
#include "../app/cfd_open_atmosphere3d.c"

bool matched_heat_velocity(const CfdOpenAtmosphere3d *coarse,
                           CfdOpenAtmosphere3d *fine, double *max_divergence) {
    if(!coarse || !fine || !max_divergence || !coarse->lateral_open ||
       !fine->lateral_open || !coarse->ground || !fine->ground ||
       coarse->solid_box || fine->solid_box)return false;
    for(int a=0;a<3;a++)if(fine->grid.n[a]!=2*coarse->grid.n[a] ||
        fine->grid.length[a]!=coarse->grid.length[a])return false;
    for(int a=0;a<3;a++)for(int k=0;k<fine->grid.n[2]+(a==2);k++)
        for(int j=0;j<fine->grid.n[1]+(a==1);j++)
        for(int i=0;i<fine->grid.n[0]+(a==0);i++) {
            int p[3]={i/2,j/2,k/2},coord=(int[]){i,j,k}[a];
            double value=coarse->velocity[cfd_open_atmosphere3d_face(coarse,a,p[0],p[1],p[2])];
            if(coord%2) {
                p[a]++;
                value=.5*(value+coarse->velocity[cfd_open_atmosphere3d_face(coarse,a,p[0],p[1],p[2])]);
            }
            if(!isfinite(value))return false;
            fine->candidate_velocity[cfd_open_atmosphere3d_face(fine,a,i,j,k)]=value;
        }
    *max_divergence=0;
    for(int q=0;q<fine->grid.count;q++)
        *max_divergence=fmax(*max_divergence,fabs(divergence(fine,fine->candidate_velocity,q)));
    return *max_divergence<1e-8;
}

bool matched_heat_advance(CfdOpenAtmosphere3d *s,const double *source_j,
                          unsigned cycles,double *courant_bound) {
    if(!s || !source_j || !courant_bound || !s->storage || !s->ground ||
       !s->lateral_open || s->solid_box || !s->muscl || cycles<1 || cycles>4 ||
       s->steps>=10000)return false;
    int n=s->grid.count,faces=s->boundary_faces;
    double alpha=s->conductivity/(s->rho*s->cp);
    for(int q=0;q<n;q++)if(!isfinite(source_j[q]) || source_j[q]<0 ||
        !isfinite(s->energy[q]) || s->energy[q]<0)return false;
    for(int a=0;a<3;a++)for(int k=0;k<s->grid.n[2]+(a==2);k++)
        for(int j=0;j<s->grid.n[1]+(a==1);j++)for(int i=0;i<s->grid.n[0]+(a==0);i++) {
            double u=s->candidate_velocity[cfd_open_atmosphere3d_face(s,a,i,j,k)];
            if(!isfinite(u) || (a==2 && k==0 && u!=0))return false;
        }
    /* The accepted local outflow/diffusion stability rate, for heat alone.
     * A sum of global absolute maxima wrongly double-counts through-flow and
     * can reject a safe explicit step. No adaptive cycles are inserted. */
    double maximum=0;
    for(int q=0;q<n;q++) {
        int i=q%s->grid.n[0],j=(q/s->grid.n[0])%s->grid.n[1],k=q/s->plane;
        double rate=0;
        for(int a=0;a<3;a++) {
            int hi_face=cfd_open_atmosphere3d_face(s,a,i+(a==0),j+(a==1),k+(a==2));
            double lo=s->candidate_velocity[a*n+q],hi=s->candidate_velocity[hi_face],h=s->grid.h[a];
            int adjacent=a==2?((q>=s->plane)+(q<(s->grid.n[2]-1)*s->plane)):2;
            rate+=(fmax(hi,0)+fmax(-lo,0))/h+adjacent*alpha/(h*h);
        }
        maximum=fmax(maximum,rate);
    }
    *courant_bound=s->dt/cycles*maximum;
    unsigned long long work=(unsigned long long)cycles*n;
    if(!isfinite(*courant_bound) || *courant_bound>.4 ||
       s->scalar_work_cells>s->max_scalar_work_cells ||
       work>s->max_scalar_work_cells-s->scalar_work_cells)return false;
    memcpy(s->scalar_candidate,s->energy,(size_t)n*sizeof(double));
    memcpy(s->candidate_flux,s->flux,(size_t)4*faces*sizeof(double));
    for(unsigned c=0;c<cycles;c++) {
        if(!scalar(s,s->scalar_candidate,s->scalar_work,source_j,
                   s->dt/cycles,1./cycles,alpha,0))return false;
        memcpy(s->scalar_candidate,s->scalar_work,(size_t)n*sizeof(double));
    }
    double added=cfd_passive3d_total(source_j,n),before=cfd_passive3d_total(s->energy,n);
    double after=cfd_passive3d_total(s->scalar_candidate,n);
    double incoming=cfd_passive3d_total(s->candidate_flux,faces)-cfd_passive3d_total(s->flux,faces);
    double outgoing=cfd_passive3d_total(s->candidate_flux+faces,faces)-cfd_passive3d_total(s->flux+faces,faces);
    if(fabs(after-before-added-incoming+outgoing)>1e-11*fmax(before+added+incoming+outgoing,1e-12))return false;
    for(int q=0;q<n;q++)if(s->scalar_candidate[q]/(s->rho*s->cp*s->grid.volume*s->reference_k)>.1)return false;
    memcpy(s->energy,s->scalar_candidate,(size_t)n*sizeof(double));
    memcpy(s->flux,s->candidate_flux,(size_t)4*faces*sizeof(double));
    s->input_j+=added;s->steps++;s->time=s->steps*s->dt;s->scalar_work_cells+=work;
    return true;
}
