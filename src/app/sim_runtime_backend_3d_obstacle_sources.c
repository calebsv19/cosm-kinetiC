#include "app/sim_runtime_backend_3d_scaffold_internal.h"
#include "app/sim_runtime_backend_3d_oriented_box.h"

#include "app/scene_state.h"
#include "app/sim_runtime_3d_space.h"

#include <math.h>

static const float SCAFFOLD_IMPORT_DESIRED_FIT = 0.25f;

typedef SimRuntimeOrientedBox3DCells SimRuntimeObstacleOrientedBox3D;

static int clamp_int_value(int value, int min_value, int max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static float clamp_positive(float value, float fallback) {
    return value > 0.0f ? value : fallback;
}

static int normalized_position_to_grid(float position, int grid_extent) {
    if (grid_extent <= 1) return 0;
    return clamp_int_value((int)lroundf(position * (float)(grid_extent - 1)), 0, grid_extent - 1);
}

static int world_z_to_grid(const SimRuntime3DDomainDesc *desc, float position_z) {
    if (!desc) return 0;
    return sim_runtime_3d_space_world_to_grid_axis(position_z,
                                                   desc->world_min_z,
                                                   desc->voxel_size,
                                                   desc->grid_d);
}

static bool cell_in_sphere(int center_x,
                           int center_y,
                           int center_z,
                           int radius_cells,
                           int x,
                           int y,
                           int z) {
    float dx = (float)(x - center_x);
    float dy = (float)(y - center_y);
    float dz = (float)(z - center_z);
    float radius = (float)radius_cells;
    return (dx * dx + dy * dy + dz * dz) <= (radius * radius);
}

static bool cell_in_oriented_box(const SimRuntimeObstacleOrientedBox3D *box,
                                 int x,
                                 int y,
                                 int z) {
    return sim_runtime_backend_3d_cell_in_oriented_box(box, x, y, z);
}

static void mark_volume_cell(SimRuntimeBackend3DScaffold *state, int x, int y, int z) {
    size_t idx = 0;
    if (!state || !state->obstacle_occupancy) return;
    idx = sim_runtime_3d_volume_index(&state->volume.desc, x, y, z);
    state->obstacle_occupancy[idx] = 1u;
}

static void rasterize_sphere(SimRuntimeBackend3DScaffold *state,
                             int center_x,
                             int center_y,
                             int center_z,
                             int radius_cells) {
    const SimRuntime3DDomainDesc *desc = NULL;
    int min_x = 0;
    int max_x = 0;
    int min_y = 0;
    int max_y = 0;
    int min_z = 0;
    int max_z = 0;
    if (!state || !state->obstacle_occupancy || radius_cells <= 0) return;
    desc = &state->volume.desc;
    min_x = clamp_int_value(center_x - radius_cells, 0, desc->grid_w - 1);
    max_x = clamp_int_value(center_x + radius_cells, 0, desc->grid_w - 1);
    min_y = clamp_int_value(center_y - radius_cells, 0, desc->grid_h - 1);
    max_y = clamp_int_value(center_y + radius_cells, 0, desc->grid_h - 1);
    min_z = clamp_int_value(center_z - radius_cells, 0, desc->grid_d - 1);
    max_z = clamp_int_value(center_z + radius_cells, 0, desc->grid_d - 1);

    for (int z = min_z; z <= max_z; ++z) {
        for (int y = min_y; y <= max_y; ++y) {
            for (int x = min_x; x <= max_x; ++x) {
                if (!cell_in_sphere(center_x, center_y, center_z, radius_cells, x, y, z)) continue;
                mark_volume_cell(state, x, y, z);
            }
        }
    }
}

static void rasterize_oriented_box(SimRuntimeBackend3DScaffold *state,
                                   const SimRuntimeObstacleOrientedBox3D *box) {
    if (!state || !box) return;
    for (int z = box->min_z; z <= box->max_z; ++z) {
        for (int y = box->min_y; y <= box->max_y; ++y) {
            for (int x = box->min_x; x <= box->max_x; ++x) {
                if (!cell_in_oriented_box(box, x, y, z)) continue;
                mark_volume_cell(state, x, y, z);
            }
        }
    }
}

static bool build_object_box(const SimRuntime3DDomainDesc *desc,
                             const PresetObject *object,
                             SimRuntimeObstacleOrientedBox3D *out_box) {
    int center_x = 0;
    int center_y = 0;
    int center_z = 0;
    if (!desc || !object || !out_box) return false;
    center_x = normalized_position_to_grid(object->position_x, desc->grid_w);
    center_y = normalized_position_to_grid(object->position_y, desc->grid_h);
    center_z = world_z_to_grid(desc, object->position_z);
    return sim_runtime_backend_3d_build_preset_object_oriented_box(desc,
                                                                   object,
                                                                   center_x,
                                                                   center_y,
                                                                   center_z,
                                                                   out_box);
}

static bool build_import_box(const SimRuntime3DDomainDesc *desc,
                             const ImportedShape *imp,
                             SimRuntimeObstacleOrientedBox3D *out_box) {
    SimRuntimeObstacleOrientedBox3D box = {0};
    float scale = 1.0f;
    float radians = 0.0f;
    float span_x = 0.0f;
    float span_y = 0.0f;
    float span_z = 0.0f;
    if (!desc || !imp || !out_box) return false;

    scale = clamp_positive(imp->scale, 1.0f);
    box.center_x = normalized_position_to_grid(imp->position_x, desc->grid_w);
    box.center_y = normalized_position_to_grid(imp->position_y, desc->grid_h);
    box.center_z = world_z_to_grid(desc, imp->position_z);
    box.half_u_cells = clamp_positive(scale * SCAFFOLD_IMPORT_DESIRED_FIT * 0.5f * (float)desc->grid_w, 1.0f);
    box.half_v_cells = clamp_positive(scale * SCAFFOLD_IMPORT_DESIRED_FIT * 0.5f * (float)desc->grid_h, 1.0f);
    box.half_w_cells = clamp_positive(scale * SCAFFOLD_IMPORT_DESIRED_FIT * 0.5f * (float)desc->grid_d, 1.0f);
    radians = imp->rotation_deg * (float)M_PI / 180.0f;
    box.axis_u_x = cosf(radians);
    box.axis_u_y = sinf(radians);
    box.axis_u_z = 0.0f;
    box.axis_v_x = -sinf(radians);
    box.axis_v_y = cosf(radians);
    box.axis_v_z = 0.0f;
    box.axis_w_x = 0.0f;
    box.axis_w_y = 0.0f;
    box.axis_w_z = 1.0f;
    span_x = fabsf(box.axis_u_x) * box.half_u_cells +
             fabsf(box.axis_v_x) * box.half_v_cells +
             fabsf(box.axis_w_x) * box.half_w_cells;
    span_y = fabsf(box.axis_u_y) * box.half_u_cells +
             fabsf(box.axis_v_y) * box.half_v_cells +
             fabsf(box.axis_w_y) * box.half_w_cells;
    span_z = fabsf(box.axis_u_z) * box.half_u_cells +
             fabsf(box.axis_v_z) * box.half_v_cells +
             fabsf(box.axis_w_z) * box.half_w_cells;
    box.min_x = clamp_int_value((int)floorf((float)box.center_x - span_x - 1.0f), 0, desc->grid_w - 1);
    box.max_x = clamp_int_value((int)ceilf((float)box.center_x + span_x + 1.0f), 0, desc->grid_w - 1);
    box.min_y = clamp_int_value((int)floorf((float)box.center_y - span_y - 1.0f), 0, desc->grid_h - 1);
    box.max_y = clamp_int_value((int)ceilf((float)box.center_y + span_y + 1.0f), 0, desc->grid_h - 1);
    box.min_z = clamp_int_value((int)floorf((float)box.center_z - span_z - 1.0f), 0, desc->grid_d - 1);
    box.max_z = clamp_int_value((int)ceilf((float)box.center_z + span_z + 1.0f), 0, desc->grid_d - 1);
    *out_box = box;
    return true;
}

static void collect_attached_source_flags(const FluidScenePreset *preset,
                                          bool *emitter_on_object,
                                          bool *emitter_on_import) {
    if (!preset) return;
    for (size_t i = 0; i < preset->emitter_count && i < MAX_FLUID_EMITTERS; ++i) {
        int object_index = preset->emitters[i].attached_object;
        int import_index = preset->emitters[i].attached_import;
        if (emitter_on_object &&
            object_index >= 0 &&
            object_index < (int)MAX_PRESET_OBJECTS) {
            emitter_on_object[object_index] = true;
        }
        if (emitter_on_import &&
            import_index >= 0 &&
            import_index < (int)MAX_IMPORTED_SHAPES) {
            emitter_on_import[import_index] = true;
        }
    }
}

void backend_3d_scaffold_rasterize_retained_object_obstacles(
    SimRuntimeBackend3DScaffold *state,
    const struct SceneState *scene) {
    bool emitter_on_object[MAX_PRESET_OBJECTS] = {0};
    if (!state || !scene || !scene->preset || !state->obstacle_occupancy) return;
    collect_attached_source_flags(scene->preset, emitter_on_object, NULL);

    for (size_t i = 0; i < scene->preset->object_count && i < MAX_PRESET_OBJECTS; ++i) {
        const PresetObject *object = &scene->preset->objects[i];
        if (!object->is_static || emitter_on_object[i]) continue;
        if (object->type == PRESET_OBJECT_CIRCLE) {
            float size_z = clamp_positive(object->size_z, object->size_x);
            int center_x = normalized_position_to_grid(object->position_x, state->volume.desc.grid_w);
            int center_y = normalized_position_to_grid(object->position_y, state->volume.desc.grid_h);
            int center_z = world_z_to_grid(&state->volume.desc, object->position_z);
            int radius_x = (int)ceilf(clamp_positive(object->size_x * (float)state->volume.desc.grid_w, 1.0f));
            int radius_y = (int)ceilf(clamp_positive(object->size_x * (float)state->volume.desc.grid_h, 1.0f));
            int radius_z = (int)ceilf(clamp_positive(size_z * (float)state->volume.desc.grid_d, 1.0f));
            int radius_cells = radius_x;
            if (radius_y > radius_cells) radius_cells = radius_y;
            if (radius_z > radius_cells) radius_cells = radius_z;
            rasterize_sphere(state, center_x, center_y, center_z, radius_cells > 0 ? radius_cells : 1);
            continue;
        }

        {
            SimRuntimeObstacleOrientedBox3D box = {0};
            if (!build_object_box(&state->volume.desc, object, &box)) continue;
            rasterize_oriented_box(state, &box);
        }
    }
}

void backend_3d_scaffold_rasterize_retained_import_obstacles(
    SimRuntimeBackend3DScaffold *state,
    const struct SceneState *scene) {
    bool emitter_on_import[MAX_IMPORTED_SHAPES] = {0};
    if (!state || !scene || !state->obstacle_occupancy) return;
    collect_attached_source_flags(scene->preset, NULL, emitter_on_import);

    for (size_t i = 0; i < scene->import_shape_count && i < MAX_IMPORTED_SHAPES; ++i) {
        const ImportedShape *imp = &scene->import_shapes[i];
        SimRuntimeObstacleOrientedBox3D box = {0};
        if (!imp->enabled || emitter_on_import[i]) continue;
        if (!imp->is_static && !imp->gravity_enabled) continue;
        if (!build_import_box(&state->volume.desc, imp, &box)) continue;
        rasterize_oriented_box(state, &box);
    }
}
