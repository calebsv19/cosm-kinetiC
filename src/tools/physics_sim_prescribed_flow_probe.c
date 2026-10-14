/* Online old-time coarse thermal forcing; fine velocity evolves independently.
 * Original source mapping and accepted heat/smoke state remain independent. */
#include "physics_sim_prescribed_flow.h"
#include "physics_sim_flow_source.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
static bool init(CfdOpenAtmosphere3d *s,int side) {
    int n[3]={side,side,side};double l[3]={1,1,1},z[2]={0,0},t[2]={300,300};
    if(!cfd_open_atmosphere3d_init(s,n,l,1.2,1.8e-5,1005,300,.026,.0005,.005,z,t,z,true,9.81,1./300,.1))return false;
    s->ground=true;s->muscl=true;s->cache_projection=true;s->max_scalar_work_cells=8000000000ULL;
    return cfd_open_atmosphere3d_lateral(s);
}
static bool write_field(const char *root,const char *name,const double *v,size_t count) {
    char p[4096];if(snprintf(p,sizeof p,"%s/%s.bin",root,name)>=(int)sizeof p)return false;
    FILE *f=fopen(p,"wbx");if(!f)return false;
    bool ok=fwrite(v,sizeof(double),count,f)==count;return fclose(f)==0 && ok;
}
static void summary(CfdOpenAtmosphere3d *s,double div,double res,size_t peak) {
    int n=s->grid.count;double total[2]={0},in[2]={0},out[2]={0},temperature=300;
    for(int q=0;q<n;q++){total[0]+=s->energy[q];total[1]+=s->smoke[q];temperature=fmax(temperature,300+s->energy[q]/(s->rho*s->cp*s->grid.volume));}
    for(int a=0;a<2;a++)for(int q=0;q<s->boundary_faces;q++){
        in[a]+=s->flux[2*a*s->boundary_faces+q];out[a]+=s->flux[(2*a+1)*s->boundary_faces+q];
    }
    printf("{\"qualification_profile\":\"prescribed_thermal_flow_v1\",\"grid\":%d,\"dt_s\":0.005,\"scalar_dt_s\":0.00125,\"time_s\":%.17g,\"flow_steps\":%d,\"scalar_cycles\":%llu,\"scalar_work_cells\":%llu,\"max_scalar_work_cells\":%llu,\"scalar_transport_scheme\":\"muscl_minmod\",\"maximum_divergence\":%.17g,\"maximum_pressure_residual\":%.17g,\"energy_j\":%.17g,\"smoke_kg\":%.17g,\"heat_balance_j\":%.17g,\"smoke_balance_kg\":%.17g,\"peak_temperature_k\":%.17g,\"memory_limit_bytes\":2147483648,\"memory_peak_bytes\":%zu}",s->grid.n[0],s->time,s->steps,s->scalar_work_cells/n,s->scalar_work_cells,s->max_scalar_work_cells,div,res,total[0],total[1],total[0]-s->input_j-in[0]+out[0],total[1]-s->input_kg-in[1]+out[1],temperature,peak);
}
int main(int argc,char **argv) {
    if(argc!=4){fprintf(stderr,"usage: prescribed-flow sources64.bin fine-support.bin fresh-output-directory\n");return 2;}
    FILE *input=fopen(argv[1],"rb"),*fine_input=fopen(argv[2],"rb");if(!input || !fine_input)return 4;
    CfdMemoryBudget budget={.limit_bytes=2147483648ULL};CfdMemoryBudget *prior=cfd_memory_scope(&budget);
    CfdOpenAtmosphere3d s[2];if(!init(s,64) || !init(s+1,128))return 3;
    int n=s[0].grid.count,fn=s[1].grid.count;
    double *source=cfd_memory_calloc((size_t)2*n,sizeof(double));
    double *fine_source=cfd_memory_calloc((size_t)2*fn,sizeof(double));
    double *support=cfd_memory_calloc((size_t)2*n,sizeof(double));
    double *forcing=cfd_memory_calloc((size_t)fn,sizeof(double));
    if(!source || !fine_source || !support || !forcing)return 3;
    double div[2]={0},res[2]={0};int forcing_steps=0;
    for(int frame=0;frame<32;frame++) {
        if(fread(source,sizeof(double),(size_t)2*n,input)!=(size_t)2*n ||
           fread(support,sizeof(double),(size_t)2*n,fine_input)!=(size_t)2*n ||
           !physics_sim_flow_support(support,128,fine_source))return 4;
        for(int q=0;q<2*n;q++)source[q]/=20;
        for(int q=0;q<2*fn;q++)fine_source[q]/=20;
        for(int step=0;step<20;step++) {
            if(s[0].steps!=s[1].steps || s[0].time!=s[1].time || !prescribed_flow_prolong(s,s+1,forcing))return 5;
            if(frame==31 && step==19 && !write_field(argv[3],"forcing-sample-64",s[0].energy,(size_t)n))return 4;
            /* Capture the driver's accepted OLD heat before advancing it: the
             * production momentum operator is explicit in old-time buoyancy. */
            if(!cfd_open_atmosphere3d_step(s,source,source+n) ||
               !prescribed_flow_step(s+1,forcing,fine_source,fine_source+fn)) {
                fprintf(stderr,"rejected frame=%d step=%d coarse=%s fine=%s\n",frame,step,
                        s[0].step_rejection?s[0].step_rejection:"accepted",s[1].step_rejection?s[1].step_rejection:"accepted");return 5;
            }
            forcing_steps++;
            for(int a=0;a<2;a++){div[a]=fmax(div[a],s[a].divergence);res[a]=fmax(res[a],s[a].residual);}
        }
        fprintf(stderr,"frame %d/32\n",frame+1);fflush(stderr);
    }
    if(fgetc(input)!=EOF || ferror(input) || fclose(input) || fgetc(fine_input)!=EOF || ferror(fine_input) || fclose(fine_input))return 4;
    if(!write_field(argv[3],"forcing-sample-128",forcing,(size_t)fn))return 4;
    for(int a=0;a<2;a++) {
        char name[64];snprintf(name,sizeof name,"fields-%d",s[a].grid.n[0]);
        /* energy/smoke are contiguous, velocity is a separate output block. */
        char path[4096];if(snprintf(path,sizeof path,"%s/%s.bin",argv[3],name)>=(int)sizeof path)return 4;
        FILE *out=fopen(path,"wbx");if(!out)return 4;
        if(fwrite(s[a].energy,sizeof(double),(size_t)2*s[a].grid.count,out)!=(size_t)2*s[a].grid.count ||
           fwrite(s[a].velocity,sizeof(double),(size_t)s[a].velocity_count,out)!=(size_t)s[a].velocity_count || fclose(out))return 4;
    }
    printf("{\"schema\":\"prescribed_thermal_flow_native_v1\",\"forcing\":\"coarse_old_time_piecewise_constant_temperature\",\"forcing_steps\":%d,\"forcing_time_offset_s\":0,\"forcing_sample_time_s\":3.195,\"runs\":[",forcing_steps);
    summary(s,div[0],res[0],budget.peak_bytes);printf(",");summary(s+1,div[1],res[1],budget.peak_bytes);
    cfd_memory_free(source);cfd_memory_free(fine_source);cfd_memory_free(support);cfd_memory_free(forcing);
    cfd_open_atmosphere3d_destroy(s);cfd_open_atmosphere3d_destroy(s+1);cfd_memory_scope(prior);
    printf("],\"memory_limit_bytes\":%zu,\"memory_peak_bytes\":%zu,\"memory_live_bytes\":%zu,\"rejected_allocations\":%zu}\n",budget.limit_bytes,budget.peak_bytes,budget.live_bytes,budget.rejected_allocations);
    return budget.live_bytes || budget.rejected_allocations?6:0;
}
