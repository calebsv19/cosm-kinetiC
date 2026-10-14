/* Online replay: one accepted evolving 64^3 flow, four independent heat
 * carriers. No velocity-history files or fine-grid momentum solve. */
#include "physics_sim_matched_heat.h"
#include "app/cfd_passive3d.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool init(CfdOpenAtmosphere3d *s,int side) {
    int n[3]={side,side,side};double length[3]={1,1,1},zero[2]={0,0},ambient[2]={300,300};
    if(!cfd_open_atmosphere3d_init(s,n,length,1.2,1.8e-5,1005,300,.026,.0005,.005,
                                  zero,ambient,zero,true,9.81,1./300,.1))return false;
    s->ground=true;s->muscl=true;s->cache_projection=true;s->max_scalar_work_cells=8000000000ULL;
    return cfd_open_atmosphere3d_lateral(s);
}

static bool write_field(const char *root,const char *name,const double *data,size_t count) {
    char path[4096];if(snprintf(path,sizeof path,"%s/%s.bin",root,name)>=(int)sizeof path)return false;
    FILE *f=fopen(path,"wbx");if(!f)return false;
    bool ok=fwrite(data,sizeof(double),count,f)==count;return fclose(f)==0 && ok;
}

int main(int argc,char **argv) {
    if(argc!=3){fprintf(stderr,"usage: matched-heat sources64.bin existing-empty-output-directory\n");return 2;}
    FILE *input=fopen(argv[1],"rb");if(!input)return 4;
    CfdMemoryBudget budget={.limit_bytes=2147483648ULL};CfdMemoryBudget *prior=cfd_memory_scope(&budget);
    CfdOpenAtmosphere3d driver,s[4];
    if(!init(&driver,64))return 3;
    for(int a=0;a<4;a++)if(!init(s+a,a<2?64:128))return 3;
    int n=driver.grid.count,fn=s[2].grid.count;
    double *source=cfd_memory_calloc((size_t)2*n,sizeof(double));
    double *fine_source=cfd_memory_calloc((size_t)fn,sizeof(double));
    if(!source || !fine_source)return 3;
    double courant[4]={0},maxdiv=0,control_delta=0,driver_maxdiv=0,driver_maxresidual=0;
    for(int frame=0;frame<32;frame++) {
        if(fread(source,sizeof(double),(size_t)2*n,input)!=(size_t)2*n)return 4;
        for(int q=0;q<2*n;q++)source[q]/=20;
        for(int k=0;k<128;k++)for(int j=0;j<128;j++)for(int i=0;i<128;i++)
            fine_source[(k*128+j)*128+i]=source[((k/2)*64+j/2)*64+i/2]/8;
        for(int t=0;t<20;t++) {
            if(!cfd_open_atmosphere3d_step(&driver,source,source+n)) {
                fprintf(stderr,"driver rejected frame=%d step=%d: %s\n",frame,t,driver.step_rejection);return 5;
            }
            driver_maxdiv=fmax(driver_maxdiv,driver.divergence);
            driver_maxresidual=fmax(driver_maxresidual,driver.residual);
            for(int a=0;a<2;a++)memcpy(s[a].candidate_velocity,driver.velocity,(size_t)driver.velocity_count*sizeof(double));
            double div;if(!matched_heat_velocity(&driver,s+2,&div))return 5;
            maxdiv=fmax(maxdiv,div);
            memcpy(s[3].candidate_velocity,s[2].candidate_velocity,(size_t)s[2].velocity_count*sizeof(double));
            for(int a=0;a<4;a++) {
                double bound;
                if(!matched_heat_advance(s+a,a<2?source:fine_source,a%2?2:1,&bound)) {
                    fprintf(stderr,"heat rejected carrier=%d frame=%d step=%d bound=%.17g\n",a,frame,t,bound);return 5;
                }
                courant[a]=fmax(courant[a],bound);
            }
            if(memcmp(driver.energy,s[0].energy,(size_t)n*sizeof(double))) {
                fprintf(stderr,"accepted control mismatch\n");return 6;
            }
        }
        fprintf(stderr,"frame %d/32\n",frame+1);fflush(stderr);
    }
    if(fgetc(input)!=EOF || ferror(input) || fclose(input))return 4;
    const char *names[4]={"heat-64-dt005","heat-64-dt0025","heat-128-dt005","heat-128-dt0025"};
    if(!write_field(argv[2],"driver-fields",driver.energy,(size_t)2*n))return 4;
    if(!write_field(argv[2],"driver-velocity",driver.velocity,(size_t)driver.velocity_count))return 4;
    printf("{\"schema\":\"matched_heat_v1\",\"velocity_driver_grid\":64,\"flow_dt_s\":0.005,\"time_s\":3.2,\"driver_scalar_cycles\":%llu,\"driver_max_divergence\":%.17g,\"driver_max_pressure_residual\":%.17g,\"fine_max_divergence\":%.17g,\"control_bit_identical_each_step\":true,\"control_max_difference_j\":%.17g,\"carriers\":[",driver.scalar_work_cells/n,driver_maxdiv,driver_maxresidual,maxdiv,control_delta);
    for(int a=0;a<4;a++) {
        if(!write_field(argv[2],names[a],s[a].energy,(size_t)s[a].grid.count))return 4;
        double total=cfd_passive3d_total(s[a].energy,s[a].grid.count);
        double incoming=cfd_passive3d_total(s[a].flux,s[a].boundary_faces);
        double outgoing=cfd_passive3d_total(s[a].flux+s[a].boundary_faces,s[a].boundary_faces),peak=300;
        for(int q=0;q<s[a].grid.count;q++)peak=fmax(peak,300+s[a].energy[q]/(1.2*1005*s[a].grid.volume));
        printf("%s{\"name\":\"%s\",\"grid\":%d,\"scalar_dt_s\":%.17g,\"scalar_cycles\":%llu,\"energy_j\":%.17g,\"input_j\":%.17g,\"inflow_j\":%.17g,\"outflow_j\":%.17g,\"heat_balance_j\":%.17g,\"peak_temperature_k\":%.17g,\"maximum_courant_bound\":%.17g}",a?",":"",names[a],s[a].grid.n[0],.005/(a%2?2:1),s[a].scalar_work_cells/s[a].grid.count,total,s[a].input_j,incoming,outgoing,total-s[a].input_j-incoming+outgoing,peak,courant[a]);
    }
    cfd_memory_free(source);cfd_memory_free(fine_source);cfd_open_atmosphere3d_destroy(&driver);
    for(int a=0;a<4;a++)cfd_open_atmosphere3d_destroy(s+a);
    cfd_memory_scope(prior);
    printf("],\"memory_limit_bytes\":%zu,\"memory_peak_bytes\":%zu,\"memory_live_bytes\":%zu,\"rejected_allocations\":%zu}\n",budget.limit_bytes,budget.peak_bytes,budget.live_bytes,budget.rejected_allocations);
    return budget.live_bytes || budget.rejected_allocations?6:0;
}
