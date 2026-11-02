/* Closed opt-in profile bridge. No scalar solver implementation lives here. */
#ifndef PHYSICS_SIM_SCALAR_PROFILE_IO_H
#define PHYSICS_SIM_SCALAR_PROFILE_IO_H
#include "app/cfd_open_atmosphere3d.h"
#include <json-c/json.h>
#include <string.h>
#define PHYSICS_SCALAR_PROFILE "mc_ssprk2/v1"
static inline bool physics_scalar_profile_load(CfdOpenAtmosphere3d *s,
    struct json_object *request) {
    struct json_object *profile=NULL;
    if(!json_object_object_get_ex(request,"scalar_transport",&profile))return true;
    struct json_object *diagnostic=NULL;
    if(!profile || !json_object_is_type(profile,json_type_string) ||
       strcmp(json_object_get_string(profile),PHYSICS_SCALAR_PROFILE) ||
       json_object_object_get_ex(request,"diagnostic_transport",&diagnostic))return false;
    s->scalar_mc=true;s->scalar_rk2=true;return true;
}
static inline bool physics_scalar_profile_restore(const CfdOpenAtmosphere3d *s,
    struct json_object *state) {
    struct json_object *profile=NULL;
    bool present=json_object_object_get_ex(state,"scalar_transport",&profile);
    if(!s->scalar_mc && !s->scalar_rk2)return !present;
    return s->scalar_mc && s->scalar_rk2 && present && profile &&
        json_object_is_type(profile,json_type_string) &&
        !strcmp(json_object_get_string(profile),PHYSICS_SCALAR_PROFILE);
}
static inline void physics_scalar_profile_fields(const CfdOpenAtmosphere3d *s,
    struct json_object *fields,struct json_object *state) {
    if(!s->scalar_mc || !s->scalar_rk2)return;
    json_object_object_add(fields,"scalar_transport",json_object_new_string(PHYSICS_SCALAR_PROFILE));
    json_object_object_add(state,"scalar_transport",json_object_new_string(PHYSICS_SCALAR_PROFILE));
}
#endif
