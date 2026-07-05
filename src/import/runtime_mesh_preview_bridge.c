#include "import/runtime_mesh_preview_bridge.h"

#include <json-c/json.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "core_scene.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static void mesh_preview_diag(char *out_diagnostics,
                              size_t out_diagnostics_size,
                              const char *message) {
    if (!out_diagnostics || out_diagnostics_size == 0u || !message) return;
    snprintf(out_diagnostics, out_diagnostics_size, "%s", message);
}

static void mesh_preview_set_diag(PhysicsSimRuntimeMeshPreviewSet *set,
                                  const char *message) {
    if (!set || !message) return;
    snprintf(set->diagnostics, sizeof(set->diagnostics), "%s", message);
}

void physics_sim_runtime_mesh_preview_set_init(PhysicsSimRuntimeMeshPreviewSet *set) {
    if (!set) return;
    memset(set, 0, sizeof(*set));
}

static char *read_text_file_alloc(const char *path) {
    FILE *f = NULL;
    long size = 0;
    char *text = NULL;
    size_t read_count = 0u;
    if (!path || !path[0]) return NULL;
    f = fopen(path, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }
    size = ftell(f);
    if (size < 0) {
        fclose(f);
        return NULL;
    }
    if (fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return NULL;
    }
    text = (char *)malloc((size_t)size + 1u);
    if (!text) {
        fclose(f);
        return NULL;
    }
    read_count = fread(text, 1u, (size_t)size, f);
    fclose(f);
    if (read_count != (size_t)size) {
        free(text);
        return NULL;
    }
    text[size] = '\0';
    return text;
}

static bool path_exists(const char *path) {
    struct stat st;
    return path && path[0] && stat(path, &st) == 0;
}

static const char *string_field(json_object *obj, const char *key) {
    json_object *value = NULL;
    if (!obj || !json_object_is_type(obj, json_type_object) || !key) return NULL;
    if (!json_object_object_get_ex(obj, key, &value) ||
        !json_object_is_type(value, json_type_string)) {
        return NULL;
    }
    return json_object_get_string(value);
}

static bool number_field(json_object *obj, const char *key, double *out_value) {
    json_object *value = NULL;
    if (!obj || !json_object_is_type(obj, json_type_object) || !key || !out_value) return false;
    if (!json_object_object_get_ex(obj, key, &value) ||
        (!json_object_is_type(value, json_type_double) &&
         !json_object_is_type(value, json_type_int))) {
        return false;
    }
    *out_value = json_object_get_double(value);
    return true;
}

static bool bool_field(json_object *obj, const char *key, bool *out_value) {
    json_object *value = NULL;
    if (!obj || !json_object_is_type(obj, json_type_object) || !key || !out_value) return false;
    if (!json_object_object_get_ex(obj, key, &value) ||
        !json_object_is_type(value, json_type_boolean)) {
        return false;
    }
    *out_value = json_object_get_boolean(value) ? true : false;
    return true;
}

static bool vec3_field(json_object *obj, const char *key, CoreObjectVec3 *out_value) {
    json_object *value = NULL;
    if (!obj || !json_object_is_type(obj, json_type_object) || !key || !out_value) return false;
    if (!json_object_object_get_ex(obj, key, &value) ||
        !json_object_is_type(value, json_type_object)) {
        return false;
    }
    return number_field(value, "x", &out_value->x) &&
           number_field(value, "y", &out_value->y) &&
           number_field(value, "z", &out_value->z);
}

static FluidEmitterType parse_emitter_type(const char *type_str) {
    if (!type_str) return EMITTER_DENSITY_SOURCE;
    if (strcmp(type_str, "Jet") == 0 || strcmp(type_str, "velocity_jet") == 0 ||
        strcmp(type_str, "boundary_flow") == 0) {
        return EMITTER_VELOCITY_JET;
    }
    if (strcmp(type_str, "Sink") == 0 || strcmp(type_str, "sink") == 0) {
        return EMITTER_SINK;
    }
    return EMITTER_DENSITY_SOURCE;
}

