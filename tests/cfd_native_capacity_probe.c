/* Bounded diagnostic, never an ordinary worker admission path.
 * Reuse the existing private bridge's exact field codec and restore helpers.
 * Rename only its CLI entry point; no solver or bridge source is overridden. */
#define main physics_sim_uninvoked_worker_entry
#include "../src/tools/physics_sim_open_atmosphere_worker.c"
#undef main
#include <sys/resource.h>
#include <time.h>
#include <stdlib.h>
static void measure(const char *stage,const CfdMemoryBudget *m,bool ok,const char *reason) {
    struct rusage ru;getrusage(RUSAGE_SELF,&ru);
    fprintf(stderr,"{\"stage\":\"%s\",\"ok\":%s,\"live_bytes\":%zu,\"peak_bytes\":%zu,\"limit_bytes\":%zu,\"rejected_allocations\":%zu,\"last_requested_bytes\":%zu,\"memory_failure\":%d,\"rss_peak_bytes\":%ld,\"reason\":\"%s\"}\n",stage,ok?"true":"false",m->live_bytes,m->peak_bytes,m->limit_bytes,m->rejected_allocations,m->last_requested_bytes,m->last_failure,ru.ru_maxrss,reason?reason:"");fflush(stderr);
}
static bool setup(CfdOpenAtmosphere3d *s,PhysicsSimInertIo *o,struct json_object *r,CfdMemoryBudget *m) {
    double nd[3],l[3],p[2],t[2],a[2],rho,mu,cp,ref,k,d,dt,g,beta,contrast;int n[3];
    struct json_object *props=get(r,"properties"),*b=get(r,"boundary_policy"),*buoy=get(r,"buoyancy");
    if(!field(get(r,"grid"),nd,3) || !field(get(r,"length_m"),l,3) || !field(get(b,"pressure_datum_pa"),p,2) || !field(get(b,"inflow_temperature_k"),t,2) || !field(get(b,"inflow_smoke_concentration_kg_m3"),a,2) || !num(get(props,"density_kg_m3"),&rho) || !num(get(props,"dynamic_viscosity_pa_s"),&mu) || !num(get(props,"heat_capacity_j_kg_k"),&cp) || !num(get(props,"reference_temperature_k"),&ref) || !num(get(props,"conductivity_w_m_k"),&k) || !num(get(props,"tracer_diffusivity_m2_s"),&d) || !num(get(r,"momentum_dt_s"),&dt) || !num(get(buoy,"gravity_m_s2"),&g) || !num(get(buoy,"expansion_per_k"),&beta) || !num(get(buoy,"max_temperature_contrast_fraction"),&contrast))return false;
    for(int z=0;z<3;z++){if(nd[z]!=nd[0] || (nd[z]!=16 && nd[z]!=80 && nd[z]!=128) || l[z]!=1)return false;n[z]=(int)nd[z];}
    if(dt!=.005 || get(r,"initial_energy_j") || get(r,"initial_face_velocity_m_s"))return false;
    bool ok=cfd_open_atmosphere3d_init(s,n,l,rho,mu,cp,ref,k,d,dt,p,t,a,json_object_get_boolean(get(buoy,"enabled")),g,beta,contrast);
    measure("initialize",m,ok,NULL);if(!ok)return false;
    s->ground=true;ok=cfd_open_atmosphere3d_lateral(s);measure("lateral_boundary",m,ok,NULL);if(!ok)return false;
    double lo[3],hi[3];int il[3],ih[3];struct json_object *box=get(r,"solid_box");
    if(!field(get(box,"lower_m"),lo,3) || !field(get(box,"upper_m"),hi,3))return false;
    for(int z=0;z<3;z++){il[z]=(int)round(lo[z]*n[z]);ih[z]=(int)round(hi[z]*n[z]);if(lo[z]*n[z]!=il[z] || hi[z]*n[z]!=ih[z])return false;}
    if(!cfd_open_atmosphere3d_box(s,il,ih) || !physics_scalar_profile_load(s,r) || !physics_open_weather_load(s,&s->weather,r))return false;
    s->max_scalar_work_cells=1000000000;s->cache_projection=true;
    if(get(r,"state") && !restore(s,get(r,"state")))return false;
    if(!physics_inert_load(o,s,r) || !o->enabled)return false;
    return cfd_open_atmosphere3d_valid(s);
}
static bool checkpoint(CfdOpenAtmosphere3d *s,const PhysicsSimInertIo *o,const char *path) {
    struct json_object *fields=json_object_new_object(),*data=json_object_new_object();int n=s->grid.count;
    const char *names[]={"face_velocity_m_s","pressure_pa","energy_j","smoke_kg","boundary_fluxes"};
    const double *arrays[]={s->velocity,s->pressure,s->energy,s->smoke,s->flux};
    const int counts[]={s->velocity_count,n,n,n,4*s->boundary_faces};
    for(int i=0;i<5;i++)json_object_object_add(data,names[i],values(arrays[i],counts[i],1,0));
    value(data,"steps",s->steps);value(data,"time_s",s->time);value(data,"scalar_work_cells",(double)s->scalar_work_cells);
    value(data,"input_energy_j",s->input_j);value(data,"input_smoke_kg",s->input_kg);value(data,"initial_energy_j",s->initial_j);value(data,"initial_smoke_kg",s->initial_kg);
    physics_open_weather_fields(s,&s->weather,&s->weather_state,fields,data);physics_inert_fields(o,s,fields,data);physics_scalar_profile_fields(s,fields,data);
    json_object_object_add(fields,"state",data);value(fields,"max_divergence_s_inv",s->divergence);value(fields,"projection_relative_residual",s->residual);
    bool ok=freopen(path,"wb",stdout) && fp_write(fields,FP_LIMIT);json_object_put(fields);return ok;
}
int main(int argc,char **argv) {
    if(argc!=5)return 2; /* compact physical request, fresh output prefix, budgetMiB, first state or '-' */
    long mib=strtol(argv[3],NULL,10);if(mib!=512 && mib!=1536)return 2;
    CfdMemoryBudget memory={.limit_bytes=(size_t)mib*1024*1024};CfdMemoryBudget *prior=cfd_memory_scope(&memory);CfdOpenAtmosphere3d s={0};PhysicsSimInertIo object={0};
    struct json_object *r=fp_read(argv[1]),*saved=NULL;double *input=NULL;int status=1;const char *stage="packet_decode";
    measure(stage,&memory,r!=NULL,NULL);if(!r)goto done;
    if(strcmp(argv[4],"-")){saved=fp_read(argv[4]);if(!saved)goto done;json_object_object_add(r,"state",json_object_get(get(saved,"state")));}
    stage="setup";if(!setup(&s,&object,r,&memory))goto done;
    stage="source_buffers";input=cfd_memory_calloc((size_t)2*s.grid.count,sizeof(double));measure(stage,&memory,input!=NULL,NULL);if(!input)goto done;
    struct json_object *steps=get(r,"steps");if(!steps || json_object_array_length(steps)!=2 || s.steps>1)goto done;
    for(int q=s.steps;q<2;q++){
        struct json_object *step=json_object_array_get_idx(steps,(size_t)q);stage="source_decode";
        if(!field(get(step,"energy_j"),input,s.grid.count) || !field(get(step,"smoke_kg"),input+s.grid.count,s.grid.count))goto done;
        stage="native_step";bool ok=physics_inert_step(&object,&s,input,input+s.grid.count);measure(stage,&memory,ok,s.step_rejection);if(!ok)goto done;
        if(!cfd_open_atmosphere3d_valid(&s))goto done;
        char path[4096];if(snprintf(path,sizeof(path),"%s-step%d.fpk",argv[2],s.steps)>=(int)sizeof(path))goto done;
        stage="checkpoint";ok=checkpoint(&s,&object,path);measure(stage,&memory,ok,NULL);if(!ok)goto done;
    }
    status=0;
 done:
    if(status)measure(stage,&memory,false,s.step_rejection);
    if(saved)json_object_put(saved);if(r)json_object_put(r);cfd_memory_free(input);cfd_open_atmosphere3d_destroy(&s);
    measure("destroy",&memory,memory.live_bytes==0,NULL);if(memory.live_bytes)status=3;cfd_memory_scope(prior);return status;
}
