#ifndef STRUCTURAL_SOLVER_H
#define STRUCTURAL_SOLVER_H

#include <stdbool.h>

#include "physics/structural/structural_scene.h"

typedef struct StructuralSolveResult {
    bool  success;
    bool  singular;
    int   iterations;
    float residual;
    char  warning[128];
} StructuralSolveResult;

bool structural_solve_truss(StructuralScene *scene, StructuralSolveResult *result);
bool structural_solve_frame(StructuralScene *scene, StructuralSolveResult *result);
void structural_compute_frame_internal_forces_ex(StructuralScene *scene,
                                                 const float *u,
                                                 float *out_forces,
                                                 size_t force_count,
                                                 bool update_edges);
void structural_compute_frame_internal_forces(StructuralScene *scene,
                                              const float *u,
                                              float *out_forces,
                                              size_t force_count);
void structural_apply_frame_stiffness(const StructuralScene *scene,
                                      const float *u,
                                      float *out_forces,
                                      size_t force_count);

#endif // STRUCTURAL_SOLVER_H
