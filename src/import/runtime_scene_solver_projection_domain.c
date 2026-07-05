#include "import/runtime_scene_solver_projection_internal.h"

#include <string.h>

#include "app/atmospheric/atmospheric_field.h"
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

static FluidEmitter3DSourceMode solver_projection_parse_emitter_source_mode_3d(const char *mode_str) {
    if (!mode_str) return EMITTER_3D_SOURCE_MODE_LEGACY_COMPAT;
    if (strcmp(mode_str, "VolumeFill") == 0) return EMITTER_3D_SOURCE_MODE_VOLUME_FILL;
    if (strcmp(mode_str, "SurfacePatch") == 0) return EMITTER_3D_SOURCE_MODE_SURFACE_PATCH;
    if (strcmp(mode_str, "SurfaceShell") == 0) return EMITTER_3D_SOURCE_MODE_SURFACE_SHELL;
    if (strcmp(mode_str, "HeatedObstacle") == 0) return EMITTER_3D_SOURCE_MODE_HEATED_OBSTACLE;
    if (strcmp(mode_str, "LegacyCompat") == 0) return EMITTER_3D_SOURCE_MODE_LEGACY_COMPAT;
    return EMITTER_3D_SOURCE_MODE_LEGACY_COMPAT;
}

static FluidEmitter3DSurface solver_projection_parse_emitter_surface_3d(const char *surface_str) {
    if (!surface_str) return EMITTER_3D_SURFACE_AUTO;
    if (strcmp(surface_str, "Top") == 0) return EMITTER_3D_SURFACE_TOP;
    if (strcmp(surface_str, "Bottom") == 0) return EMITTER_3D_SURFACE_BOTTOM;
    if (strcmp(surface_str, "Left") == 0) return EMITTER_3D_SURFACE_LEFT;
    if (strcmp(surface_str, "Right") == 0) return EMITTER_3D_SURFACE_RIGHT;
    if (strcmp(surface_str, "Front") == 0) return EMITTER_3D_SURFACE_FRONT;
    if (strcmp(surface_str, "Back") == 0) return EMITTER_3D_SURFACE_BACK;
    if (strcmp(surface_str, "AllFaces") == 0) return EMITTER_3D_SURFACE_ALL_FACES;
    if (strcmp(surface_str, "Auto") == 0) return EMITTER_3D_SURFACE_AUTO;
    return EMITTER_3D_SURFACE_AUTO;
}

static FluidEmitter3DObstacleMode solver_projection_parse_emitter_obstacle_mode_3d(const char *mode_str) {
    if (!mode_str) return EMITTER_3D_OBSTACLE_MODE_AUTO;
    if (strcmp(mode_str, "ClearAttached") == 0) return EMITTER_3D_OBSTACLE_MODE_CLEAR_ATTACHED;
    if (strcmp(mode_str, "RetainAttached") == 0) return EMITTER_3D_OBSTACLE_MODE_RETAIN_ATTACHED;
    if (strcmp(mode_str, "Auto") == 0) return EMITTER_3D_OBSTACLE_MODE_AUTO;
    return EMITTER_3D_OBSTACLE_MODE_AUTO;
}

float runtime_scene_solver_projection_clampf_dim(float v, float min_v, float max_v) {
    if (v < min_v) return min_v;
    if (v > max_v) return max_v;
    return v;
}

float runtime_scene_solver_projection_domain_dimension(
    double extent [[fisics::dim(length)]] [[fisics::unit(meter)]],
    double world_scale,
    float fallback [[fisics::dim(length)]] [[fisics::unit(meter)]]) {
    double scene_extent [[fisics::dim(length)]] [[fisics::unit(meter)]] = extent;
    double scaled_extent [[fisics::dim(length)]] [[fisics::unit(meter)]] = scene_extent * world_scale;
    double zero_length [[fisics::dim(length)]] [[fisics::unit(meter)]] = 0.0;
    float fallback_length [[fisics::dim(length)]] [[fisics::unit(meter)]] = fallback;
    double max_length [[fisics::dim(length)]] [[fisics::unit(meter)]] = 4096.0;

    if (scaled_extent <= zero_length) scaled_extent = fallback_length;
    if (scaled_extent > max_length) scaled_extent = max_length;
    return (float)scaled_extent;
}

