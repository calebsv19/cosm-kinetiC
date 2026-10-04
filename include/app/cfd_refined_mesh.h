#ifndef PHYSICS_SIM_CFD_REFINED_MESH_H
#define PHYSICS_SIM_CFD_REFINED_MESH_H
#include <stdbool.h>
#include <stddef.h>
/* Fixed balanced rectangular leaves. Integer coordinates use the finest lattice.
 * Every internal face is stored once, including each coarse/fine subface.
 * Positive flux points in +x or +y; lo/hi are the cells on those sides.
 * This is topology/transfer infrastructure, not a qualified flow solver. */
typedef struct {
    int x, y, span, parent;
    double cx, cy, volume;
} CfdRefinedCell;
enum { CFD_REFINED_INTERIOR = 0, CFD_REFINED_DOMAIN = 1, CFD_REFINED_SOLID = 2 };
typedef struct {
    int lo, hi, axis;
    double cx, cy, area;
    int boundary;
} CfdRefinedFace;
typedef struct {
    int nx, ny, cell_count, face_count, lattice_scale;
    double dx, dy;
    CfdRefinedCell *cells;
    CfdRefinedFace *faces;
} CfdRefinedMesh;
bool cfd_refined_mesh_init(CfdRefinedMesh *m, int nx, int ny, double length, double height, int x0,
                           int y0, int x1, int y1);
typedef struct {
    double x0, y0, x1, y1;
    int level;
} CfdRefinementRegion;
/* Fixed nested/overlapping physical regions, levels 0..10. Leaves intersecting
 * a region are refined to its level; face neighbors are balanced to 2:1.
 * Sparse side construction avoids a full finest-lattice ownership array.
 * max_cells includes balance refinement and fails before exceeding the budget.
 * Like init, pass an uninitialized/destroyed mesh; failure leaves it empty. */
bool cfd_refined_mesh_init_regions(CfdRefinedMesh *m, int nx, int ny, double length, double height,
                                   const CfdRefinementRegion *regions, int region_count,
                                   int max_cells);
/* Remove one strictly interior rectangle composed entirely of refined leaves.
 * Call before creating any operator; success invalidates existing cell IDs.
 * Failure leaves the mesh unchanged. */
bool cfd_refined_mesh_remove_rectangle(CfdRefinedMesh *m, double x0, double y0, double x1,
                                       double y1);
void cfd_refined_mesh_destroy(CfdRefinedMesh *m);
/* Volume-weighted leaf values -> coarse cell averages; removed solid volume
 * contributes zero. These are full coarse-volume, not fluid-only averages. */
void cfd_refined_restrict(const CfdRefinedMesh *m, const double *leaf, double *coarse);
/* Reconstruct leaf averages from coarse averages and physical gradients.
 * User-supplied gradients allow a later bounded reconstruction policy. */
void cfd_refined_prolong(const CfdRefinedMesh *m, const double *coarse, const double *gx,
                         const double *gy, double *leaf);
/* Integrated outward flux per cell; shared faces cancel exactly in the sum. */
void cfd_refined_flux_balance(const CfdRefinedMesh *m, const double *face_velocity,
                              double *balance);
#endif
