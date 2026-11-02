/* Closed test-qualified normal worker capacity; ordinary builds refuse it. */
#ifndef PHYSICS_SIM_SCALAR_GUST128_IO_H
#define PHYSICS_SIM_SCALAR_GUST128_IO_H
#include <sys/resource.h>
static size_t physics_gust128_rss(void) {
    struct rusage ru;if(getrusage(RUSAGE_SELF,&ru))return SIZE_MAX;
#ifdef __APPLE__
    return (size_t)ru.ru_maxrss;
#else
    return (size_t)ru.ru_maxrss*1024;
#endif
}
#if defined(PHYSICS_SIM_SCALAR_GUST128) || defined(PHYSICS_SIM_SCALAR_WAKE128) || defined(PHYSICS_SIM_SCALAR_WAKE128_WORK)
static bool physics_scalar128_capacity(struct json_object *r,bool packet,bool qualification,
                                      const char *identity,size_t max_steps,size_t max_packet) {
    struct json_object *profile=get(r,"capacity_profile"),*limits=get(r,"resource_limits");
    if(!profile || !json_object_is_type(profile,json_type_string) || strcmp(json_object_get_string(profile),identity) || !packet || qualification)return false;
    double cells,bytes,wire1,wire2;struct json_object *steps=get(r,"steps"),*state=get(r,"state");double first=0;
    if(state && !json_object_is_type(state,json_type_null) && !num(get(state,"steps"),&first))return false;
    if(!steps || !json_object_is_type(steps,json_type_array) || json_object_array_length(steps)>max_packet || first<0 || first+json_object_array_length(steps)>max_steps)return false;
    const char *groups[]={"properties","buoyancy","inert_object","open_domain_forcing"};
    const char *names[][7]={{"density_kg_m3","dynamic_viscosity_pa_s","heat_capacity_j_kg_k","reference_temperature_k","conductivity_w_m_k","tracer_diffusivity_m2_s",NULL},{"gravity_m_s2","expansion_per_k","max_temperature_contrast_fraction",NULL},{"axis","side","initial_temperature_k","capacity_j_k","h_w_m2_k","support_depth_m",NULL},{"ramp_height_m","response_s",NULL}};
    const double wanted[][7]={{1.2,1.8e-5,1005,300,.026,0},{9.81,1./300,.1},{0,-1,300,10,2,.125},{.25,.2}};
    for(int g=0;g<4;g++)for(int z=0;names[g][z];z++){double v;if(!num(get(get(r,groups[g]),names[g][z]),&v) || v!=wanted[g][z])return false;}
    struct json_object *enabled=get(get(r,"buoyancy"),"enabled"),*cache=get(limits,"cache_pressure_operator"),*weather=get(r,"open_domain_forcing"),*knots=get(weather,"knots");
    if(!enabled || !json_object_is_type(enabled,json_type_boolean) || !json_object_get_boolean(enabled) || !cache || !json_object_is_type(cache,json_type_boolean) || !json_object_get_boolean(cache) || !knots || !json_object_is_type(knots,json_type_array) || json_object_array_length(knots)!=3)return false;
    const char *interpolation=json_object_get_string(get(weather,"interpolation"));if(!interpolation || strcmp(interpolation,"hold"))return false;
    for(int i=0;i<3;i++){struct json_object *k=json_object_array_get_idx(knots,i);double step,target,strength;if(!num(get(k,"step"),&step) || step!=(double[]){0,80,240}[i] || !num(get(k,"target_m_s"),&target) || target!=.5 || !num(get(k,"strength"),&strength) || strength!=(double[]){0,1,0}[i])return false;}
    return num(get(limits,"max_cells"),&cells) && cells==2097152 && num(get(limits,"numerical_bytes"),&bytes) && bytes==2147483648. && num(get(limits,"native_request_bytes"),&wire1) && wire1==268435456 && num(get(limits,"native_output_bytes"),&wire2) && wire2==268435456;
}
#endif
static bool physics_scalar_gust128(struct json_object *r,bool packet,bool qualification) {
#ifdef PHYSICS_SIM_SCALAR_GUST128
    return physics_scalar128_capacity(r,packet,qualification,"scalar-gust128/v1",100,4);
#else
    (void)r;(void)packet;(void)qualification;return false;
#endif
}
#endif
