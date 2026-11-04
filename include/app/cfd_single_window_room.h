#ifndef PHYSICS_SIM_SINGLE_WINDOW_ROOM_H
#define PHYSICS_SIM_SINGLE_WINDOW_ROOM_H
#include <stdbool.h>
struct CfdOpenAtmosphere3d;
/* Unit, no-slip chamber; one east opening, no extra exterior reservoirs. */
bool cfd_single_window_room(struct CfdOpenAtmosphere3d *s,bool large_window);
#endif
