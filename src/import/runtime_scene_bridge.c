#include "import/runtime_scene_bridge.h"
#include "import/runtime_scene_solver_projection.h"
#include "import/runtime_scene_solver_projection_internal.h"

#include "core_scene_overlay_merge_shared.h"
#include "core_io.h"

#include <json-c/json.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static PhysicsSimRetainedRuntimeScene g_last_retained_scene;

static bool parse_vec3(json_object *obj,
                       const char *key,
                       double *out_x,
                       double *out_y,
                       double *out_z);
static bool parse_scene_domain_overlay(json_object *root,
                                       PhysicsSimRuntimeSceneBounds *out_bounds);

static void preflight_reset(RuntimeSceneBridgePreflight *out_preflight) {
    if (!out_preflight) return;
    memset(out_preflight, 0, sizeof(*out_preflight));
    out_preflight->valid_contract = false;
}

static void preflight_diag(RuntimeSceneBridgePreflight *out_preflight, const char *message) {
    if (!out_preflight || !message) return;
    snprintf(out_preflight->diagnostics, sizeof(out_preflight->diagnostics), "%s", message);
}

static void bridge_diag(char *out_diagnostics, size_t out_diagnostics_size, const char *message) {
    if (!out_diagnostics || out_diagnostics_size == 0 || !message) return;
    snprintf(out_diagnostics, out_diagnostics_size, "%s", message);
}

static void retained_scene_reset(PhysicsSimRetainedRuntimeScene *out_scene) {
    int i = 0;
    if (!out_scene) return;
    memset(out_scene, 0, sizeof(*out_scene));
    core_scene_root_contract_init(&out_scene->root);
    for (i = 0; i < PHYSICS_SIM_RUNTIME_SCENE_MAX_OBJECTS; ++i) {
        core_scene_object_contract_init(&out_scene->objects[i]);
    }
}

static void retained_scene_diag(PhysicsSimRetainedRuntimeScene *out_scene, const char *message) {
    if (!out_scene || !message) return;
    snprintf(out_scene->diagnostics, sizeof(out_scene->diagnostics), "%s", message);
}

static bool parse_json_bool(json_object *obj, const char *key, bool *out_value) {
    json_object *node = NULL;
    if (!obj || !key || !out_value) return false;
    if (!json_object_object_get_ex(obj, key, &node) || !json_object_is_type(node, json_type_boolean)) {
        return false;
    }
    *out_value = json_object_get_boolean(node) ? true : false;
    return true;
}

static bool parse_json_number(json_object *obj, const char *key, double *out_value) {
    json_object *node = NULL;
    if (!obj || !key || !out_value) return false;
    if (!json_object_object_get_ex(obj, key, &node) ||
        (!json_object_is_type(node, json_type_double) &&
         !json_object_is_type(node, json_type_int))) {
        return false;
    }
    *out_value = json_object_get_double(node);
    return true;
}

static double vec3_length(CoreObjectVec3 value) {
    return sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
}

static bool normalize_vec3(CoreObjectVec3 *value) {
    double len = 0.0;
    if (!value) return false;
    len = vec3_length(*value);
    if (!(len > 1e-9) || !isfinite(len)) return false;
    value->x /= len;
    value->y /= len;
    value->z /= len;
    return true;
}

static CoreObjectVec3 scene_up_for_axis_plane(CoreObjectPlane axis_plane) {
    switch (axis_plane) {
    case CORE_OBJECT_PLANE_YZ:
        return (CoreObjectVec3){1.0, 0.0, 0.0};
    case CORE_OBJECT_PLANE_XZ:
        return (CoreObjectVec3){0.0, 1.0, 0.0};
    case CORE_OBJECT_PLANE_XY:
    default:
        return (CoreObjectVec3){0.0, 0.0, 1.0};
    }
}

static PhysicsSimRuntimeSceneUpVector retained_scene_resolve_scene_up(
    const PhysicsSimRetainedRuntimeScene *scene) {
    PhysicsSimRuntimeSceneUpVector result = {0};
    CoreObjectVec3 direction = {0};
    if (!scene) return result;

    if (scene->construction_plane.valid) {
        direction = scene->construction_plane.custom_frame.normal;
        if (normalize_vec3(&direction)) {
            result.valid = true;
            result.direction = direction;
            result.source = PHYSICS_SIM_RUNTIME_SCENE_UP_CONSTRUCTION_PLANE_FRAME;
            return result;
        }

        direction = scene_up_for_axis_plane(scene->construction_plane.axis_plane);
        if (normalize_vec3(&direction)) {
            result.valid = true;
            result.direction = direction;
            result.source = PHYSICS_SIM_RUNTIME_SCENE_UP_CONSTRUCTION_PLANE_AXIS;
            return result;
        }
    }

    if (scene->root.space_mode_default == CORE_SCENE_SPACE_MODE_3D) {
        result.valid = true;
        result.direction = (CoreObjectVec3){0.0, 0.0, 1.0};
        result.source = PHYSICS_SIM_RUNTIME_SCENE_UP_FALLBACK_POSITIVE_Z;
    }
    return result;
}

static bool parse_frame3_with_axis_keys(json_object *obj,
                                        const char *key,
                                        const char *axis_u_key,
                                        const char *axis_v_key,
                                        CoreSceneFrame3 *out_frame) {
    json_object *node = NULL;
    if (!obj || !key || !axis_u_key || !axis_v_key || !out_frame) return false;
    if (!json_object_object_get_ex(obj, key, &node) || !json_object_is_type(node, json_type_object)) {
        return false;
    }
    if (!parse_vec3(node, "origin",
                    &out_frame->origin.x,
                    &out_frame->origin.y,
                    &out_frame->origin.z)) {
        return false;
    }
    if (!parse_vec3(node, axis_u_key,
                    &out_frame->axis_u.x,
                    &out_frame->axis_u.y,
                    &out_frame->axis_u.z)) {
        return false;
    }
    if (!parse_vec3(node, axis_v_key,
                    &out_frame->axis_v.x,
                    &out_frame->axis_v.y,
                    &out_frame->axis_v.z)) {
        return false;
    }
    if (!parse_vec3(node, "normal",
                    &out_frame->normal.x,
                    &out_frame->normal.y,
                    &out_frame->normal.z)) {
        return false;
    }
    return true;
}

static bool parse_frame3(json_object *obj, const char *key, CoreSceneFrame3 *out_frame) {
    return parse_frame3_with_axis_keys(obj, key, "axis_u", "axis_v", out_frame);
}

static bool parse_plane_lock(const char *text, CoreObjectPlane *out_plane) {
    if (!text || !out_plane) return false;
    if (strcmp(text, "xy") == 0) {
        *out_plane = CORE_OBJECT_PLANE_XY;
        return true;
    }
    if (strcmp(text, "yz") == 0) {
        *out_plane = CORE_OBJECT_PLANE_YZ;
        return true;
    }
    if (strcmp(text, "xz") == 0) {
        *out_plane = CORE_OBJECT_PLANE_XZ;
        return true;
    }
    return false;
}

