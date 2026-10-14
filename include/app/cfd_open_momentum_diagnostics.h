#ifndef PHYSICS_SIM_OPEN_MOMENTUM_DIAGNOSTICS_H
#define PHYSICS_SIM_OPEN_MOMENTUM_DIAGNOSTICS_H
/* Single-threaded contained diagnostic build only; no ordinary ABI change. */
typedef struct {
    const char *check, *linear_failure;
    double courant, courant_limit, pressure_residual, maximum_divergence;
    double cg_recursive_residual, cg_true_residual, cg_preconditioned_dot, cg_operator_dot;
    int face_axis, face_i, face_j, face_k, divergence_cell;
    int pressure_iterations, cg_iteration, pressure_attempted;
} CfdOpenMomentumDiagnostics;
extern CfdOpenMomentumDiagnostics cfd_open_momentum_diagnostics;
void cfd_open_momentum_diagnostics_begin(void);
#endif