static FluidEmitter3DSourceMode parse_emitter_source_mode_3d(const char *mode_str) {
    if (!mode_str) return EMITTER_3D_SOURCE_MODE_SURFACE_SHELL;
    if (strcmp(mode_str, "VolumeFill") == 0 || strcmp(mode_str, "volume_fill") == 0) {
        return EMITTER_3D_SOURCE_MODE_VOLUME_FILL;
    }
    if (strcmp(mode_str, "SurfacePatch") == 0 || strcmp(mode_str, "surface_patch") == 0) {
        return EMITTER_3D_SOURCE_MODE_SURFACE_PATCH;
    }
    if (strcmp(mode_str, "SurfaceShell") == 0 || strcmp(mode_str, "surface_shell") == 0) {
        return EMITTER_3D_SOURCE_MODE_SURFACE_SHELL;
    }
    if (strcmp(mode_str, "HeatedObstacle") == 0 ||
        strcmp(mode_str, "heated_obstacle") == 0 ||
        strcmp(mode_str, "surface_heat") == 0) {
        return EMITTER_3D_SOURCE_MODE_HEATED_OBSTACLE;
    }
    if (strcmp(mode_str, "LegacyCompat") == 0 || strcmp(mode_str, "legacy_compat") == 0) {
        return EMITTER_3D_SOURCE_MODE_LEGACY_COMPAT;
    }
    return EMITTER_3D_SOURCE_MODE_SURFACE_SHELL;
}

static FluidEmitter3DSurface parse_emitter_surface_3d(const char *surface_str) {
    if (!surface_str) return EMITTER_3D_SURFACE_ALL_FACES;
    if (strcmp(surface_str, "Top") == 0 || strcmp(surface_str, "top") == 0) return EMITTER_3D_SURFACE_TOP;
    if (strcmp(surface_str, "Bottom") == 0 || strcmp(surface_str, "bottom") == 0) return EMITTER_3D_SURFACE_BOTTOM;
    if (strcmp(surface_str, "Left") == 0 || strcmp(surface_str, "left") == 0) return EMITTER_3D_SURFACE_LEFT;
    if (strcmp(surface_str, "Right") == 0 || strcmp(surface_str, "right") == 0) return EMITTER_3D_SURFACE_RIGHT;
    if (strcmp(surface_str, "Front") == 0 || strcmp(surface_str, "front") == 0) return EMITTER_3D_SURFACE_FRONT;
    if (strcmp(surface_str, "Back") == 0 || strcmp(surface_str, "back") == 0) return EMITTER_3D_SURFACE_BACK;
    if (strcmp(surface_str, "Auto") == 0 || strcmp(surface_str, "auto") == 0) return EMITTER_3D_SURFACE_AUTO;
    return EMITTER_3D_SURFACE_ALL_FACES;
}

static FluidEmitter3DObstacleMode parse_emitter_obstacle_mode_3d(const char *mode_str) {
    if (!mode_str) return EMITTER_3D_OBSTACLE_MODE_CLEAR_ATTACHED;
    if (strcmp(mode_str, "RetainAttached") == 0 || strcmp(mode_str, "retain_attached") == 0) {
        return EMITTER_3D_OBSTACLE_MODE_RETAIN_ATTACHED;
    }
    if (strcmp(mode_str, "Auto") == 0 || strcmp(mode_str, "auto") == 0) {
        return EMITTER_3D_OBSTACLE_MODE_AUTO;
    }
    return EMITTER_3D_OBSTACLE_MODE_CLEAR_ATTACHED;
}

