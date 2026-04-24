#include "import/runtime_scene_solver_projection_internal.h"

#include <string.h>

#include "app/sim_runtime_3d_space.h"

static const float SOLVER_PLANE_THICKNESS = 0.02f;

static PresetObjectType map_object_type(const CoreSceneObjectContract *object) {
    if (!object) return PRESET_OBJECT_BOX;
    if (object->kind == CORE_SCENE_OBJECT_KIND_PLANE_PRIMITIVE ||
        object->kind == CORE_SCENE_OBJECT_KIND_RECT_PRISM_PRIMITIVE) {
        return PRESET_OBJECT_BOX;
    }
    if (strstr(object->object.object_type, "circle") != NULL) return PRESET_OBJECT_CIRCLE;
    return PRESET_OBJECT_BOX;
}

static void apply_motion_overlay(const SolverProjectionPhysicsOverlay *overlay,
                                 const SolverProjectionXYDomainMapping *mapping,
                                 PresetObject *dst) {
    if (!overlay || !dst) return;
    if (overlay->has_motion_mode) {
        dst->is_static = overlay->is_static;
        dst->gravity_enabled = !overlay->is_static;
    }
    if (overlay->has_initial_velocity) {
        if (mapping && mapping->valid) {
            dst->initial_velocity_x = runtime_scene_solver_projection_normalize_velocity(
                overlay->initial_velocity_x, mapping->span_x);
            dst->initial_velocity_y = runtime_scene_solver_projection_normalize_velocity(
                overlay->initial_velocity_y, mapping->span_y);
        } else {
            dst->initial_velocity_x = (float)overlay->initial_velocity_x;
            dst->initial_velocity_y = (float)overlay->initial_velocity_y;
        }
        dst->initial_velocity_z = (float)overlay->initial_velocity_z;
    }
}

static void apply_normalized_position(double x,
                                      double y,
                                      const SolverProjectionXYDomainMapping *mapping,
                                      PresetObject *dst) {
    if (!mapping || !mapping->valid || !dst) return;
    dst->position_x = (float)sim_runtime_3d_space_normalize_world_axis(x,
                                                                       mapping->min_x,
                                                                       mapping->max_x);
    dst->position_y = (float)sim_runtime_3d_space_normalize_world_axis(y,
                                                                       mapping->min_y,
                                                                       mapping->max_y);
}

static void apply_frame_orientation(const CoreSceneFrame3 *frame, PresetObject *dst) {
    if (!frame || !dst) return;
    dst->orientation_basis_valid = true;
    dst->orientation_u_x = (float)frame->axis_u.x;
    dst->orientation_u_y = (float)frame->axis_u.y;
    dst->orientation_u_z = (float)frame->axis_u.z;
    dst->orientation_v_x = (float)frame->axis_v.x;
    dst->orientation_v_y = (float)frame->axis_v.y;
    dst->orientation_v_z = (float)frame->axis_v.z;
    dst->orientation_w_x = (float)frame->normal.x;
    dst->orientation_w_y = (float)frame->normal.y;
    dst->orientation_w_z = (float)frame->normal.z;
}

