#ifndef PHYSICS_SIM_JOB_LOGS_H
#define PHYSICS_SIM_JOB_LOGS_H
#include <stdbool.h>
typedef struct PhysicsSimJobLogs {
    char root[1024];
    int root_descriptor, stdout_descriptor, stderr_descriptor;
} PhysicsSimJobLogs;
bool physics_sim_job_logs_prepare(const char *root, PhysicsSimJobLogs *logs);
bool physics_sim_job_logs_check(const PhysicsSimJobLogs *logs);
bool physics_sim_job_logs_redirect(PhysicsSimJobLogs *logs);
void physics_sim_job_logs_close(PhysicsSimJobLogs *logs);
#endif
