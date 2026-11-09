#ifndef PHYSICS_SIM_SESSION_CLIENT_LIFETIME_H
#define PHYSICS_SIM_SESSION_CLIENT_LIFETIME_H
#include <stdbool.h>
#include <sys/types.h>
/* Transfer only a freshly spawned, unreaped direct command client. The separate
 * simulation session is not owned by this call and must remain running. */
bool physics_sim_session_client_release(pid_t pid);
#endif
