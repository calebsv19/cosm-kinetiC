#define main weather_force_component_main
#include "cfd_open_weather_test.c"
#undef main
#include "../src/tools/physics_sim_open_weather_io.h"
static struct json_object *policy(void) {
    const char *text="{\"open_domain_forcing\":{\"schema\":\"physics_sim_open_weather_x/v1\",\"ramp_height_m\":0.125,\"response_s\":0.1,\"interpolation\":\"hold\",\"momentum_backflow\":\"quiescent_exterior/v1\",\"knots\":[{\"step\":0,\"target_m_s\":0.5,\"strength\":1}]}}";
    struct json_object *r=json_tokener_parse(text);assert(r);return r;
}
static void actual_volume(void) {
    CfdOpenAtmosphere3d s={0};setup(&s,false);
    for(int k=0;k<8;k++)for(int j=0;j<8;j++)for(int i=0;i<=8;i++)
        s.candidate_velocity[cfd_open_atmosphere3d_face(&s,0,i,j,k)]=.2;
    CfdOpenWeatherState accepted={.air_in_m3=.003,.air_out_m3=.003};double totals[2]={-1,-1};
    assert(cfd_open_weather_volume(&s,&accepted,totals));
    assert(fabs(totals[0]-.004)<1e-16 && fabs(totals[1]-.004)<1e-16);
    assert(accepted.air_in_m3==.003 && accepted.air_out_m3==.003);
    /* Deliberate imbalance must refuse without replacing output totals. */
    s.candidate_velocity[cfd_open_atmosphere3d_face(&s,0,8,0,0)]=0;
    double a=totals[0],b=totals[1];
    assert(!cfd_open_weather_volume(&s,&accepted,totals) && totals[0]==a && totals[1]==b);
    cfd_open_atmosphere3d_destroy(&s);
}
static void all_open_faces(void) {
    CfdOpenAtmosphere3d s={0};setup(&s,false);
    /* Divergence-free manufactured field: u=.2-.1x, v=.1, w=.1z.
     * Ground w=0; Qin=.2+.1=.3 and Qout=.1+.1+.1=.3 m3/s. */
    for(int a=0;a<3;a++)for(int k=0;k<8+(a==2);k++)
    for(int j=0;j<8+(a==1);j++)for(int i=0;i<8+(a==0);i++) {
        double value=a==0?.2-.1*i/8:a==1?.1:.1*k/8;
        s.candidate_velocity[cfd_open_atmosphere3d_face(&s,a,i,j,k)]=value;
    }
    CfdOpenWeatherState accepted={0};double totals[2]={0};
    assert(cfd_open_weather_volume(&s,&accepted,totals));
    assert(fabs(totals[0]-.3*s.dt)<1e-16 && fabs(totals[1]-.3*s.dt)<1e-16);
    assert(accepted.air_in_m3==0 && accepted.air_out_m3==0);
    cfd_open_atmosphere3d_destroy(&s);
}
static void io_and_work(void) {
    CfdOpenAtmosphere3d s={0},restored={0};setup(&s,false);setup(&restored,false);
    CfdOpenAtmosphereWeather w={0},other={0};struct json_object *request=policy();
    assert(physics_open_weather_load(&s,&w,request));
    assert(physics_open_weather_load(&restored,&other,request));
    assert(s.wind.quiescent_backflow && !s.wind.enabled);
    CfdOpenWeatherState accepted={0};assert(cfd_open_weather_state_valid(&s,&w,&accepted));
    for(int k=0;k<8;k++)for(int j=0;j<8;j++)for(int i=0;i<=8;i++)
        s.velocity[cfd_open_atmosphere3d_face(&s,0,i,j,k)]=.2;
    s.steps=1;s.time=s.dt;accepted.external_work_j=cfd_open_kinetic(&s,s.velocity);
    accepted.air_in_m3=accepted.air_out_m3=.001;
    assert(cfd_open_weather_state_valid(&s,&w,&accepted));
    struct json_object *fields=json_object_new_object(),*state=json_object_new_object();
    physics_open_weather_fields(&s,&w,&accepted,fields,state);
    struct json_object *receipt=NULL,*field_receipt=NULL;
    assert(json_object_object_get_ex(state,"open_domain_forcing_state",&receipt));
    assert(json_object_object_get_ex(fields,"open_domain_forcing_state",&field_receipt));
    assert(json_object_equal(receipt,field_receipt));
    struct json_object *decoded=json_tokener_parse(json_object_to_json_string_ext(state,JSON_C_TO_STRING_PLAIN));
    assert(decoded && json_object_equal(decoded,state));
    json_object_put(state);state=decoded;
    assert(json_object_object_get_ex(state,"open_domain_forcing_state",&receipt));
    restored.steps=1;restored.time=restored.dt;
    memcpy(restored.velocity,s.velocity,(size_t)s.velocity_count*sizeof(double));
    CfdOpenWeatherState output={0};assert(physics_open_weather_restore(&restored,&other,&output,state));
    assert(output.external_work_j==accepted.external_work_j && output.air_in_m3==.001);
    CfdOpenWeatherState prior=output;double prior_ledger[10];memcpy(prior_ledger,restored.kinetic_ledger,sizeof(prior_ledger));
    json_object_object_add(receipt,"external_work_j",json_object_new_double(accepted.external_work_j+1));
    assert(!physics_open_weather_restore(&restored,&other,&output,state));
    assert(!memcmp(prior_ledger,restored.kinetic_ledger,sizeof(prior_ledger)));
    assert(output.external_work_j==prior.external_work_j && output.air_in_m3==prior.air_in_m3);
    CfdOpenAtmosphereWeather disabled={0};assert(!physics_open_weather_restore(&restored,&disabled,&output,state));
    struct json_object *conflict=policy();json_object_object_add(conflict,"wind_forcing",NULL);
    CfdOpenAtmosphereWeather refused={0};assert(!physics_open_weather_load(&s,&refused,conflict));
    json_object_put(conflict);json_object_put(state);json_object_put(fields);json_object_put(request);
    cfd_open_atmosphere3d_destroy(&s);cfd_open_atmosphere3d_destroy(&restored);
}
int main(void) {
    assert(weather_force_component_main()==0);actual_volume();all_open_faces();io_and_work();
    puts("open weather observed volume, separate state, JSON restart and atomic refusal: passed");return 0;
}
