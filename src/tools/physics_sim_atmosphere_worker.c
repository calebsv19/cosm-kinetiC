/* Private JSON bridge; strict public admission/checkpoint integrity is Python-owned. */
#include "app/cfd_atmosphere3d.h"
#include <json-c/json.h>
#include <math.h>
#include <stdio.h>
#include <sys/stat.h>
static struct json_object *get(struct json_object *o,const char *key) {
    struct json_object *v=NULL;if(o)json_object_object_get_ex(o,key,&v);return v;
}
static bool num(struct json_object *v,double *out) {
    if(!v || (!json_object_is_type(v,json_type_double) && !json_object_is_type(v,json_type_int)))return false;
    *out=json_object_get_double(v);return isfinite(*out);
}
static bool field(struct json_object *v,double *out,int n) {
    if(!v || !json_object_is_type(v,json_type_array) || json_object_array_length(v)!=(size_t)n)return false;
    for(int q=0;q<n;q++)if(!num(json_object_array_get_idx(v,q),out+q))return false;
    return true;
}
static void value(struct json_object *o,const char *key,double v) {json_object_object_add(o,key,json_object_new_double(v));}
static struct json_object *values(const double *v,int n,double scale,double offset) {
    struct json_object *a=json_object_new_array_ext(n);
    for(int q=0;q<n;q++)json_object_array_add(a,json_object_new_double(offset+scale*v[q]));
    return a;
}
static bool restore(CfdAtmosphere3d *s,struct json_object *state) {
    int n=s->flow.grid.count;double steps,time,work,j,kg,prev,older,kinetic;
    if(!field(get(state,"flow_storage"),s->flow.storage,27*n) ||
        !field(get(state,"energy_j"),s->scalar.energy,n) || !field(get(state,"smoke_kg"),s->scalar.smoke,n) ||
        !num(get(state,"steps"),&steps) || steps<0 || steps>10000 || floor(steps)!=steps ||
        !num(get(state,"time_s"),&time) || fabs(time-steps*s->flow.dt)>1e-12*fmax(1,time) ||
        !num(get(state,"scalar_work_cells"),&work) || work<0 || work>100000000 || floor(work)!=work ||
        !num(get(state,"input_energy_j"),&j) || j<0 || !num(get(state,"input_smoke_kg"),&kg) || kg<0 ||
        !num(get(state,"previous_kinetic_j"),&prev) || prev<0 || !num(get(state,"older_kinetic_j"),&older) || older<0 ||
        !num(get(state,"kinetic_j"),&kinetic) || kinetic<0)return false;
    for(int q=0;q<n;q++)if(s->scalar.energy[q]<0 || s->scalar.smoke[q]<0)return false;
    cfd_cartesian3d_divergence(&s->flow.grid,s->flow.velocity,s->flow.divergence);
    for(int q=0;q<n;q++)if(!isfinite(s->flow.divergence[q]) || fabs(s->flow.divergence[q])>=1e-8)return false;
    /* Divergence above is overwritten scratch. Restore exact warm-start carrier. */
    if(!field(get(state,"flow_storage"),s->flow.storage,27*n))return false;
    s->flow.steps=(int)steps;s->flow.time=time;s->flow.previous_energy=prev;s->flow.older_energy=older;s->flow.kinetic_j=kinetic;
    s->scalar.steps=(unsigned)steps;s->scalar.time=time;s->scalar.work_cells=(unsigned long long)work;s->scalar.input_j=j;s->scalar.input_kg=kg;
    return true;
}
int main(int argc,char **argv) {
    struct stat st;if(argc!=2 || stat(argv[1],&st) || st.st_size<=0 || st.st_size>64*1024*1024)return 2;
    struct json_object *r=json_object_from_file(argv[1]),*result=NULL;CfdAtmosphere3d s={0};double *input=NULL;int status=1;
    CfdMemoryBudget memory={.limit_bytes=128*1024*1024};CfdMemoryBudget *prior=cfd_memory_scope(&memory);
    double nd[3],length[3],rho,mu,dt,cp,ref,k,d;int grid[3];
    if(!r || !field(get(r,"grid"),nd,3) || !field(get(r,"length_m"),length,3))goto done;
    for(int a=0;a<3;a++){if(nd[a]<4 || nd[a]>64 || floor(nd[a])!=nd[a])goto done;grid[a]=(int)nd[a];}
    CfdCartesian3d g;if(!cfd_cartesian3d_init(&g,grid,length) || g.count>32768)goto done;
    input=cfd_memory_calloc((size_t)5*g.count,sizeof(double));if(!input)goto done;
    struct json_object *p=get(r,"properties");
    if(!num(get(p,"density_kg_m3"),&rho) || !num(get(p,"dynamic_viscosity_pa_s"),&mu) ||
        !num(get(r,"momentum_dt_s"),&dt) || !num(get(p,"heat_capacity_j_kg_k"),&cp) || !num(get(p,"reference_temperature_k"),&ref) ||
        !num(get(p,"conductivity_w_m_k"),&k) || !num(get(p,"tracer_diffusivity_m2_s"),&d) ||
        !field(get(r,"initial_face_velocity_m_s"),input,3*g.count) ||
        !cfd_atmosphere3d_init(&s,grid,length,rho,mu,dt,input,cp,ref,k,d))goto done;
    struct json_object *state=get(r,"state");
    if(state && !json_object_is_type(state,json_type_null)){if(!restore(&s,state))goto done;}
    double start=s.flow.time;
    struct json_object *steps=get(r,"steps");
    if(!steps || !json_object_is_type(steps,json_type_array) || json_object_array_length(steps)>256)goto done;
    for(size_t i=0;i<json_object_array_length(steps);i++) {
        struct json_object *step=json_object_array_get_idx(steps,i);
        if(!field(get(step,"energy_j"),input+3*g.count,g.count) || !field(get(step,"smoke_kg"),input+4*g.count,g.count) ||
            s.flow.steps>=10000 || !cfd_atmosphere3d_step(&s,input+3*g.count,input+4*g.count))goto done;
    }
    result=json_object_new_object();json_object_object_add(result,"schema",json_object_new_string("physics_sim_evolving_atmosphere_fields/v1"));
    value(result,"start_time_s",start);value(result,"time_s",s.flow.time);value(result,"max_divergence_s_inv",s.flow.max_divergence);value(result,"momentum_relative_residual",s.flow.true_residual);
    json_object_object_add(result,"face_velocity_m_s",values(s.flow.velocity,3*g.count,1,0));json_object_object_add(result,"pressure_pa",values(s.flow.pressure,g.count,1,0));
    json_object_object_add(result,"energy_j",values(s.scalar.energy,g.count,1,0));json_object_object_add(result,"smoke_kg",values(s.scalar.smoke,g.count,1,0));
    json_object_object_add(result,"temperature_k",values(s.scalar.energy,g.count,1/(rho*cp*g.volume),ref));json_object_object_add(result,"smoke_concentration_kg_m3",values(s.scalar.smoke,g.count,1/g.volume,0));
    struct json_object *cpstate=json_object_new_object();
    json_object_object_add(cpstate,"flow_storage",values(s.flow.storage,27*g.count,1,0));
    json_object_object_add(cpstate,"energy_j",values(s.scalar.energy,g.count,1,0));json_object_object_add(cpstate,"smoke_kg",values(s.scalar.smoke,g.count,1,0));
    value(cpstate,"steps",s.flow.steps);value(cpstate,"time_s",s.flow.time);value(cpstate,"scalar_work_cells",(double)s.scalar.work_cells);
    value(cpstate,"input_energy_j",s.scalar.input_j);value(cpstate,"input_smoke_kg",s.scalar.input_kg);
    value(cpstate,"previous_kinetic_j",s.flow.previous_energy);value(cpstate,"older_kinetic_j",s.flow.older_energy);value(cpstate,"kinetic_j",s.flow.kinetic_j);
    json_object_object_add(result,"state",cpstate);value(result,"numerical_peak_bytes",(double)memory.peak_bytes);
    puts(json_object_to_json_string_ext(result,JSON_C_TO_STRING_PLAIN));status=0;
done:
    if(status)fputs("evolving atmosphere state, CFL, conservation or solve rejection\n",stderr);
    if(result)json_object_put(result);
    if(r)json_object_put(r);
    cfd_atmosphere3d_destroy(&s);cfd_memory_free(input);cfd_memory_scope(prior);return status;
}
