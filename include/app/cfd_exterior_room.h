#ifndef PHYSICS_SIM_EXTERIOR_ROOM_H
#define PHYSICS_SIM_EXTERIOR_ROOM_H
#include <stdbool.h>
struct CfdOpenAtmosphere3d;
bool cfd_exterior_room(struct CfdOpenAtmosphere3d *s, bool large_window, bool far_exterior);
#endif
