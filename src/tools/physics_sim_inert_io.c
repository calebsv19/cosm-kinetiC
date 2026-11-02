/* Closed optional object codec; existing conservative exchange remains authority. */
#include "physics_sim_inert_io.h"
#include <math.h>
#include <string.h>

static struct json_object *get(struct json_object *o,const char *key) {
    struct json_object *v=NULL;if(o)json_object_object_get_ex(o,key,&v);return v;
}
static bool number(struct json_object *o,const char *key,double *out) {
    struct json_object *v=get(o,key);
    if(!v || (!json_object_is_type(v,json_type_double) && !json_object_is_type(v,json_type_int)))return false;
    *out=json_object_get_double(v);return isfinite(*out);
}
static bool schema(struct json_object *o,const char *value,int count) {
    struct json_object *v=get(o,"schema");
    return o && json_object_is_type(o,json_type_object) &&
        json_object_object_length(o)==count && v && json_object_is_type(v,json_type_string) &&
        !strcmp(json_object_get_string(v),value);
}

bool physics_inert_load(PhysicsSimInertIo *o,CfdOpenAtmosphere3d *s,
    struct json_object *r) {
    memset(o,0,sizeof(*o));
    struct json_object *config=get(r,"inert_object"),*state=get(r,"state");
    struct json_object *saved=get(state,"inert_object");
    if(!config)return saved==NULL;
    double axis,side;
    if(!schema(config,"physics_sim_inert_object/v1",7) ||
       !number(config,"axis",&axis) || axis<0 || axis>2 || floor(axis)!=axis ||
       !number(config,"side",&side) || (side!=-1 && side!=1) ||
       !number(config,"initial_temperature_k",&o->initial_k) || o->initial_k<s->reference_k || o->initial_k>1e9 ||
       !number(config,"capacity_j_k",&o->face.capacity_j_k) || o->face.capacity_j_k<1e-12 || o->face.capacity_j_k>1e12 ||
       !number(config,"h_w_m2_k",&o->face.h_w_m2_k) || o->face.h_w_m2_k<0 || o->face.h_w_m2_k>1e9 ||
       !number(config,"support_depth_m",&o->depth_m) || o->depth_m<=0 || o->depth_m>1000)return false;
    o->face.axis=(int)axis;o->face.side=(int)side;o->face.temperature_k=o->initial_k;
    if(state && !json_object_is_type(state,json_type_null)) {
        double steps,time;
        if(!schema(saved,"physics_sim_inert_object_state/v1",6) ||
           !number(saved,"temperature_k",&o->face.temperature_k) ||
           !number(saved,"temperature_correction_k",&o->face.temperature_correction_k) ||
           !number(saved,"gas_to_object_j",&o->face.gas_to_surface_j) ||
           !number(saved,"steps",&steps) || steps!=s->steps ||
           !number(saved,"time_s",&time) || time!=s->time)return false;
    }
    double rise=(o->face.temperature_k-s->reference_k)+o->face.temperature_correction_k;
    double delta=o->face.capacity_j_k*((o->face.temperature_k-o->initial_k)+o->face.temperature_correction_k);
    double rounding=nextafter(o->face.temperature_k,INFINITY)-o->face.temperature_k;
    double sample;
    if(o->face.temperature_k<s->reference_k || rise<0 ||
       fabs(o->face.temperature_correction_k)>rounding || !isfinite(delta) ||
       fabs(delta-o->face.gas_to_surface_j)>1e-9 ||
       !physics_sim_heated_face_sample(s,&o->face,o->depth_m,&sample))return false;
    o->enabled=true;return true;
}

bool physics_inert_step(PhysicsSimInertIo *o,CfdOpenAtmosphere3d *s,
    const double *energy,const double *smoke) {
    if(!o->enabled)return cfd_open_atmosphere3d_step(s,energy,smoke);
    return physics_sim_inert_coupling_step(s,&o->face,energy,smoke,o->depth_m,false);
}

void physics_inert_fields(const PhysicsSimInertIo *o,const CfdOpenAtmosphere3d *s,
    struct json_object *fields,struct json_object *state) {
    if(!o->enabled)return;
    struct json_object *v=json_object_new_object();
    json_object_object_add(v,"schema",json_object_new_string("physics_sim_inert_object_state/v1"));
    json_object_object_add(v,"temperature_k",json_object_new_double(o->face.temperature_k));
    json_object_object_add(v,"temperature_correction_k",json_object_new_double(o->face.temperature_correction_k));
    json_object_object_add(v,"gas_to_object_j",json_object_new_double(o->face.gas_to_surface_j));
    json_object_object_add(v,"steps",json_object_new_int(s->steps));
    json_object_object_add(v,"time_s",json_object_new_double(s->time));
    json_object_object_add(fields,"inert_object",v);
    json_object_object_add(state,"inert_object",json_object_get(v));
}