static void retained_scene_capture_root(json_object *root,
                                        const RuntimeSceneBridgePreflight *preflight,
                                        PhysicsSimRetainedRuntimeScene *out_scene) {
    json_object *scene_id = NULL;
    json_object *space_mode_default = NULL;
    json_object *world_scale = NULL;
    const char *scene_id_str = NULL;
    const char *space_mode_str = "2d";
    if (!root || !preflight || !out_scene) return;

    out_scene->valid_contract = preflight->valid_contract;
    out_scene->object_count = preflight->object_count;
    out_scene->material_count = preflight->material_count;
    out_scene->light_count = preflight->light_count;
    out_scene->camera_count = preflight->camera_count;
    retained_scene_diag(out_scene, preflight->diagnostics);

    json_object_object_get_ex(root, "scene_id", &scene_id);
    if (scene_id && json_object_is_type(scene_id, json_type_string)) {
        scene_id_str = json_object_get_string(scene_id);
    }
    json_object_object_get_ex(root, "space_mode_default", &space_mode_default);
    if (space_mode_default && json_object_is_type(space_mode_default, json_type_string)) {
        space_mode_str = json_object_get_string(space_mode_default);
    }
    json_object_object_get_ex(root, "world_scale", &world_scale);

    if (scene_id_str) {
        (void)core_scene_root_contract_set_scene_id(&out_scene->root, scene_id_str);
    }
    if (core_scene_space_mode_parse(space_mode_str, &out_scene->root.space_mode_default).code != CORE_OK) {
        out_scene->root.space_mode_default = CORE_SCENE_SPACE_MODE_2D;
    }
    out_scene->root.space_mode_intent = out_scene->root.space_mode_default;
    out_scene->root.unit_kind = CORE_UNIT_METER;
    out_scene->root.world_scale =
        (world_scale && (json_object_is_type(world_scale, json_type_double) ||
                         json_object_is_type(world_scale, json_type_int)))
            ? json_object_get_double(world_scale)
            : 1.0;
}

static void retained_scene_capture_line_drawing_root(json_object *root,
                                                     PhysicsSimRetainedRuntimeScene *out_scene) {
    json_object *extensions = NULL;
    json_object *line_drawing = NULL;
    json_object *scene3d = NULL;
    json_object *bounds = NULL;
    json_object *construction_plane = NULL;
    const char *axis_text = NULL;
    if (!root || !out_scene) return;
    if (!json_object_object_get_ex(root, "extensions", &extensions) ||
        !json_object_is_type(extensions, json_type_object)) {
        return;
    }
    if (!json_object_object_get_ex(extensions, "line_drawing", &line_drawing) ||
        !json_object_is_type(line_drawing, json_type_object)) {
        return;
    }
    if (!json_object_object_get_ex(line_drawing, "scene3d", &scene3d) ||
        !json_object_is_type(scene3d, json_type_object)) {
        return;
    }

    out_scene->has_line_drawing_scene3d = true;

    if (json_object_object_get_ex(scene3d, "bounds", &bounds) &&
        json_object_is_type(bounds, json_type_object)) {
        (void)parse_json_bool(bounds, "enabled", &out_scene->bounds.enabled);
        (void)parse_json_bool(bounds, "clamp_on_edit", &out_scene->bounds.clamp_on_edit);
        (void)parse_vec3(bounds, "min",
                         &out_scene->bounds.min.x,
                         &out_scene->bounds.min.y,
                         &out_scene->bounds.min.z);
        (void)parse_vec3(bounds, "max",
                         &out_scene->bounds.max.x,
                         &out_scene->bounds.max.y,
                         &out_scene->bounds.max.z);
    }

    if (json_object_object_get_ex(scene3d, "construction_plane", &construction_plane) &&
        json_object_is_type(construction_plane, json_type_object)) {
        json_object *axis = NULL;
        out_scene->construction_plane.valid = true;
        out_scene->construction_plane.axis_plane = CORE_OBJECT_PLANE_XY;
        if (json_object_object_get_ex(construction_plane, "axis", &axis) &&
            json_object_is_type(axis, json_type_string)) {
            axis_text = json_object_get_string(axis);
            if (!parse_plane_lock(axis_text, &out_scene->construction_plane.axis_plane)) {
                out_scene->construction_plane.axis_plane = CORE_OBJECT_PLANE_XY;
            }
        }
        (void)parse_json_number(construction_plane, "offset", &out_scene->construction_plane.offset);
        (void)parse_frame3_with_axis_keys(construction_plane,
                                          "custom_frame",
                                          "axisU",
                                          "axisV",
                                          &out_scene->construction_plane.custom_frame);
    }
}

