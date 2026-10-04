#ifndef PHYSICS_SIM_CFD_STEADY_MONITOR_H
#define PHYSICS_SIM_CFD_STEADY_MONITOR_H
#include "app/cfd_open2d.h"
/* Whole-field, every-step observation: no endpoint-only aliasing. App-owned
 * acceptance policy: two full one-second windows after five seconds, with
 * velocity span <= 1e-6 of prescribed mean inlet velocity. */
typedef struct CfdSteadyMonitor {
    double *low, *high;
    double start, last_time, last_span, active_span;
    int count, consecutive, windows;
    bool invalid;
} CfdSteadyMonitor;
bool cfd_steady_init(CfdSteadyMonitor *m, const CfdOpen2D *c);
void cfd_steady_destroy(CfdSteadyMonitor *m);
void cfd_steady_observe(CfdSteadyMonitor *m, const CfdOpen2D *c);
bool cfd_steady_passed(const CfdSteadyMonitor *m);
#endif
