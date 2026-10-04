#ifndef PHYSICS_SIM_CFD_PRESSURE_MG_H
#define PHYSICS_SIM_CFD_PRESSURE_MG_H
#include <stdbool.h>
#include <stddef.h>
/* App-owned Galerkin aggregation hierarchy for the open MAC pressure matrix.
 * Solid rows are isolated. Coarse operators only precondition the unchanged
 * fine-grid projection; they never alter the physical solid topology. */
typedef struct CfdPressureMG CfdPressureMG;
CfdPressureMG *cfd_pressure_mg_create(int nx, int ny, double dx, double dy,
                                      const unsigned char *solid);
size_t cfd_pressure_mg_storage_bytes(int nx, int ny);
void cfd_pressure_mg_destroy(CfdPressureMG *mg);
bool cfd_pressure_mg_apply(CfdPressureMG *mg, const double *rhs, double *solution);
#endif
