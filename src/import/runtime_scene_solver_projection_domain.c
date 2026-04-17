#include "import/runtime_scene_solver_projection_internal.h"

#include <string.h>

#include "app/sim_runtime_3d_space.h"

static const float SOLVER_POSITION_LIMIT = 1024.0f;
static const float SOLVER_SIZE_MIN = 0.005f;
static const float SOLVER_SIZE_MAX = 1024.0f;

static SpaceMode retained_root_space_mode(const PhysicsSimRetainedRuntimeScene *retained_scene) {
    CoreSceneSpaceMode mode = CORE_SCENE_SPACE_MODE_UNKNOWN;
    if (!retained_scene) return SPACE_MODE_2D;
    mode = retained_scene->root.space_mode_default;
    if (mode == CORE_SCENE_SPACE_MODE_UNKNOWN) {
        mode = retained_scene->root.space_mode_intent;
    }
    return (mode == CORE_SCENE_SPACE_MODE_3D) ? SPACE_MODE_3D : SPACE_MODE_2D;
}

static FluidEmitterType solver_projection_parse_emitter_type(const char *type_str) {
    if (!type_str) return EMITTER_DENSITY_SOURCE;
    if (strcmp(type_str, "Jet") == 0) return EMITTER_VELOCITY_JET;
    if (strcmp(type_str, "Sink") == 0) return EMITTER_SINK;
    return EMITTER_DENSITY_SOURCE;
}

float runtime_scene_solver_projection_clampf_dim(float v, float min_v, float max_v) {
    if (v < min_v) return min_v;
    if (v > max_v) return max_v;
    return v;
}

float runtime_scene_solver_projection_domain_dimension(double extent, double world_scale, float fallback) {
    double scaled = extent * world_scale;
    if (scaled <= 0.0) scaled = fallback;
    if (scaled > 4096.0) scaled = 4096.0;
    return (float)scaled;
}

float runtime_scene_solver_projection_scaled_size(double dimension, double world_scale, float fallback) {
    double scaled = dimension * world_scale;
    if (scaled <= 0.0) scaled = fallback;
    return runtime_scene_solver_projection_clampf_dim((float)scaled, SOLVER_SIZE_MIN, SOLVER_SIZE_MAX);
}

float runtime_scene_solver_projection_scaled_position(double coord, double world_scale) {
    return runtime_scene_solver_projection_clampf_dim((float)(coord * world_scale),
                                                      -SOLVER_POSITION_LIMIT,
                                                      SOLVER_POSITION_LIMIT);
}

float runtime_scene_solver_projection_normalize_velocity(double value, double span) {
    if (!(span > 0.0)) return (float)value;
    return (float)(value / span);
}