static bool retained_scene_capture_object(json_object *obj,
                                          PhysicsSimRetainedRuntimeScene *out_scene) {
    json_object *object_id = NULL;
    json_object *object_type = NULL;
    json_object *dimensional_mode = NULL;
    json_object *locked_plane = NULL;
    json_object *transform = NULL;
    json_object *position = NULL;
    json_object *rotation = NULL;
    json_object *scale = NULL;
    json_object *flags = NULL;
    json_object *primitive = NULL;
    json_object *primitive_kind = NULL;
    const char *object_id_str = NULL;
    const char *object_type_str = NULL;
    const char *dimensional_mode_str = NULL;
    const char *locked_plane_str = NULL;
    CoreSceneObjectKind kind = CORE_SCENE_OBJECT_KIND_UNKNOWN;
    CoreResult r;
    CoreSceneObjectContract *dst = NULL;

    if (!obj || !out_scene || !json_object_is_type(obj, json_type_object)) return false;
    if (out_scene->retained_object_count >= PHYSICS_SIM_RUNTIME_SCENE_MAX_OBJECTS) {
        out_scene->object_capacity_clamped = true;
        return false;
    }
    if (!json_object_object_get_ex(obj, "object_id", &object_id) ||
        !json_object_is_type(object_id, json_type_string) ||
        !json_object_object_get_ex(obj, "object_type", &object_type) ||
        !json_object_is_type(object_type, json_type_string)) {
        out_scene->invalid_object_count++;
        return false;
    }

    object_id_str = json_object_get_string(object_id);
    object_type_str = json_object_get_string(object_type);
    if (!object_id_str || !object_type_str || !object_id_str[0] || !object_type_str[0]) {
        out_scene->invalid_object_count++;
        return false;
    }

    if (core_scene_object_kind_parse(object_type_str, &kind).code != CORE_OK) {
        kind = CORE_SCENE_OBJECT_KIND_UNKNOWN;
    }
    if (json_object_object_get_ex(obj, "primitive", &primitive) &&
        json_object_is_type(primitive, json_type_object) &&
        json_object_object_get_ex(primitive, "kind", &primitive_kind) &&
        json_object_is_type(primitive_kind, json_type_string)) {
        const char *primitive_kind_str = json_object_get_string(primitive_kind);
        CoreSceneObjectKind primitive_parsed = CORE_SCENE_OBJECT_KIND_UNKNOWN;
        if (primitive_kind_str &&
            core_scene_object_kind_parse(primitive_kind_str, &primitive_parsed).code == CORE_OK) {
            kind = primitive_parsed;
        }
    }

    dst = &out_scene->objects[out_scene->retained_object_count];
    core_scene_object_contract_init(dst);
    r = core_scene_object_contract_prepare(dst, object_id_str, kind);
    if (r.code != CORE_OK) {
        out_scene->invalid_object_count++;
        return false;
    }

    if (json_object_object_get_ex(obj, "dimensional_mode", &dimensional_mode) &&
        json_object_is_type(dimensional_mode, json_type_string)) {
        dimensional_mode_str = json_object_get_string(dimensional_mode);
    }
    if (dimensional_mode_str && strcmp(dimensional_mode_str, "full_3d") == 0) {
        (void)core_object_promote_to_full_3d(&dst->object);
    } else {
        CoreObjectPlane plane = CORE_OBJECT_PLANE_XY;
        if (json_object_object_get_ex(obj, "locked_plane", &locked_plane) &&
            json_object_is_type(locked_plane, json_type_string)) {
            locked_plane_str = json_object_get_string(locked_plane);
            if (locked_plane_str && parse_plane_lock(locked_plane_str, &plane)) {
                (void)core_object_set_plane_lock(&dst->object, plane);
            }
        }
    }

    if (json_object_object_get_ex(obj, "transform", &transform) &&
        json_object_is_type(transform, json_type_object)) {
        if (json_object_object_get_ex(transform, "position", &position) &&
            json_object_is_type(position, json_type_object)) {
            (void)parse_vec3(transform, "position",
                             &dst->object.transform.position.x,
                             &dst->object.transform.position.y,
                             &dst->object.transform.position.z);
        }
        if (json_object_object_get_ex(transform, "rotation", &rotation) &&
            json_object_is_type(rotation, json_type_object)) {
            (void)parse_vec3(transform, "rotation",
                             &dst->object.transform.rotation_deg.x,
                             &dst->object.transform.rotation_deg.y,
                             &dst->object.transform.rotation_deg.z);
        }
        if (json_object_object_get_ex(transform, "scale", &scale) &&
            json_object_is_type(scale, json_type_object)) {
            (void)parse_vec3(transform, "scale",
                             &dst->object.transform.scale.x,
                             &dst->object.transform.scale.y,
                             &dst->object.transform.scale.z);
        }
    }

    if (json_object_object_get_ex(obj, "flags", &flags) && json_object_is_type(flags, json_type_object)) {
        (void)parse_json_bool(flags, "visible", &dst->object.flags.visible);
        (void)parse_json_bool(flags, "locked", &dst->object.flags.locked);
        (void)parse_json_bool(flags, "selectable", &dst->object.flags.selectable);
    }

    if (primitive && json_object_is_type(primitive, json_type_object)) {
        if (kind == CORE_SCENE_OBJECT_KIND_PLANE_PRIMITIVE) {
            core_scene_plane_primitive_init(&dst->plane_primitive);
            dst->has_plane_primitive = true;
            (void)parse_json_number(primitive, "width", &dst->plane_primitive.width);
            (void)parse_json_number(primitive, "height", &dst->plane_primitive.height);
            (void)parse_json_bool(primitive,
                                  "lock_to_construction_plane",
                                  &dst->plane_primitive.lock_to_construction_plane);
            (void)parse_json_bool(primitive,
                                  "lock_to_bounds",
                                  &dst->plane_primitive.lock_to_bounds);
            (void)parse_frame3(primitive, "frame", &dst->plane_primitive.frame);
        } else if (kind == CORE_SCENE_OBJECT_KIND_RECT_PRISM_PRIMITIVE) {
            core_scene_rect_prism_primitive_init(&dst->rect_prism_primitive);
            dst->has_rect_prism_primitive = true;
            (void)parse_json_number(primitive, "width", &dst->rect_prism_primitive.width);
            (void)parse_json_number(primitive, "height", &dst->rect_prism_primitive.height);
            (void)parse_json_number(primitive, "depth", &dst->rect_prism_primitive.depth);
            (void)parse_json_bool(primitive,
                                  "lock_to_construction_plane",
                                  &dst->rect_prism_primitive.lock_to_construction_plane);
            (void)parse_json_bool(primitive,
                                  "lock_to_bounds",
                                  &dst->rect_prism_primitive.lock_to_bounds);
            (void)parse_frame3(primitive, "frame", &dst->rect_prism_primitive.frame);
        }
    }

    r = core_scene_object_contract_validate(dst);
    if (r.code != CORE_OK) {
        core_scene_object_contract_init(dst);
        out_scene->invalid_object_count++;
        return false;
    }

    if (kind == CORE_SCENE_OBJECT_KIND_PLANE_PRIMITIVE ||
        kind == CORE_SCENE_OBJECT_KIND_RECT_PRISM_PRIMITIVE) {
        out_scene->primitive_object_count++;
    }

    out_scene->retained_object_count++;
    return true;
}

static void retained_scene_capture(json_object *root,
                                   const RuntimeSceneBridgePreflight *preflight) {
    json_object *objects = NULL;
    json_object *hierarchy = NULL;
    size_t i = 0;
    retained_scene_reset(&g_last_retained_scene);
    if (!root || !preflight) return;

    retained_scene_capture_root(root, preflight, &g_last_retained_scene);
    retained_scene_capture_line_drawing_root(root, &g_last_retained_scene);
    if (core_scene_root_contract_validate(&g_last_retained_scene.root).code != CORE_OK) {
        retained_scene_diag(&g_last_retained_scene, "retained root contract invalid");
    }

    if (json_object_object_get_ex(root, "hierarchy", &hierarchy) &&
        json_object_is_type(hierarchy, json_type_array)) {
        g_last_retained_scene.hierarchy_edge_count = (int)json_object_array_length(hierarchy);
    }

    if (!json_object_object_get_ex(root, "objects", &objects) || !json_object_is_type(objects, json_type_array)) {
        return;
    }
    for (i = 0; i < json_object_array_length(objects); ++i) {
        json_object *obj = json_object_array_get_idx(objects, i);
        (void)retained_scene_capture_object(obj, &g_last_retained_scene);
    }
}

