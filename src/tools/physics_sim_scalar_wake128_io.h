/* Separately selected fixed-input wake capacity; short/default policies unchanged. */
#ifndef PHYSICS_SIM_SCALAR_WAKE128_IO_H
#define PHYSICS_SIM_SCALAR_WAKE128_IO_H
static bool physics_scalar_wake128(struct json_object *r,bool packet,bool qualification) {
#ifdef PHYSICS_SIM_SCALAR_WAKE128
    return physics_scalar128_capacity(r,packet,qualification,"scalar-wake128/v1",640,2);
#else
    (void)r;(void)packet;(void)qualification;return false;
#endif
}
#endif
