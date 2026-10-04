#ifndef PHYSICS_SIM_CFD_REFINED_MIXED_H
#define PHYSICS_SIM_CFD_REFINED_MIXED_H
#include "app/cfd_refined_mesh.h"
typedef struct CfdRefinedMixed CfdRefinedMixed;
typedef struct {
    int iterations;
    double relative_residual, divergence;
    size_t storage_bytes;
} CfdRefinedMixedReport;
/* Compatible mixed velocity/pressure system on the hybrid mesh. Velocity is
 * Dirichlet at exterior faces except the right natural-traction outlet.
 * Outlet condition is nu*du/dn - p*n = 0, not a separately imposed p=0.
 * Pressure is kinematic (Pa/rho). Mesh must outlive this cached operator. */
CfdRefinedMixed *cfd_refined_mixed_create(const CfdRefinedMesh *m, double nu);
void cfd_refined_mixed_destroy(CfdRefinedMixed *s);
/* mass*u - nu*laplacian(u) + grad(p) = rhs; div(u)=0.
 * Cell rhs has acceleration units, boundary arrays have velocity units.
 * Operator, multilevel hierarchy and Krylov buffers persist; mass changes
 * rebuild the velocity hierarchy. CFD_REFINED_VERIFY_ILU retains the old
 * factorization only as a numerical/performance verification control.
 * CFD_REFINED_VERIFY_RESTART_ONLY disables the periodic true-residual candidate
 * checks for matched cost verification; accepted tolerance is identical. */
bool cfd_refined_mixed_solve(CfdRefinedMixed *s, double mass, const double *rhs_u,
                             const double *rhs_v, const double *boundary_u,
                             const double *boundary_v, double *u, double *v, double *face_u,
                             double *face_v, double *pressure, CfdRefinedMixedReport *report);
typedef struct {
    double pressure[2], viscous[2], total[2];
} CfdRefinedForce;
/* Integrated weak boundary reaction on the solid (or selected boundary).
 * boundary_kind=-1 selects all boundaries. Physical force uses rho and span.
 * Individual traction components still require external accuracy qualification. */
bool cfd_refined_mixed_boundary_force(const CfdRefinedMixed *s, int boundary_kind, double rho,
                                      double span, CfdRefinedForce *out);
typedef struct {
    double kinetic_j, strain_dissipation_w, discrete_diffusion_w, stabilization_w;
    double weak_boundary_power_w, stress_boundary_power_w;
    double body_force_power_w, outward_kinetic_flux_w;
} CfdRefinedEnergy;
/* Physical strain quadrature is evaluated separately from the matrix energy.
 * Boundary work uses the weak reaction plus the symmetric-stress correction.
 * Returns raw budget terms; caller supplies the energy derivative and chooses
 * Stokes versus Navier-Stokes transport semantics. Uses solver scratch space. */
bool cfd_refined_mixed_energy(CfdRefinedMixed *s, double rho, double span,
                              const double *acceleration_x, const double *acceleration_y,
                              CfdRefinedEnergy *out);
#endif
