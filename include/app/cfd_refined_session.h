#ifndef PHYSICS_SIM_CFD_REFINED_SESSION_H
#define PHYSICS_SIM_CFD_REFINED_SESSION_H
#include "app/cfd_memory.h"
#include "app/cfd_refined_channel.h"
#include <json-c/json.h>
/* Owned adapter for the existing trusted-local session worker. Do not copy a
 * live instance: allocation headers retain the address of its memory budget. */
typedef struct {
    CfdMemoryBudget memory;
    CfdRefinedMesh mesh;
    CfdRefinedChannel channel;
    int *heads, *next;
    double *gradient;
    double span, dt, linear_residual;
    double setup_cpu_ms, observation_cpu_ms, min_spacing_m, max_spacing_m;
    int cells_per_level[11];
    bool ready, steady_stokes, has_obstacle, observed, failed;
    double bounds[4];
    const char *error;
    CfdRefinedForce body_force;
    CfdRefinedEnergy energy;
} CfdRefinedSession;
bool cfd_refined_session_init(CfdRefinedSession *s, struct json_object *request);
bool cfd_refined_session_step(CfdRefinedSession *s);
void cfd_refined_session_destroy(CfdRefinedSession *s);
void cfd_refined_session_snapshot(const CfdRefinedSession *s, struct json_object *out, bool full);
struct json_object *cfd_refined_session_sample(const CfdRefinedSession *s,
                                               struct json_object *request);
#endif
