#ifndef PHYSICS_SIM_CFD_REFINED_DIFFUSION_H
#define PHYSICS_SIM_CFD_REFINED_DIFFUSION_H
#include "app/cfd_refined_mesh.h"
typedef struct CfdRefinedDiffusion CfdRefinedDiffusion;
typedef struct {
    int iterations;
    double residual, flux_mismatch;
} CfdRefinedSolve;
/* Isotropic unit diffusion, hybrid cell/face unknowns. Exterior faces use the supplied
 * Dirichlet mask (NULL means all Dirichlet); others prescribe outward
 * diffusive flux. At least one Dirichlet face is required. Mesh must outlive it. */
CfdRefinedDiffusion *cfd_refined_diffusion_create(const CfdRefinedMesh *m,
                                                  const unsigned char *dirichlet);
void cfd_refined_diffusion_destroy(CfdRefinedDiffusion *d);
/* -laplacian(p)=source; boundary values indexed by mesh face
 * are pressure on Dirichlet faces, outward flux density on Neumann faces. Outputs have
 * cell_count / face_count entries. flux is -grad(p) dot positive axis. */
bool cfd_refined_diffusion_solve(CfdRefinedDiffusion *d, const double *source,
                                 const double *boundary, double *cell_p, double *face_p,
                                 double *flux, CfdRefinedSolve *report);
/* mass*u - diffusivity*laplacian(u)=source. The positive mass term applies
 * to cell volumes; face values retain conservative diffusive coupling.
 * flux returns -diffusivity*grad(u), including prescribed Neumann flux. */
bool cfd_refined_helmholtz_solve(CfdRefinedDiffusion *d, double mass, double diffusivity,
                                 const double *source, const double *boundary, double *cell_u,
                                 double *face_u, double *flux, CfdRefinedSolve *report);
/* Visit the unconstrained unit-diffusion matrix, with duplicate entries from
 * neighboring local matrices. Intended for alternative algebra backends and
 * verification. Ordering is cells followed by faces; no boundary elimination. */
typedef bool (*CfdRefinedMatrixEntry)(int row, int column, double value, void *context);
bool cfd_refined_diffusion_matrix_visit(const CfdRefinedDiffusion *d, CfdRefinedMatrixEntry entry,
                                        void *context);
#endif
