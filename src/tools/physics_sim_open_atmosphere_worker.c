/* Private native bridge; closed public policies/checkpoint integrity are Python-owned. */
#include "app/cfd_open_atmosphere3d.h"
#include "app/cfd_passive3d.h"
#include <json-c/json.h>
#include <math.h>
#include <stdio.h>
#include <sys/stat.h>
static struct json_object *get(struct json_object *o,const char *key) {struct json_object *v=NULL;if(o)json_object_object_get_ex(o,key,&v);return v;}
static bool num(struct json_object *v,double *out) {
    if(!v || (!json_object_is_type(v,json_type_double) && !json_object_is_type(v,json_type_int)))return false;
    *out=json_object_get_double(v);return isfinite(*out);
}
static bool field(struct json_object *v,double *out,int n) {
    if(!v || !json_object_is_type(v,json_type_array) || json_object_array_length(v)!=(size_t)n)return false;
    for(int q=0;q<n;q++)if(!num(json_object_array_get_idx(v,q),out+q))return false;return true;
}
static void value(struct json_object *o,const char *key,double v) {json_object_object_add(o,key,json_object_new_double(v));}
static struct json_object *values(const double *v,int n,double scale,double offset) {
    struct json_object *a=json_object_new_array_ext(n);for(int q=0;q<n;q++)json_object_array_add(a,json_object_new_double(offset+scale*v[q]));return a;
}
static bool restore(CfdOpenAtmosphere3d *s,struct json_object *state) {
    int n=s->grid.count;double steps,work;
    if(!field(get(state,"face_velocity_m_s"),s->velocity,s->velocity_count) || !field(get(state,"pressure_pa"),s->pressure,n) ||
        !field(get(state,"energy_j"),s->energy,n) || !field(get(state,"smoke_kg"),s->smoke,n) || !field(get(state,"boundary_fluxes"),s->flux,8*s->plane) ||
        !num(get(state,"steps"),&steps) || floor(steps)!=steps || steps<0 || steps>10000 ||
        !num(get(state,"scalar_work_cells"),&work) || floor(work)!=work || work<0 || work>100000000 ||
        !num(get(state,"time_s"),&s->time) || !num(get(state,"input_energy_j"),&s->input_j) || s->input_j<0 ||
        !num(get(state,"input_smoke_kg"),&s->input_kg) || s->input_kg<0 || !num(get(state,"initial_energy_j"),&s->initial_j) || s->initial_j<0 ||
        !num(get(state,"initial_smoke_kg"),&s->initial_kg) || s->initial_kg<0)return false;
    s->steps=(int)steps;s->scalar_work_cells=(unsigned long long)work;return cfd_open_atmosphere3d_valid(s);
}
int main(int argc,char **argv) {
    struct stat st;if(argc!=2 || stat(argv[1],&st) || st.st_size<=0 || st.st_size>64*1024*1024)return 2;
    struct json_object *r=json_object_from_file(argv[1]),*result=NULL;CfdOpenAtmosphere3d s={0};double *input=NULL;int status=1;
    CfdMemoryBudget memory={.limit_bytes=128*1024*1024};CfdMemoryBudget *prior=cfd_memory_scope(&memory);
    double nd[3],length[3],rho,mu,cp,ref,k,d,dt,pressure[2],ambient[2],smoke[2],gravity,beta,contrast;int grid[3];
    if(!r || !field(get(r,"grid"),nd,3) || !field(get(r,"length_m"),length,3))goto done;
    for(int a=0;a<3;a++){if(nd[a]<4 || nd[a]>64 || floor(nd[a])!=nd[a])goto done;grid[a]=(int)nd[a];}
    struct json_object *p=get(r,"properties"),*b=get(r,"boundary_policy"),*buoy=get(r,"buoyancy");
    struct json_object *enabled=get(buoy,"enabled");if(!enabled || !json_object_is_type(enabled,json_type_boolean))goto done;
    if(!num(get(p,"density_kg_m3"),&rho) || !num(get(p,"dynamic_viscosity_pa_s"),&mu) || !num(get(p,"heat_capacity_j_kg_k"),&cp) ||
        !num(get(p,"reference_temperature_k"),&ref) || !num(get(p,"conductivity_w_m_k"),&k) || !num(get(p,"tracer_diffusivity_m2_s"),&d) ||
        !num(get(r,"momentum_dt_s"),&dt) || !field(get(b,"pressure_datum_pa"),pressure,2) || !field(get(b,"inflow_temperature_k"),ambient,2) ||
        !field(get(b,"inflow_smoke_concentration_kg_m3"),smoke,2) || !num(get(buoy,"gravity_m_s2"),&gravity) ||
        !num(get(buoy,"expansion_per_k"),&beta) || !num(get(buoy,"max_temperature_contrast_fraction"),&contrast) ||
        !cfd_open_atmosphere3d_init(&s,grid,length,rho,mu,cp,ref,k,d,dt,pressure,ambient,smoke,json_object_get_boolean(enabled),gravity,beta,contrast))goto done;
    int n=s.grid.count;input=cfd_memory_calloc((size_t)2*n,sizeof(double));if(!input)goto done;
    struct json_object *state=get(r,"state");
    if(state && !json_object_is_type(state,json_type_null)){if(!restore(&s,state))goto done;}
    else {
        if(!field(get(r,"initial_face_velocity_m_s"),s.velocity,s.velocity_count) || !field(get(r,"initial_energy_j"),s.energy,n) || !field(get(r,"initial_smoke_kg"),s.smoke,n) || !cfd_open_atmosphere3d_valid(&s))goto done;
        s.initial_j=cfd_passive3d_total(s.energy,n);s.initial_kg=cfd_passive3d_total(s.smoke,n);
    }
    double start=s.time;struct json_object *steps=get(r,"steps");
    if(!steps || !json_object_is_type(steps,json_type_array) || json_object_array_length(steps)>256)goto done;
    for(size_t i=0;i<json_object_array_length(steps);i++) {
        struct json_object *step=json_object_array_get_idx(steps,i);
        if(!field(get(step,"energy_j"),input,n) || !field(get(step,"smoke_kg"),input+n,n) || !cfd_open_atmosphere3d_step(&s,input,input+n))goto done;
    }
    result=json_object_new_object();json_object_object_add(result,"schema",json_object_new_string("physics_sim_open_atmosphere_fields/v1"));
    value(result,"start_time_s",start);value(result,"time_s",s.time);value(result,"max_divergence_s_inv",s.divergence);value(result,"projection_relative_residual",s.residual);
    json_object_object_add(result,"face_velocity_m_s",values(s.velocity,s.velocity_count,1,0));json_object_object_add(result,"pressure_pa",values(s.pressure,n,1,0));
    json_object_object_add(result,"energy_j",values(s.energy,n,1,0));json_object_object_add(result,"smoke_kg",values(s.smoke,n,1,0));
    json_object_object_add(result,"temperature_k",values(s.energy,n,1/(rho*cp*s.grid.volume),ref));json_object_object_add(result,"smoke_concentration_kg_m3",values(s.smoke,n,1/s.grid.volume,0));
    json_object_object_add(result,"boundary_fluxes",values(s.flux,8*s.plane,1,0));
    struct json_object *state_out=json_object_new_object();
    for(int i=0;i<5;i++){const char *name=(const char*[]) {"face_velocity_m_s","pressure_pa","energy_j","smoke_kg","boundary_fluxes"}[i];json_object_object_add(state_out,name,json_object_get(get(result,name)));}
    value(state_out,"steps",s.steps);value(state_out,"time_s",s.time);value(state_out,"scalar_work_cells",(double)s.scalar_work_cells);
    value(state_out,"input_energy_j",s.input_j);value(state_out,"input_smoke_kg",s.input_kg);value(state_out,"initial_energy_j",s.initial_j);value(state_out,"initial_smoke_kg",s.initial_kg);
    json_object_object_add(result,"state",state_out);value(result,"numerical_peak_bytes",(double)memory.peak_bytes);
    puts(json_object_to_json_string_ext(result,JSON_C_TO_STRING_PLAIN));status=0;
done:
    if(status)fputs("open atmosphere policy, divergence, CFL, flux budget or contrast rejection\n",stderr);
    if(result)json_object_put(result);if(r)json_object_put(r);cfd_open_atmosphere3d_destroy(&s);cfd_memory_free(input);cfd_memory_scope(prior);return status;
}
