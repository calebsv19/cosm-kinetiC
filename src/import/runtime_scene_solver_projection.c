#include "import/runtime_scene_solver_projection.h"

#include <string.h>

static const float SOLVER_POSITION_LIMIT = 1024.0f;
static const float SOLVER_SIZE_MIN = 0.005f;
static const float SOLVER_SIZE_MAX = 1024.0f;
static const float SOLVER_PLANE_THICKNESS = 0.02f;

typedef struct SolverProjectionPhysicsOverlay {
    bool found;
    bool has_motion_mode;
    bool is_static;
} SolverProjectionPhysicsOverlay;

static float clampf_dim(float v, float min_v, float max_v) {
    if (v < min_v) return min_v;
    if (v > max_v) return max_v;
    return v;
}

static PresetObjectType map_object_type(const CoreSceneObjectContract *object) {
    if (!object) return PRESET_OBJECT_BOX;
    if (object->kind == CORE_SCENE_OBJECT_KIND_PLANE_PRIMITIVE ||
        object->kind == CORE_SCENE_OBJECT_KIND_RECT_PRISM_PRIMITIVE) {
        return PRESET_OBJECT_BOX;
    }
    if (strstr(object->object.object_type, "circle") != NULL) return PRESET_OBJECT_CIRCLE;
    return PRESET_OBJECT_BOX;
}

static SpaceMode retained_root_space_mode(const PhysicsSimRetainedRuntimeScene *retained_scene) {
    CoreSceneSpaceMode mode = CORE_SCENE_SPACE_MODE_UNKNOWN;
    if (!retained_scene) return SPACE_MODE_2D;
    mode = retained_scene->root.space_mode_default;
    if (mode == CORE_SCENE_SPACE_MODE_UNKNOWN) {
        mode = retained_scene->root.space_mode_intent;
    }
    return (mode == CORE_SCENE_SPACE_MODE_3D) ? SPACE_MODE_3D : SPACE_MODE_2D;
}

static void solver_projection_apply_space_mode(const PhysicsSimRetainedRuntimeScene *retained_scene,
                                               AppConfig *in_out_cfg,
                                               FluidScenePreset *in_out_preset) {
    SpaceMode mode = retained_root_space_mode(retained_scene);
    if (in_out_cfg) {
        in_out_cfg->space_mode = mode;
    }
    if (in_out_preset) {
        in_out_preset->dimension_mode = (mode == SPACE_MODE_3D)
                                            ? SCENE_DIMENSION_MODE_3D
                                            : SCENE_DIMENSION_MODE_2D;
    }
}

static float scaled_solver_size(double dimension, double world_scale, float fallback) {
    double scaled = dimension * world_scale;
    if (scaled <= 0.0) scaled = fallback;
    return clampf_dim((float)scaled, SOLVER_SIZE_MIN, SOLVER_SIZE_MAX);
}

static float scaled_solver_position(double coord, double world_scale) {
    return clampf_dim((float)(coord * world_scale), -SOLVER_POSITION_LIMIT, SOLVER_POSITION_LIMIT);
}

static bool solver_projection_parse_vec3(json_object *root,
                                         const char *key,
                                         double *out_x,
                                         double *out_y,
                                         double *out_z) {
    json_object *vec = NULL;
    json_object *x = NULL;
    json_object *y = NULL;
    json_object *z = NULL;
    if (!root || !key || !out_x || !out_y || !out_z) return false;
    if (!json_object_object_get_ex(root, key, &vec) || !json_object_is_type(vec, json_type_object)) {
        return false;
    }
    if (!json_object_object_get_ex(vec, "x", &x) ||
        !json_object_object_get_ex(vec, "y", &y) ||
        !json_object_object_get_ex(vec, "z", &z)) {
        return false;
    }
    *out_x = json_object_get_double(x);
    *out_y = json_object_get_double(y);
    *out_z = json_object_get_double(z);
    return true;
}

static bool solver_projection_parse_bool(json_object *root, const char *key, bool *out_value) {
    json_object *node = NULL;
    if (!root || !key || !out_value) return false;
    if (!json_object_object_get_ex(root, key, &node) || !json_object_is_type(node, json_type_boolean)) {
        return false;
    }
    *out_value = json_object_get_boolean(node) ? true : false;
    return true;
}

