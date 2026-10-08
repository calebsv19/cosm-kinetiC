#ifndef PHYSICS_SIM_JOB_GUARD_H
#define PHYSICS_SIM_JOB_GUARD_H
#include <stdbool.h>
#include <sys/types.h>
typedef struct PhysicsSimJobGuard {
    char root[1024], parent[1024];
    int root_descriptor, parent_descriptor, lock_descriptor;
    pid_t process;
} PhysicsSimJobGuard;
bool physics_sim_job_guard_begin(const char *root, PhysicsSimJobGuard *guard);
bool physics_sim_job_guard_check(const PhysicsSimJobGuard *guard);
#define PHYSICS_SIM_JOB_PAIR_PENDING ".physics-sim-job-publication.pending"
bool physics_sim_job_guard_owns(const char *root);
bool physics_sim_job_guard_current(void);
bool physics_sim_job_guard_covers(const char *path);
void physics_sim_job_guard_end(PhysicsSimJobGuard *guard);
#endif
