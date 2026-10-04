#include "app/cfd_open2d_force_check.h"
bool cfd_open2d_force_check(const CfdOpen2D *c, int x0, int y0, int x1, int y1,
                            CfdMac2DForceCheck *out) {
    if (!c || !c->u || !c->solid || !out)
        return false;
    CfdMac2D view = {0};
    view.nx = c->nx;
    view.ny = c->ny;
    view.parameters.length = c->length;
    view.parameters.height = c->height;
    view.parameters.width = c->width;
    view.parameters.rho = c->rho;
    view.parameters.mu = c->mu;
    view.p = c->p;
    view.v = c->v;
    view.solid = c->solid;
    view.u = c->u;
    bool ok = cfd_mac2d_force_check_strided(&view, c->nx + 1, x0, y0, x1, y1, out);
    if (!ok || !out->higher_order_available)
        return false;
    /* Third-fluid-row reconstruction is independently calibrated on quadratic
     * pressure and cubic no-slip velocity, then compared with a P2/P1 reference.
     * Keep the lower-order periodic estimator unchanged. */
    out->surface_pressure_x_n = out->quadratic_pressure_x_n;
    out->surface_viscous_x_n = out->cubic_viscous_x_n;
    return true;
}
