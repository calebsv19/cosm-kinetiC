#ifndef PHYSICS_SIM_PATH_OPENER_H
#define PHYSICS_SIM_PATH_OPENER_H

typedef enum {
    PHYSICS_SIM_PATH_OPENER_OPENED = 0,
    PHYSICS_SIM_PATH_OPENER_INVALID_PATH,
    PHYSICS_SIM_PATH_OPENER_UNAVAILABLE,
    PHYSICS_SIM_PATH_OPENER_FAILED
} PhysicsSimPathOpenerResult;

/* Opens an existing directory in the host file manager without a shell. */
PhysicsSimPathOpenerResult PhysicsSim_PathOpener_OpenDirectory(const char *path);

#endif
