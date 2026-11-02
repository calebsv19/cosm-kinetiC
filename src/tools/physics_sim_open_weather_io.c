/* Explicit JSON bridge for force configuration and separate accepted work. */
#include "physics_sim_open_weather_io.h"
#include "app/cfd_open_energy.h"
#include <math.h>
#include <string.h>
static struct json_object *get(struct json_object *o,const char *key) {
    struct json_object *v=NULL;
    if(o && json_object_is_type(o,json_type_object))json_object_object_get_ex(o,key,&v);
    return v;
}
static bool has(struct json_object *o,const char *key) {
    struct json_object *v=NULL;
    return o && json_object_is_type(o,json_type_object) && json_object_object_get_ex(o,key,&v);
}
static bool number(struct json_object *v,double *out) {
    if(!v || (!json_object_is_type(v,json_type_double) && !json_object_is_type(v,json_type_int)))return false;
    *out=json_object_get_double(v);return isfinite(*out);
}
static bool text(struct json_object *v,const char *expected) {
    return v && json_object_is_type(v,json_type_string) && !strcmp(json_object_get_string(v),expected);
}
bool physics_open_weather_load(CfdOpenAtmosphere3d *s,
    CfdOpenAtmosphereWeather *weather,struct json_object *request) {
    if(!has(request,"open_domain_forcing"))return true;
    if(has(request,"wind_forcing") || has(request,"diagnostic_transport"))return false;
    struct json_object *w=get(request,"open_domain_forcing");double height,response;
    if(!w || !json_object_is_type(w,json_type_object) || json_object_object_length(w)!=6 ||
       !text(get(w,"schema"),"physics_sim_open_weather_x/v1") ||
       !text(get(w,"momentum_backflow"),"quiescent_exterior/v1") ||
       !number(get(w,"ramp_height_m"),&height) || !number(get(w,"response_s"),&response))return false;
    bool linear=text(get(w,"interpolation"),"linear");
    if(!linear && !text(get(w,"interpolation"),"hold"))return false;
    struct json_object *knots=get(w,"knots");
    if(!knots || !json_object_is_type(knots,json_type_array))return false;
    int count=(int)json_object_array_length(knots),steps[16];double targets[16],strengths[16];
    if(count<1 || count>16)return false;
    for(int i=0;i<count;i++) {
        struct json_object *k=json_object_array_get_idx(knots,(size_t)i);double step;
        if(!k || !json_object_is_type(k,json_type_object) || json_object_object_length(k)!=3 ||
           !number(get(k,"step"),&step) || floor(step)!=step || step<0 || step>10000 ||
           !number(get(k,"target_m_s"),targets+i) || !number(get(k,"strength"),strengths+i))return false;
        steps[i]=(int)step;
    }
    if(s->pressure_boundary[0]!=0 || s->pressure_boundary[1]!=0 ||
       !cfd_open_weather_configure(s,weather,height,response,linear,count,steps,targets,strengths))return false;
    s->wind.quiescent_backflow=true;return true;
}
bool physics_open_weather_restore(CfdOpenAtmosphere3d *s,
    const CfdOpenAtmosphereWeather *weather,CfdOpenWeatherState *accepted,
    struct json_object *state) {
    if(!weather->enabled)return !has(state,"open_domain_forcing_state");
    struct json_object *r=get(state,"open_domain_forcing_state"),*ledger=get(state,"kinetic_ledger_j");
    CfdOpenWeatherState next={0};double values[10],stored;
    if(!r || !json_object_is_type(r,json_type_object) || json_object_object_length(r)!=4 ||
       !text(get(r,"schema"),"physics_sim_open_weather_state/v1") ||
       !number(get(r,"external_work_j"),&next.external_work_j) ||
       !number(get(r,"air_in_m3"),&next.air_in_m3) || !number(get(r,"air_out_m3"),&next.air_out_m3) ||
       !ledger || !json_object_is_type(ledger,json_type_array) || json_object_array_length(ledger)!=10 ||
       !number(get(state,"kinetic_stored_j"),&stored))return false;
    for(int i=0;i<10;i++)if(!number(json_object_array_get_idx(ledger,(size_t)i),values+i))return false;
    double expected=cfd_open_kinetic(s,s->velocity);
    if(stored<0 || fabs(stored-expected)>1e-11*fmax(1,fabs(stored)))return false;
    CfdOpenAtmosphere3d copy=*s;memcpy(copy.kinetic_ledger,values,sizeof(values));
    if(!cfd_open_weather_state_valid(&copy,weather,&next))return false;
    memcpy(s->kinetic_ledger,values,sizeof(values));*accepted=next;return true;
}
void physics_open_weather_fields(const CfdOpenAtmosphere3d *s,
    const CfdOpenAtmosphereWeather *weather,const CfdOpenWeatherState *accepted,
    struct json_object *fields,struct json_object *state) {
    if(!weather->enabled)return;
    struct json_object *r=json_object_new_object(),*ledger=json_object_new_array();
    json_object_object_add(r,"schema",json_object_new_string("physics_sim_open_weather_state/v1"));
    json_object_object_add(r,"external_work_j",json_object_new_double(accepted->external_work_j));
    json_object_object_add(r,"air_in_m3",json_object_new_double(accepted->air_in_m3));
    json_object_object_add(r,"air_out_m3",json_object_new_double(accepted->air_out_m3));
    json_object_object_add(fields,"open_domain_forcing_state",r);
    json_object_object_add(state,"open_domain_forcing_state",json_object_get(r));
    for(int i=0;i<10;i++)json_object_array_add(ledger,json_object_new_double(s->kinetic_ledger[i]));
    json_object_object_add(fields,"kinetic_ledger_j",ledger);
    json_object_object_add(state,"kinetic_ledger_j",json_object_get(ledger));
    double energy=cfd_open_kinetic(s,s->velocity);
    json_object_object_add(fields,"kinetic_stored_j",json_object_new_double(energy));
    json_object_object_add(state,"kinetic_stored_j",json_object_new_double(energy));
}
