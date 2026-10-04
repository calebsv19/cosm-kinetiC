#ifndef PHYSICS_SIM_CFD_3D_SESSION_H
#define PHYSICS_SIM_CFD_3D_SESSION_H
#include "app/cfd_duct3d.h"
#include "app/cfd_obstacle3d.h"
#include "app/cfd_open3d.h"
#include "app/cfd_periodic3d.h"
#include "app/cfd_startup3d.h"
#include "app/cfd_wall3d.h"
#include <json-c/json.h>
typedef enum {
    CFD_3D_TRANSIENT_NONE,
    CFD_3D_WALL_STOKES,
    CFD_3D_WALL_TRANSPORT,
    CFD_3D_PRESSURE_STARTUP,
    CFD_3D_OPEN_WALL_STOKES
} Cfd3dTransientKind;
typedef struct {
    double sum_sin, sum_cos, sum_sin2, sum_cos2, sum_sincos;
    double velocity[3], pressure[3], last_time;
    int count;
    bool complete;
} Cfd3dHarmonic;
/* Owned adapter; never copy while live: allocation owners point at memory. */
typedef struct {
    CfdMemoryBudget memory;
    CfdDuct3d duct;
    CfdObstacle3d obstacle_duct;
    bool obstacle, box;
    CfdOpen3d open_duct;
    CfdPeriodic3d transient;
    CfdWall3d wall;
    CfdStartup3d startup;
    Cfd3dTransientKind transient_kind;
    Cfd3dHarmonic harmonic;
    CfdCartesian3d grid;
    bool ready, observed, failed, steady, open, cancelled;
    double time, rho, mu, dt;
    /* These native work metrics are overwritten while forming a candidate. */
    double accepted_transport_cfl, accepted_transport_power_w, accepted_transport_self_power_w;
    const char *error;
} Cfd3dSession;
bool cfd_3d_session_init(Cfd3dSession *s, struct json_object *request);
bool cfd_3d_session_step(Cfd3dSession *s);
const char *cfd_3d_session_mode(const Cfd3dSession *s);
void cfd_3d_session_checkpoint(Cfd3dSession *s, CfdMixed3dCheckpoint function, void *context);
void cfd_transient3d_session_snapshot(const Cfd3dSession *s, struct json_object *out, bool full);
void cfd_transient3d_sample_values(const Cfd3dSession *s, const double xyz[3], double out[11]);
void cfd_3d_harmonic_observe(Cfd3dSession *s);
struct json_object *cfd_3d_harmonic_snapshot(const Cfd3dSession *s);
void cfd_obstacle3d_session_snapshot(const Cfd3dSession *, struct json_object *, bool);
void cfd_obstacle3d_sample_values(const Cfd3dSession *,const double[3],double[11]);
void cfd_3d_session_destroy(Cfd3dSession *s);
void cfd_3d_session_snapshot(const Cfd3dSession *s, struct json_object *out, bool full);
void cfd_open3d_session_snapshot(const Cfd3dSession *s, struct json_object *out, bool full);
struct json_object *cfd_3d_session_sample(const Cfd3dSession *s, struct json_object *request);
#endif
