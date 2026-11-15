#ifndef PHYSICS_SIM_EXTERIOR_WORK30_IO_H
#define PHYSICS_SIM_EXTERIOR_WORK30_IO_H
/* Capacity covers every cycle already admitted by the ordinary numerical solver.
 * No CFL, pressure, contrast, scalar-cycle, memory or byte bound changes here. */
#define PHYSICS_EXTERIOR_STEPS 6000
#define PHYSICS_EXTERIOR_WORK (6000ULL * 262144ULL * 2ULL * 4096ULL)
static bool physics_exterior_work30(struct json_object *r,bool packet,bool qualification) {
    struct json_object *g=get(r,"rig_geometry"),*knots=get(get(r,"open_domain_forcing"),"knots");
    const char *schema=json_object_get_string(get(g,"schema"));
    const char *profile=json_object_get_string(get(g,"computation_profile"));
    const char *extent=json_object_get_string(get(g,"exterior_profile"));
    const char *scalar=json_object_get_string(get(r,"scalar_transport"));
    double grid[3],length[3],dt,work,target,strength;
    if(!packet || qualification || !schema || strcmp(schema,"physics_sim_exterior_room/v1") ||
       !profile || strcmp(profile,"exterior-room30/v1") || !extent || strcmp(extent,"far") ||
       !scalar || strcmp(scalar,"mc_ssprk2/v1") || json_object_object_length(g)!=4 ||
       !field(get(r,"grid"),grid,3) || !field(get(r,"length_m"),length,3) ||
       !num(get(r,"momentum_dt_s"),&dt) || dt!=.005 ||
       !num(get(get(r,"resource_limits"),"scalar_work_cells"),&work) || work!=(double)PHYSICS_EXTERIOR_WORK ||
       !knots || !json_object_is_type(knots,json_type_array) || json_object_array_length(knots)!=1)return false;
    for(int a=0;a<3;a++)if(grid[a]!=64 || length[a]!=2)return false;
    struct json_object *k=json_object_array_get_idx(knots,0);
    return num(get(k,"target_m_s"),&target) && target==0 && num(get(k,"strength"),&strength) && strength==0;
}
#endif