bool runtime_scene_solver_projection_parse_vec3(json_object *root,
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

bool runtime_scene_solver_projection_parse_bool(json_object *root,
                                                const char *key,
                                                bool *out_value) {
    json_object *node = NULL;
    if (!root || !key || !out_value) return false;
    if (!json_object_object_get_ex(root, key, &node) || !json_object_is_type(node, json_type_boolean)) {
        return false;
    }
    *out_value = json_object_get_boolean(node) ? true : false;
    return true;
}

void runtime_scene_solver_projection_apply_space_mode(const PhysicsSimRetainedRuntimeScene *retained_scene,
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

bool runtime_scene_solver_projection_overlay_scene_domain(json_object *runtime_root,
                                                          SolverProjectionSceneDomain *out_domain) {
    json_object *extensions = NULL;
    json_object *physics_sim = NULL;
    json_object *scene_domain = NULL;
    json_object *shape = NULL;
    const char *shape_str = NULL;
    if (out_domain) memset(out_domain, 0, sizeof(*out_domain));
    if (!runtime_root || !out_domain) return false;

    if (!json_object_object_get_ex(runtime_root, "extensions", &extensions) ||
        !json_object_is_type(extensions, json_type_object)) {
        return false;
    }
    if (!json_object_object_get_ex(extensions, "physics_sim", &physics_sim) ||
        !json_object_is_type(physics_sim, json_type_object)) {
        return false;
    }
    if (!json_object_object_get_ex(physics_sim, "scene_domain", &scene_domain) ||
        !json_object_is_type(scene_domain, json_type_object)) {
        return false;
    }

    out_domain->found = true;
    (void)runtime_scene_solver_projection_parse_bool(scene_domain, "active", &out_domain->active);
    if (!out_domain->active) return true;

    if (json_object_object_get_ex(scene_domain, "shape", &shape) &&
        json_object_is_type(shape, json_type_string)) {
        shape_str = json_object_get_string(shape);
        if (shape_str && strcmp(shape_str, "box") != 0) {
            out_domain->active = false;
            return true;
        }
    }

    if (!runtime_scene_solver_projection_parse_vec3(scene_domain,
                                                    "min",
                                                    &out_domain->min_x,
                                                    &out_domain->min_y,
                                                    &out_domain->min_z) ||
        !runtime_scene_solver_projection_parse_vec3(scene_domain,
                                                    "max",
                                                    &out_domain->max_x,
                                                    &out_domain->max_y,
                                                    &out_domain->max_z)) {
        out_domain->active = false;
        return true;
    }

    return true;
}

void runtime_scene_solver_projection_resolve_xy_domain_mapping(
    const PhysicsSimRetainedRuntimeScene *retained_scene,
    json_object *runtime_root,
    SolverProjectionXYDomainMapping *out_mapping) {
    SolverProjectionSceneDomain authored = {0};
    if (!out_mapping) return;
    memset(out_mapping, 0, sizeof(*out_mapping));

    if (runtime_scene_solver_projection_overlay_scene_domain(runtime_root, &authored) &&
        authored.found && authored.active &&
        authored.max_x > authored.min_x &&
        authored.max_y > authored.min_y) {
        out_mapping->valid = true;
        out_mapping->min_x = authored.min_x;
        out_mapping->min_y = authored.min_y;
        out_mapping->max_x = authored.max_x;
        out_mapping->max_y = authored.max_y;
    } else if (retained_scene &&
               retained_scene->has_line_drawing_scene3d &&
               retained_scene->bounds.enabled &&
               retained_scene->bounds.max.x > retained_scene->bounds.min.x &&
               retained_scene->bounds.max.y > retained_scene->bounds.min.y) {
        out_mapping->valid = true;
        out_mapping->min_x = retained_scene->bounds.min.x;
        out_mapping->min_y = retained_scene->bounds.min.y;
        out_mapping->max_x = retained_scene->bounds.max.x;
        out_mapping->max_y = retained_scene->bounds.max.y;
    }

    if (!out_mapping->valid) return;
    out_mapping->span_x = out_mapping->max_x - out_mapping->min_x;
    out_mapping->span_y = out_mapping->max_y - out_mapping->min_y;
    if (out_mapping->span_x <= 0.0 || out_mapping->span_y <= 0.0) {
        memset(out_mapping, 0, sizeof(*out_mapping));
    }
}

bool runtime_scene_solver_projection_overlay_for_object(json_object *runtime_root,
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
        json_object *initial_velocity = NULL;
        json_object *emitter = NULL;
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
        if (json_object_object_get_ex(entry, "initial_velocity", &initial_velocity) &&
            json_object_is_type(initial_velocity, json_type_object)) {
            json_object *vx = NULL;
            json_object *vy = NULL;
            json_object *vz = NULL;
            bool any = false;
            if (json_object_object_get_ex(initial_velocity, "x", &vx) &&
                (json_object_is_type(vx, json_type_double) || json_object_is_type(vx, json_type_int))) {
                out_overlay->initial_velocity_x = json_object_get_double(vx);
                any = true;
            }
            if (json_object_object_get_ex(initial_velocity, "y", &vy) &&
                (json_object_is_type(vy, json_type_double) || json_object_is_type(vy, json_type_int))) {
                out_overlay->initial_velocity_y = json_object_get_double(vy);
                any = true;
            }
            if (json_object_object_get_ex(initial_velocity, "z", &vz) &&
                (json_object_is_type(vz, json_type_double) || json_object_is_type(vz, json_type_int))) {
                out_overlay->initial_velocity_z = json_object_get_double(vz);
                any = true;
            }
            out_overlay->has_initial_velocity = any;
        }
        if (json_object_object_get_ex(entry, "emitter", &emitter) &&
            json_object_is_type(emitter, json_type_object)) {
            json_object *type = NULL;
            json_object *radius = NULL;
            json_object *strength = NULL;
            json_object *direction = NULL;
            bool emitter_active = false;
            if (runtime_scene_solver_projection_parse_bool(emitter, "active", &emitter_active) &&
                emitter_active) {
                out_overlay->has_emitter = true;
                if (json_object_object_get_ex(emitter, "type", &type) &&
                    json_object_is_type(type, json_type_string)) {
                    out_overlay->emitter_type =
                        solver_projection_parse_emitter_type(json_object_get_string(type));
                } else {
                    out_overlay->emitter_type = EMITTER_DENSITY_SOURCE;
                }
                if (json_object_object_get_ex(emitter, "radius", &radius) &&
                    (json_object_is_type(radius, json_type_double) || json_object_is_type(radius, json_type_int))) {
                    out_overlay->emitter_radius = json_object_get_double(radius);
                }
                if (json_object_object_get_ex(emitter, "strength", &strength) &&
                    (json_object_is_type(strength, json_type_double) || json_object_is_type(strength, json_type_int))) {
                    out_overlay->emitter_strength = json_object_get_double(strength);
                }
                if (json_object_object_get_ex(emitter, "direction", &direction) &&
                    json_object_is_type(direction, json_type_object)) {
                    json_object *vx = NULL;
                    json_object *vy = NULL;
                    json_object *vz = NULL;
                    if (json_object_object_get_ex(direction, "x", &vx) &&
                        (json_object_is_type(vx, json_type_double) || json_object_is_type(vx, json_type_int))) {
                        out_overlay->emitter_dir_x = json_object_get_double(vx);
                    }
                    if (json_object_object_get_ex(direction, "y", &vy) &&
                        (json_object_is_type(vy, json_type_double) || json_object_is_type(vy, json_type_int))) {
                        out_overlay->emitter_dir_y = json_object_get_double(vy);
                    }
                    if (json_object_object_get_ex(direction, "z", &vz) &&
                        (json_object_is_type(vz, json_type_double) || json_object_is_type(vz, json_type_int))) {
                        out_overlay->emitter_dir_z = json_object_get_double(vz);
                    }
                }
            }
        }
        return true;
    }
    return false;
}

void runtime_scene_solver_projection_apply_scene_domain(
    const PhysicsSimRetainedRuntimeScene *retained_scene,
    json_object *runtime_root,
    FluidScenePreset *in_out_preset) {
    SolverProjectionSceneDomain authored = {0};
    double world_scale = 1.0;
    double min_x = 0.0;
    double min_y = 0.0;
    double max_x = 0.0;
    double max_y = 0.0;
    bool have_domain = false;
    if (!in_out_preset) return;

    if (retained_scene && retained_scene->root.world_scale > 0.0) {
        world_scale = retained_scene->root.world_scale;
    }

    if (runtime_scene_solver_projection_overlay_scene_domain(runtime_root, &authored) &&
        authored.found && authored.active &&
        authored.max_x > authored.min_x &&
        authored.max_y > authored.min_y) {
        min_x = authored.min_x;
        min_y = authored.min_y;
        max_x = authored.max_x;
        max_y = authored.max_y;
        have_domain = true;
    } else if (retained_scene &&
               retained_scene->has_line_drawing_scene3d &&
               retained_scene->bounds.enabled &&
               retained_scene->bounds.max.x > retained_scene->bounds.min.x &&
               retained_scene->bounds.max.y > retained_scene->bounds.min.y) {
        min_x = retained_scene->bounds.min.x;
        min_y = retained_scene->bounds.min.y;
        max_x = retained_scene->bounds.max.x;
        max_y = retained_scene->bounds.max.y;
        have_domain = true;
    }

    if (!have_domain) return;

    in_out_preset->domain = SCENE_DOMAIN_STRUCTURAL;
    in_out_preset->domain_width =
        runtime_scene_solver_projection_domain_dimension(max_x - min_x, world_scale, 1.0f);
    in_out_preset->domain_height =
        runtime_scene_solver_projection_domain_dimension(max_y - min_y, world_scale, 1.0f);
}
