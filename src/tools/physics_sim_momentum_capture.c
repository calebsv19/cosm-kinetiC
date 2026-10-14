/* Trusted-local, same-ABI diagnostic state. No pointer or solver scratch is
 * stored as accepted state. Cached matrices are rebuilt on load. */
#include "physics_sim_momentum_capture.h"
#include "app/cfd_open_momentum_diagnostics.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
CfdOpenMomentumDiagnostics cfd_open_momentum_diagnostics;
void cfd_open_momentum_diagnostics_begin(void) {
    memset(&cfd_open_momentum_diagnostics,0,sizeof cfd_open_momentum_diagnostics);
    cfd_open_momentum_diagnostics.check="not_evaluated";
    cfd_open_momentum_diagnostics.linear_failure="not_evaluated";
    cfd_open_momentum_diagnostics.pressure_iterations=-1;
    cfd_open_momentum_diagnostics.divergence_cell=-1;
    cfd_open_momentum_diagnostics.pressure_residual=NAN;
    cfd_open_momentum_diagnostics.maximum_divergence=NAN;
    cfd_open_momentum_diagnostics.cg_recursive_residual=NAN;
    cfd_open_momentum_diagnostics.cg_true_residual=NAN;
    cfd_open_momentum_diagnostics.cg_preconditioned_dot=NAN;
    cfd_open_momentum_diagnostics.cg_operator_dot=NAN;
}
static void header(const CfdOpenAtmosphere3d *s,uint64_t u[12],double v[26]) {
    uint64_t a[12]={UINT64_C(0x4d4f4d5354415445),1,s->grid.n[0],s->grid.n[1],s->grid.n[2],s->grid.count,
        s->velocity_count,s->boundary_faces,s->steps,s->scalar_work_cells,s->max_scalar_work_cells,
        s->ground+2*s->muscl+4*s->buoyancy+8*s->lateral_open+16*s->cache_projection};
    double b[26]={s->grid.length[0],s->grid.length[1],s->grid.length[2],s->rho,s->mu,s->cp,s->reference_k,
        s->conductivity,s->diffusivity,s->dt,s->gravity,s->beta,s->max_contrast,
        s->pressure_boundary[0],s->pressure_boundary[1],s->ambient_k[0],s->ambient_k[1],s->ambient_smoke[0],s->ambient_smoke[1],
        s->time,s->initial_j,s->initial_kg,s->input_j,s->input_kg,s->divergence,s->residual};
    memcpy(u,a,sizeof a);memcpy(v,b,sizeof b);
}
static uint64_t hash(uint64_t h,const void *p,size_t size) {
    const unsigned char *b=p;for(size_t i=0;i<size;i++){h^=b[i];h*=UINT64_C(1099511628211);}return h;
}
uint64_t physics_sim_accepted_fingerprint(const CfdOpenAtmosphere3d *s) {
    uint64_t u[12];double v[26];header(s,u,v);uint64_t h=UINT64_C(14695981039346656037);
    h=hash(h,u,sizeof u);h=hash(h,v,sizeof v);
    h=hash(h,s->velocity,(size_t)s->velocity_count*8);h=hash(h,s->pressure,(size_t)s->grid.count*8);
    h=hash(h,s->energy,(size_t)2*s->grid.count*8);return hash(h,s->flux,(size_t)4*s->boundary_faces*8);
}
static bool write_array(FILE *f,const void *p,size_t size,size_t count){return fwrite(p,size,count,f)==count;}
static FILE *open_part(const char *prefix,const char *suffix){char path[4096];if(snprintf(path,sizeof path,"%s%s",prefix,suffix)>=(int)sizeof path)return NULL;return fopen(path,"wbx");}
static void number(FILE *f,const char *name,double v){fprintf(f,"\"%s\":",name);if(isfinite(v))fprintf(f,"%.17g",v);else fputs("null",f);fputc(',',f);}
bool physics_sim_momentum_capture(const CfdOpenAtmosphere3d *s,const double *source,const char *prefix,
    int frame,int step,uint64_t before,const CfdMemoryBudget *budget) {
    if(!s || s->solid_box || !source || !prefix || !budget)return false;
    uint64_t u[12];double v[26];header(s,u,v);FILE *f=open_part(prefix,".accepted.bin");if(!f)return false;
    bool ok=write_array(f,u,8,12)&&write_array(f,v,8,26)&&write_array(f,s->velocity,8,s->velocity_count)&&
        write_array(f,s->pressure,8,s->grid.count)&&write_array(f,s->energy,8,2*s->grid.count)&&write_array(f,s->flux,8,4*s->boundary_faces);
    if(fclose(f)||!ok)return false;
    f=open_part(prefix,".candidate.bin");if(!f)return false;
    ok=write_array(f,s->candidate_velocity,8,s->velocity_count)&&write_array(f,s->candidate_pressure,8,s->grid.count)&&write_array(f,s->rhs,8,s->grid.count);
    if(fclose(f)||!ok)return false;
    f=open_part(prefix,".source.bin");if(!f)return false;
    ok=write_array(f,source,8,2*s->grid.count);if(fclose(f)||!ok)return false;
    f=open_part(prefix,".rejection.json");if(!f)return false;
    const CfdOpenMomentumDiagnostics *d=&cfd_open_momentum_diagnostics;uint64_t after=physics_sim_accepted_fingerprint(s);
    fprintf(f,"{\"schema\":\"momentum_rejection_v1\",\"frame_zero_based\":%d,\"step_zero_based\":%d,\"grid\":[%d,%d,%d],\"accepted_steps\":%d,",frame,step,s->grid.n[0],s->grid.n[1],s->grid.n[2],s->steps);
    number(f,"accepted_time_s",s->time);number(f,"flow_dt_s",s->dt);number(f,"scalar_dt_s",PHYSICS_SIM_OPEN_EXPLICIT_SCALAR_DT);
    fprintf(f,"\"step_rejection\":\"%s\",\"exact_momentum_check\":\"%s\",\"linear_failure\":\"%s\",",s->step_rejection?s->step_rejection:"none",d->check,d->linear_failure);
    number(f,"momentum_courant",d->courant);number(f,"momentum_courant_limit",d->courant_limit);
    number(f,"pressure_true_relative_residual",d->pressure_residual);number(f,"projection_maximum_divergence",d->maximum_divergence);
    number(f,"cg_recursive_relative_residual",d->cg_recursive_residual);number(f,"cg_true_relative_residual",d->cg_true_residual);
    number(f,"cg_preconditioned_dot",d->cg_preconditioned_dot);number(f,"cg_operator_dot",d->cg_operator_dot);
    fprintf(f,"\"maximum_rate_face\":[%d,%d,%d,%d],\"divergence_cell\":%d,\"pressure_iterations\":%d,\"cg_iteration\":%d,\"pressure_attempted\":%s,",d->face_axis,d->face_i,d->face_j,d->face_k,d->divergence_cell,d->pressure_iterations,d->cg_iteration,d->pressure_attempted?"true":"false");
    fprintf(f,"\"accepted_guard_available\":%s,\"accepted_unchanged\":%s,\"accepted_fingerprint_before\":\"%016" PRIx64 "\",\"accepted_fingerprint_after\":\"%016" PRIx64 "\",",before?"true":"false",before&&before==after?"true":"false",before,after);
    fprintf(f,"\"memory_live_bytes\":%zu,\"memory_peak_bytes\":%zu,\"rejected_allocations\":%zu,\"heat_gate_relative\":0.1,\"candidate_is_rejected_scratch\":%s}\n",budget->live_bytes,budget->peak_bytes,budget->rejected_allocations,s->step_rejection?"true":"false");
    return fclose(f)==0;
}
bool physics_sim_accepted_load(const char *path,CfdOpenAtmosphere3d *s) {
    FILE *f=fopen(path,"rb");if(!f)return false;uint64_t u[12];double v[26];bool initialized=false,ok=false;
    if(fread(u,8,12,f)!=12 || fread(v,8,26,f)!=26 || u[0]!=UINT64_C(0x4d4f4d5354415445) || u[1]!=1 || u[11]!=31 || u[8]>10000)goto done;
    for(int i=0;i<26;i++)if(!isfinite(v[i]))goto done;
    if(u[10]>12000000000ULL || u[9]>u[10])goto done;
    for(int i=0;i<3;i++)if(u[2+i]<4 || u[2+i]>256)goto done;
    int grid[3]={(int)u[2],(int)u[3],(int)u[4]};
    if(!cfd_open_atmosphere3d_init(s,grid,v,v[3],v[4],v[5],v[6],v[7],v[8],v[9],v+13,v+15,v+17,true,v[10],v[11],v[12]))goto done;
    initialized=true;s->ground=true;s->muscl=true;s->cache_projection=true;
    if(!cfd_open_atmosphere3d_lateral(s) || u[5]!=(uint64_t)s->grid.count || u[6]!=(uint64_t)s->velocity_count || u[7]!=(uint64_t)s->boundary_faces)goto done;
    if(fread(s->velocity,8,s->velocity_count,f)!=(size_t)s->velocity_count || fread(s->pressure,8,s->grid.count,f)!=(size_t)s->grid.count ||
       fread(s->energy,8,2*s->grid.count,f)!=(size_t)2*s->grid.count || fread(s->flux,8,4*s->boundary_faces,f)!=(size_t)4*s->boundary_faces || fgetc(f)!=EOF)goto done;
    s->steps=(int)u[8];s->scalar_work_cells=u[9];s->max_scalar_work_cells=u[10];
    s->time=v[19];s->initial_j=v[20];s->initial_kg=v[21];s->input_j=v[22];s->input_kg=v[23];s->divergence=v[24];s->residual=v[25];
    ok=cfd_open_atmosphere3d_valid(s);
 done:
    fclose(f);if(initialized&&!ok)cfd_open_atmosphere3d_destroy(s);return ok;
}
