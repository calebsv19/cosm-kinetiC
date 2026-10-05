/* Private bounded JSON bridge. Public admission is the Python source CLI. */
#include "app/cfd_passive3d.h"
#include <json-c/json.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
static struct json_object *get(struct json_object *o,const char *name) {
    struct json_object *v=NULL;json_object_object_get_ex(o,name,&v);return v;
}
static bool numeric(struct json_object *v,double *out) {
    if (!v || (!json_object_is_type(v,json_type_double) && !json_object_is_type(v,json_type_int))) return false;
    *out=json_object_get_double(v);return isfinite(*out);
}
static bool array(struct json_object *v,double *out,int n) {
    if (!v || !json_object_is_type(v,json_type_array) || json_object_array_length(v)!=(size_t)n) return false;
    for (int i=0;i<n;i++) if (!numeric(json_object_array_get_idx(v,i),out+i)) return false;
    return true;
}
static void value(struct json_object *o,const char *key,double v) {
    json_object_object_add(o,key,json_object_new_double(v));
}
static struct json_object *values(const double *v,int n,double multiplier,double offset) {
    struct json_object *a=json_object_new_array_ext(n);
    for (int i=0;i<n;i++) json_object_array_add(a,json_object_new_double(offset+multiplier*v[i]));
    return a;
}
int main(int argc,char **argv) {
    struct stat st; if (argc!=2 || stat(argv[1],&st) || st.st_size<=0 || st.st_size>64*1024*1024) return 2;
    struct json_object *request=json_object_from_file(argv[1]),*result=NULL;
    CfdPassive3d scalar={0}; double *input=NULL;int status=1;
    CfdMemoryBudget memory={.limit_bytes=128*1024*1024};CfdMemoryBudget *prior=cfd_memory_scope(&memory);
    if (!request) goto done;
    int n[3];double nd[3],length[3],rho,cp,ref,k,d;
    if (!array(get(request,"grid"),nd,3) || !array(get(request,"length_m"),length,3)) goto done;
    for (int a=0;a<3;a++) { if (nd[a]<4 || nd[a]>256 || floor(nd[a])!=nd[a]) goto done; n[a]=(int)nd[a]; }
    struct json_object *p=get(request,"properties");
    if (!numeric(get(p,"density_kg_m3"),&rho) || !numeric(get(p,"heat_capacity_j_kg_k"),&cp) ||
        !numeric(get(p,"reference_temperature_k"),&ref) || !numeric(get(p,"conductivity_w_m_k"),&k) ||
        !numeric(get(p,"tracer_diffusivity_m2_s"),&d) || !cfd_passive3d_init(&scalar,n,length,rho,cp,ref,k,d)) goto done;
    int count=scalar.grid.count;input=cfd_memory_calloc((size_t)5*count,sizeof(double)); if (!input) goto done;
    if (!array(get(request,"initial_energy_j"),scalar.energy,count) || !array(get(request,"initial_smoke_kg"),scalar.smoke,count)) goto done;
    for (int q=0;q<count;q++) if (scalar.energy[q]<0 || scalar.smoke[q]<0) goto done;
    double initial_j=cfd_passive3d_total(scalar.energy,count),initial_kg=cfd_passive3d_total(scalar.smoke,count);
    struct json_object *steps=get(request,"steps");
    if (!steps || !json_object_is_type(steps,json_type_array) || json_object_array_length(steps)<1 || json_object_array_length(steps)>256) goto done;
    unsigned long long work=0;
    for (size_t i=0;i<json_object_array_length(steps);i++) {
        struct json_object *step=json_object_array_get_idx(steps,i);double dt;
        if (!numeric(get(step,"dt_s"),&dt) || !array(get(step,"face_velocity_m_s"),input,3*count) ||
            !array(get(step,"energy_j"),input+3*count,count) || !array(get(step,"smoke_kg"),input+4*count,count) ||
            !cfd_passive3d_step(&scalar,input,input+3*count,input+4*count,dt)) goto done;
        work+=(unsigned long long)scalar.substeps*count;if (work>100000000) goto done;
    }
    result=json_object_new_object();
    json_object_object_add(result,"schema",json_object_new_string("physics_sim_passive_fields/v1"));
    json_object_object_add(result,"model",json_object_new_string("periodic_constant_property_passive3d_v1"));
    value(result,"time_s",scalar.time);value(result,"initial_energy_j",initial_j);value(result,"initial_smoke_kg",initial_kg);
    value(result,"input_energy_j",scalar.input_j);value(result,"input_smoke_kg",scalar.input_kg);
    value(result,"stored_energy_j",cfd_passive3d_total(scalar.energy,count));value(result,"stored_smoke_kg",cfd_passive3d_total(scalar.smoke,count));
    json_object_object_add(result,"scalar_substep_cells",json_object_new_int64((int64_t)work));
    json_object_object_add(result,"numerical_peak_bytes",json_object_new_int64((int64_t)memory.peak_bytes));
    json_object_object_add(result,"energy_j",values(scalar.energy,count,1,0));
    json_object_object_add(result,"smoke_kg",values(scalar.smoke,count,1,0));
    json_object_object_add(result,"temperature_k",values(scalar.energy,count,1/(rho*cp*scalar.grid.volume),ref));
    json_object_object_add(result,"smoke_concentration_kg_m3",values(scalar.smoke,count,1/scalar.grid.volume,0));
    puts(json_object_to_json_string_ext(result,JSON_C_TO_STRING_PLAIN));status=0;
done:
    if (status) fputs("passive transport input, stability, budget or numerical rejection\n",stderr);
    if (result) json_object_put(result);
    if (request) json_object_put(request);
    cfd_memory_free(input);cfd_passive3d_destroy(&scalar);cfd_memory_scope(prior);
    return status;
}