static CoreObjectVec3 rotate_xyz_degrees(CoreObjectVec3 value, CoreObjectVec3 rotation_deg) {
    const double rx = rotation_deg.x * M_PI / 180.0;
    const double ry = rotation_deg.y * M_PI / 180.0;
    const double rz = rotation_deg.z * M_PI / 180.0;
    const double cx = cos(rx);
    const double sx = sin(rx);
    const double cy = cos(ry);
    const double sy = sin(ry);
    const double cz = cos(rz);
    const double sz = sin(rz);
    CoreObjectVec3 r = value;
    CoreObjectVec3 tmp = r;

    tmp.y = r.y * cx - r.z * sx;
    tmp.z = r.y * sx + r.z * cx;
    r = tmp;

    tmp = r;
    tmp.x = r.x * cy + r.z * sy;
    tmp.z = -r.x * sy + r.z * cy;
    r = tmp;

    tmp = r;
    tmp.x = r.x * cz - r.y * sz;
    tmp.y = r.x * sz + r.y * cz;
    return tmp;
}

static bool vec3_finite(CoreObjectVec3 value) {
    return isfinite(value.x) && isfinite(value.y) && isfinite(value.z);
}

static bool preview_bounds_valid(const CoreMeshPreviewRuntimeMetadata *metadata) {
    return metadata &&
           vec3_finite(metadata->local_bounds.min) &&
           vec3_finite(metadata->local_bounds.max) &&
           metadata->local_bounds.max.x >= metadata->local_bounds.min.x &&
           metadata->local_bounds.max.y >= metadata->local_bounds.min.y &&
           metadata->local_bounds.max.z >= metadata->local_bounds.min.z;
}

static CoreObjectVec3 transform_preview_corner(CoreObjectVec3 local,
                                               CoreObjectVec3 position,
                                               CoreObjectVec3 rotation_deg,
                                               CoreObjectVec3 scale) {
    CoreObjectVec3 scaled = {
        local.x * scale.x,
        local.y * scale.y,
        local.z * scale.z
    };
    CoreObjectVec3 rotated = rotate_xyz_degrees(scaled, rotation_deg);
    rotated.x += position.x;
    rotated.y += position.y;
    rotated.z += position.z;
    return rotated;
}

static bool compute_preview_world_bounds(PhysicsSimRuntimeMeshPreviewInstance *instance) {
    const CoreMeshAssetBounds3 *local = NULL;
    bool have_corner = false;
    if (!instance || !instance->preview_metadata_valid ||
        !preview_bounds_valid(&instance->metadata)) {
        return false;
    }
    local = &instance->metadata.local_bounds;
    for (int sx = 0; sx <= 1; ++sx) {
        for (int sy = 0; sy <= 1; ++sy) {
            for (int sz = 0; sz <= 1; ++sz) {
                CoreObjectVec3 corner = {
                    sx ? local->max.x : local->min.x,
                    sy ? local->max.y : local->min.y,
                    sz ? local->max.z : local->min.z
                };
                CoreObjectVec3 world =
                    transform_preview_corner(corner,
                                             instance->transform_position,
                                             instance->transform_rotation_deg,
                                             instance->transform_scale);
                if (!vec3_finite(world)) return false;
                if (!have_corner) {
                    instance->world_bounds_min = world;
                    instance->world_bounds_max = world;
                    have_corner = true;
                } else {
                    if (world.x < instance->world_bounds_min.x) instance->world_bounds_min.x = world.x;
                    if (world.y < instance->world_bounds_min.y) instance->world_bounds_min.y = world.y;
                    if (world.z < instance->world_bounds_min.z) instance->world_bounds_min.z = world.z;
                    if (world.x > instance->world_bounds_max.x) instance->world_bounds_max.x = world.x;
                    if (world.y > instance->world_bounds_max.y) instance->world_bounds_max.y = world.y;
                    if (world.z > instance->world_bounds_max.z) instance->world_bounds_max.z = world.z;
                }
            }
        }
    }
    instance->has_world_bounds = have_corner;
    return have_corner;
}

