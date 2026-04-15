#include "import/runtime_scene_bridge.h"
#include "import/runtime_scene_solver_projection.h"

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

    preflight_diag(out_summary, "ok");
    json_object_put(root);
    return true;
}

void runtime_scene_bridge_get_last_retained_scene(PhysicsSimRetainedRuntimeScene *out_scene) {
    if (!out_scene) return;
    *out_scene = g_last_retained_scene;
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
