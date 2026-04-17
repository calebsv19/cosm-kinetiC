#include "import/runtime_scene_solver_projection_internal.h"

#include <string.h>

#include "app/sim_runtime_3d_anchor.h"
#include "app/sim_runtime_3d_space.h"

static void apply_normalized_emitter_position(double x,
                                              double y,
                                              const SolverProjectionXYDomainMapping *mapping,
                                              FluidEmitter *dst) {
    if (!mapping || !mapping->valid || !dst) return;
    dst->position_x = (float)sim_runtime_3d_space_normalize_world_axis(x,
                                                                       mapping->min_x,
                                                                       mapping->max_x);
    dst->position_y = (float)sim_runtime_3d_space_normalize_world_axis(y,
                                                                       mapping->min_y,
                                                                       mapping->max_y);
}

void runtime_scene_solver_projection_apply_emitters_from_lights(json_object *runtime_root,
                                                                double world_scale,
                                                                FluidScenePreset *in_out_preset) {
    json_object *lights_array = NULL;
    size_t src_count = 0;
    size_t i = 0;
    if (!runtime_root || !in_out_preset) return;
    in_out_preset->emitter_count = 0;
    if (!json_object_object_get_ex(runtime_root, "lights", &lights_array) ||
        !json_object_is_type(lights_array, json_type_array)) {
        return;
    }

    src_count = json_object_array_length(lights_array);
    if (src_count > MAX_FLUID_EMITTERS) src_count = MAX_FLUID_EMITTERS;

    for (i = 0; i < src_count; ++i) {
        json_object *light = json_object_array_get_idx(lights_array, i);
        json_object *intensity = NULL;
        double lx = 0.5, ly = 0.5, lz = 0.0;
        double strength = 5.0;
        FluidEmitter *dst = NULL;
        if (!light || !json_object_is_type(light, json_type_object)) continue;
        dst = &in_out_preset->emitters[in_out_preset->emitter_count];
        memset(dst, 0, sizeof(*dst));

        if (runtime_scene_solver_projection_parse_vec3(light, "position", &lx, &ly, &lz)) {
            dst->position_x = runtime_scene_solver_projection_scaled_position(lx, world_scale);
            dst->position_y = runtime_scene_solver_projection_scaled_position(ly, world_scale);
            dst->position_z = runtime_scene_solver_projection_scaled_position(lz, world_scale);
        } else {
            dst->position_x = 0.5f;
            dst->position_y = 0.5f;
            dst->position_z = 0.0f;
        }
        if (json_object_object_get_ex(light, "intensity", &intensity)) {
            strength = json_object_get_double(intensity);
        }

        dst->type = EMITTER_DENSITY_SOURCE;
        dst->radius = 0.08f;
        dst->strength = runtime_scene_solver_projection_clampf_dim((float)strength, 0.0f, 5000.0f);
        dst->dir_x = 0.0f;
        dst->dir_y = -1.0f;
        dst->dir_z = 0.0f;
        dst->attached_object = -1;
        dst->attached_import = -1;
        in_out_preset->emitter_count++;
    }
}