static void capture_object_transform(json_object *object,
                                     PhysicsSimRuntimeMeshPreviewInstance *instance) {
    json_object *transform = NULL;
    if (!instance) return;
    instance->transform_position = (CoreObjectVec3){0.0, 0.0, 0.0};
    instance->transform_rotation_deg = (CoreObjectVec3){0.0, 0.0, 0.0};
    instance->transform_scale = (CoreObjectVec3){1.0, 1.0, 1.0};
    if (!object || !json_object_object_get_ex(object, "transform", &transform) ||
        !json_object_is_type(transform, json_type_object)) {
        return;
    }
    (void)vec3_field(transform, "position", &instance->transform_position);
    (void)vec3_field(transform, "rotation", &instance->transform_rotation_deg);
    (void)vec3_field(transform, "scale", &instance->transform_scale);
}

static const char *object_runtime_mesh_path_hint(json_object *object) {
    json_object *extensions = NULL;
    json_object *line_drawing = NULL;
    const char *path = NULL;
    if (!object || !json_object_is_type(object, json_type_object)) return NULL;
    if (!json_object_object_get_ex(object, "extensions", &extensions) ||
        !json_object_is_type(extensions, json_type_object)) {
        return NULL;
    }
    if (!json_object_object_get_ex(extensions, "line_drawing", &line_drawing) ||
        !json_object_is_type(line_drawing, json_type_object)) {
        return NULL;
    }
    path = string_field(line_drawing, "runtime_mesh_path");
    return (path && path[0]) ? path : NULL;
}