float runtime_scene_solver_projection_scaled_size(
    double dimension [[fisics::dim(length)]] [[fisics::unit(meter)]],
    double world_scale,
    float fallback [[fisics::dim(length)]] [[fisics::unit(meter)]]) {
    double scene_dimension [[fisics::dim(length)]] [[fisics::unit(meter)]] = dimension;
    double scaled_dimension [[fisics::dim(length)]] [[fisics::unit(meter)]] = scene_dimension * world_scale;
    double zero_length [[fisics::dim(length)]] [[fisics::unit(meter)]] = 0.0;
    float fallback_length [[fisics::dim(length)]] [[fisics::unit(meter)]] = fallback;
    float min_length [[fisics::dim(length)]] [[fisics::unit(meter)]] = SOLVER_SIZE_MIN;
    float max_length [[fisics::dim(length)]] [[fisics::unit(meter)]] = SOLVER_SIZE_MAX;

    if (scaled_dimension <= zero_length) scaled_dimension = fallback_length;
    return runtime_scene_solver_projection_clampf_dim((float)scaled_dimension, min_length, max_length);
}

float runtime_scene_solver_projection_scaled_position(
    double coord [[fisics::dim(length)]] [[fisics::unit(meter)]],
    double world_scale) {
    double scene_coord [[fisics::dim(length)]] [[fisics::unit(meter)]] = coord;
    double scaled_coord [[fisics::dim(length)]] [[fisics::unit(meter)]] = scene_coord * world_scale;
    float min_position [[fisics::dim(length)]] [[fisics::unit(meter)]] = -SOLVER_POSITION_LIMIT;
    float max_position [[fisics::dim(length)]] [[fisics::unit(meter)]] = SOLVER_POSITION_LIMIT;

    return runtime_scene_solver_projection_clampf_dim((float)scaled_coord,
                                                      min_position,
                                                      max_position);
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

static bool runtime_scene_solver_projection_parse_number(json_object *root,
                                                         const char *key,
                                                         double *out_value) {
    json_object *node = NULL;
    if (!root || !key || !out_value) return false;
    if (!json_object_object_get_ex(root, key, &node) ||
        (!json_object_is_type(node, json_type_double) &&
         !json_object_is_type(node, json_type_int))) {
        return false;
    }
    *out_value = json_object_get_double(node);
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

bool runtime_scene_solver_projection_overlay_wind_tunnel(json_object *runtime_root,
                                                         const AppConfig *base_cfg,
                                                         WindTunnel3DConfig *out_config) {
    json_object *extensions = NULL;
    json_object *physics_sim = NULL;
    json_object *wind_tunnel = NULL;
    json_object *node = NULL;
    WindTunnel3DConfig config = wind_tunnel_3d_config_default(base_cfg);
    double number = 0.0;
    if (out_config) memset(out_config, 0, sizeof(*out_config));
    if (!runtime_root || !out_config) return false;
    if (!json_object_object_get_ex(runtime_root, "extensions", &extensions) ||
        !json_object_is_type(extensions, json_type_object)) {
        return false;
    }
    if (!json_object_object_get_ex(extensions, "physics_sim", &physics_sim) ||
        !json_object_is_type(physics_sim, json_type_object)) {
        return false;
    }
    if (!json_object_object_get_ex(physics_sim, "wind_tunnel", &wind_tunnel) ||
        !json_object_is_type(wind_tunnel, json_type_object)) {
        return false;
    }

    (void)runtime_scene_solver_projection_parse_bool(wind_tunnel, "active", &config.active);
    if (!config.active) {
        *out_config = config;
        return true;
    }
    if (json_object_object_get_ex(wind_tunnel, "inlet_face", &node) &&
        json_object_is_type(node, json_type_string)) {
        (void)wind_tunnel_3d_face_from_label(json_object_get_string(node), &config.inlet_face);
    }
    if (json_object_object_get_ex(wind_tunnel, "outlet_face", &node) &&
        json_object_is_type(node, json_type_string)) {
        (void)wind_tunnel_3d_face_from_label(json_object_get_string(node), &config.outlet_face);
    }
    if (runtime_scene_solver_projection_parse_number(wind_tunnel, "inflow_speed", &number)) {
        config.inflow_speed = (float)number;
    }
    if (runtime_scene_solver_projection_parse_number(wind_tunnel, "inflow_density", &number)) {
        config.inflow_density = (float)number;
    }
    if (runtime_scene_solver_projection_parse_number(wind_tunnel, "inlet_slab_cells", &number)) {
        config.inlet_slab_cells = (int)number;
    }
    if (json_object_object_get_ex(wind_tunnel, "outlet_policy", &node) &&
        json_object_is_type(node, json_type_string)) {
        (void)wind_tunnel_3d_outlet_policy_from_label(json_object_get_string(node),
                                                      &config.outlet_policy);
    }
    if (json_object_object_get_ex(wind_tunnel, "wall_policy", &node) &&
        json_object_is_type(node, json_type_string)) {
        (void)wind_tunnel_3d_wall_policy_from_label(json_object_get_string(node),
                                                    &config.wall_policy);
    }
    if (!wind_tunnel_3d_config_validate(&config)) return false;
    *out_config = config;
    return true;
}

static bool atmosphere_region_shape_from_label(const char *label,
                                               AtmosphericRegionShape *out_shape) {
    if (!label || !out_shape) return false;
    if (strcmp(label, "rect") == 0 ||
        strcmp(label, "rectangle") == 0 ||
        strcmp(label, "box") == 0) {
        *out_shape = ATMOSPHERIC_REGION_RECT;
        return true;
    }
    if (strcmp(label, "ellipse") == 0 ||
        strcmp(label, "ellipsoid") == 0 ||
        strcmp(label, "sphere") == 0) {
        *out_shape = ATMOSPHERIC_REGION_ELLIPSE;
        return true;
    }
    return false;
}

bool runtime_scene_solver_projection_overlay_atmosphere(json_object *runtime_root,
                                                        AtmosphericPresetSettings *out_settings) {
    json_object *extensions = NULL;
    json_object *physics_sim = NULL;
    json_object *atmosphere = NULL;
    json_object *regions = NULL;
    json_object *node = NULL;
    double number = 0.0;
    AtmosphericPresetSettings settings = atmospheric_preset_default_settings();

    if (out_settings) memset(out_settings, 0, sizeof(*out_settings));
    if (!runtime_root || !out_settings) return false;
    if (!json_object_object_get_ex(runtime_root, "extensions", &extensions) ||
        !json_object_is_type(extensions, json_type_object)) {
        return false;
    }
    if (!json_object_object_get_ex(extensions, "physics_sim", &physics_sim) ||
        !json_object_is_type(physics_sim, json_type_object)) {
        return false;
    }
    if (!json_object_object_get_ex(physics_sim, "atmosphere", &atmosphere) ||
        !json_object_is_type(atmosphere, json_type_object)) {
        return false;
    }

    (void)runtime_scene_solver_projection_parse_bool(atmosphere, "enabled", &settings.enabled);
    if (runtime_scene_solver_projection_parse_number(atmosphere, "seed", &number)) {
        settings.seed = (uint32_t)number;
    }
    if (runtime_scene_solver_projection_parse_number(atmosphere, "base_density", &number)) {
        settings.base_density = (float)number;
    }
    if (runtime_scene_solver_projection_parse_number(atmosphere, "density_scale", &number)) {
        settings.density_scale = (float)number;
    }
    if (runtime_scene_solver_projection_parse_number(atmosphere, "density_threshold", &number)) {
        settings.density_threshold = (float)number;
    }
    if (runtime_scene_solver_projection_parse_number(atmosphere, "base_wind_x", &number)) {
        settings.base_wind_x = (float)number;
    }
    if (runtime_scene_solver_projection_parse_number(atmosphere, "base_wind_y", &number)) {
        settings.base_wind_y = (float)number;
    }
    if (runtime_scene_solver_projection_parse_number(atmosphere, "base_wind_z", &number)) {
        settings.base_wind_z = (float)number;
    }
    if (runtime_scene_solver_projection_parse_number(atmosphere, "turbulence_strength", &number)) {
        settings.turbulence_strength = (float)number;
    }
    if (runtime_scene_solver_projection_parse_number(atmosphere, "noise_scale", &number)) {
        settings.noise_scale = (float)number;
    }
    if (runtime_scene_solver_projection_parse_number(atmosphere, "detail_scale", &number)) {
        settings.detail_scale = (float)number;
    }
    if (runtime_scene_solver_projection_parse_number(atmosphere, "band_min_y", &number)) {
        settings.band_min_y = (float)number;
    }
    if (runtime_scene_solver_projection_parse_number(atmosphere, "band_max_y", &number)) {
        settings.band_max_y = (float)number;
    }
    if (runtime_scene_solver_projection_parse_number(atmosphere, "band_edge_falloff", &number)) {
        settings.band_edge_falloff = (float)number;
    }

    if (json_object_object_get_ex(atmosphere, "regions", &regions) &&
        json_object_is_type(regions, json_type_array)) {
        size_t count = json_object_array_length(regions);
        if (count > MAX_ATMOSPHERIC_DENSITY_REGIONS) count = MAX_ATMOSPHERIC_DENSITY_REGIONS;
        settings.region_count = count;
        for (size_t i = 0; i < count; ++i) {
            json_object *region = json_object_array_get_idx(regions, i);
            AtmosphericDensityRegion *dst = &settings.regions[i];
            if (!region || !json_object_is_type(region, json_type_object)) {
                memset(dst, 0, sizeof(*dst));
                continue;
            }
            (void)runtime_scene_solver_projection_parse_bool(region, "enabled", &dst->enabled);
            if (json_object_object_get_ex(region, "shape", &node) &&
                json_object_is_type(node, json_type_string)) {
                (void)atmosphere_region_shape_from_label(json_object_get_string(node), &dst->shape);
            }
            if (runtime_scene_solver_projection_parse_number(region, "center_x", &number)) {
                dst->center_x = (float)number;
            }
            if (runtime_scene_solver_projection_parse_number(region, "center_y", &number)) {
                dst->center_y = (float)number;
            }
            if (runtime_scene_solver_projection_parse_number(region, "center_z", &number)) {
                dst->center_z = (float)number;
            }
            if (runtime_scene_solver_projection_parse_number(region, "size_x", &number)) {
                dst->size_x = (float)number;
            }
            if (runtime_scene_solver_projection_parse_number(region, "size_y", &number)) {
                dst->size_y = (float)number;
            }
            if (runtime_scene_solver_projection_parse_number(region, "size_z", &number)) {
                dst->size_z = (float)number;
            }
            if (runtime_scene_solver_projection_parse_number(region, "density", &number)) {
                dst->density = (float)number;
            }
            if (runtime_scene_solver_projection_parse_number(region, "falloff", &number)) {
                dst->falloff = (float)number;
            }
        }
    }

    atmospheric_preset_sanitize(&settings);
    *out_settings = settings;
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
        out_mapping->min_z = authored.min_z;
        out_mapping->max_x = authored.max_x;
        out_mapping->max_y = authored.max_y;
        out_mapping->max_z = authored.max_z;
    } else if (retained_scene &&
               retained_scene->has_line_drawing_scene3d &&
               retained_scene->bounds.enabled &&
               retained_scene->bounds.max.x > retained_scene->bounds.min.x &&
               retained_scene->bounds.max.y > retained_scene->bounds.min.y) {
        out_mapping->valid = true;
        out_mapping->min_x = retained_scene->bounds.min.x;
        out_mapping->min_y = retained_scene->bounds.min.y;
        out_mapping->min_z = retained_scene->bounds.min.z;
        out_mapping->max_x = retained_scene->bounds.max.x;
        out_mapping->max_y = retained_scene->bounds.max.y;
        out_mapping->max_z = retained_scene->bounds.max.z;
    }

    if (!out_mapping->valid) return;
    out_mapping->span_x = out_mapping->max_x - out_mapping->min_x;
    out_mapping->span_y = out_mapping->max_y - out_mapping->min_y;
    out_mapping->span_z = out_mapping->max_z - out_mapping->min_z;
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
            json_object *mode_3d = NULL;
            json_object *surface_3d = NULL;
            json_object *obstacle_mode_3d = NULL;
            json_object *thermal_buoyancy_3d = NULL;
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
                        out_overlay->has_emitter_direction = true;
                    }
                    if (json_object_object_get_ex(direction, "y", &vy) &&
                        (json_object_is_type(vy, json_type_double) || json_object_is_type(vy, json_type_int))) {
                        out_overlay->emitter_dir_y = json_object_get_double(vy);
                        out_overlay->has_emitter_direction = true;
                    }
                    if (json_object_object_get_ex(direction, "z", &vz) &&
                        (json_object_is_type(vz, json_type_double) || json_object_is_type(vz, json_type_int))) {
                        out_overlay->emitter_dir_z = json_object_get_double(vz);
                        out_overlay->has_emitter_direction = true;
                    }
                }
                if (json_object_object_get_ex(emitter, "mode_3d", &mode_3d) &&
                    json_object_is_type(mode_3d, json_type_string)) {
                    out_overlay->emitter_source_mode_3d =
                        solver_projection_parse_emitter_source_mode_3d(json_object_get_string(mode_3d));
                    out_overlay->has_emitter_source_mode_3d = true;
                }
                if (json_object_object_get_ex(emitter, "surface_3d", &surface_3d) &&
                    json_object_is_type(surface_3d, json_type_string)) {
                    out_overlay->emitter_surface_3d =
                        solver_projection_parse_emitter_surface_3d(json_object_get_string(surface_3d));
                    out_overlay->has_emitter_surface_3d = true;
                }
                if (json_object_object_get_ex(emitter, "obstacle_mode_3d", &obstacle_mode_3d) &&
                    json_object_is_type(obstacle_mode_3d, json_type_string)) {
                    out_overlay->emitter_obstacle_mode_3d =
                        solver_projection_parse_emitter_obstacle_mode_3d(
                            json_object_get_string(obstacle_mode_3d));
                    out_overlay->has_emitter_obstacle_mode_3d = true;
                }
                if (json_object_object_get_ex(emitter, "thermal_buoyancy_3d", &thermal_buoyancy_3d) &&
                    (json_object_is_type(thermal_buoyancy_3d, json_type_double) ||
                     json_object_is_type(thermal_buoyancy_3d, json_type_int))) {
                    out_overlay->emitter_thermal_buoyancy_3d =
                        json_object_get_double(thermal_buoyancy_3d);
                    out_overlay->has_emitter_thermal_buoyancy_3d = true;
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

void runtime_scene_solver_projection_apply_atmosphere(json_object *runtime_root,
                                                      AppConfig *in_out_cfg,
                                                      FluidScenePreset *in_out_preset) {
    AtmosphericPresetSettings settings = {0};
    if (!runtime_root || !in_out_cfg || !in_out_preset) return;
    if (!runtime_scene_solver_projection_overlay_atmosphere(runtime_root, &settings)) return;
    if (!settings.enabled) return;

    in_out_cfg->sim_mode = SIM_MODE_ATMOSPHERIC;
    in_out_cfg->space_mode = SPACE_MODE_3D;
    in_out_preset->dimension_mode = SCENE_DIMENSION_MODE_3D;
    in_out_preset->domain = SCENE_DOMAIN_ATMOSPHERIC;
    in_out_preset->atmospheric_initial_state_enabled = true;
    in_out_preset->atmosphere = settings;
}

void runtime_scene_solver_projection_apply_wind_tunnel(json_object *runtime_root,
                                                       AppConfig *in_out_cfg,
                                                       FluidScenePreset *in_out_preset) {
    WindTunnel3DConfig config = {0};
    if (!runtime_root || !in_out_cfg || !in_out_preset) return;
    if (!runtime_scene_solver_projection_overlay_wind_tunnel(runtime_root, in_out_cfg, &config)) {
        return;
    }
    if (!config.active) return;
    in_out_cfg->sim_mode = SIM_MODE_WIND_TUNNEL;
    in_out_cfg->space_mode = SPACE_MODE_3D;
    in_out_cfg->tunnel_inflow_speed = config.inflow_speed;
    in_out_cfg->tunnel_inflow_density = config.inflow_density;
    in_out_preset->dimension_mode = SCENE_DIMENSION_MODE_3D;
    in_out_preset->domain = SCENE_DOMAIN_WIND_TUNNEL;
}
