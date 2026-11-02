/* JSON bridge for the opt-in immutable inlet schedule and accepted air ledger. */
#include "physics_sim_open_wind_io.h"
#include <math.h>
#include "app/cfd_open_energy.h"
#include <string.h>
static struct json_object *get(struct json_object *o,const char *k) {
    struct json_object *v=NULL;if(o && json_object_is_type(o,json_type_object))json_object_object_get_ex(o,k,&v);return v;
}
static bool number(struct json_object *v,double *out) {
    if(!v || (!json_object_is_type(v,json_type_double) && !json_object_is_type(v,json_type_int)))return false;
    *out=json_object_get_double(v);return isfinite(*out);
}
static bool load_wind(CfdOpenAtmosphere3d *s,struct json_object *r) {
    struct json_object *w=get(r,"wind_forcing");if(!w)return true;
    const char *schema=json_object_get_string(get(w,"schema")),*side=json_object_get_string(get(w,"inlet"));
    const char *mode=json_object_get_string(get(w,"interpolation"));double height;
    if(!schema || !side || !mode)return false;
    struct json_object *policy=get(w,"momentum_backflow");
    if(policy && (!json_object_is_type(policy,json_type_string) ||
        strcmp(json_object_get_string(policy),"quiescent_exterior/v1")))return false;
    s->wind.quiescent_backflow=policy!=NULL;
    bool channel=!strcmp(schema,"physics_sim_channel_x_inlet/v1");
    bool balanced=!strcmp(schema,"physics_sim_balanced_x_wind/v1");
    if((!channel && !balanced && strcmp(schema,"physics_sim_driven_x_inlet/v1")) ||
       (balanced?strcmp(side,"x_pair"):(strcmp(side,"x_min") && strcmp(side,"x_max"))) ||
       (strcmp(mode,"hold") && strcmp(mode,"linear")) || !number(get(w,"ramp_height_m"),&height) ||
       json_object_object_length(w)!=(channel?6:5)+(policy?1:0))return false;
    struct json_object *knots=get(w,"knots");
    if(!knots || !json_object_is_type(knots,json_type_array))return false;
    int count=(int)json_object_array_length(knots),steps[16];double speeds[16];
    if(count<1 || count>16)return false;
    for(int i=0;i<count;i++) {
        struct json_object *k=json_object_array_get_idx(knots,(size_t)i);double step;
        if(!json_object_is_type(k,json_type_object) || json_object_object_length(k)!=2 ||
           !number(get(k,"step"),&step) || floor(step)!=step || step<0 || step>10000 ||
           !number(get(k,"speed_m_s"),speeds+i))return false;
        steps[i]=(int)step;
    }
    if(balanced)return cfd_open_wind_balanced(s,height,!strcmp(mode,"linear"),count,steps,speeds);
    if(!cfd_open_wind_configure(s,!strcmp(side,"x_max"),height,!strcmp(mode,"linear"),count,steps,speeds))return false;
    if(!channel)return true;
    struct json_object *patch=get(w,"inlet_patch_m"),*lower=get(patch,"lower_yz_m"),*upper=get(patch,"upper_yz_m");
    if(!patch || !json_object_is_type(patch,json_type_object) || json_object_object_length(patch)!=2 ||
       !lower || !upper || !json_object_is_type(lower,json_type_array) || !json_object_is_type(upper,json_type_array) ||
       json_object_array_length(lower)!=2 || json_object_array_length(upper)!=2)return false;
    int lo[2],hi[2];
    for(int a=0;a<2;a++) {
        double l,h;if(!number(json_object_array_get_idx(lower,(size_t)a),&l) ||
            !number(json_object_array_get_idx(upper,(size_t)a),&h) || l<0 || h>s->grid.length[a+1] || h<=l)return false;
        double x=l/s->grid.h[a+1],y=h/s->grid.h[a+1];
        if(fabs(x-round(x))>1e-12 || fabs(y-round(y))>1e-12)return false;
        lo[a]=(int)round(x);hi[a]=(int)round(y);
    }
    return cfd_open_wind_patch(s,lo,hi);
}
bool physics_open_wind_load(CfdOpenAtmosphere3d *s,struct json_object *r) {
    if(!load_wind(s,r))return false;
    s->wind.quiescent_backflow=get(get(r,"wind_forcing"),"momentum_backflow")!=NULL;
    return true;
}
bool physics_open_wind_restore(CfdOpenAtmosphere3d *s,struct json_object *state) {
    if(!s->wind.enabled)return true;
    if(s->wind.quiescent_backflow) {
        struct json_object *v=get(state,"kinetic_ledger_j");
        if(!v || !json_object_is_type(v,json_type_array) || json_object_array_length(v)!=10)return false;
        for(int i=0;i<10;i++)if(!number(json_object_array_get_idx(v,(size_t)i),s->kinetic_ledger+i))return false;
        double stored,total=0;
        if(!number(get(state,"kinetic_stored_j"),&stored))return false;
        for(int i=0;i<10;i++)total+=s->kinetic_ledger[i];
        if(fabs(stored-cfd_open_kinetic(s,s->velocity))>1e-11*fmax(1,fabs(stored)) ||
           fabs(stored-total)>1e-10*fmax(1,fabs(stored)))return false;
    }
    return number(get(state,"wind_air_in_m3"),&s->wind_air_in) &&
        number(get(state,"wind_air_out_m3"),&s->wind_air_out) &&
        number(get(state,"wind_inlet_volume_m3"),&s->wind_inlet_volume) &&
        s->wind_air_in>=0 && s->wind_air_out>=0 && s->wind_inlet_volume>=0;
}
void physics_open_wind_fields(const CfdOpenAtmosphere3d *s,struct json_object *fields,struct json_object *state) {
    if(!s->wind.enabled)return;
    if(s->wind.quiescent_backflow) {
        double stored=cfd_open_kinetic(s,s->velocity);
        struct json_object *v=json_object_new_array();
        for(int i=0;i<10;i++)json_object_array_add(v,json_object_new_double(i==KE_INITIAL && s->steps==0?stored:s->kinetic_ledger[i]));
        json_object_object_add(fields,"kinetic_ledger_j",v);
        json_object_object_add(state,"kinetic_ledger_j",json_object_get(v));
        json_object_object_add(fields,"kinetic_stored_j",json_object_new_double(stored));
        json_object_object_add(state,"kinetic_stored_j",json_object_new_double(stored));
    }
    const char *names[3]={"wind_air_in_m3","wind_air_out_m3","wind_inlet_volume_m3"};
    double values[3]={s->wind_air_in,s->wind_air_out,s->wind_inlet_volume};
    for(int i=0;i<3;i++) {
        json_object_object_add(fields,names[i],json_object_new_double(values[i]));
        json_object_object_add(state,names[i],json_object_new_double(values[i]));
    }
}