static void capture_physics_sim_mesh_behavior(json_object *object,
                                              PhysicsSimRuntimeMeshPreviewInstance *instance) {
    json_object *extensions = NULL;
    json_object *physics_sim = NULL;
    json_object *emitter = NULL;
    const char *fluid_behavior = NULL;
    const char *mesh_proxy_mode = NULL;
    bool fluid_obstacle = false;
    if (!object || !instance) return;
    snprintf(instance->fluid_behavior, sizeof(instance->fluid_behavior), "%s", "solid_obstacle");
    snprintf(instance->mesh_proxy_mode,
             sizeof(instance->mesh_proxy_mode),
             "%s",
             "surface_and_closed_fill");
    instance->fluid_obstacle_enabled = true;
    instance->fluid_emitter_enabled = false;
    instance->emitter_type = EMITTER_DENSITY_SOURCE;
    instance->emitter_source_mode_3d = EMITTER_3D_SOURCE_MODE_SURFACE_SHELL;
    instance->emitter_surface_3d = EMITTER_3D_SURFACE_ALL_FACES;
    instance->emitter_obstacle_mode_3d = EMITTER_3D_OBSTACLE_MODE_CLEAR_ATTACHED;
    instance->emitter_strength = 5.0f;
    instance->emitter_radius = 0.08f;
    instance->emitter_direction = (CoreObjectVec3){0.0, 0.0, 1.0};
    instance->emitter_thermal_buoyancy_3d = 0.0f;

    if (!json_object_object_get_ex(object, "extensions", &extensions) ||
        !json_object_is_type(extensions, json_type_object) ||
        !json_object_object_get_ex(extensions, "physics_sim", &physics_sim) ||
        !json_object_is_type(physics_sim, json_type_object)) {
        return;
    }

    fluid_behavior = string_field(physics_sim, "fluid_behavior");
    mesh_proxy_mode = string_field(physics_sim, "mesh_proxy_mode");
    if (fluid_behavior && fluid_behavior[0]) {
        snprintf(instance->fluid_behavior, sizeof(instance->fluid_behavior), "%s", fluid_behavior);
        if (strcmp(fluid_behavior, "solid_obstacle") == 0 ||
            strcmp(fluid_behavior, "solid") == 0) {
            instance->fluid_obstacle_enabled = true;
        } else if (strcmp(fluid_behavior, "visual_only") == 0 ||
                   strcmp(fluid_behavior, "none") == 0) {
            instance->fluid_obstacle_enabled = false;
        } else if (strcmp(fluid_behavior, "emitter") == 0 ||
                   strcmp(fluid_behavior, "surface_emitter") == 0) {
            instance->fluid_emitter_enabled = true;
            instance->fluid_obstacle_enabled = false;
            instance->emitter_source_mode_3d = EMITTER_3D_SOURCE_MODE_SURFACE_SHELL;
        } else if (strcmp(fluid_behavior, "surface_heat_emitter") == 0 ||
                   strcmp(fluid_behavior, "heat_emitter") == 0) {
            instance->fluid_emitter_enabled = true;
            instance->fluid_obstacle_enabled = false;
            instance->emitter_type = EMITTER_DENSITY_SOURCE;
            instance->emitter_source_mode_3d = EMITTER_3D_SOURCE_MODE_HEATED_OBSTACLE;
            instance->emitter_thermal_buoyancy_3d = 1.0f;
        } else if (strcmp(fluid_behavior, "boundary_flow_emitter") == 0 ||
                   strcmp(fluid_behavior, "velocity_emitter") == 0) {
            instance->fluid_emitter_enabled = true;
            instance->fluid_obstacle_enabled = false;
            instance->emitter_type = EMITTER_VELOCITY_JET;
            instance->emitter_source_mode_3d = EMITTER_3D_SOURCE_MODE_SURFACE_SHELL;
        }
    }
    if (json_object_object_get_ex(physics_sim, "emitter", &emitter) &&
        json_object_is_type(emitter, json_type_object)) {
        const char *type = string_field(emitter, "type");
        const char *mode_3d = string_field(emitter, "mode_3d");
        const char *surface_3d = string_field(emitter, "surface_3d");
        const char *obstacle_mode_3d = string_field(emitter, "obstacle_mode_3d");
        CoreObjectVec3 direction = instance->emitter_direction;
        double number = 0.0;
        bool active = false;
        if (bool_field(emitter, "active", &active) && active) {
            instance->fluid_emitter_enabled = true;
            instance->fluid_obstacle_enabled = false;
        }
        if (type && type[0]) instance->emitter_type = parse_emitter_type(type);
        if (mode_3d && mode_3d[0]) instance->emitter_source_mode_3d = parse_emitter_source_mode_3d(mode_3d);
        if (surface_3d && surface_3d[0]) instance->emitter_surface_3d = parse_emitter_surface_3d(surface_3d);
        if (obstacle_mode_3d && obstacle_mode_3d[0]) {
            instance->emitter_obstacle_mode_3d = parse_emitter_obstacle_mode_3d(obstacle_mode_3d);
        }
        if (number_field(emitter, "strength", &number)) instance->emitter_strength = (float)number;
        if (number_field(emitter, "radius", &number)) instance->emitter_radius = (float)number;
        if (number_field(emitter, "thermal_buoyancy_3d", &number)) {
            instance->emitter_thermal_buoyancy_3d = (float)number;
        }
        if (vec3_field(emitter, "direction", &direction)) instance->emitter_direction = direction;
    }
    if (bool_field(physics_sim, "fluid_obstacle", &fluid_obstacle)) {
        instance->fluid_obstacle_enabled = fluid_obstacle;
        if (fluid_obstacle && strcmp(instance->fluid_behavior, "visual_only") == 0) {
            snprintf(instance->fluid_behavior, sizeof(instance->fluid_behavior), "%s", "solid_obstacle");
        }
    }
    if (instance->fluid_emitter_enabled) {
        instance->fluid_obstacle_enabled = false;
    }
    if (mesh_proxy_mode && mesh_proxy_mode[0]) {
        snprintf(instance->mesh_proxy_mode, sizeof(instance->mesh_proxy_mode), "%s", mesh_proxy_mode);
    }
}

