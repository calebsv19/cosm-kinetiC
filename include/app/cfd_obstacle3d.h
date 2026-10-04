#ifndef PHYSICS_SIM_CFD_OBSTACLE3D_H
#define PHYSICS_SIM_CFD_OBSTACLE3D_H
#include "app/cfd_mixed3d.h"
typedef struct CfdObstacleMixed3d CfdObstacleMixed3d;
CfdObstacleMixed3d *cfd_obstacle_mixed3d_create(const CfdCartesian3d *, double, const int[3],
                                                const int[3]);
void cfd_obstacle_mixed3d_destroy(CfdObstacleMixed3d *);
int cfd_obstacle_mixed3d_count(const CfdObstacleMixed3d *);
int cfd_obstacle_mixed3d_cells(const CfdObstacleMixed3d *);
int cfd_obstacle_mixed3d_cell(const CfdObstacleMixed3d *, int, int, int);
int cfd_obstacle_mixed3d_index(const CfdObstacleMixed3d *, int, int, int, int);
void cfd_obstacle_mixed3d_position(const CfdObstacleMixed3d *, int, int *, double[3], int[3]);
double cfd_obstacle_mixed3d_volume(const CfdObstacleMixed3d *, int);
void cfd_obstacle_mixed3d_divergence(const CfdObstacleMixed3d *, const double *, double *);
bool cfd_obstacle_mixed3d_solve(CfdObstacleMixed3d *, const double *, double *, double *);
void cfd_obstacle_mixed3d_checkpoint(CfdObstacleMixed3d *, CfdMixed3dCheckpoint, void *);
int cfd_obstacle_mixed3d_iterations(const CfdObstacleMixed3d *);
int cfd_obstacle_mixed3d_inner_iterations(const CfdObstacleMixed3d *);
double cfd_obstacle_mixed3d_residual(const CfdObstacleMixed3d *);
const char *cfd_obstacle_mixed3d_error(const CfdObstacleMixed3d *);
void cfd_obstacle_mixed3d_reaction(const CfdObstacleMixed3d *, const double *, const double *,
                                   double[3], double[3], double[4][3]);
double cfd_obstacle_mixed3d_work(CfdObstacleMixed3d *, const double *);
#ifdef CFD_MIXED3D_VERIFY
bool cfd_obstacle_mixed3d_verify(CfdObstacleMixed3d *);
#endif
typedef struct {
    CfdCartesian3d grid;
    CfdObstacleMixed3d *mixed;
    int lo[3], hi[3], count, cells, iterations, inner_iterations;
    double rho, mu, requested_flow, center_x, inlet_pressure, inlet_flow, outlet_flow, flux_error;
    double *u, *p, *candidate_u, *candidate_p, *rhs, *reconstruction_planes;
    double reconstruction_divergence_l2, legacy_physical_dissipation, legacy_viscous_force[3];
    double pressure_force[3], viscous_force[3], side_pressure[6][3], side_viscous[6][3],
        wall_force[4][3];
    double discrete_pressure_force[3], discrete_viscous_force[3], discrete_walls[4][3],
        discrete_momentum_residual[3];
    double momentum_residual[3], physical_dissipation, physical_power, natural_power,
        discrete_dissipation;
    double physical_energy_imbalance, discrete_energy_imbalance, max_divergence, max_speed,
        relative_residual;
    double setup_cpu_ms, solve_cpu_ms;
    bool solved;
    const char *error;
} CfdObstacle3d;
bool cfd_obstacle3d_init(CfdObstacle3d *, const int[3], const double[3], double, double, double,
                         double);
bool cfd_obstacle3d_solve(CfdObstacle3d *);
void cfd_obstacle3d_destroy(CfdObstacle3d *);
void cfd_obstacle3d_derivatives(const CfdObstacle3d *, int, int, int, double[3][3]);
void cfd_obstacle3d_reconstruct_strain(CfdObstacle3d *, const double *);
void cfd_obstacle3d_trace_viscous(const CfdObstacle3d *, const double *, int, int, const int[3],
                                  double[3]);
void cfd_obstacle3d_measure(CfdObstacle3d *, const double *, const double *);
double cfd_obstacle3d_face(const CfdObstacle3d *, int, int, int, int);
double cfd_obstacle3d_pressure(const CfdObstacle3d *, int, int, int);
#endif
