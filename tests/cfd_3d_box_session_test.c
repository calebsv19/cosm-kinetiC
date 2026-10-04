#include "app/cfd_3d_session.h"
#include "app/cfd_obstacle3d_box.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static struct json_object *get(struct json_object *o,const char *key) {
    struct json_object *v=NULL;assert(json_object_object_get_ex(o,key,&v));return v;
}
static struct json_object *request(void) {
    return json_tokener_parse("{\"grid\":[32,16,16],\"steps\":1,\"dt\":0.01,\"numerical_memory_limit_mib\":64,"
        "\"solver_cell_budget\":262144,\"fluid\":{\"density_kg_m3\":1,\"dynamic_viscosity_pa_s\":0.1},"
        "\"channel\":{\"solve_mode\":\"steady_box_duct\",\"dimensions_m\":[4,2,2],\"volume_flow_m3_s\":0.008,"
        "\"body_min_m\":[1.5,0.375,0.375],\"body_max_m\":[2.25,1.125,1.625]}}");
}
static void verify_snapshot(const Cfd3dSession *s,bool observed) {
    struct json_object *o=json_object_new_object();cfd_3d_session_snapshot(s,o,true);
    assert(!strcmp(json_object_get_string(get(o,"solve_mode")),"steady_box_duct"));
    struct json_object *geometry=get(o,"geometry"),*body=json_object_array_get_idx(geometry,0);
    assert(!strcmp(json_object_get_string(get(body,"kind")),"stationary_aligned_box"));
    double lo[3]={1.5,.375,.375},hi[3]={2.25,1.125,1.625};
    for(int a=0;a<3;a++) {
        assert(json_object_get_double(json_object_array_get_idx(get(body,"min_m"),a))==lo[a]);
        assert(json_object_get_double(json_object_array_get_idx(get(body,"max_m"),a))==hi[a]);
    }
    assert(json_object_get_double(get(body,"characteristic_length_m"))==1.25);
    assert(json_object_get_double(get(body,"projected_area_m2"))==.9375);
    assert(fabs(json_object_get_double(get(get(o,"physics"),"mean_body_reynolds"))-.025)<1e-15);
    assert(!json_object_get_boolean(get(get(o,"qualification"),"physical_accuracy_certified")));
    struct json_object *fields=get(o,"cartesian_fields"),*mask=get(fields,"solid_mask");
    int count=0;
    for(size_t i=0;i<json_object_array_length(mask);i++)count+=json_object_get_boolean(json_object_array_get_idx(mask,i));
    assert(count==6*6*10);
    if(observed) {
        struct json_object *sides=get(get(o,"boundary_force_budget"),"body_sides");
        double areas[3]={.9375,.9375,.5625};
        for(int face=0;face<6;face++)
            assert(json_object_get_double(get(json_object_array_get_idx(sides,face),"area_m2"))==areas[face/2]);
        const CfdObstacle3d *d=&s->obstacle_duct;
        double maximum=0;
        for(int a=0;a<3;a++)maximum=fmax(maximum,fabs(d->momentum_residual[a]));
        assert(json_object_get_double(get(get(o,"boundary_force_budget"),"momentum_relative_residual"))==maximum/(4*d->inlet_pressure));
        double cd=2*(d->pressure_force[0]+d->viscous_force[0])/(s->rho*pow(d->requested_flow/4,2)*.9375);
        assert(json_object_get_double(get(get(o,"boundary_force_budget"),"drag_coefficient"))==cd);
        struct json_object *points=get(o,"centerline_recovery");
        assert(json_object_array_length(points)>0);
        struct json_object *point=json_object_array_get_idx(points,0);
        assert(json_object_get_double(get(point,"x_m"))==2.5);
        assert(json_object_get_double(get(point,"y_m"))==.75);
        assert(json_object_get_double(get(point,"z_m"))==1);
    }
    json_object_put(o);
}
int main(void) {
    struct json_object *r=request();Cfd3dSession s;
    assert(cfd_3d_session_init(&s,r));assert(s.box && s.obstacle && s.steady);
    assert(!strcmp(cfd_3d_session_mode(&s),"steady_box_duct"));verify_snapshot(&s,false);
    assert(cfd_3d_session_step(&s));verify_snapshot(&s,true);assert(s.time==0);
    double saved=s.obstacle_duct.momentum_residual[1];
    s.obstacle_duct.momentum_residual[1]=8*s.obstacle_duct.inlet_pressure;
    verify_snapshot(&s,true);
    s.obstacle_duct.momentum_residual[1]=saved;
    CfdMemoryBudget budget={.limit_bytes=64*1024*1024};CfdMemoryBudget *previous=cfd_memory_scope(&budget);
    CfdObstacle3d direct;int n[3]={32,16,16};double length[3]={4,2,2},lo[3]={1.5,.375,.375},hi[3]={2.25,1.125,1.625};
    assert(cfd_obstacle3d_box_init(&direct,n,length,1,.1,.008,lo,hi));assert(cfd_obstacle3d_solve(&direct));
    assert(direct.count==s.obstacle_duct.count && direct.cells==s.obstacle_duct.cells);
    assert(!memcmp(direct.u,s.obstacle_duct.u,(size_t)direct.count*sizeof(double)));
    assert(!memcmp(direct.p,s.obstacle_duct.p,(size_t)direct.cells*sizeof(double)));
    cfd_obstacle3d_destroy(&direct);assert(budget.live_bytes==0);cfd_memory_scope(previous);
    cfd_3d_session_destroy(&s);assert(s.memory.live_bytes==0);json_object_put(r);
    for(int kind=0;kind<5;kind++) {
        r=request();struct json_object *ch=get(r,"channel");
        if(kind==0)json_object_object_del(ch,"body_min_m");
        if(kind==1)json_object_object_add(ch,"body_min_m",json_tokener_parse("[1.5,0.38,0.375]"));
        if(kind==2)json_object_object_add(ch,"center_x_m",json_object_new_double(2));
        if(kind==3)json_object_object_add(ch,"solve_mode",json_object_new_string("steady_obstacle_duct"));
        if(kind==4)json_object_object_add(ch,"body_max_m",json_tokener_parse("[2.25,0.375,1.625]"));
        assert(!cfd_3d_session_init(&s,r));assert(s.failed && !s.ready);
        cfd_3d_session_destroy(&s);assert(s.memory.live_bytes==0);json_object_put(r);
    }
    puts("Native typed box session: direct whole-field parity, actual geometry/mask/area/Re, body-centred sampling, false qualification and five rejection controls passed");
    return 0;
}