static bool validate_runtime_scene_header(json_object *root,
                                          PhysicsSimRuntimeMeshPreviewSet *out_set,
                                          char *out_diagnostics,
                                          size_t out_diagnostics_size) {
    const char *schema_family = string_field(root, "schema_family");
    const char *schema_variant = string_field(root, "schema_variant");
    const char *scene_id = string_field(root, "scene_id");
    if (!schema_family || strcmp(schema_family, "codework_scene") != 0) {
        mesh_preview_diag(out_diagnostics, out_diagnostics_size, "schema_family must be codework_scene");
        mesh_preview_set_diag(out_set, "schema_family must be codework_scene");
        return false;
    }
    if (!schema_variant || strcmp(schema_variant, "scene_runtime_v1") != 0) {
        mesh_preview_diag(out_diagnostics, out_diagnostics_size, "schema_variant must be scene_runtime_v1");
        mesh_preview_set_diag(out_set, "schema_variant must be scene_runtime_v1");
        return false;
    }
    if (!scene_id || !scene_id[0]) {
        mesh_preview_diag(out_diagnostics, out_diagnostics_size, "scene_id missing");
        mesh_preview_set_diag(out_set, "scene_id missing");
        return false;
    }
    out_set->valid_contract = true;
    return true;
}

static bool resolve_with_base(const char *runtime_scene_path_hint,
                              const char *candidate,
                              char *out_path,
                              size_t out_path_size) {
    char base_dir[PHYSICS_SIM_RUNTIME_MESH_PREVIEW_PATH_MAX] = {0};
    CoreResult r = core_result_ok();
    if (!candidate || !candidate[0] || !out_path || out_path_size == 0u) return false;
    if (candidate[0] == '/') {
        if (!path_exists(candidate)) return false;
        snprintf(out_path, out_path_size, "%s", candidate);
        return true;
    }
    if (!runtime_scene_path_hint || !runtime_scene_path_hint[0]) return false;
    if (core_scene_dirname(runtime_scene_path_hint, base_dir, sizeof(base_dir)).code != CORE_OK) {
        return false;
    }
    r = core_scene_resolve_path(base_dir, candidate, out_path, out_path_size);
    if (r.code != CORE_OK || !out_path[0] || !path_exists(out_path)) {
        if (out_path_size > 0u) out_path[0] = '\0';
        return false;
    }
    return true;
}

static bool resolve_migrated_desktop_stls_path(const char *candidate,
                                               char *out_path,
                                               size_t out_path_size) {
    const char *desktop_segment = NULL;
    const char *tail = NULL;
    const char *home = NULL;
    if (!candidate || candidate[0] != '/' || !out_path || out_path_size == 0u) {
        return false;
    }
    desktop_segment = strstr(candidate, "/Desktop/");
    if (!desktop_segment) return false;
    tail = desktop_segment + strlen("/Desktop/");
    if (!tail[0]) return false;
    home = getenv("HOME");
    if (!home || !home[0]) return false;
    if (snprintf(out_path, out_path_size, "%s/Desktop/stls/%s", home, tail) >=
        (int)out_path_size) {
        out_path[0] = '\0';
        return false;
    }
    if (!path_exists(out_path)) {
        out_path[0] = '\0';
        return false;
    }
    return true;
}

static bool resolve_path_candidate(const char *runtime_scene_path_hint,
                                   const char *candidate,
                                   char *out_path,
                                   size_t out_path_size) {
    if (out_path && out_path_size > 0u) out_path[0] = '\0';
    if (!candidate || !candidate[0] || !out_path || out_path_size == 0u) return false;
    if (resolve_with_base(runtime_scene_path_hint, candidate, out_path, out_path_size)) {
        return true;
    }
    if (candidate[0] == '/' &&
        resolve_migrated_desktop_stls_path(candidate, out_path, out_path_size)) {
        return true;
    }
    return false;
}

