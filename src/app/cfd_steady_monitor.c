#include "app/cfd_steady_monitor.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
static double value(const CfdOpen2D *c, int k) {
    int nu = (c->nx + 1) * c->ny;
    return k < nu ? c->u[k] : c->v[k - nu];
}
static void reset(CfdSteadyMonitor *m, const CfdOpen2D *c) {
    m->start = c->time;
    m->active_span = 0;
    for (int k = 0; k < m->count; k++)
        m->low[k] = m->high[k] = value(c, k);
}
bool cfd_steady_init(CfdSteadyMonitor *m, const CfdOpen2D *c) {
    memset(m, 0, sizeof(*m));
    if (c->nx < 1 || c->ny < 1 || c->nx > 4096 || c->ny > 4096 || !isfinite(c->inlet_mean) ||
        c->inlet_mean <= 0) {
        m->invalid = true;
        return false;
    }
    m->count = (c->nx + 1) * c->ny + c->nx * (c->ny + 1);
    m->low = calloc((size_t)m->count * 2, sizeof(double));
    if (!m->low) {
        m->invalid = true;
        return false;
    }
    m->high = m->low + m->count;
    m->last_time = c->time;
    reset(m, c);
    return true;
}
void cfd_steady_destroy(CfdSteadyMonitor *m) {
    free(m->low);
    memset(m, 0, sizeof(*m));
}
void cfd_steady_observe(CfdSteadyMonitor *m, const CfdOpen2D *c) {
    if (m->invalid)
        return;
    if (!isfinite(c->time) || c->time <= m->last_time || !isfinite(c->inlet_mean) ||
        c->inlet_mean <= 0) {
        m->invalid = true;
        return;
    }
    m->last_time = c->time;
    for (int k = 0; k < m->count; k++) {
        double v = value(c, k);
        if (!isfinite(v)) {
            m->invalid = true;
            return;
        }
        m->low[k] = fmin(m->low[k], v);
        m->high[k] = fmax(m->high[k], v);
        m->active_span = fmax(m->active_span, (m->high[k] - m->low[k]) / c->inlet_mean);
    }
    if (c->time - m->start >= 1 - 1e-10) {
        m->last_span = m->active_span;
        m->windows++;
        m->consecutive = m->start >= 5 - 1e-10 && m->last_span <= 1e-6 ? m->consecutive + 1 : 0;
        reset(m, c);
    }
}
bool cfd_steady_passed(const CfdSteadyMonitor *m) {
    return !m->invalid && m->consecutive >= 2 && m->active_span <= 1e-6;
}
