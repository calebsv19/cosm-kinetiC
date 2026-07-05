#include "app/sim_runtime_mesh_obstacle_proxy.h"

#include "app/scene_state.h"
#include "app/sim_runtime_backend_3d_scaffold_internal.h"

#include <stdlib.h>

static int mesh_proxy_clamp_int(int value, int min_value, int max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static bool mesh_proxy_instance_has_emitter_attachment(const struct SceneState *scene,
                                                       int mesh_instance_index) {
    if (!scene || !scene->preset || mesh_instance_index < 0) return false;
    for (size_t i = 0; i < scene->preset->emitter_count && i < MAX_FLUID_EMITTERS; ++i) {
        const FluidEmitter *emitter = &scene->preset->emitters[i];
        if (emitter->attached_runtime_mesh_enabled &&
            emitter->attached_runtime_mesh == mesh_instance_index) {
            return true;
        }
    }
    return false;
}

void backend_3d_scaffold_rasterize_runtime_mesh_asset_obstacles(
    SimRuntimeBackend3DScaffold *state,
    const struct SceneState *scene) {
    const PhysicsSimRuntimeMeshPreviewSet *set = NULL;
    if (!state || !scene || state->volume.desc.cell_count == 0u) return;
    set = &scene->runtime_visual.mesh_previews;
    if (!set->valid_contract || set->instance_count <= 0) return;

    for (int i = 0; i < set->instance_count; ++i) {
        const PhysicsSimRuntimeMeshPreviewInstance *instance = &set->instances[i];
        PhysicsSimRuntimeMeshObstacleReport report;
        uint8_t *mask = NULL;
        if (mesh_proxy_instance_has_emitter_attachment(scene, i)) continue;
        if (!physics_sim_runtime_mesh_obstacle_instance_enabled(instance)) continue;
        mask = (uint8_t *)calloc(state->volume.desc.cell_count, sizeof(uint8_t));
        if (!mask) continue;
        if (physics_sim_runtime_mesh_obstacle_voxelize_instance_cached(
                state->runtime_mesh_obstacle_cache,
                instance,
                &state->volume.desc,
                mask,
                state->volume.desc.cell_count,
                &report)) {
            int min_x = mesh_proxy_clamp_int(report.min_x, 0, state->volume.desc.grid_w - 1);
            int max_x = mesh_proxy_clamp_int(report.max_x, 0, state->volume.desc.grid_w - 1);
            int min_y = mesh_proxy_clamp_int(report.min_y, 0, state->volume.desc.grid_h - 1);
            int max_y = mesh_proxy_clamp_int(report.max_y, 0, state->volume.desc.grid_h - 1);
            int min_z = mesh_proxy_clamp_int(report.min_z, 0, state->volume.desc.grid_d - 1);
            int max_z = mesh_proxy_clamp_int(report.max_z, 0, state->volume.desc.grid_d - 1);
            for (int z = min_z; z <= max_z; ++z) {
                for (int y = min_y; y <= max_y; ++y) {
                    for (int x = min_x; x <= max_x; ++x) {
                        size_t idx = sim_runtime_3d_volume_index(&state->volume.desc, x, y, z);
                        if (!mask[idx]) continue;
                        backend_3d_scaffold_set_obstacle_cell(state, x, y, z, true);
                    }
                }
            }
        }
        free(mask);
    }
}
