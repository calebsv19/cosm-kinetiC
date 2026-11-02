/* Separate fixed work admission; no broader ordinary/legacy capacity. */
#ifndef PHYSICS_SIM_SCALAR_WAKE128_WORK_IO_H
#define PHYSICS_SIM_SCALAR_WAKE128_WORK_IO_H
#define PHYSICS_WAKE128_WORK_LIMIT 10737418240ULL
#define PHYSICS_WAKE128_MAX_CYCLES 4
static bool physics_scalar_wake128_work(struct json_object *r,bool packet,bool qualification) {
#ifdef PHYSICS_SIM_SCALAR_WAKE128_WORK
    double work;
    return physics_scalar128_capacity(r,packet,qualification,"scalar-wake128/v2",640,2) &&
        num(get(get(r,"resource_limits"),"scalar_work_cells"),&work) &&
        work==(double)PHYSICS_WAKE128_WORK_LIMIT;
#else
    (void)r;(void)packet;(void)qualification;return false;
#endif
}
#endif