static bool solver_projection_overlay_for_object(json_object *runtime_root,
                                                 const char *object_id,
                                                 SolverProjectionPhysicsOverlay *out_overlay) {
    json_object *extensions = NULL;
    json_object *physics_sim = NULL;
    json_object *object_overlays = NULL;
    size_t count = 0;
    size_t i = 0;
    if (out_overlay) memset(out_overlay, 0, sizeof(*out_overlay));
    if (!runtime_root || !object_id || !object_id[0] || !out_overlay) return false;

    if (!json_object_object_get_ex(runtime_root, "extensions", &extensions) ||
        !json_object_is_type(extensions, json_type_object)) {
        return false;
    }
    if (!json_object_object_get_ex(extensions, "physics_sim", &physics_sim) ||
        !json_object_is_type(physics_sim, json_type_object)) {
        return false;
    }
    if (!json_object_object_get_ex(physics_sim, "object_overlays", &object_overlays) ||
        !json_object_is_type(object_overlays, json_type_array)) {
        return false;
    }

    count = json_object_array_length(object_overlays);
    for (i = 0; i < count; ++i) {
        json_object *entry = json_object_array_get_idx(object_overlays, i);
        json_object *entry_object_id = NULL;
        json_object *motion_mode = NULL;
        const char *entry_object_id_str = NULL;
        const char *motion_mode_str = NULL;
        if (!entry || !json_object_is_type(entry, json_type_object)) continue;
        if (!json_object_object_get_ex(entry, "object_id", &entry_object_id) ||
            !json_object_is_type(entry_object_id, json_type_string)) {
            continue;
        }
        entry_object_id_str = json_object_get_string(entry_object_id);
        if (!entry_object_id_str || strcmp(entry_object_id_str, object_id) != 0) continue;

        out_overlay->found = true;
        if (json_object_object_get_ex(entry, "motion_mode", &motion_mode) &&
            json_object_is_type(motion_mode, json_type_string)) {
            motion_mode_str = json_object_get_string(motion_mode);
            if (motion_mode_str) {
                out_overlay->has_motion_mode = true;
                out_overlay->is_static = (strcmp(motion_mode_str, "Static") == 0);
            }
        }
        return true;
    }
    return false;
}

static void solver_projection_apply_json_object(json_object *src,
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
        (void)solver_projection_parse_vec3(transform, "position", &px, &py, &pz);
        (void)solver_projection_parse_vec3(transform, "scale", &sx, &sy, &sz);
        (void)solver_projection_parse_vec3(transform, "rotation", &rx, &ry, &rz);
    }

    if (json_object_object_get_ex(src, "flags", &flags) && json_object_is_type(flags, json_type_object)) {
        (void)solver_projection_parse_bool(flags, "locked", &locked);
    }

    dst->position_x = scaled_solver_position(px, world_scale);
    dst->position_y = scaled_solver_position(py, world_scale);
    dst->position_z = scaled_solver_position(pz, world_scale);
    dst->size_x = scaled_solver_size(sx, world_scale, 0.08f);
    dst->size_y = scaled_solver_size(sy, world_scale, 0.08f);
    dst->size_z = scaled_solver_size(sz, world_scale, 0.08f);
    dst->angle = (float)rz;
    dst->is_static = locked;
    dst->gravity_enabled = !locked;
    if (solver_projection_overlay_for_object(runtime_root, object_id_str, &overlay) &&
        overlay.has_motion_mode) {
        dst->is_static = overlay.is_static;
        dst->gravity_enabled = !overlay.is_static;
    }
}