static void runtime_scene_solver_projection_apply_json_object(json_object *src,
                                                              json_object *runtime_root,
                                                              double world_scale,
                                                              PresetObject *dst) {
    json_object *object_type = NULL;
    json_object *object_id = NULL;
    json_object *transform = NULL;
    json_object *flags = NULL;
    const char *object_type_str = "box";
    const char *object_id_str = NULL;
    double px = 0.0;
    double py = 0.0;
    double pz = 0.0;
    double sx = 0.08;
    double sy = 0.08;
    double sz = 0.08;
    double rx = 0.0;
    double ry = 0.0;
    double rz = 0.0;
    bool locked = false;
    SolverProjectionPhysicsOverlay overlay = {0};

    if (!src || !dst) return;
    memset(dst, 0, sizeof(*dst));

    if (json_object_object_get_ex(src, "object_id", &object_id) &&
        json_object_is_type(object_id, json_type_string)) {
        object_id_str = json_object_get_string(object_id);
    }
    if (json_object_object_get_ex(src, "object_type", &object_type) &&
        json_object_is_type(object_type, json_type_string)) {
        object_type_str = json_object_get_string(object_type);
    }

    dst->type = (object_type_str && strstr(object_type_str, "circle") != NULL)
                    ? PRESET_OBJECT_CIRCLE
                    : PRESET_OBJECT_BOX;

    if (json_object_object_get_ex(src, "transform", &transform) &&
        json_object_is_type(transform, json_type_object)) {
        (void)runtime_scene_solver_projection_parse_vec3(transform, "position", &px, &py, &pz);
        (void)runtime_scene_solver_projection_parse_vec3(transform, "scale", &sx, &sy, &sz);
        (void)runtime_scene_solver_projection_parse_vec3(transform, "rotation", &rx, &ry, &rz);
    }

    if (json_object_object_get_ex(src, "flags", &flags) && json_object_is_type(flags, json_type_object)) {
        (void)runtime_scene_solver_projection_parse_bool(flags, "locked", &locked);
    }

    dst->position_x = runtime_scene_solver_projection_scaled_position(px, world_scale);
    dst->position_y = runtime_scene_solver_projection_scaled_position(py, world_scale);
    dst->position_z = runtime_scene_solver_projection_scaled_position(pz, world_scale);
    dst->size_x = runtime_scene_solver_projection_scaled_size(sx, world_scale, 0.08f);
    dst->size_y = runtime_scene_solver_projection_scaled_size(sy, world_scale, 0.08f);
    dst->size_z = runtime_scene_solver_projection_scaled_size(sz, world_scale, 0.08f);
    dst->angle = (float)rz;
    dst->is_static = locked;
    dst->gravity_enabled = !locked;
    if (runtime_scene_solver_projection_overlay_for_object(runtime_root, object_id_str, &overlay)) {
        apply_motion_overlay(&overlay, NULL, dst);
    }
}

static void runtime_scene_solver_projection_apply_object(
    const CoreSceneObjectContract *object,
    json_object *runtime_root,
    const SolverProjectionXYDomainMapping *mapping,
    double world_scale,
    PresetObject *dst) {
    CoreObjectVec3 position = {0};
    CoreObjectVec3 scale = {0};
    float fallback_size = SOLVER_PLANE_THICKNESS;
    SolverProjectionPhysicsOverlay overlay = {0};
    if (!object || !dst) return;
    memset(dst, 0, sizeof(*dst));

    dst->type = map_object_type(object);
    dst->angle = (float)object->object.transform.rotation_deg.z;
    dst->is_static = object->object.flags.locked;
    dst->gravity_enabled = !dst->is_static;

    if (object->has_plane_primitive) {
        position = object->plane_primitive.frame.origin;
        apply_frame_orientation(&object->plane_primitive.frame, dst);
        if (mapping && mapping->valid) {
            dst->size_x = (float)sim_runtime_3d_space_normalize_half_extent(
                object->plane_primitive.width * 0.5, mapping->span_x, 0.04f);
            dst->size_y = (float)sim_runtime_3d_space_normalize_half_extent(
                object->plane_primitive.height * 0.5, mapping->span_y, 0.04f);
        } else {
            dst->size_x = runtime_scene_solver_projection_scaled_size(
                object->plane_primitive.width * 0.5, world_scale, fallback_size);
            dst->size_y = runtime_scene_solver_projection_scaled_size(
                object->plane_primitive.height * 0.5, world_scale, fallback_size);
        }
        dst->size_z = runtime_scene_solver_projection_scaled_size(
            SOLVER_PLANE_THICKNESS * 0.5, world_scale, fallback_size);
    } else if (object->has_rect_prism_primitive) {
        position = object->rect_prism_primitive.frame.origin;
        apply_frame_orientation(&object->rect_prism_primitive.frame, dst);
        if (mapping && mapping->valid) {
            dst->size_x = (float)sim_runtime_3d_space_normalize_half_extent(
                object->rect_prism_primitive.width * 0.5, mapping->span_x, 0.04f);
            dst->size_y = (float)sim_runtime_3d_space_normalize_half_extent(
                object->rect_prism_primitive.height * 0.5, mapping->span_y, 0.04f);
        } else {
            dst->size_x = runtime_scene_solver_projection_scaled_size(
                object->rect_prism_primitive.width * 0.5, world_scale, fallback_size);
            dst->size_y = runtime_scene_solver_projection_scaled_size(
                object->rect_prism_primitive.height * 0.5, world_scale, fallback_size);
        }
        dst->size_z = runtime_scene_solver_projection_scaled_size(
            object->rect_prism_primitive.depth * 0.5, world_scale, fallback_size);
    } else {
        position = object->object.transform.position;
        scale = object->object.transform.scale;
        dst->size_x = runtime_scene_solver_projection_scaled_size(scale.x, world_scale, 0.08f);
        dst->size_y = runtime_scene_solver_projection_scaled_size(scale.y, world_scale, 0.08f);
        dst->size_z = runtime_scene_solver_projection_scaled_size(scale.z, world_scale, 0.08f);
    }

    if (mapping && mapping->valid) {
        apply_normalized_position(position.x, position.y, mapping, dst);
    } else {
        dst->position_x = runtime_scene_solver_projection_scaled_position(position.x, world_scale);
        dst->position_y = runtime_scene_solver_projection_scaled_position(position.y, world_scale);
    }
    dst->position_z = runtime_scene_solver_projection_scaled_position(position.z, world_scale);
    if (runtime_scene_solver_projection_overlay_for_object(runtime_root,
                                                           object->object.object_id,
                                                           &overlay)) {
        apply_motion_overlay(&overlay, mapping, dst);
    }
}

