/* Fully developed slit driven from rest by a uniform pressure-gradient
 * equivalent body force. The native step/operator is included to permit that
 * uniform periodic forcing, outside the public interior-impulse envelope.
 * The reference-only ceiling mirrors the ordinary floor's odd ghost closure.
 */
static double ceiling_speed;
static double physics_sim_gap_ceiling_velocity(int component) {
    return component==0?ceiling_speed:0;
}
#define PHYSICS_SIM_OPEN_NO_SLIP_CEILING_REFERENCE 1
#include "../app/cfd_open_atmosphere3d.c"
#include <stdio.h>
#include <stdlib.h>

int main(int argc,char **argv) {
    if(argc!=4 && argc!=7)return 2;
    int layers=atoi(argv[1]),higher=atoi(argv[2]);double dt=strtod(argv[3],NULL);
    const char *profile=argc==7?argv[4]:"slit";
    int driving=argc==7?atoi(argv[5]):1,curvature=argc==7?atoi(argv[6]):0;
    if((layers!=4 && layers!=8) || (higher!=0 && higher!=1) ||
       (strcmp(profile,"slit") && strcmp(profile,"couette")) ||
       driving < -1 || driving > 1 || (curvature!=0 && curvature!=1) ||
       !isfinite(dt) || dt<=0 || dt>.1)return 2;
    CfdMemoryBudget budget={.limit_bytes=128*1024*1024};
    CfdMemoryBudget *prior=cfd_memory_scope(&budget);
    CfdOpenAtmosphere3d s={0};double *zero=NULL,*impulse=NULL,*previous=NULL;
    int status=1,n[3]={4,4,layers};
    double height=.0625,length[3]={.5,.5,height},p[2]={0},ambient[2]={300,300};
    double rho=1.2,mu=1.8e-5,target_mean=.05;
    bool couette=!strcmp(profile,"couette");
    ceiling_speed=couette?.1*driving:0;
    double acceleration=couette?0:12*(mu/rho)*target_mean/(height*height)*driving;
    if(!cfd_open_atmosphere3d_init(&s,n,length,rho,mu,1005,300,0,0,dt,
                                  p,ambient,p,false,0,0,.1))goto done;
    s.ground=true;s.muscl=higher;s.cache_projection=true;
    s.viscous_wall_curvature=curvature;
    zero=cfd_memory_calloc((size_t)s.grid.count,sizeof(double));
    impulse=cfd_memory_calloc((size_t)s.velocity_count,sizeof(double));
    previous=cfd_memory_calloc((size_t)s.velocity_count,sizeof(double));
    if(!zero || !impulse || !previous)goto done;
    for(int k=0;k<layers;k++)for(int j=0;j<4;j++)for(int i=0;i<4;i++) {
        int q=cfd_open_atmosphere3d_face(&s,0,i,j,k);
        impulse[q]=cfd_open_face_mass(&s,0,i,j,k)*acceleration*dt;
    }
    double change=1;int stable=0;
    while(s.steps<9900 && stable<20) {
        memcpy(previous,s.velocity,(size_t)s.velocity_count*sizeof(double));
        CfdOpenMomentumReceipt receipt={0};
        if(!open_step(&s,zero,zero,impulse,&receipt)) {
            fprintf(stderr,"step %d refused: %s\n",s.steps,s.step_rejection);goto done;
        }
        change=0;
        for(int q=0;q<s.velocity_count;q++) {
            if(!isfinite(s.velocity[q]))goto done;
            change=fmax(change,fabs(s.velocity[q]-previous[q])/target_mean);
        }
        for(int k=0;k<=layers;k++)for(int j=0;j<4;j++)for(int i=0;i<4;i++)
            if(s.velocity[cfd_open_atmosphere3d_face(&s,2,i,j,k)]!=0)goto done;
        if(s.divergence>1e-10 || s.residual>1e-11)goto done;
        stable=change<1e-10?stable+1:0;
    }
    if(stable<20) {fprintf(stderr,"steady criterion not reached\n");goto done;}
    printf("{\"layers\":%d,\"higher_order\":%d,\"dt_s\":%.17g,\"steps\":%d,"
           "\"height_m\":%.17g,\"width_m\":0.5,\"rho\":%.17g,\"mu\":%.17g,"
           "\"acceleration_m_s2\":%.17g,\"last_relative_change\":%.17g,"
           "\"divergence_s_inv\":%.17g,\"profile_m_s\":[",
           layers,higher,dt,s.steps,height,rho,mu,acceleration,change,s.divergence);
    for(int k=0;k<layers;k++) {
        double value=s.velocity[cfd_open_atmosphere3d_face(&s,0,0,0,k)];
        for(int j=0;j<4;j++)for(int i=0;i<4;i++)
            if(fabs(s.velocity[cfd_open_atmosphere3d_face(&s,0,i,j,k)]-value)>1e-12)goto done;
        printf("%s%.17g",k?",":"",value);
    }
    printf("],\"profile\":\"%s\",\"driving\":%d,\"curvature\":%d,"
           "\"ceiling_speed_m_s\":%.17g,\"peak_allocated_bytes\":%zu}\n",
           profile,driving,curvature,ceiling_speed,budget.peak_bytes);status=0;
done:
    cfd_memory_free(zero);cfd_memory_free(impulse);cfd_memory_free(previous);
    cfd_open_atmosphere3d_destroy(&s);
    if(budget.live_bytes)status=1;
    cfd_memory_scope(prior);return status;
}
