#ifndef PHYSICS_SIM_PERSISTENCE_H
#define PHYSICS_SIM_PERSISTENCE_H
#include "app/physics_sim_headless_output.h"
#include <stdint.h>
typedef struct PhysicsSimPersistence {
    PhysicsSimHeadlessSidecar sidecar;
    int lock_descriptor;
    uint64_t byte_limit;
} PhysicsSimPersistence;
/* Existing admitted parent required. Failed stages are retained. */
FILE *physics_sim_persistence_begin(const char *path, PhysicsSimPersistence *save);
/* Explicit class bound: positive and at most 8 GiB. Default begin is 16 MiB. */
FILE *physics_sim_persistence_begin_bounded(const char *path, uint64_t byte_limit, PhysicsSimPersistence *save);
/* Consume stream and release ownership without publishing; candidate retained. */
void physics_sim_persistence_abort(PhysicsSimPersistence *save, FILE *stream);
/* Always consumes stream and closes ownership. False after rename is unconfirmed. */
bool physics_sim_persistence_finish(PhysicsSimPersistence *save, FILE *stream);
bool physics_sim_persistence_text(const char *path, const char *text);
/* Creates only data/runtime under current cwd using nofollow directory descriptors. */
bool physics_sim_persistence_runtime_directory(void);
#endif