void runtime_scene_solver_projection_apply_objects(const PhysicsSimRetainedRuntimeScene *retained_scene,
                                                   json_object *runtime_root,
                                                   FluidScenePreset *in_out_preset,
                                                   RuntimeSceneBridgePreflight *out_summary) {
    int src_count = 0;
    int i = 0;
    double world_scale = 1.0;
    SolverProjectionXYDomainMapping mapping = {0};
    if (!retained_scene || !in_out_preset) return;

    in_out_preset->object_count = 0;
    src_count = retained_scene->retained_object_count;
    if (src_count > MAX_PRESET_OBJECTS) src_count = MAX_PRESET_OBJECTS;
    world_scale = retained_scene->root.world_scale;
    if (world_scale <= 0.0) world_scale = 1.0;
    runtime_scene_solver_projection_resolve_xy_domain_mapping(retained_scene, runtime_root, &mapping);

    for (i = 0; i < src_count; ++i) {
        PresetObject *dst = &in_out_preset->objects[in_out_preset->object_count];
        runtime_scene_solver_projection_apply_object(&retained_scene->objects[i],
                                                     runtime_root,
                                                     &mapping,
                                                     world_scale,
                                                     dst);
        in_out_preset->object_count++;
    }

    if (out_summary) out_summary->object_count = (int)in_out_preset->object_count;
}

void runtime_scene_solver_projection_apply_runtime_root_objects(json_object *runtime_root,
                                                                double world_scale,
                                                                FluidScenePreset *in_out_preset,
                                                                RuntimeSceneBridgePreflight *out_summary) {
    json_object *objects = NULL;
    size_t src_count = 0;
    size_t i = 0;
    if (!runtime_root || !in_out_preset) return;

    in_out_preset->object_count = 0;
    if (!json_object_object_get_ex(runtime_root, "objects", &objects) ||
        !json_object_is_type(objects, json_type_array)) {
        if (out_summary) out_summary->object_count = 0;
        return;
    }

    src_count = json_object_array_length(objects);
    if (src_count > MAX_PRESET_OBJECTS) src_count = MAX_PRESET_OBJECTS;
    for (i = 0; i < src_count; ++i) {
        json_object *src = json_object_array_get_idx(objects, i);
        PresetObject *dst = &in_out_preset->objects[in_out_preset->object_count];
        if (!src || !json_object_is_type(src, json_type_object)) continue;
        runtime_scene_solver_projection_apply_json_object(src, runtime_root, world_scale, dst);
        in_out_preset->object_count++;
    }

    if (out_summary) out_summary->object_count = (int)in_out_preset->object_count;
}