static bool resolve_runtime_mesh_path(const char *runtime_scene_path_hint,
                                      const char *asset_id,
                                      const char *explicit_runtime_path,
                                      char *out_path,
                                      size_t out_path_size) {
    char relative_path[256] = {0};
    if (out_path && out_path_size > 0u) out_path[0] = '\0';
    if (!asset_id || !asset_id[0] || !out_path || out_path_size == 0u) return false;
    if (explicit_runtime_path && explicit_runtime_path[0] &&
        resolve_path_candidate(runtime_scene_path_hint, explicit_runtime_path, out_path, out_path_size)) {
        return true;
    }
    snprintf(relative_path, sizeof(relative_path), "assets/mesh_assets/%s.runtime.json", asset_id);
    if (resolve_path_candidate(runtime_scene_path_hint, relative_path, out_path, out_path_size)) {
        return true;
    }
    snprintf(relative_path, sizeof(relative_path), "mesh_assets/%s.runtime.json", asset_id);
    return resolve_path_candidate(runtime_scene_path_hint, relative_path, out_path, out_path_size);
}

static void probe_preview_for_instance(PhysicsSimRuntimeMeshPreviewInstance *instance) {
    CoreResult path_result = core_result_ok();
    CoreMeshPreviewFileProbe probe;
    if (!instance) return;
    core_mesh_preview_runtime_metadata_init(&instance->metadata);
    if (!instance->runtime_path_resolved || !instance->runtime_mesh_path[0]) return;

    path_result = core_mesh_preview_path_from_runtime(instance->runtime_mesh_path,
                                                      instance->preview_path,
                                                      sizeof(instance->preview_path));
    if (path_result.code != CORE_OK) {
        instance->preview_path[0] = '\0';
        return;
    }
    instance->preview_path_resolved = true;

    core_mesh_preview_file_probe_init(&probe);
    (void)core_mesh_preview_probe_file(instance->preview_path, &probe);
    instance->preview_file_exists = probe.exists;
    instance->preview_file_readable = probe.readable;
    instance->preview_schema_supported = probe.schema_supported;
    instance->preview_metadata_valid = probe.metadata_valid;
    if (probe.metadata_valid) {
        instance->metadata = probe.metadata;
    }
    (void)compute_preview_world_bounds(instance);
}

static void scan_mesh_asset_object(json_object *object,
                                   int scene_object_index,
                                   const char *runtime_scene_path_hint,
                                   PhysicsSimRuntimeMeshPreviewSet *out_set) {
    json_object *geometry_ref = NULL;
    const char *object_type = string_field(object, "object_type");
    const char *object_id = string_field(object, "object_id");
    const char *geometry_kind = NULL;
    const char *asset_id = NULL;
    const char *runtime_path_hint = NULL;
    PhysicsSimRuntimeMeshPreviewInstance *instance = NULL;

    if (!object_type || strcmp(object_type, "mesh_asset_instance") != 0) return;
    if (!object_id || !object_id[0]) {
        out_set->invalid_reference_count += 1;
        return;
    }
    if (!json_object_object_get_ex(object, "geometry_ref", &geometry_ref) ||
        !json_object_is_type(geometry_ref, json_type_object)) {
        out_set->invalid_reference_count += 1;
        return;
    }
    geometry_kind = string_field(geometry_ref, "kind");
    asset_id = string_field(geometry_ref, "id");
    if (!geometry_kind || strcmp(geometry_kind, "mesh_asset") != 0 ||
        !asset_id || !asset_id[0]) {
        out_set->invalid_reference_count += 1;
        return;
    }
    if (out_set->instance_count >= PHYSICS_SIM_RUNTIME_MESH_PREVIEW_MAX_INSTANCES) {
        out_set->instance_capacity_clamped = true;
        return;
    }

    instance = &out_set->instances[out_set->instance_count++];
    memset(instance, 0, sizeof(*instance));
    snprintf(instance->object_id, sizeof(instance->object_id), "%s", object_id);
    snprintf(instance->asset_id, sizeof(instance->asset_id), "%s", asset_id);
    instance->scene_object_index = scene_object_index;
    core_mesh_preview_runtime_metadata_init(&instance->metadata);
    capture_object_transform(object, instance);
    capture_physics_sim_mesh_behavior(object, instance);

    runtime_path_hint = object_runtime_mesh_path_hint(object);
    if (runtime_path_hint && runtime_path_hint[0]) {
        snprintf(instance->runtime_mesh_path_hint,
                 sizeof(instance->runtime_mesh_path_hint),
                 "%s",
                 runtime_path_hint);
    }
    instance->runtime_path_resolved = resolve_runtime_mesh_path(runtime_scene_path_hint,
                                                               asset_id,
                                                               runtime_path_hint,
                                                               instance->runtime_mesh_path,
                                                               sizeof(instance->runtime_mesh_path));
    instance->runtime_path_recovered =
        instance->runtime_path_resolved &&
        runtime_path_hint &&
        runtime_path_hint[0] == '/' &&
        strcmp(instance->runtime_mesh_path, runtime_path_hint) != 0;
    probe_preview_for_instance(instance);
}