int runtime_scene_solver_projection_apply_emitters_from_retained_objects(
    const PhysicsSimRetainedRuntimeScene *retained_scene,
    json_object *runtime_root,
    double world_scale,
    FluidScenePreset *in_out_preset) {
    int i = 0;
    int added = 0;
    SolverProjectionXYDomainMapping mapping = {0};
    if (!retained_scene || !runtime_root || !in_out_preset) return 0;
    in_out_preset->emitter_count = 0;
    runtime_scene_solver_projection_resolve_xy_domain_mapping(retained_scene, runtime_root, &mapping);
    for (i = 0; i < retained_scene->retained_object_count; ++i) {
        const CoreSceneObjectContract *object = &retained_scene->objects[i];
        SolverProjectionPhysicsOverlay overlay = {0};
        CoreObjectVec3 origin = {0};
        FluidEmitter *dst = NULL;
        if (in_out_preset->emitter_count >= MAX_FLUID_EMITTERS) break;
        if (!runtime_scene_solver_projection_overlay_for_object(runtime_root,
                                                                object->object.object_id,
                                                                &overlay) ||
            !overlay.has_emitter) {
            continue;
        }
        origin = sim_runtime_3d_anchor_retained_object_origin(object);
        dst = &in_out_preset->emitters[in_out_preset->emitter_count];
        memset(dst, 0, sizeof(*dst));
        dst->type = overlay.emitter_type;
        if (mapping.valid) {
            apply_normalized_emitter_position(origin.x, origin.y, &mapping, dst);
            dst->radius = (float)sim_runtime_3d_space_normalize_half_extent(
                overlay.emitter_radius,
                (mapping.span_x < mapping.span_y ? mapping.span_x : mapping.span_y),
                0.0);
        } else {
            dst->position_x = runtime_scene_solver_projection_scaled_position(origin.x, world_scale);
            dst->position_y = runtime_scene_solver_projection_scaled_position(origin.y, world_scale);
            dst->radius = runtime_scene_solver_projection_clampf_dim(
                (float)(overlay.emitter_radius * world_scale), 0.0f, 5000.0f);
        }
        dst->position_z = runtime_scene_solver_projection_scaled_position(origin.z, world_scale);
        dst->strength = runtime_scene_solver_projection_clampf_dim((float)overlay.emitter_strength,
                                                                   0.0f,
                                                                   5000.0f);
        dst->dir_x = (float)overlay.emitter_dir_x;
        dst->dir_y = (float)overlay.emitter_dir_y;
        dst->dir_z = (float)overlay.emitter_dir_z;
        dst->attached_object = i;
        dst->attached_import = -1;
        in_out_preset->emitter_count++;
        added++;
    }
    return added;
}

int runtime_scene_solver_projection_apply_emitters_from_runtime_root_objects(
    json_object *runtime_root,
    double world_scale,
    FluidScenePreset *in_out_preset) {
    json_object *objects = NULL;
    size_t src_count = 0;
    size_t i = 0;
    int added = 0;
    if (!runtime_root || !in_out_preset) return 0;
    in_out_preset->emitter_count = 0;
    if (!json_object_object_get_ex(runtime_root, "objects", &objects) ||
        !json_object_is_type(objects, json_type_array)) {
        return 0;
    }
    src_count = json_object_array_length(objects);
    for (i = 0; i < src_count && in_out_preset->emitter_count < MAX_FLUID_EMITTERS; ++i) {
        json_object *src = json_object_array_get_idx(objects, i);
        json_object *object_id = NULL;
        json_object *transform = NULL;
        const char *object_id_str = NULL;
        SolverProjectionPhysicsOverlay overlay = {0};
        double px = 0.0;
        double py = 0.0;
        double pz = 0.0;
        FluidEmitter *dst = NULL;
        if (!src || !json_object_is_type(src, json_type_object)) continue;
        if (!json_object_object_get_ex(src, "object_id", &object_id) ||
            !json_object_is_type(object_id, json_type_string)) {
            continue;
        }
        object_id_str = json_object_get_string(object_id);
        if (!runtime_scene_solver_projection_overlay_for_object(runtime_root, object_id_str, &overlay) ||
            !overlay.has_emitter) {
            continue;
        }
        if (json_object_object_get_ex(src, "transform", &transform) &&
            json_object_is_type(transform, json_type_object)) {
            (void)runtime_scene_solver_projection_parse_vec3(transform, "position", &px, &py, &pz);
        }
        dst = &in_out_preset->emitters[in_out_preset->emitter_count];
        memset(dst, 0, sizeof(*dst));
        dst->type = overlay.emitter_type;
        dst->position_x = runtime_scene_solver_projection_scaled_position(px, world_scale);
        dst->position_y = runtime_scene_solver_projection_scaled_position(py, world_scale);
        dst->position_z = runtime_scene_solver_projection_scaled_position(pz, world_scale);
        dst->radius = runtime_scene_solver_projection_clampf_dim(
            (float)(overlay.emitter_radius * world_scale), 0.0f, 5000.0f);
        dst->strength = runtime_scene_solver_projection_clampf_dim((float)overlay.emitter_strength,
                                                                   0.0f,
                                                                   5000.0f);
        dst->dir_x = (float)overlay.emitter_dir_x;
        dst->dir_y = (float)overlay.emitter_dir_y;
        dst->dir_z = (float)overlay.emitter_dir_z;
        dst->attached_object = (int)i;
        dst->attached_import = -1;
        in_out_preset->emitter_count++;
        added++;
    }
    return added;
}
