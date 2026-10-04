#ifndef PHYSICS_SIM_CFD_OPEN2D_H
#define PHYSICS_SIM_CFD_OPEN2D_H
#include "app/cfd_pressure_mg.h"
#include <stdbool.h>
typedef struct CfdOpen2D {
    int nx, ny, iterations;
    double length, height, width, rho, mu, inlet_mean, time;
    double *u, *v, *p, *ut, *vt, *r, *d, *a;
    CfdPressureMG *pressure_mg;
    double *z;
    unsigned char *solid;
    int obstacle_x0, obstacle_y0, obstacle_x1, obstacle_y1;
    double divergence, residual, inlet_flux, outlet_flux, inlet_pressure, outlet_backflow;
    double predictor_ms, pressure_ms;
} CfdOpen2D;
bool cfd_open2d_init(CfdOpen2D *c, int nx, int ny, double length, double height, double width,
                     double rho, double mu, double inlet_mean);
/* One stationary, grid-aligned interior rectangle, authored only at time zero.
 * Initial channel velocities are clipped at blocked faces; first projection
 * resolves that initial divergence. This is not a steady initial condition. */
bool cfd_open2d_set_obstacle(CfdOpen2D *c, int x0, int y0, int x1, int y1);
void cfd_open2d_destroy(CfdOpen2D *c);
bool cfd_open2d_step(CfdOpen2D *c, double dt);
#endif
