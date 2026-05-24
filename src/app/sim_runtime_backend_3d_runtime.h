#ifndef SIM_RUNTIME_BACKEND_3D_RUNTIME_H
#define SIM_RUNTIME_BACKEND_3D_RUNTIME_H

#include <stdbool.h>
#include <stddef.h>

#include "app/sim_runtime_backend_3d_scaffold_internal.h"

size_t backend_3d_scaffold_runtime_default_solver_region_cell_budget(void);
void backend_3d_scaffold_runtime_reset_metrics(SimRuntimeBackend3DScaffold *state);
void backend_3d_scaffold_runtime_note_export_cache_materialized(SimRuntimeBackend3DScaffold *state);
bool backend_3d_scaffold_runtime_step(SimRuntimeBackend *backend,
                                      struct SceneState *scene,
                                      const AppConfig *cfg,
                                      double dt [[fisics::dim(time)]] [[fisics::unit(second)]]);

#endif