bool physics_sim_runtime_mesh_preview_scan_scene_json(
    const char *runtime_scene_json,
    const char *runtime_scene_path_hint,
    PhysicsSimRuntimeMeshPreviewSet *out_set,
    char *out_diagnostics,
    size_t out_diagnostics_size) {
    json_object *root = NULL;
    json_object *objects = NULL;
    size_t count = 0u;

    if (!out_set) {
        mesh_preview_diag(out_diagnostics, out_diagnostics_size, "mesh preview output missing");
        return false;
    }
    physics_sim_runtime_mesh_preview_set_init(out_set);
    mesh_preview_diag(out_diagnostics, out_diagnostics_size, "ok");
    mesh_preview_set_diag(out_set, "ok");
    if (!runtime_scene_json || !runtime_scene_json[0]) {
        mesh_preview_diag(out_diagnostics, out_diagnostics_size, "runtime scene json missing");
        mesh_preview_set_diag(out_set, "runtime scene json missing");
        return false;
    }

    root = json_tokener_parse(runtime_scene_json);
    if (!root || !json_object_is_type(root, json_type_object)) {
        if (root) json_object_put(root);
        mesh_preview_diag(out_diagnostics, out_diagnostics_size, "invalid runtime scene json");
        mesh_preview_set_diag(out_set, "invalid runtime scene json");
        return false;
    }
    if (!validate_runtime_scene_header(root, out_set, out_diagnostics, out_diagnostics_size)) {
        json_object_put(root);
        return false;
    }
    if (!json_object_object_get_ex(root, "objects", &objects) ||
        !json_object_is_type(objects, json_type_array)) {
        json_object_put(root);
        mesh_preview_diag(out_diagnostics, out_diagnostics_size, "runtime scene objects missing");
        mesh_preview_set_diag(out_set, "runtime scene objects missing");
        return false;
    }

    count = (size_t)json_object_array_length(objects);
    for (size_t i = 0u; i < count; ++i) {
        json_object *object = json_object_array_get_idx(objects, i);
        if (!object || !json_object_is_type(object, json_type_object)) continue;
        scan_mesh_asset_object(object, (int)i, runtime_scene_path_hint, out_set);
    }

    json_object_put(root);
    mesh_preview_diag(out_diagnostics, out_diagnostics_size, "ok");
    mesh_preview_set_diag(out_set, "ok");
    return true;
}

bool physics_sim_runtime_mesh_preview_scan_scene_file(
    const char *runtime_scene_path,
    PhysicsSimRuntimeMeshPreviewSet *out_set,
    char *out_diagnostics,
    size_t out_diagnostics_size) {
    char *text = NULL;
    bool ok = false;
    if (!runtime_scene_path || !runtime_scene_path[0]) {
        mesh_preview_diag(out_diagnostics, out_diagnostics_size, "runtime scene path missing");
        return false;
    }
    text = read_text_file_alloc(runtime_scene_path);
    if (!text) {
        mesh_preview_diag(out_diagnostics, out_diagnostics_size, "failed to read runtime scene");
        return false;
    }
    ok = physics_sim_runtime_mesh_preview_scan_scene_json(text,
                                                          runtime_scene_path,
                                                          out_set,
                                                          out_diagnostics,
                                                          out_diagnostics_size);
    free(text);
    return ok;
}
