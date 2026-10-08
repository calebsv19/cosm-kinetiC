#ifndef PHYSICS_SIM_JOB_STARTUP_H
#define PHYSICS_SIM_JOB_STARTUP_H
#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>
typedef struct PhysicsSimJobStartup { int reader, writer; } PhysicsSimJobStartup;
bool physics_sim_job_startup_open(PhysicsSimJobStartup *startup);
void physics_sim_job_startup_close(PhysicsSimJobStartup *startup);
void physics_sim_job_startup_child(PhysicsSimJobStartup *startup);
void physics_sim_job_startup_error(PhysicsSimJobStartup *startup, int phase, int error);
/* Only the caller's freshly forked, unreaped direct child is eligible for cleanup. */
bool physics_sim_job_startup_wait(PhysicsSimJobStartup *startup, pid_t pid, int timeout_ms,
    char *diagnostics, size_t size);
#endif
