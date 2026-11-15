/* Closed worker bridge for the qualified low-box closure. */
#ifndef PHYSICS_SIM_VISCOUS_WALL_IO_H
#define PHYSICS_SIM_VISCOUS_WALL_IO_H
#include "app/cfd_open_atmosphere3d.h"
#include <json-c/json.h>
#include <string.h>
#define PHYSICS_VISCOUS_WALL_PROFILE "low_box_quadratic/v1"
static inline bool physics_viscous_wall_load(CfdOpenAtmosphere3d *s,
    struct json_object *request) {
    struct json_object *profile=NULL,*unused=NULL;
    if(!json_object_object_get_ex(request,"viscous_wall_closure",&profile))return true;
    if(!profile || !json_object_is_type(profile,json_type_string) ||
       strcmp(json_object_get_string(profile),PHYSICS_VISCOUS_WALL_PROFILE))return false;
    const char *excluded[]={"capacity_profile","diagnostic_transport","inert_object","wind_forcing","rig_geometry"};
    for(size_t i=0;i<sizeof excluded/sizeof *excluded;i++)
        if(json_object_object_get_ex(request,excluded[i],&unused))return false;
    if(s->grid.n[0]!=32 || s->grid.n[1]!=32 ||
       (s->grid.n[2]!=32 && s->grid.n[2]!=64) || !s->ground ||
       !s->lateral_open || !s->solid_box)return false;
    const double lo[3]={.125,.125,.0625},hi[3]={.375,.375,.3125};
    for(int a=0;a<3;a++)
        if(s->grid.h[a]*s->grid.n[a]!=.5 ||
           s->solid_lo[a]*s->grid.h[a]!=lo[a] ||
           s->solid_hi[a]*s->grid.h[a]!=hi[a])return false;
    s->viscous_wall_curvature=true;return true;
}
static inline bool physics_viscous_wall_restore(const CfdOpenAtmosphere3d *s,
    struct json_object *state) {
    struct json_object *profile=NULL;
    bool present=json_object_object_get_ex(state,"viscous_wall_closure",&profile);
    if(!s->viscous_wall_curvature)return !present;
    return present && profile && json_object_is_type(profile,json_type_string) &&
        !strcmp(json_object_get_string(profile),PHYSICS_VISCOUS_WALL_PROFILE);
}
static inline void physics_viscous_wall_fields(const CfdOpenAtmosphere3d *s,
    struct json_object *fields,struct json_object *state) {
    if(!s->viscous_wall_curvature)return;
    json_object_object_add(fields,"viscous_wall_closure",json_object_new_string(PHYSICS_VISCOUS_WALL_PROFILE));
    json_object_object_add(state,"viscous_wall_closure",json_object_new_string(PHYSICS_VISCOUS_WALL_PROFILE));
}
#endif