static void solver_projection_apply_object(const CoreSceneObjectContract *object,
                                           json_object *runtime_root,
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
        dst->size_x = scaled_solver_size(object->plane_primitive.width, world_scale, fallback_size);
        dst->size_y = scaled_solver_size(object->plane_primitive.height, world_scale, fallback_size);
        dst->size_z = scaled_solver_size(SOLVER_PLANE_THICKNESS, world_scale, fallback_size);
    } else if (object->has_rect_prism_primitive) {
        position = object->rect_prism_primitive.frame.origin;
        dst->size_x = scaled_solver_size(object->rect_prism_primitive.width, world_scale, fallback_size);
        dst->size_y = scaled_solver_size(object->rect_prism_primitive.height, world_scale, fallback_size);
        dst->size_z = scaled_solver_size(object->rect_prism_primitive.depth, world_scale, fallback_size);
    } else {
        position = object->object.transform.position;
        scale = object->object.transform.scale;
        dst->size_x = scaled_solver_size(scale.x, world_scale, 0.08f);
        dst->size_y = scaled_solver_size(scale.y, world_scale, 0.08f);
        dst->size_z = scaled_solver_size(scale.z, world_scale, 0.08f);
    }

    dst->position_x = scaled_solver_position(position.x, world_scale);
    dst->position_y = scaled_solver_position(position.y, world_scale);
    dst->position_z = scaled_solver_position(position.z, world_scale);
    if (solver_projection_overlay_for_object(runtime_root, object->object.object_id, &overlay) &&
        overlay.has_motion_mode) {
        dst->is_static = overlay.is_static;
        dst->gravity_enabled = !overlay.is_static;
    }
}

static void solver_projection_apply_objects(const PhysicsSimRetainedRuntimeScene *retained_scene,
                                            json_object *runtime_root,
                                            FluidScenePreset *in_out_preset,
                                            RuntimeSceneBridgePreflight *out_summary) {
    int src_count = 0;
    int i = 0;
    double world_scale = 1.0;
    if (!retained_scene || !in_out_preset) return;

    in_out_preset->object_count = 0;
    src_count = retained_scene->retained_object_count;
    if (src_count > MAX_PRESET_OBJECTS) src_count = MAX_PRESET_OBJECTS;
    world_scale = retained_scene->root.world_scale;
    if (world_scale <= 0.0) world_scale = 1.0;

    for (i = 0; i < src_count; ++i) {
        PresetObject *dst = &in_out_preset->objects[in_out_preset->object_count];
        solver_projection_apply_object(&retained_scene->objects[i], runtime_root, world_scale, dst);
        in_out_preset->object_count++;
    }

    if (out_summary) out_summary->object_count = (int)in_out_preset->object_count;
}

static void solver_projection_apply_runtime_root_objects(json_object *runtime_root,
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
        solver_projection_apply_json_object(src, runtime_root, world_scale, dst);
        in_out_preset->object_count++;
    }

    if (out_summary) out_summary->object_count = (int)in_out_preset->object_count;
}

static void solver_projection_apply_emitters_from_lights(json_object *runtime_root,
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

        if (solver_projection_parse_vec3(light, "position", &lx, &ly, &lz)) {
            dst->position_x = scaled_solver_position(lx, world_scale);
            dst->position_y = scaled_solver_position(ly, world_scale);
            dst->position_z = scaled_solver_position(lz, world_scale);
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
        dst->strength = clampf_dim((float)strength, 0.0f, 5000.0f);
        dst->dir_x = 0.0f;
        dst->dir_y = -1.0f;
        dst->dir_z = 0.0f;
        dst->attached_object = -1;
        dst->attached_import = -1;
        in_out_preset->emitter_count++;
    }
}

bool runtime_scene_solver_projection_apply_runtime(const PhysicsSimRetainedRuntimeScene *retained_scene,
                                                   json_object *runtime_root,
                                                   AppConfig *in_out_cfg,
                                                   FluidScenePreset *in_out_preset,
                                                   RuntimeSceneBridgePreflight *out_summary) {
    double world_scale = 1.0;
    if (!retained_scene || !runtime_root || !in_out_cfg || !in_out_preset || !out_summary) return false;
    world_scale = retained_scene->root.world_scale;
    if (world_scale <= 0.0) world_scale = 1.0;

    solver_projection_apply_space_mode(retained_scene, in_out_cfg, in_out_preset);
    if (retained_scene->retained_object_count > 0) {
        solver_projection_apply_objects(retained_scene, runtime_root, in_out_preset, out_summary);
    } else {
        solver_projection_apply_runtime_root_objects(runtime_root,
                                                     world_scale,
                                                     in_out_preset,
                                                     out_summary);
    }
    solver_projection_apply_emitters_from_lights(runtime_root, world_scale, in_out_preset);
    return true;
}