static int json_array_len_or_zero(json_object *obj, const char *key) {
    json_object *array_obj = NULL;
    if (!obj || !key) return 0;
    if (!json_object_object_get_ex(obj, key, &array_obj)) return 0;
    if (!array_obj || !json_object_is_type(array_obj, json_type_array)) return 0;
    return (int)json_object_array_length(array_obj);
}

static bool parse_vec3(json_object *obj,
                       const char *key,
                       double *out_x,
                       double *out_y,
                       double *out_z) {
    json_object *node = NULL;
    json_object *x = NULL;
    json_object *y = NULL;
    json_object *z = NULL;
    if (!obj || !key || !out_x || !out_y || !out_z) return false;
    if (!json_object_object_get_ex(obj, key, &node) || !json_object_is_type(node, json_type_object)) {
        return false;
    }
    if (!json_object_object_get_ex(node, "x", &x) ||
        !json_object_object_get_ex(node, "y", &y) ||
        !json_object_object_get_ex(node, "z", &z)) {
        return false;
    }
    *out_x = json_object_get_double(x);
    *out_y = json_object_get_double(y);
    *out_z = json_object_get_double(z);
    return true;
}

static bool parse_scene_domain_overlay(json_object *root,
                                       PhysicsSimRuntimeSceneBounds *out_bounds) {
    json_object *extensions = NULL;
    json_object *physics_sim = NULL;
    json_object *scene_domain = NULL;
    bool active = false;
    if (!root || !out_bounds) return false;
    memset(out_bounds, 0, sizeof(*out_bounds));
    if (!json_object_object_get_ex(root, "extensions", &extensions) ||
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
    if (!parse_json_bool(scene_domain, "active", &active) || !active) {
        return false;
    }
    if (!parse_vec3(scene_domain, "min",
                    &out_bounds->min.x,
                    &out_bounds->min.y,
                    &out_bounds->min.z) ||
        !parse_vec3(scene_domain, "max",
                    &out_bounds->max.x,
                    &out_bounds->max.y,
                    &out_bounds->max.z)) {
        return false;
    }
    out_bounds->enabled = true;
    return true;
}

static bool validate_runtime_scene_root(json_object *root,
                                        RuntimeSceneBridgePreflight *out_preflight) {
    json_object *schema_family = NULL;
    json_object *schema_variant = NULL;
    json_object *scene_id = NULL;
    json_object *unit_system = NULL;
    json_object *world_scale = NULL;
    const char *schema_family_str = NULL;
    const char *schema_variant_str = NULL;
    const char *scene_id_str = NULL;
    const char *unit_system_str = NULL;
    double world_scale_value = 1.0;

    if (!root || !out_preflight) return false;

    if (!json_object_object_get_ex(root, "schema_family", &schema_family) ||
        !json_object_is_type(schema_family, json_type_string)) {
        preflight_diag(out_preflight, "missing schema_family");
        return false;
    }
    schema_family_str = json_object_get_string(schema_family);
    if (!schema_family_str || strcmp(schema_family_str, "codework_scene") != 0) {
        preflight_diag(out_preflight, "schema_family must be codework_scene");
        return false;
    }

    if (!json_object_object_get_ex(root, "schema_variant", &schema_variant) ||
        !json_object_is_type(schema_variant, json_type_string)) {
        preflight_diag(out_preflight, "missing schema_variant");
        return false;
    }
    schema_variant_str = json_object_get_string(schema_variant);
    if (!schema_variant_str || strcmp(schema_variant_str, "scene_runtime_v1") != 0) {
        preflight_diag(out_preflight, "schema_variant must be scene_runtime_v1");
        return false;
    }

    if (!json_object_object_get_ex(root, "scene_id", &scene_id) ||
        !json_object_is_type(scene_id, json_type_string)) {
        preflight_diag(out_preflight, "missing scene_id");
        return false;
    }
    scene_id_str = json_object_get_string(scene_id);
    if (!scene_id_str || !scene_id_str[0]) {
        preflight_diag(out_preflight, "scene_id is empty");
        return false;
    }

    if (!json_object_object_get_ex(root, "unit_system", &unit_system) ||
        !json_object_is_type(unit_system, json_type_string)) {
        preflight_diag(out_preflight, "missing unit_system");
        return false;
    }
    unit_system_str = json_object_get_string(unit_system);
    if (!unit_system_str || strcmp(unit_system_str, "meters") != 0) {
        preflight_diag(out_preflight, "unit_system must be meters");
        return false;
    }

    if (!json_object_object_get_ex(root, "world_scale", &world_scale) ||
        (!json_object_is_type(world_scale, json_type_double) &&
         !json_object_is_type(world_scale, json_type_int))) {
        preflight_diag(out_preflight, "missing world_scale");
        return false;
    }
    world_scale_value = json_object_get_double(world_scale);
    if (!(world_scale_value > 0.0) || !isfinite(world_scale_value)) {
        preflight_diag(out_preflight, "world_scale must be finite and > 0");
        return false;
    }

    snprintf(out_preflight->scene_id, sizeof(out_preflight->scene_id), "%s", scene_id_str);
    out_preflight->object_count = json_array_len_or_zero(root, "objects");
    out_preflight->material_count = json_array_len_or_zero(root, "materials");
    out_preflight->light_count = json_array_len_or_zero(root, "lights");
    out_preflight->camera_count = json_array_len_or_zero(root, "cameras");
    out_preflight->valid_contract = true;
    preflight_diag(out_preflight, "ok");
    return true;
}

static bool validate_runtime_scene_root_diag(json_object *root,
                                             char *out_diagnostics,
                                             size_t out_diagnostics_size) {
    RuntimeSceneBridgePreflight preflight;
    preflight_reset(&preflight);
    if (!validate_runtime_scene_root(root, &preflight)) {
        bridge_diag(out_diagnostics, out_diagnostics_size, preflight.diagnostics);
        return false;
    }
    bridge_diag(out_diagnostics, out_diagnostics_size, "ok");
    return true;
}

bool runtime_scene_bridge_preflight_json(const char *runtime_scene_json,
                                         RuntimeSceneBridgePreflight *out_preflight) {
    json_object *root = NULL;

    if (!runtime_scene_json || !out_preflight) return false;
    preflight_reset(out_preflight);

    root = json_tokener_parse(runtime_scene_json);
    if (!root || !json_object_is_type(root, json_type_object)) {
        preflight_diag(out_preflight, "invalid JSON object");
        if (root) json_object_put(root);
        return false;
    }

    if (!validate_runtime_scene_root(root, out_preflight)) {
        json_object_put(root);
        return false;
    }

    json_object_put(root);
    return true;
}

bool runtime_scene_bridge_preflight_file(const char *runtime_scene_path,
                                         RuntimeSceneBridgePreflight *out_preflight) {
    CoreBuffer file_data = {0};
    CoreResult io_result;
    char *json_text = NULL;
    bool ok = false;

    if (!runtime_scene_path || !out_preflight) return false;
    preflight_reset(out_preflight);

    io_result = core_io_read_all(runtime_scene_path, &file_data);
    if (io_result.code != CORE_OK || !file_data.data || file_data.size == 0) {
        preflight_diag(out_preflight, "failed to read runtime scene file");
        core_io_buffer_free(&file_data);
        return false;
    }

    json_text = (char *)malloc(file_data.size + 1u);
    if (!json_text) {
        preflight_diag(out_preflight, "out of memory");
        core_io_buffer_free(&file_data);
        return false;
    }

    memcpy(json_text, file_data.data, file_data.size);
    json_text[file_data.size] = '\0';
    core_io_buffer_free(&file_data);
    ok = runtime_scene_bridge_preflight_json(json_text, out_preflight);
    free(json_text);
    return ok;
}

static void runtime_scene_bridge_append_runtime_mesh_emitters(
    const char *runtime_scene_json,
    FluidScenePreset *in_out_preset) {
    PhysicsSimRuntimeMeshPreviewSet mesh_set;
    if (!runtime_scene_json || !in_out_preset) return;
    if (!physics_sim_runtime_mesh_preview_scan_scene_json(runtime_scene_json,
                                                          NULL,
                                                          &mesh_set,
                                                          NULL,
                                                          0)) {
        return;
    }
    for (int i = 0; i < mesh_set.instance_count && in_out_preset->emitter_count < MAX_FLUID_EMITTERS; ++i) {
        const PhysicsSimRuntimeMeshPreviewInstance *instance = &mesh_set.instances[i];
        FluidEmitter *dst = NULL;
        if (!instance->fluid_emitter_enabled) continue;
        dst = &in_out_preset->emitters[in_out_preset->emitter_count];
        memset(dst, 0, sizeof(*dst));
        dst->type = instance->emitter_type;
        dst->position_x = (float)instance->transform_position.x;
        dst->position_y = (float)instance->transform_position.y;
        dst->position_z = (float)instance->transform_position.z;
        dst->radius = instance->emitter_radius > 0.0f ? instance->emitter_radius : 0.08f;
        dst->strength = instance->emitter_strength;
        dst->dir_x = (float)instance->emitter_direction.x;
        dst->dir_y = (float)instance->emitter_direction.y;
        dst->dir_z = (float)instance->emitter_direction.z;
        dst->attached_object = -1;
        dst->attached_import = -1;
        dst->attached_runtime_mesh_enabled = true;
        dst->attached_runtime_mesh = i;
        dst->source_mode_3d = instance->emitter_source_mode_3d;
        dst->surface_3d = instance->emitter_surface_3d;
        dst->obstacle_mode_3d = instance->emitter_obstacle_mode_3d;
        dst->thermal_buoyancy_3d = instance->emitter_thermal_buoyancy_3d;
        in_out_preset->emitter_count += 1u;
    }
}

bool runtime_scene_bridge_apply_json(const char *runtime_scene_json,
                                     AppConfig *in_out_cfg,
                                     FluidScenePreset *in_out_preset,
                                     RuntimeSceneBridgePreflight *out_summary) {
    json_object *root = NULL;

    if (!runtime_scene_json || !in_out_cfg || !in_out_preset || !out_summary) return false;
    preflight_reset(out_summary);

    root = json_tokener_parse(runtime_scene_json);
    if (!root || !json_object_is_type(root, json_type_object)) {
        preflight_diag(out_summary, "invalid JSON object");
        if (root) json_object_put(root);
        return false;
    }

    if (!validate_runtime_scene_root(root, out_summary)) {
        json_object_put(root);
        return false;
    }

    retained_scene_capture(root, out_summary);
    (void)runtime_scene_solver_projection_apply_runtime(&g_last_retained_scene,
                                                        root,
                                                        in_out_cfg,
                                                        in_out_preset,
                                                        out_summary);
    runtime_scene_bridge_append_runtime_mesh_emitters(runtime_scene_json, in_out_preset);

    preflight_diag(out_summary, "ok");
    json_object_put(root);
    return true;
}

void runtime_scene_bridge_get_last_retained_scene(PhysicsSimRetainedRuntimeScene *out_scene) {
    if (!out_scene) return;
    *out_scene = g_last_retained_scene;
}

bool runtime_scene_bridge_load_visual_bootstrap_json(const char *runtime_scene_json,
                                                     PhysicsSimRuntimeVisualBootstrap *out_bootstrap,
                                                     char *out_diagnostics,
                                                     size_t out_diagnostics_size) {
    json_object *root = NULL;
    RuntimeSceneBridgePreflight preflight = {0};

    if (out_bootstrap) memset(out_bootstrap, 0, sizeof(*out_bootstrap));
    bridge_diag(out_diagnostics, out_diagnostics_size, "invalid input");
    if (!runtime_scene_json || !out_bootstrap) return false;

    root = json_tokener_parse(runtime_scene_json);
    if (!root || !json_object_is_type(root, json_type_object)) {
        if (root) json_object_put(root);
        bridge_diag(out_diagnostics, out_diagnostics_size, "invalid JSON object");
        return false;
    }
    preflight_reset(&preflight);
    if (!validate_runtime_scene_root(root, &preflight)) {
        bridge_diag(out_diagnostics, out_diagnostics_size, preflight.diagnostics);
        json_object_put(root);
        return false;
    }

    retained_scene_capture(root, &preflight);
    out_bootstrap->valid = g_last_retained_scene.valid_contract;
    out_bootstrap->retained_scene = g_last_retained_scene;
    out_bootstrap->scene_up = retained_scene_resolve_scene_up(&g_last_retained_scene);
    if (parse_scene_domain_overlay(root, &out_bootstrap->scene_domain)) {
        out_bootstrap->scene_domain_authored = true;
    } else if (g_last_retained_scene.has_line_drawing_scene3d &&
               g_last_retained_scene.bounds.enabled) {
        out_bootstrap->scene_domain = g_last_retained_scene.bounds;
        out_bootstrap->scene_domain_authored = false;
    }
    if (runtime_scene_solver_projection_overlay_wind_tunnel(root,
                                                            NULL,
                                                            &out_bootstrap->wind_tunnel)) {
        out_bootstrap->wind_tunnel_authored = out_bootstrap->wind_tunnel.active;
    }
    (void)physics_sim_runtime_mesh_preview_scan_scene_json(runtime_scene_json,
                                                           NULL,
                                                           &out_bootstrap->mesh_previews,
                                                           NULL,
                                                           0);

    bridge_diag(out_diagnostics, out_diagnostics_size, "ok");
    json_object_put(root);
    return true;
}

bool runtime_scene_bridge_load_visual_bootstrap_file(const char *runtime_scene_path,
                                                     PhysicsSimRuntimeVisualBootstrap *out_bootstrap,
                                                     char *out_diagnostics,
                                                     size_t out_diagnostics_size) {
    CoreBuffer file_data = {0};
    CoreResult io_result;
    char *json_text = NULL;
    bool ok = false;

    if (out_bootstrap) memset(out_bootstrap, 0, sizeof(*out_bootstrap));
    bridge_diag(out_diagnostics, out_diagnostics_size, "invalid input");
    if (!runtime_scene_path || !out_bootstrap) return false;

    io_result = core_io_read_all(runtime_scene_path, &file_data);
    if (io_result.code != CORE_OK || !file_data.data || file_data.size == 0) {
        bridge_diag(out_diagnostics, out_diagnostics_size, "failed to read runtime scene file");
        core_io_buffer_free(&file_data);
        return false;
    }

    json_text = (char *)malloc(file_data.size + 1u);
    if (!json_text) {
        bridge_diag(out_diagnostics, out_diagnostics_size, "out of memory");
        core_io_buffer_free(&file_data);
        return false;
    }
    memcpy(json_text, file_data.data, file_data.size);
    json_text[file_data.size] = '\0';
    core_io_buffer_free(&file_data);

    ok = runtime_scene_bridge_load_visual_bootstrap_json(json_text,
                                                         out_bootstrap,
                                                         out_diagnostics,
                                                         out_diagnostics_size);
    if (ok) {
        (void)physics_sim_runtime_mesh_preview_scan_scene_json(json_text,
                                                               runtime_scene_path,
                                                               &out_bootstrap->mesh_previews,
                                                               NULL,
                                                               0);
    }
    free(json_text);
    return ok;
}

bool runtime_scene_bridge_apply_file(const char *runtime_scene_path,
                                     AppConfig *in_out_cfg,
                                     FluidScenePreset *in_out_preset,
                                     RuntimeSceneBridgePreflight *out_summary) {
    CoreBuffer file_data = {0};
    CoreResult io_result;
    char *json_text = NULL;
    bool ok = false;

    if (!runtime_scene_path || !in_out_cfg || !in_out_preset || !out_summary) return false;
    preflight_reset(out_summary);

    io_result = core_io_read_all(runtime_scene_path, &file_data);
    if (io_result.code != CORE_OK || !file_data.data || file_data.size == 0) {
        preflight_diag(out_summary, "failed to read runtime scene file");
        core_io_buffer_free(&file_data);
        return false;
    }

    json_text = (char *)malloc(file_data.size + 1u);
    if (!json_text) {
        preflight_diag(out_summary, "out of memory");
        core_io_buffer_free(&file_data);
        return false;
    }
    memcpy(json_text, file_data.data, file_data.size);
    json_text[file_data.size] = '\0';
    core_io_buffer_free(&file_data);

    ok = runtime_scene_bridge_apply_json(json_text, in_out_cfg, in_out_preset, out_summary);
    free(json_text);
    return ok;
}

static const char *bridge_string_field(json_object *obj, const char *key) {
    json_object *node = NULL;
    if (!obj || !json_object_is_type(obj, json_type_object) || !key) return NULL;
    if (!json_object_object_get_ex(obj, key, &node) || !json_object_is_type(node, json_type_string)) {
        return NULL;
    }
    return json_object_get_string(node);
}

static bool bridge_number_field(json_object *obj, const char *key, double *out_value) {
    json_object *node = NULL;
    if (!obj || !key || !out_value) return false;
    if (!json_object_object_get_ex(obj, key, &node) ||
        (!json_object_is_type(node, json_type_double) &&
         !json_object_is_type(node, json_type_int))) {
        return false;
    }
    *out_value = json_object_get_double(node);
    return true;
}

static bool bridge_vec3_field(json_object *obj, const char *key, CoreObjectVec3 *out_value) {
    json_object *node = NULL;
    if (!obj || !key || !out_value) return false;
    if (!json_object_object_get_ex(obj, key, &node) || !json_object_is_type(node, json_type_object)) {
        return false;
    }
    return bridge_number_field(node, "x", &out_value->x) &&
           bridge_number_field(node, "y", &out_value->y) &&
           bridge_number_field(node, "z", &out_value->z);
}

static bool bridge_text_is(const char *text, const char *a, const char *b) {
    if (!text || !text[0]) return false;
    if (a && strcmp(text, a) == 0) return true;
    if (b && strcmp(text, b) == 0) return true;
    return false;
}

static const char *bridge_runtime_mesh_behavior_from_overlay(json_object *overlay_obj) {
    const char *text = bridge_string_field(overlay_obj, "fluid_behavior");
    if (!text || !text[0]) {
        text = bridge_string_field(overlay_obj, "role");
    }
    if (bridge_text_is(text, "visual_only", "none") ||
        bridge_text_is(text, "Visual", "VisualOnly")) {
        return "visual_only";
    }
    if (bridge_text_is(text, "surface_emitter", "emitter") ||
        bridge_text_is(text, "Surface", "SurfaceEmitter")) {
        return "surface_emitter";
    }
    if (bridge_text_is(text, "surface_heat_emitter", "heat_emitter") ||
        bridge_text_is(text, "Heat", "SurfaceHeatEmitter")) {
        return "surface_heat_emitter";
    }
    if (bridge_text_is(text, "boundary_flow_emitter", "velocity_emitter") ||
        bridge_text_is(text, "Flow", "BoundaryFlowEmitter")) {
        return "boundary_flow_emitter";
    }
    return "solid_obstacle";
}

static bool bridge_runtime_mesh_behavior_is_emitter(const char *behavior) {
    return behavior &&
           (strcmp(behavior, "surface_emitter") == 0 ||
            strcmp(behavior, "surface_heat_emitter") == 0 ||
            strcmp(behavior, "boundary_flow_emitter") == 0);
}

static bool bridge_ensure_object_child(json_object *parent,
                                       const char *key,
                                       json_object **out_child,
                                       char *out_diagnostics,
                                       size_t out_diagnostics_size) {
    json_object *child = NULL;
    if (out_child) *out_child = NULL;
    if (!parent || !key || !out_child) return false;
    if (json_object_object_get_ex(parent, key, &child) && json_object_is_type(child, json_type_object)) {
        *out_child = child;
        return true;
    }
    child = json_object_new_object();
    if (!child) {
        bridge_diag(out_diagnostics, out_diagnostics_size, "out of memory");
        return false;
    }
    json_object_object_add(parent, key, child);
    *out_child = child;
    return true;
}

static json_object *bridge_find_runtime_mesh_object(json_object *runtime_root, const char *object_id) {
    json_object *objects = NULL;
    if (!runtime_root || !object_id || !object_id[0]) return NULL;
    if (!json_object_object_get_ex(runtime_root, "objects", &objects) ||
        !json_object_is_type(objects, json_type_array)) {
        return NULL;
    }
    for (size_t i = 0; i < json_object_array_length(objects); ++i) {
        json_object *object = json_object_array_get_idx(objects, i);
        const char *candidate_id = bridge_string_field(object, "object_id");
        const char *object_type = bridge_string_field(object, "object_type");
        if (candidate_id && object_type &&
            strcmp(candidate_id, object_id) == 0 &&
            strcmp(object_type, "mesh_asset_instance") == 0) {
            return object;
        }
    }
    return NULL;
}

static void bridge_add_vec3(json_object *parent, const char *key, CoreObjectVec3 value) {
    json_object *vec = json_object_new_object();
    if (!parent || !key || !vec) return;
    json_object_object_add(vec, "x", json_object_new_double(value.x));
    json_object_object_add(vec, "y", json_object_new_double(value.y));
    json_object_object_add(vec, "z", json_object_new_double(value.z));
    json_object_object_add(parent, key, vec);
}

static void bridge_add_runtime_mesh_emitter(json_object *physics_sim,
                                            json_object *overlay_emitter,
                                            const char *behavior) {
    json_object *emitter = json_object_new_object();
    const char *type = NULL;
    const char *mode_3d = NULL;
    const char *surface_3d = NULL;
    const char *obstacle_mode_3d = NULL;
    double radius = 0.08;
    double strength = 5.0;
    double thermal_buoyancy_3d = 0.0;
    CoreObjectVec3 direction = {0.0, 0.0, 1.0};
    if (!physics_sim || !emitter) {
        if (emitter) json_object_put(emitter);
        return;
    }

    if (strcmp(behavior, "boundary_flow_emitter") == 0) {
        type = "Jet";
        strength = 40.0;
        mode_3d = "SurfaceShell";
        surface_3d = "AllFaces";
        obstacle_mode_3d = "ClearAttached";
    } else if (strcmp(behavior, "surface_heat_emitter") == 0) {
        type = "Source";
        strength = 8.0;
        thermal_buoyancy_3d = 6.0;
        mode_3d = "HeatedObstacle";
        surface_3d = "AllFaces";
        obstacle_mode_3d = "ClearAttached";
    } else {
        type = "Source";
        strength = 8.0;
        mode_3d = "SurfaceShell";
        surface_3d = "AllFaces";
        obstacle_mode_3d = "ClearAttached";
    }

    if (overlay_emitter && json_object_is_type(overlay_emitter, json_type_object)) {
        const char *src_type = bridge_string_field(overlay_emitter, "type");
        const char *src_mode = bridge_string_field(overlay_emitter, "mode_3d");
        const char *src_surface = bridge_string_field(overlay_emitter, "surface_3d");
        const char *src_obstacle = bridge_string_field(overlay_emitter, "obstacle_mode_3d");
        if (src_type && src_type[0]) type = src_type;
        if (src_mode && src_mode[0]) mode_3d = src_mode;
        if (src_surface && src_surface[0]) surface_3d = src_surface;
        if (src_obstacle && src_obstacle[0]) obstacle_mode_3d = src_obstacle;
        (void)bridge_number_field(overlay_emitter, "radius", &radius);
        (void)bridge_number_field(overlay_emitter, "strength", &strength);
        (void)bridge_number_field(overlay_emitter, "thermal_buoyancy_3d", &thermal_buoyancy_3d);
        (void)bridge_vec3_field(overlay_emitter, "direction", &direction);
    }

    json_object_object_add(emitter, "active", json_object_new_boolean(1));
    json_object_object_add(emitter, "type", json_object_new_string(type));
    json_object_object_add(emitter, "radius", json_object_new_double(radius));
    json_object_object_add(emitter, "strength", json_object_new_double(strength));
    json_object_object_add(emitter, "mode_3d", json_object_new_string(mode_3d));
    json_object_object_add(emitter, "surface_3d", json_object_new_string(surface_3d));
    json_object_object_add(emitter, "obstacle_mode_3d", json_object_new_string(obstacle_mode_3d));
    json_object_object_add(emitter, "thermal_buoyancy_3d", json_object_new_double(thermal_buoyancy_3d));
    bridge_add_vec3(emitter, "direction", direction);
    json_object_object_add(physics_sim, "emitter", emitter);
}

static bool runtime_scene_bridge_apply_runtime_mesh_overlay_objects(json_object *runtime_root,
                                                                    json_object *overlay_root,
                                                                    char *out_diagnostics,
                                                                    size_t out_diagnostics_size) {
    json_object *extensions = NULL;
    json_object *physics_ext = NULL;
    json_object *mesh_overlays = NULL;
    if (!runtime_root || !overlay_root) return false;
    if (!json_object_object_get_ex(overlay_root, "extensions", &extensions) ||
        !json_object_is_type(extensions, json_type_object) ||
        !json_object_object_get_ex(extensions, "physics_sim", &physics_ext) ||
        !json_object_is_type(physics_ext, json_type_object) ||
        !json_object_object_get_ex(physics_ext, "mesh_overlays", &mesh_overlays)) {
        return true;
    }
    if (!json_object_is_type(mesh_overlays, json_type_array)) {
        bridge_diag(out_diagnostics, out_diagnostics_size, "mesh_overlays must be array");
        return false;
    }

    for (size_t i = 0; i < json_object_array_length(mesh_overlays); ++i) {
        json_object *overlay_obj = json_object_array_get_idx(mesh_overlays, i);
        json_object *runtime_object = NULL;
        json_object *object_extensions = NULL;
        json_object *object_physics = NULL;
        json_object *overlay_emitter = NULL;
        const char *object_id = NULL;
        const char *behavior = NULL;
        if (!overlay_obj || !json_object_is_type(overlay_obj, json_type_object)) continue;
        object_id = bridge_string_field(overlay_obj, "object_id");
        if (!object_id || !object_id[0]) continue;
        runtime_object = bridge_find_runtime_mesh_object(runtime_root, object_id);
        if (!runtime_object) continue;

        behavior = bridge_runtime_mesh_behavior_from_overlay(overlay_obj);
        if (!bridge_ensure_object_child(runtime_object,
                                        "extensions",
                                        &object_extensions,
                                        out_diagnostics,
                                        out_diagnostics_size) ||
            !bridge_ensure_object_child(object_extensions,
                                        "physics_sim",
                                        &object_physics,
                                        out_diagnostics,
                                        out_diagnostics_size)) {
            return false;
        }

        json_object_object_add(object_physics, "fluid_behavior", json_object_new_string(behavior));
        json_object_object_add(object_physics,
                               "fluid_obstacle",
                               json_object_new_boolean(strcmp(behavior, "solid_obstacle") == 0 ? 1 : 0));
        if (bridge_runtime_mesh_behavior_is_emitter(behavior)) {
            (void)json_object_object_get_ex(overlay_obj, "emitter", &overlay_emitter);
            bridge_add_runtime_mesh_emitter(object_physics, overlay_emitter, behavior);
        } else {
            json_object_object_del(object_physics, "emitter");
        }
    }
    bridge_diag(out_diagnostics, out_diagnostics_size, "ok");
    return true;
}

bool runtime_scene_bridge_writeback_runtime_mesh_overlays_json(const char *runtime_scene_json,
                                                               const char *overlay_json,
                                                               char **out_runtime_scene_json,
                                                               char *out_diagnostics,
                                                               size_t out_diagnostics_size) {
    json_object *runtime_root = NULL;
    json_object *overlay_root = NULL;
    const char *serialized = NULL;
    char *out = NULL;
    size_t out_len = 0;

    if (out_runtime_scene_json) *out_runtime_scene_json = NULL;
    bridge_diag(out_diagnostics, out_diagnostics_size, "invalid input");
    if (!runtime_scene_json || !overlay_json || !out_runtime_scene_json) return false;

    runtime_root = json_tokener_parse(runtime_scene_json);
    overlay_root = json_tokener_parse(overlay_json);
    if (!runtime_root || !json_object_is_type(runtime_root, json_type_object) ||
        !overlay_root || !json_object_is_type(overlay_root, json_type_object)) {
        bridge_diag(out_diagnostics, out_diagnostics_size, "invalid JSON object");
        if (runtime_root) json_object_put(runtime_root);
        if (overlay_root) json_object_put(overlay_root);
        return false;
    }
    if (!validate_runtime_scene_root_diag(runtime_root, out_diagnostics, out_diagnostics_size) ||
        !runtime_scene_bridge_apply_runtime_mesh_overlay_objects(runtime_root,
                                                                 overlay_root,
                                                                 out_diagnostics,
                                                                 out_diagnostics_size)) {
        json_object_put(runtime_root);
        json_object_put(overlay_root);
        return false;
    }

    serialized = json_object_to_json_string_ext(runtime_root,
                                                JSON_C_TO_STRING_PRETTY | JSON_C_TO_STRING_NOSLASHESCAPE);
    if (!serialized) {
        bridge_diag(out_diagnostics, out_diagnostics_size, "failed to serialize mesh overlay writeback");
        json_object_put(runtime_root);
        json_object_put(overlay_root);
        return false;
    }
    out_len = strlen(serialized);
    out = (char *)malloc(out_len + 1u);
    if (!out) {
        bridge_diag(out_diagnostics, out_diagnostics_size, "out of memory");
        json_object_put(runtime_root);
        json_object_put(overlay_root);
        return false;
    }
    memcpy(out, serialized, out_len + 1u);
    *out_runtime_scene_json = out;
    bridge_diag(out_diagnostics, out_diagnostics_size, "ok");
    json_object_put(runtime_root);
    json_object_put(overlay_root);
    return true;
}

bool runtime_scene_bridge_writeback_physics_overlay_json(const char *runtime_scene_json,
                                                         const char *overlay_json,
                                                         char **out_runtime_scene_json,
                                                         char *out_diagnostics,
                                                         size_t out_diagnostics_size) {
    json_object *runtime_root = NULL;
    json_object *overlay_root = NULL;
    const char *serialized = NULL;
    char *out = NULL;
    size_t out_len = 0;

    if (out_runtime_scene_json) *out_runtime_scene_json = NULL;
    bridge_diag(out_diagnostics, out_diagnostics_size, "invalid input");
    if (!runtime_scene_json || !overlay_json || !out_runtime_scene_json) return false;

    runtime_root = json_tokener_parse(runtime_scene_json);
    overlay_root = json_tokener_parse(overlay_json);
    if (!runtime_root || !json_object_is_type(runtime_root, json_type_object) ||
        !overlay_root || !json_object_is_type(overlay_root, json_type_object)) {
        bridge_diag(out_diagnostics, out_diagnostics_size, "invalid JSON object");
        if (runtime_root) json_object_put(runtime_root);
        if (overlay_root) json_object_put(overlay_root);
        return false;
    }

    if (!validate_runtime_scene_root_diag(runtime_root, out_diagnostics, out_diagnostics_size)) {
        json_object_put(runtime_root);
        json_object_put(overlay_root);
        return false;
    }
    if (!core_scene_overlay_merge_apply(runtime_root,
                                        overlay_root,
                                        "physics_sim",
                                        "physics_sim",
                                        out_diagnostics,
                                        out_diagnostics_size)) {
        json_object_put(runtime_root);
        json_object_put(overlay_root);
        return false;
    }
    if (!runtime_scene_bridge_apply_runtime_mesh_overlay_objects(runtime_root,
                                                                 overlay_root,
                                                                 out_diagnostics,
                                                                 out_diagnostics_size)) {
        json_object_put(runtime_root);
        json_object_put(overlay_root);
        return false;
    }

    serialized = json_object_to_json_string_ext(runtime_root,
                                                JSON_C_TO_STRING_PRETTY | JSON_C_TO_STRING_NOSLASHESCAPE);
    if (!serialized) {
        bridge_diag(out_diagnostics, out_diagnostics_size, "failed to serialize merged runtime scene");
        json_object_put(runtime_root);
        json_object_put(overlay_root);
        return false;
    }

    out_len = strlen(serialized);
    out = (char *)malloc(out_len + 1u);
    if (!out) {
        bridge_diag(out_diagnostics, out_diagnostics_size, "out of memory");
        json_object_put(runtime_root);
        json_object_put(overlay_root);
        return false;
    }
    memcpy(out, serialized, out_len + 1u);
    *out_runtime_scene_json = out;
    bridge_diag(out_diagnostics, out_diagnostics_size, "ok");

    json_object_put(runtime_root);
    json_object_put(overlay_root);
    return true;
}

const char *physics_sim_runtime_scene_up_source_label(PhysicsSimRuntimeSceneUpSource source) {
    switch (source) {
    case PHYSICS_SIM_RUNTIME_SCENE_UP_CONSTRUCTION_PLANE_FRAME:
        return "construction-plane-frame";
    case PHYSICS_SIM_RUNTIME_SCENE_UP_CONSTRUCTION_PLANE_AXIS:
        return "construction-plane-axis";
    case PHYSICS_SIM_RUNTIME_SCENE_UP_FALLBACK_POSITIVE_Z:
        return "fallback-+z";
    case PHYSICS_SIM_RUNTIME_SCENE_UP_STANDALONE_WATER:
        return "standalone-water";
    case PHYSICS_SIM_RUNTIME_SCENE_UP_NONE:
    default:
        return "none";
    }
}
