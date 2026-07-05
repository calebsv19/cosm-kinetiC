#include "runtime_scene_bridge_contract_session_suite.h"
#include "import/runtime_scene_bridge.h"
#include "app/scene_objects.h"
#include "app/editor/scene_editor_session.h"

#include <json-c/json.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *read_text_file_alloc(const char *path, size_t *out_size) {
    FILE *f = NULL;
    long sz = 0;
    size_t got = 0;
    char *buf = NULL;
    if (out_size) *out_size = 0;
    if (!path) return NULL;
    f = fopen(path, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }
    sz = ftell(f);
    if (sz < 0) {
        fclose(f);
        return NULL;
    }
    rewind(f);
    buf = (char *)malloc((size_t)sz + 1u);
    if (!buf) {
        fclose(f);
        return NULL;
    }
    got = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    if (got != (size_t)sz) {
        free(buf);
        return NULL;
    }
    buf[sz] = '\0';
    if (out_size) *out_size = (size_t)sz;
    return buf;
}

static bool write_text_file(const char *path, const char *text) {
    FILE *f = NULL;
    size_t len = 0;
    size_t wrote = 0;
    if (!path || !text) return false;
    f = fopen(path, "wb");
    if (!f) return false;
    len = strlen(text);
    wrote = fwrite(text, 1, len, f);
    fclose(f);
    return wrote == len;
}

static json_object *test_json_object_child(json_object *parent, const char *key) {
    json_object *child = NULL;
    if (!parent || !key) return NULL;
    if (!json_object_object_get_ex(parent, key, &child) ||
        !json_object_is_type(child, json_type_object)) {
        return NULL;
    }
    return child;
}

static const char *test_json_string_child(json_object *parent, const char *key) {
    json_object *child = NULL;
    if (!parent || !key) return NULL;
    if (!json_object_object_get_ex(parent, key, &child) ||
        !json_object_is_type(child, json_type_string)) {
        return NULL;
    }
    return json_object_get_string(child);
}

static bool test_json_bool_child(json_object *parent, const char *key, bool *out_value) {
    json_object *child = NULL;
    if (out_value) *out_value = false;
    if (!parent || !key || !out_value) return false;
    if (!json_object_object_get_ex(parent, key, &child) ||
        !json_object_is_type(child, json_type_boolean)) {
        return false;
    }
    *out_value = json_object_get_boolean(child) != 0;
    return true;
}

static bool test_json_double_child(json_object *parent, const char *key, double *out_value) {
    json_object *child = NULL;
    if (out_value) *out_value = 0.0;
    if (!parent || !key || !out_value) return false;
    if (!json_object_object_get_ex(parent, key, &child) ||
        !json_object_is_type(child, json_type_double)) {
        return false;
    }
    *out_value = json_object_get_double(child);
    return true;
}

bool test_scene_editor_session_bootstrap_preserves_retained_scene(void) {
    RuntimeSceneBridgePreflight summary;
    PhysicsSimRetainedRuntimeScene retained = {0};
    SceneEditorBootstrap bootstrap = {0};
    PhysicsSimEditorSession session = {0};
    AppConfig cfg = app_config_default();
    const FluidScenePreset *base = scene_presets_get_default();
    FluidScenePreset preset = base ? *base : (FluidScenePreset){0};

    if (!runtime_scene_bridge_apply_file("tests/fixtures/runtime_scene_primitive_retained.json",
                                         &cfg,
                                         &preset,
                                         &summary)) {
        return false;
    }

    runtime_scene_bridge_get_last_retained_scene(&retained);
    if (!retained.valid_contract) return false;

    bootstrap.has_retained_scene = true;
    bootstrap.retained_scene = retained;
    physics_sim_editor_session_init(&session, &preset, &bootstrap);

    if (!physics_sim_editor_session_has_retained_scene(&session)) return false;
    if (!session.using_legacy_preset_adapter) return false;
    if (physics_sim_editor_session_retained_object_count(&session) != 2) return false;
    if (strcmp(physics_sim_editor_session_scene_id(&session), "scene_physics_retained_primitives") != 0) return false;

    const CoreSceneObjectContract *selected = physics_sim_editor_session_selected_object(&session);
    const PhysicsSimObjectOverlay *selected_overlay = physics_sim_editor_session_selected_object_overlay(&session);
    const PhysicsSimDomainOverlay *scene_domain = physics_sim_editor_session_scene_domain(&session);
    if (!selected) return false;
    if (strcmp(selected->object.object_id, "obj3d_1") != 0) return false;
    if (selected->kind != CORE_SCENE_OBJECT_KIND_PLANE_PRIMITIVE) return false;
    if (!session.has_physics_overlay) return false;
    if (session.physics_overlay.object_overlay_count != 2) return false;
    if (!selected_overlay) return false;
    if (strcmp(selected_overlay->object_id, "obj3d_1") != 0) return false;
    if (selected_overlay->motion_mode != PHYSICS_SIM_OVERLAY_MOTION_DYNAMIC) return false;
    if (fabs(selected_overlay->initial_velocity.x) > 1e-9) return false;
    if (fabs(selected_overlay->initial_velocity.y) > 1e-9) return false;
    if (fabs(selected_overlay->initial_velocity.z) > 1e-9) return false;
    if (!scene_domain || !scene_domain->active) return false;
    if (!scene_domain->seeded_from_retained_bounds) return false;
    if (fabs(scene_domain->min.x - (-12.0)) > 1e-9) return false;
    if (fabs(scene_domain->max.z - 8.0) > 1e-9) return false;

    return true;
}

bool test_scene_editor_session_overlay_defaults_respect_locked_objects(void) {
    SceneEditorBootstrap bootstrap = {0};
    PhysicsSimEditorSession session = {0};

    bootstrap.has_retained_scene = true;
    bootstrap.retained_scene.valid_contract = true;
    snprintf(bootstrap.retained_scene.root.scene_id,
             sizeof(bootstrap.retained_scene.root.scene_id),
             "scene_overlay_defaults");
    bootstrap.retained_scene.retained_object_count = 1;
    snprintf(bootstrap.retained_scene.objects[0].object.object_id,
             sizeof(bootstrap.retained_scene.objects[0].object.object_id),
             "obj_locked");
    bootstrap.retained_scene.objects[0].object.flags.locked = true;

    physics_sim_editor_session_init(&session, NULL, &bootstrap);

    {
        const PhysicsSimObjectOverlay *selected_overlay = physics_sim_editor_session_selected_object_overlay(&session);
        if (!selected_overlay) return false;
        if (selected_overlay->motion_mode != PHYSICS_SIM_OVERLAY_MOTION_STATIC) return false;
        if (strcmp(physics_sim_editor_session_motion_mode_label(selected_overlay->motion_mode), "Static") != 0) {
            return false;
        }
    }

    return true;
}

bool test_scene_editor_session_overlay_mutation_updates_selected_object(void) {
    SceneEditorBootstrap bootstrap = {0};
    PhysicsSimEditorSession session = {0};

    bootstrap.has_retained_scene = true;
    bootstrap.retained_scene.valid_contract = true;
    snprintf(bootstrap.retained_scene.root.scene_id,
             sizeof(bootstrap.retained_scene.root.scene_id),
             "scene_overlay_mutation");
    bootstrap.retained_scene.retained_object_count = 1;
    snprintf(bootstrap.retained_scene.objects[0].object.object_id,
             sizeof(bootstrap.retained_scene.objects[0].object.object_id),
             "obj_mutate");

    physics_sim_editor_session_init(&session, NULL, &bootstrap);

    if (!physics_sim_editor_session_set_selected_motion_mode(&session, PHYSICS_SIM_OVERLAY_MOTION_STATIC)) {
        return false;
    }
    if (!physics_sim_editor_session_nudge_selected_velocity(&session, 0.25, -0.50, 1.25)) {
        return false;
    }
    if (!physics_sim_editor_session_set_selected_emitter_type(&session, EMITTER_VELOCITY_JET, true)) {
        return false;
    }
    if (!physics_sim_editor_session_cycle_selected_emitter_source_mode_3d(&session)) {
        return false;
    }
    if (!physics_sim_editor_session_cycle_selected_emitter_surface_3d(&session)) {
        return false;
    }
    if (!physics_sim_editor_session_cycle_selected_emitter_obstacle_mode_3d(&session)) {
        return false;
    }
    if (!physics_sim_editor_session_set_selected_emitter_radius(&session, 0.44f)) {
        return false;
    }
    if (!physics_sim_editor_session_set_selected_emitter_strength(&session, 96.5f)) {
        return false;
    }
    if (!physics_sim_editor_session_set_selected_emitter_thermal_buoyancy_3d(&session, 4.5f)) {
        return false;
    }

    {
        const PhysicsSimObjectOverlay *selected_overlay = physics_sim_editor_session_selected_object_overlay(&session);
        const PhysicsSimEmitterOverlay *selected_emitter = physics_sim_editor_session_selected_object_emitter(&session);
        if (!selected_overlay) return false;
        if (selected_overlay->motion_mode != PHYSICS_SIM_OVERLAY_MOTION_DYNAMIC) return false;
        if (fabs(selected_overlay->initial_velocity.x - 0.25) > 1e-9) return false;
        if (fabs(selected_overlay->initial_velocity.y - (-0.50)) > 1e-9) return false;
        if (fabs(selected_overlay->initial_velocity.z - 1.25) > 1e-9) return false;
        if (!selected_emitter) return false;
        if (selected_emitter->type != EMITTER_VELOCITY_JET) return false;
        if (selected_emitter->source_mode_3d != EMITTER_3D_SOURCE_MODE_SURFACE_SHELL) return false;
        if (selected_emitter->surface_3d != EMITTER_3D_SURFACE_BOTTOM) return false;
        if (selected_emitter->obstacle_mode_3d != EMITTER_3D_OBSTACLE_MODE_CLEAR_ATTACHED) return false;
        if (fabs(selected_emitter->radius - 0.44) > 1e-6) return false;
        if (fabs(selected_emitter->strength - 96.5) > 1e-9) return false;
        if (fabs(selected_emitter->thermal_buoyancy_3d - 4.5) > 1e-6) return false;
        if (strcmp(physics_sim_editor_session_emitter_type_label(selected_emitter->type), "Jet") != 0) return false;
    }

    if (!physics_sim_editor_session_reset_selected_velocity(&session)) return false;
    if (!physics_sim_editor_session_set_selected_emitter_type(&session, EMITTER_VELOCITY_JET, true)) {
        return false;
    }
    {
        const PhysicsSimObjectOverlay *selected_overlay = physics_sim_editor_session_selected_object_overlay(&session);
        const PhysicsSimEmitterOverlay *selected_emitter = physics_sim_editor_session_selected_object_emitter(&session);
        if (!selected_overlay) return false;
        if (fabs(selected_overlay->initial_velocity.x) > 1e-9) return false;
        if (fabs(selected_overlay->initial_velocity.y) > 1e-9) return false;
        if (fabs(selected_overlay->initial_velocity.z) > 1e-9) return false;
        if (selected_emitter) return false;
    }

    return true;
}

bool test_scene_editor_session_retained_emitter_defaults_to_object_normal(void) {
    SceneEditorBootstrap bootstrap = {0};
    PhysicsSimEditorSession session = {0};
    const PhysicsSimEmitterOverlay *selected_emitter = NULL;

    bootstrap.has_retained_scene = true;
    bootstrap.retained_scene.valid_contract = true;
    snprintf(bootstrap.retained_scene.root.scene_id,
             sizeof(bootstrap.retained_scene.root.scene_id),
             "scene_emitter_normal_default");
    bootstrap.retained_scene.retained_object_count = 1;
    snprintf(bootstrap.retained_scene.objects[0].object.object_id,
             sizeof(bootstrap.retained_scene.objects[0].object.object_id),
             "obj_prism");
    bootstrap.retained_scene.objects[0].kind = CORE_SCENE_OBJECT_KIND_RECT_PRISM_PRIMITIVE;
    bootstrap.retained_scene.objects[0].has_rect_prism_primitive = true;
    bootstrap.retained_scene.objects[0].rect_prism_primitive.width = 2.0;
    bootstrap.retained_scene.objects[0].rect_prism_primitive.height = 1.5;
    bootstrap.retained_scene.objects[0].rect_prism_primitive.depth = 2.5;
    bootstrap.retained_scene.objects[0].rect_prism_primitive.frame.normal = (CoreObjectVec3){0.0, 0.0, 1.0};

    physics_sim_editor_session_init(&session, NULL, &bootstrap);
    if (!physics_sim_editor_session_set_selected_emitter_type(&session, EMITTER_VELOCITY_JET, true)) {
        return false;
    }

    selected_emitter = physics_sim_editor_session_selected_object_emitter(&session);
    if (!selected_emitter) return false;
    if (fabs(selected_emitter->direction.x - 0.0) > 1e-9) return false;
    if (fabs(selected_emitter->direction.y - 0.0) > 1e-9) return false;
    if (fabs(selected_emitter->direction.z - 1.0) > 1e-9) return false;
    if (selected_emitter->source_mode_3d != EMITTER_3D_SOURCE_MODE_SURFACE_PATCH) return false;
    if (selected_emitter->surface_3d != EMITTER_3D_SURFACE_TOP) return false;
    if (selected_emitter->obstacle_mode_3d != EMITTER_3D_OBSTACLE_MODE_RETAIN_ATTACHED) return false;
    return true;
}

bool test_scene_editor_session_overlay_json_build_and_merge(void) {
    RuntimeSceneBridgePreflight summary;
    PhysicsSimRetainedRuntimeScene retained = {0};
    SceneEditorBootstrap bootstrap = {0};
    PhysicsSimEditorSession session = {0};
    PhysicsSimEditorSession rehydrated = {0};
    AppConfig cfg = app_config_default();
    const FluidScenePreset *base = scene_presets_get_default();
    FluidScenePreset preset = base ? *base : (FluidScenePreset){0};
    size_t runtime_size = 0;
    char *runtime_json = read_text_file_alloc("tests/fixtures/runtime_scene_primitive_retained.json", &runtime_size);
    char *overlay_json = NULL;
    char *merged_json = NULL;
    char diagnostics[256];
    bool ok = false;

    if (!runtime_json || runtime_size == 0) {
        free(runtime_json);
        return false;
    }
    if (!runtime_scene_bridge_apply_json(runtime_json, &cfg, &preset, &summary)) {
        free(runtime_json);
        return false;
    }

    runtime_scene_bridge_get_last_retained_scene(&retained);
    if (!retained.valid_contract) {
        free(runtime_json);
        return false;
    }

    bootstrap.has_retained_scene = true;
    bootstrap.retained_scene = retained;
    physics_sim_editor_session_init(&session, &preset, &bootstrap);
    physics_sim_editor_session_select_retained_index(&session, 1);
    if (!physics_sim_editor_session_set_selected_motion_mode(&session, PHYSICS_SIM_OVERLAY_MOTION_STATIC)) {
        free(runtime_json);
        return false;
    }
    if (!physics_sim_editor_session_nudge_selected_velocity(&session, 1.25, -0.75, 0.50)) {
        free(runtime_json);
        return false;
    }
    if (!physics_sim_editor_session_set_selected_emitter_type(&session, EMITTER_VELOCITY_JET, true)) {
        free(runtime_json);
        return false;
    }
    if (!physics_sim_editor_session_cycle_selected_emitter_source_mode_3d(&session)) {
        free(runtime_json);
        return false;
    }
    if (!physics_sim_editor_session_cycle_selected_emitter_surface_3d(&session)) {
        free(runtime_json);
        return false;
    }
    if (!physics_sim_editor_session_cycle_selected_emitter_obstacle_mode_3d(&session)) {
        free(runtime_json);
        return false;
    }
    if (!physics_sim_editor_session_set_selected_emitter_radius(&session, 0.41f)) {
        free(runtime_json);
        return false;
    }
    if (!physics_sim_editor_session_set_selected_emitter_strength(&session, 96.5f)) {
        free(runtime_json);
        return false;
    }
    if (!physics_sim_editor_session_set_selected_emitter_thermal_buoyancy_3d(&session, 3.5f)) {
        free(runtime_json);
        return false;
    }
    if (!physics_sim_editor_session_set_scene_domain_size(&session, 18.0, 12.0, 6.0)) {
        free(runtime_json);
        return false;
    }
    if (!physics_sim_editor_session_build_overlay_json(&session,
                                                       &overlay_json,
                                                       diagnostics,
                                                       sizeof(diagnostics))) {
        free(runtime_json);
        free(overlay_json);
        return false;
    }
    ok = runtime_scene_bridge_writeback_physics_overlay_json(runtime_json,
                                                             overlay_json,
                                                             &merged_json,
                                                             diagnostics,
                                                             sizeof(diagnostics));
    free(runtime_json);
    if (!ok || !merged_json) {
        free(overlay_json);
        free(merged_json);
        return false;
    }

    physics_sim_editor_session_init(&rehydrated, &preset, &bootstrap);
    if (!physics_sim_editor_session_hydrate_overlay_from_runtime_scene_json(&rehydrated,
                                                                            merged_json,
                                                                            diagnostics,
                                                                            sizeof(diagnostics))) {
        free(overlay_json);
        free(merged_json);
        return false;
    }

    ok = strstr(overlay_json, "\"overlay_variant\"") != NULL &&
         strstr(overlay_json, "retained_object_overlay_v1") != NULL &&
         strstr(overlay_json, "\"scene_domain\"") != NULL &&
         strstr(overlay_json, "\"seeded_from_retained_bounds\":false") != NULL &&
         strstr(overlay_json, "\"object_id\"") != NULL &&
         strstr(overlay_json, "obj3d_2") != NULL &&
         strstr(overlay_json, "\"motion_mode\"") != NULL &&
         strstr(overlay_json, "Dynamic") != NULL &&
         strstr(overlay_json, "\"emitter\"") != NULL &&
         strstr(overlay_json, "\"type\"") != NULL &&
         strstr(overlay_json, "Jet") != NULL &&
         strstr(overlay_json, "\"mode_3d\"") != NULL &&
         strstr(overlay_json, "SurfaceShell") != NULL &&
         strstr(overlay_json, "\"surface_3d\"") != NULL &&
         strstr(overlay_json, "Bottom") != NULL &&
         strstr(overlay_json, "\"obstacle_mode_3d\"") != NULL &&
         strstr(overlay_json, "ClearAttached") != NULL &&
         strstr(overlay_json, "\"thermal_buoyancy_3d\"") != NULL &&
         strstr(overlay_json, "96.5") != NULL &&
         strstr(overlay_json, "\"logical_clock\"") != NULL &&
         strstr(merged_json, "\"physics_sim\"") != NULL &&
         strstr(merged_json, "\"scene_domain\"") != NULL &&
         strstr(merged_json, "\"object_overlays\"") != NULL &&
         strstr(merged_json, "obj3d_2") != NULL;

    if (ok) {
        const PhysicsSimDomainOverlay *scene_domain = physics_sim_editor_session_scene_domain(&rehydrated);
        double width = 0.0;
        double height = 0.0;
        double depth = 0.0;
        if (!scene_domain || scene_domain->seeded_from_retained_bounds) {
            ok = false;
        } else {
            physics_sim_editor_session_scene_domain_dimensions(&rehydrated, &width, &height, &depth);
            if (fabs(width - 18.0) > 1e-9 ||
                fabs(height - 12.0) > 1e-9 ||
                fabs(depth - 6.0) > 1e-9) {
                ok = false;
            }
        }
    }

    free(overlay_json);
    free(merged_json);
    return ok;
}

bool test_runtime_scene_bridge_apply_merged_overlay_affects_solver_mapping(void) {
    RuntimeSceneBridgePreflight summary = {0};
    PhysicsSimRetainedRuntimeScene retained = {0};
    SceneEditorBootstrap bootstrap = {0};
    PhysicsSimEditorSession session = {0};
    AppConfig cfg = app_config_default();
    const FluidScenePreset *base = scene_presets_get_default();
    FluidScenePreset preset = base ? *base : (FluidScenePreset){0};
    char *runtime_json = NULL;
    char *overlay_json = NULL;
    char *merged_json = NULL;
    char diagnostics[256];
    size_t runtime_size = 0;
    bool ok = false;

    runtime_json = read_text_file_alloc("tests/fixtures/runtime_scene_primitive_retained.json", &runtime_size);
    if (!runtime_json || runtime_size == 0) {
        free(runtime_json);
        return false;
    }
    if (!runtime_scene_bridge_apply_json(runtime_json, &cfg, &preset, &summary)) {
        free(runtime_json);
        return false;
    }
    runtime_scene_bridge_get_last_retained_scene(&retained);
    if (!retained.valid_contract) {
        free(runtime_json);
        return false;
    }

    bootstrap.has_retained_scene = true;
    bootstrap.retained_scene = retained;
    physics_sim_editor_session_init(&session, &preset, &bootstrap);
    physics_sim_editor_session_select_retained_index(&session, 0);
    if (!physics_sim_editor_session_set_selected_motion_mode(&session, PHYSICS_SIM_OVERLAY_MOTION_STATIC)) {
        free(runtime_json);
        return false;
    }
    physics_sim_editor_session_select_retained_index(&session, 1);
    if (!physics_sim_editor_session_set_selected_motion_mode(&session, PHYSICS_SIM_OVERLAY_MOTION_DYNAMIC)) {
        free(runtime_json);
        return false;
    }
    if (!physics_sim_editor_session_build_overlay_json(&session,
                                                       &overlay_json,
                                                       diagnostics,
                                                       sizeof(diagnostics))) {
        free(runtime_json);
        free(overlay_json);
        return false;
    }
    if (!runtime_scene_bridge_writeback_physics_overlay_json(runtime_json,
                                                             overlay_json,
                                                             &merged_json,
                                                             diagnostics,
                                                             sizeof(diagnostics))) {
        free(runtime_json);
        free(overlay_json);
        free(merged_json);
        return false;
    }
    cfg = app_config_default();
    preset = base ? *base : (FluidScenePreset){0};
    ok = runtime_scene_bridge_apply_json(merged_json, &cfg, &preset, &summary);
    free(runtime_json);
    free(overlay_json);
    free(merged_json);
    if (!ok) return false;
    if (preset.object_count != 2) return false;
    if (!preset.objects[0].is_static) return false;
    if (preset.objects[0].gravity_enabled) return false;
    if (preset.objects[1].is_static) return false;
    if (!preset.objects[1].gravity_enabled) return false;
    return true;
}

bool test_runtime_scene_bridge_apply_merged_scene_domain_affects_solver_domain(void) {
    RuntimeSceneBridgePreflight summary = {0};
    PhysicsSimRetainedRuntimeScene retained = {0};
    SceneEditorBootstrap bootstrap = {0};
    PhysicsSimEditorSession session = {0};
    AppConfig cfg = app_config_default();
    const FluidScenePreset *base = scene_presets_get_default();
    FluidScenePreset preset = base ? *base : (FluidScenePreset){0};
    char *runtime_json = NULL;
    char *overlay_json = NULL;
    char *merged_json = NULL;
    char diagnostics[256];
    size_t runtime_size = 0;
    bool ok = false;

    runtime_json = read_text_file_alloc("tests/fixtures/runtime_scene_primitive_retained.json", &runtime_size);
    if (!runtime_json || runtime_size == 0) {
        free(runtime_json);
        return false;
    }
    if (!runtime_scene_bridge_apply_json(runtime_json, &cfg, &preset, &summary)) {
        free(runtime_json);
        return false;
    }
    runtime_scene_bridge_get_last_retained_scene(&retained);
    if (!retained.valid_contract) {
        free(runtime_json);
        return false;
    }

    bootstrap.has_retained_scene = true;
    bootstrap.retained_scene = retained;
    physics_sim_editor_session_init(&session, &preset, &bootstrap);
    if (!physics_sim_editor_session_set_scene_domain_size(&session, 18.0, 12.0, 6.0)) {
        free(runtime_json);
        return false;
    }
    if (!physics_sim_editor_session_build_overlay_json(&session,
                                                       &overlay_json,
                                                       diagnostics,
                                                       sizeof(diagnostics))) {
        free(runtime_json);
        free(overlay_json);
        return false;
    }
    if (!runtime_scene_bridge_writeback_physics_overlay_json(runtime_json,
                                                             overlay_json,
                                                             &merged_json,
                                                             diagnostics,
                                                             sizeof(diagnostics))) {
        free(runtime_json);
        free(overlay_json);
        free(merged_json);
        return false;
    }
    cfg = app_config_default();
    preset = base ? *base : (FluidScenePreset){0};
    ok = runtime_scene_bridge_apply_json(merged_json, &cfg, &preset, &summary);
    free(runtime_json);
    free(overlay_json);
    free(merged_json);
    if (!ok) return false;
    if (preset.domain != SCENE_DOMAIN_STRUCTURAL) return false;
    if (fabsf(preset.domain_width - 18.0f) > 1e-6f) return false;
    if (fabsf(preset.domain_height - 12.0f) > 1e-6f) return false;
    return true;
}

bool test_runtime_scene_bridge_apply_merged_emitter_overlay_affects_solver_emitters(void) {
    RuntimeSceneBridgePreflight summary = {0};
    PhysicsSimRetainedRuntimeScene retained = {0};
    SceneEditorBootstrap bootstrap = {0};
    PhysicsSimEditorSession session = {0};
    AppConfig cfg = app_config_default();
    const FluidScenePreset *base = scene_presets_get_default();
    FluidScenePreset preset = base ? *base : (FluidScenePreset){0};
    char *runtime_json = NULL;
    char *overlay_json = NULL;
    char *merged_json = NULL;
    char diagnostics[256];
    size_t runtime_size = 0;
    bool ok = false;

    runtime_json = read_text_file_alloc("config/samples/ps4d_runtime_scene_visual_test.json", &runtime_size);
    if (!runtime_json || runtime_size == 0) {
        free(runtime_json);
        return false;
    }
    if (!runtime_scene_bridge_apply_json(runtime_json, &cfg, &preset, &summary)) {
        free(runtime_json);
        return false;
    }
    runtime_scene_bridge_get_last_retained_scene(&retained);
    if (!retained.valid_contract) {
        free(runtime_json);
        return false;
    }

    bootstrap.has_retained_scene = true;
    bootstrap.retained_scene = retained;
    physics_sim_editor_session_init(&session, &preset, &bootstrap);
    physics_sim_editor_session_select_retained_index(&session, 1);
    if (!physics_sim_editor_session_set_selected_emitter_type(&session, EMITTER_VELOCITY_JET, true)) {
        free(runtime_json);
        return false;
    }
    if (!physics_sim_editor_session_set_selected_emitter_strength(&session, 96.5f)) {
        free(runtime_json);
        return false;
    }
    if (!physics_sim_editor_session_build_overlay_json(&session,
                                                       &overlay_json,
                                                       diagnostics,
                                                       sizeof(diagnostics))) {
        free(runtime_json);
        free(overlay_json);
        return false;
    }
    if (!runtime_scene_bridge_writeback_physics_overlay_json(runtime_json,
                                                             overlay_json,
                                                             &merged_json,
                                                             diagnostics,
                                                             sizeof(diagnostics))) {
        free(runtime_json);
        free(overlay_json);
        free(merged_json);
        return false;
    }

    cfg = app_config_default();
    preset = base ? *base : (FluidScenePreset){0};
    ok = runtime_scene_bridge_apply_json(merged_json, &cfg, &preset, &summary);
    free(runtime_json);
    free(overlay_json);
    free(merged_json);
    if (!ok) return false;
    if (preset.emitter_count != 1) return false;
    if (preset.emitters[0].type != EMITTER_VELOCITY_JET) return false;
    if (preset.emitters[0].attached_object != 1) return false;
    if (preset.emitters[0].attached_import != -1) return false;
    if (fabsf(preset.emitters[0].strength - 96.5f) > 1e-6f) return false;
    if (fabsf(preset.emitters[0].position_x - 0.5f) > 1e-6f) return false;
    if (fabsf(preset.emitters[0].position_y - 0.5f) > 1e-6f) return false;
    if (fabsf(preset.emitters[0].position_z - 0.25f) > 1e-6f) return false;
    return true;
}

bool test_scene_editor_session_roundtrip_reopen_hydrates_saved_overlay(void) {
    const char *saved_path = "/tmp/physics_sim_save5_roundtrip_scene.json";
    RuntimeSceneBridgePreflight summary = {0};
    PhysicsSimRetainedRuntimeScene retained = {0};
    SceneEditorBootstrap bootstrap = {0};
    PhysicsSimEditorSession saved_session = {0};
    PhysicsSimEditorSession reopened_session = {0};
    AppConfig cfg = app_config_default();
    const FluidScenePreset *base = scene_presets_get_default();
    FluidScenePreset preset = base ? *base : (FluidScenePreset){0};
    FluidScenePreset reopened_preset = base ? *base : (FluidScenePreset){0};
    char *runtime_json = NULL;
    char *overlay_json = NULL;
    char *merged_json = NULL;
    char *reopened_json = NULL;
    char diagnostics[256];
    size_t runtime_size = 0;
    size_t reopened_size = 0;
    bool ok = false;

    runtime_json = read_text_file_alloc("config/samples/ps4d_runtime_scene_visual_test.json", &runtime_size);
    if (!runtime_json || runtime_size == 0) {
        free(runtime_json);
        return false;
    }
    if (!runtime_scene_bridge_apply_json(runtime_json, &cfg, &preset, &summary)) {
        free(runtime_json);
        return false;
    }
    runtime_scene_bridge_get_last_retained_scene(&retained);
    if (!retained.valid_contract) {
        free(runtime_json);
        return false;
    }

    bootstrap.has_retained_scene = true;
    bootstrap.retained_scene = retained;
    physics_sim_editor_session_init(&saved_session, &preset, &bootstrap);
    physics_sim_editor_session_select_retained_index(&saved_session, 1);
    if (!physics_sim_editor_session_set_selected_motion_mode(&saved_session, PHYSICS_SIM_OVERLAY_MOTION_STATIC)) {
        free(runtime_json);
        return false;
    }
    if (!physics_sim_editor_session_nudge_selected_velocity(&saved_session, 0.50, -1.25, 2.00)) {
        free(runtime_json);
        return false;
    }
    if (!physics_sim_editor_session_set_selected_emitter_type(&saved_session, EMITTER_VELOCITY_JET, true)) {
        free(runtime_json);
        return false;
    }
    if (!physics_sim_editor_session_cycle_selected_emitter_source_mode_3d(&saved_session)) {
        free(runtime_json);
        return false;
    }
    if (!physics_sim_editor_session_cycle_selected_emitter_surface_3d(&saved_session)) {
        free(runtime_json);
        return false;
    }
    if (!physics_sim_editor_session_cycle_selected_emitter_obstacle_mode_3d(&saved_session)) {
        free(runtime_json);
        return false;
    }
    if (!physics_sim_editor_session_set_selected_emitter_radius(&saved_session, 0.41f)) {
        free(runtime_json);
        return false;
    }
    if (!physics_sim_editor_session_set_selected_emitter_strength(&saved_session, 96.5f)) {
        free(runtime_json);
        return false;
    }
    if (!physics_sim_editor_session_set_selected_emitter_thermal_buoyancy_3d(&saved_session, 3.5f)) {
        free(runtime_json);
        return false;
    }
    if (!physics_sim_editor_session_set_scene_domain_size(&saved_session, 14.0, 9.0, 5.0)) {
        free(runtime_json);
        return false;
    }
    if (!physics_sim_editor_session_build_overlay_json(&saved_session,
                                                       &overlay_json,
                                                       diagnostics,
                                                       sizeof(diagnostics))) {
        free(runtime_json);
        free(overlay_json);
        return false;
    }
    if (!runtime_scene_bridge_writeback_physics_overlay_json(runtime_json,
                                                             overlay_json,
                                                             &merged_json,
                                                             diagnostics,
                                                             sizeof(diagnostics))) {
        free(runtime_json);
        free(overlay_json);
        free(merged_json);
        return false;
    }
    if (strstr(merged_json, "\"emitter\"") == NULL ||
        strstr(merged_json, "\"type\"") == NULL ||
        strstr(merged_json, "Jet") == NULL ||
        strstr(merged_json, "\"mode_3d\"") == NULL ||
        strstr(merged_json, "SurfaceShell") == NULL ||
        strstr(merged_json, "\"strength\"") == NULL ||
        strstr(merged_json, "\"scene_domain\"") == NULL) {
        free(runtime_json);
        free(overlay_json);
        free(merged_json);
        return false;
    }
    if (!write_text_file(saved_path, merged_json)) {
        free(runtime_json);
        free(overlay_json);
        free(merged_json);
        return false;
    }

    cfg = app_config_default();
    if (!runtime_scene_bridge_apply_file(saved_path, &cfg, &reopened_preset, &summary)) {
        remove(saved_path);
        free(runtime_json);
        free(overlay_json);
        free(merged_json);
        return false;
    }
    runtime_scene_bridge_get_last_retained_scene(&retained);
    if (!retained.valid_contract) {
        remove(saved_path);
        free(runtime_json);
        free(overlay_json);
        free(merged_json);
        return false;
    }
    reopened_json = read_text_file_alloc(saved_path, &reopened_size);
    if (!reopened_json || reopened_size == 0) {
        remove(saved_path);
        free(runtime_json);
        free(overlay_json);
        free(merged_json);
        free(reopened_json);
        return false;
    }
    if (strstr(reopened_json, "\"emitter\"") == NULL ||
        strstr(reopened_json, "\"type\"") == NULL ||
        strstr(reopened_json, "Jet") == NULL ||
        strstr(reopened_json, "\"mode_3d\"") == NULL ||
        strstr(reopened_json, "SurfaceShell") == NULL ||
        strstr(reopened_json, "\"strength\"") == NULL ||
        strstr(reopened_json, "\"scene_domain\"") == NULL) {
        remove(saved_path);
        free(runtime_json);
        free(overlay_json);
        free(merged_json);
        free(reopened_json);
        return false;
    }

    bootstrap.retained_scene = retained;
    physics_sim_editor_session_init(&reopened_session, &reopened_preset, &bootstrap);
    if (!physics_sim_editor_session_hydrate_overlay_from_runtime_scene_json(&reopened_session,
                                                                            reopened_json,
                                                                            diagnostics,
                                                                            sizeof(diagnostics))) {
        remove(saved_path);
        free(runtime_json);
        free(overlay_json);
        free(merged_json);
        free(reopened_json);
        return false;
    }
    physics_sim_editor_session_select_retained_index(&reopened_session, 1);
    {
        const PhysicsSimObjectOverlay *selected_overlay =
            physics_sim_editor_session_selected_object_overlay(&reopened_session);
        const PhysicsSimEmitterOverlay *selected_emitter =
            physics_sim_editor_session_selected_object_emitter(&reopened_session);
        const PhysicsSimDomainOverlay *scene_domain =
            physics_sim_editor_session_scene_domain(&reopened_session);
        double width = 0.0;
        double height = 0.0;
        double depth = 0.0;
        physics_sim_editor_session_scene_domain_dimensions(&reopened_session, &width, &height, &depth);
        ok = selected_overlay &&
             selected_emitter &&
             scene_domain &&
             selected_overlay->motion_mode == PHYSICS_SIM_OVERLAY_MOTION_DYNAMIC &&
             fabs(selected_overlay->initial_velocity.x - 0.50) <= 1e-9 &&
             fabs(selected_overlay->initial_velocity.y - (-1.25)) <= 1e-9 &&
             fabs(selected_overlay->initial_velocity.z - 2.00) <= 1e-9 &&
             selected_emitter->type == EMITTER_VELOCITY_JET &&
             selected_emitter->source_mode_3d == EMITTER_3D_SOURCE_MODE_SURFACE_SHELL &&
             selected_emitter->surface_3d == EMITTER_3D_SURFACE_BOTTOM &&
             selected_emitter->obstacle_mode_3d == EMITTER_3D_OBSTACLE_MODE_CLEAR_ATTACHED &&
             fabs(selected_emitter->radius - 0.41) <= 1e-6 &&
             fabs(selected_emitter->strength - 96.5) <= 1e-9 &&
             fabs(selected_emitter->thermal_buoyancy_3d - 3.5) <= 1e-6 &&
             !scene_domain->seeded_from_retained_bounds &&
             fabs(width - 14.0) <= 1e-9 &&
             fabs(height - 9.0) <= 1e-9 &&
             fabs(depth - 5.0) <= 1e-9 &&
             reopened_preset.domain == SCENE_DOMAIN_STRUCTURAL &&
             fabsf(reopened_preset.domain_width - 14.0f) <= 1e-6f &&
             fabsf(reopened_preset.domain_height - 9.0f) <= 1e-6f &&
             reopened_session.physics_overlay.logical_clock == 1 &&
             !reopened_session.physics_overlay.derived_defaults;
    }

    remove(saved_path);
    free(runtime_json);
    free(overlay_json);
    free(merged_json);
    free(reopened_json);
    return ok;
}

bool test_scene_editor_session_wind_tunnel_face_writeback_roundtrips(void) {
    RuntimeSceneBridgePreflight summary = {0};
    PhysicsSimRuntimeVisualBootstrap visual_bootstrap = {0};
    SceneEditorBootstrap bootstrap = {0};
    PhysicsSimEditorSession session = {0};
    PhysicsSimEditorSession rehydrated = {0};
    AppConfig cfg = app_config_default();
    const FluidScenePreset *base = scene_presets_get_default();
    FluidScenePreset preset = base ? *base : (FluidScenePreset){0};
    char *runtime_json = NULL;
    char *overlay_json = NULL;
    char *merged_json = NULL;
    char diagnostics[256];
    size_t runtime_size = 0;
    bool ok = false;

    runtime_json = read_text_file_alloc("tests/fixtures/runtime_scene_wind_tunnel_3d_minimal.json",
                                        &runtime_size);
    if (!runtime_json || runtime_size == 0) {
        free(runtime_json);
        return false;
    }
    if (!runtime_scene_bridge_apply_json(runtime_json, &cfg, &preset, &summary)) {
        free(runtime_json);
        return false;
    }
    if (!runtime_scene_bridge_load_visual_bootstrap_json(runtime_json,
                                                         &visual_bootstrap,
                                                         diagnostics,
                                                         sizeof(diagnostics)) ||
        !visual_bootstrap.wind_tunnel_authored ||
        visual_bootstrap.wind_tunnel.inlet_face != WIND_TUNNEL_3D_FACE_LEFT ||
        visual_bootstrap.wind_tunnel.outlet_face != WIND_TUNNEL_3D_FACE_RIGHT) {
        free(runtime_json);
        return false;
    }

    bootstrap.has_retained_scene = true;
    bootstrap.retained_scene = visual_bootstrap.retained_scene;
    bootstrap.wind_tunnel_authored = visual_bootstrap.wind_tunnel_authored;
    bootstrap.wind_tunnel = visual_bootstrap.wind_tunnel;
    physics_sim_editor_session_init(&session, &preset, &bootstrap);
    if (!physics_sim_editor_session_set_wind_tunnel_faces(&session,
                                                          &cfg,
                                                          WIND_TUNNEL_3D_FACE_TOP,
                                                          WIND_TUNNEL_3D_FACE_BOTTOM)) {
        free(runtime_json);
        return false;
    }
    if (!physics_sim_editor_session_build_overlay_json(&session,
                                                       &overlay_json,
                                                       diagnostics,
                                                       sizeof(diagnostics))) {
        free(runtime_json);
        free(overlay_json);
        return false;
    }
    if (!runtime_scene_bridge_writeback_physics_overlay_json(runtime_json,
                                                             overlay_json,
                                                             &merged_json,
                                                             diagnostics,
                                                             sizeof(diagnostics))) {
        free(runtime_json);
        free(overlay_json);
        free(merged_json);
        return false;
    }
    if (!merged_json ||
        strstr(merged_json, "\"wind_tunnel\"") == NULL ||
        strstr(merged_json, "\"inlet_face\":\"top\"") == NULL ||
        strstr(merged_json, "\"outlet_face\":\"bottom\"") == NULL) {
        free(runtime_json);
        free(overlay_json);
        free(merged_json);
        return false;
    }

    memset(&visual_bootstrap, 0, sizeof(visual_bootstrap));
    if (!runtime_scene_bridge_load_visual_bootstrap_json(merged_json,
                                                         &visual_bootstrap,
                                                         diagnostics,
                                                         sizeof(diagnostics)) ||
        !visual_bootstrap.wind_tunnel_authored) {
        free(runtime_json);
        free(overlay_json);
        free(merged_json);
        return false;
    }

    bootstrap.retained_scene = visual_bootstrap.retained_scene;
    bootstrap.wind_tunnel_authored = false;
    memset(&bootstrap.wind_tunnel, 0, sizeof(bootstrap.wind_tunnel));
    physics_sim_editor_session_init(&rehydrated, &preset, &bootstrap);
    if (!physics_sim_editor_session_hydrate_overlay_from_runtime_scene_json(&rehydrated,
                                                                            merged_json,
                                                                            diagnostics,
                                                                            sizeof(diagnostics))) {
        free(runtime_json);
        free(overlay_json);
        free(merged_json);
        return false;
    }

    ok = visual_bootstrap.wind_tunnel.inlet_face == WIND_TUNNEL_3D_FACE_TOP &&
         visual_bootstrap.wind_tunnel.outlet_face == WIND_TUNNEL_3D_FACE_BOTTOM &&
         rehydrated.has_wind_tunnel_config &&
         rehydrated.wind_tunnel_authored &&
         rehydrated.wind_tunnel.inlet_face == WIND_TUNNEL_3D_FACE_TOP &&
         rehydrated.wind_tunnel.outlet_face == WIND_TUNNEL_3D_FACE_BOTTOM;

    free(runtime_json);
    free(overlay_json);
    free(merged_json);
    return ok;
}

bool test_scene_editor_session_wind_tunnel_writeback_creates_missing_extension(void) {
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"physics_sim_wind_tunnel_missing_extension\","
        "\"space_mode_default\":\"3d\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"objects\":[],"
        "\"materials\":[],"
        "\"lights\":[],"
        "\"cameras\":[],"
        "\"constraints\":[],"
        "\"extensions\":{"
          "\"physics_sim\":{"
            "\"scene_domain\":{"
              "\"active\":true,"
              "\"shape\":\"box\","
              "\"min\":{\"x\":0.0,\"y\":0.0,\"z\":0.0},"
              "\"max\":{\"x\":2.0,\"y\":1.0,\"z\":1.0}"
            "}"
          "}"
        "}"
        "}";
    PhysicsSimRuntimeVisualBootstrap visual_bootstrap = {0};
    SceneEditorBootstrap bootstrap = {0};
    PhysicsSimEditorSession session = {0};
    PhysicsSimEditorSession rehydrated = {0};
    AppConfig cfg = app_config_default();
    const FluidScenePreset *base = scene_presets_get_default();
    FluidScenePreset preset = base ? *base : (FluidScenePreset){0};
    char *overlay_json = NULL;
    char *merged_json = NULL;
    char diagnostics[256];
    bool ok = false;

    cfg.sim_mode = SIM_MODE_WIND_TUNNEL;
    cfg.space_mode = SPACE_MODE_3D;
    cfg.tunnel_inflow_speed = 41.0f;
    cfg.tunnel_inflow_density = 12.0f;
    if (!runtime_scene_bridge_load_visual_bootstrap_json(runtime_json,
                                                         &visual_bootstrap,
                                                         diagnostics,
                                                         sizeof(diagnostics)) ||
        visual_bootstrap.wind_tunnel_authored ||
        !visual_bootstrap.retained_scene.valid_contract) {
        return false;
    }

    bootstrap.has_retained_scene = true;
    bootstrap.retained_scene = visual_bootstrap.retained_scene;
    physics_sim_editor_session_init(&session, &preset, &bootstrap);
    if (!physics_sim_editor_session_set_wind_tunnel_faces(&session,
                                                          &cfg,
                                                          WIND_TUNNEL_3D_FACE_FRONT,
                                                          WIND_TUNNEL_3D_FACE_BACK)) {
        free(overlay_json);
        return false;
    }
    if (!physics_sim_editor_session_build_overlay_json(&session,
                                                       &overlay_json,
                                                       diagnostics,
                                                       sizeof(diagnostics))) {
        free(overlay_json);
        return false;
    }
    if (!runtime_scene_bridge_writeback_physics_overlay_json(runtime_json,
                                                             overlay_json,
                                                             &merged_json,
                                                             diagnostics,
                                                             sizeof(diagnostics))) {
        free(overlay_json);
        free(merged_json);
        return false;
    }
    if (!merged_json ||
        strstr(merged_json, "\"wind_tunnel\"") == NULL ||
        strstr(merged_json, "\"active\":true") == NULL ||
        strstr(merged_json, "\"inlet_face\":\"front\"") == NULL ||
        strstr(merged_json, "\"outlet_face\":\"back\"") == NULL ||
        strstr(merged_json, "\"inflow_speed\":41.0") == NULL ||
        strstr(merged_json, "\"inflow_density\":12.0") == NULL ||
        strstr(merged_json, "\"inlet_slab_cells\"") == NULL ||
        strstr(merged_json, "\"outlet_policy\":\"receive\"") == NULL ||
        strstr(merged_json, "\"wall_policy\":\"no_slip\"") == NULL) {
        free(overlay_json);
        free(merged_json);
        return false;
    }

    memset(&visual_bootstrap, 0, sizeof(visual_bootstrap));
    if (!runtime_scene_bridge_load_visual_bootstrap_json(merged_json,
                                                         &visual_bootstrap,
                                                         diagnostics,
                                                         sizeof(diagnostics)) ||
        !visual_bootstrap.wind_tunnel_authored) {
        free(overlay_json);
        free(merged_json);
        return false;
    }
    bootstrap.retained_scene = visual_bootstrap.retained_scene;
    bootstrap.wind_tunnel_authored = false;
    memset(&bootstrap.wind_tunnel, 0, sizeof(bootstrap.wind_tunnel));
    physics_sim_editor_session_init(&rehydrated, &preset, &bootstrap);
    if (!physics_sim_editor_session_hydrate_overlay_from_runtime_scene_json(&rehydrated,
                                                                            merged_json,
                                                                            diagnostics,
                                                                            sizeof(diagnostics))) {
        free(overlay_json);
        free(merged_json);
        return false;
    }

    ok = visual_bootstrap.wind_tunnel.inlet_face == WIND_TUNNEL_3D_FACE_FRONT &&
         visual_bootstrap.wind_tunnel.outlet_face == WIND_TUNNEL_3D_FACE_BACK &&
         visual_bootstrap.wind_tunnel.active &&
         rehydrated.has_wind_tunnel_config &&
         rehydrated.wind_tunnel_authored &&
         rehydrated.wind_tunnel.inlet_face == WIND_TUNNEL_3D_FACE_FRONT &&
         rehydrated.wind_tunnel.outlet_face == WIND_TUNNEL_3D_FACE_BACK;

    free(overlay_json);
    free(merged_json);
    return ok;
}

bool test_scene_editor_session_runtime_mesh_role_writeback_updates_object_extension(void) {
    const char *runtime_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_mesh_role_writeback\","
        "\"space_mode_default\":\"3d\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"objects\":[{"
        "\"object_id\":\"mesh_obj\","
        "\"object_type\":\"mesh_asset_instance\","
        "\"geometry_ref\":{\"kind\":\"mesh_asset\",\"id\":\"cube_asset\"},"
        "\"dimensional_mode\":\"full_3d\","
        "\"transform\":{"
        "\"position\":{\"x\":0.0,\"y\":0.0,\"z\":0.0},"
        "\"rotation\":{\"x\":0.0,\"y\":0.0,\"z\":0.0},"
        "\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}},"
        "\"extensions\":{"
        "\"line_drawing\":{\"runtime_mesh_path\":\"assets/mesh_assets/cube_asset.runtime.json\"},"
        "\"physics_sim\":{\"fluid_behavior\":\"solid_obstacle\",\"fluid_obstacle\":true}"
        "}"
        "}],"
        "\"materials\":[],"
        "\"lights\":[],"
        "\"cameras\":[],"
        "\"constraints\":[],"
        "\"extensions\":{}"
        "}";
    SceneEditorBootstrap bootstrap = {0};
    PhysicsSimEditorSession session = {0};
    char *overlay_json = NULL;
    char *merged_json = NULL;
    char diagnostics[256];
    bool ok = false;
    json_object *merged_root = NULL;
    json_object *objects = NULL;
    json_object *object = NULL;
    json_object *extensions = NULL;
    json_object *line_drawing = NULL;
    json_object *physics_sim = NULL;
    json_object *emitter = NULL;
    const char *fluid_behavior = NULL;
    const char *type = NULL;
    double strength = 0.0;
    double radius = 0.0;
    bool fluid_obstacle = true;

    bootstrap.has_retained_scene = true;
    bootstrap.retained_scene.valid_contract = true;
    snprintf(bootstrap.retained_scene.root.scene_id,
             sizeof(bootstrap.retained_scene.root.scene_id),
             "scene_mesh_role_writeback");
    bootstrap.retained_scene.retained_object_count = 1;
    bootstrap.retained_scene.objects[0].kind = CORE_SCENE_OBJECT_KIND_MESH_ASSET_INSTANCE;
    snprintf(bootstrap.retained_scene.objects[0].object.object_id,
             sizeof(bootstrap.retained_scene.objects[0].object.object_id),
             "mesh_obj");
    bootstrap.mesh_previews.valid_contract = true;
    bootstrap.mesh_previews.instance_count = 1;
    bootstrap.mesh_previews.instances[0].scene_object_index = 0;
    bootstrap.mesh_previews.instances[0].runtime_path_resolved = true;
    bootstrap.mesh_previews.instances[0].fluid_obstacle_enabled = true;
    bootstrap.mesh_previews.instances[0].transform_scale = (CoreObjectVec3){1.0, 1.0, 1.0};
    snprintf(bootstrap.mesh_previews.instances[0].object_id,
             sizeof(bootstrap.mesh_previews.instances[0].object_id),
             "mesh_obj");
    snprintf(bootstrap.mesh_previews.instances[0].asset_id,
             sizeof(bootstrap.mesh_previews.instances[0].asset_id),
             "cube_asset");
    snprintf(bootstrap.mesh_previews.instances[0].runtime_mesh_path,
             sizeof(bootstrap.mesh_previews.instances[0].runtime_mesh_path),
             "assets/mesh_assets/cube_asset.runtime.json");
    snprintf(bootstrap.mesh_previews.instances[0].fluid_behavior,
             sizeof(bootstrap.mesh_previews.instances[0].fluid_behavior),
             "solid_obstacle");

    physics_sim_editor_session_init(&session, NULL, &bootstrap);
    physics_sim_editor_session_select_retained_index(&session, 0);
    if (!physics_sim_editor_session_selected_runtime_mesh_overlay(&session)) return false;
    if (!physics_sim_editor_session_set_selected_runtime_mesh_role(
            &session,
            PHYSICS_SIM_RUNTIME_MESH_EDITOR_ROLE_BOUNDARY_FLOW_EMITTER)) {
        return false;
    }
    if (!physics_sim_editor_session_set_selected_runtime_mesh_emitter_strength(&session, 44.0f)) {
        return false;
    }
    if (!physics_sim_editor_session_set_selected_runtime_mesh_emitter_radius(&session, 0.22f)) {
        return false;
    }
    if (!physics_sim_editor_session_build_overlay_json(&session,
                                                       &overlay_json,
                                                       diagnostics,
                                                       sizeof(diagnostics))) {
        free(overlay_json);
        return false;
    }
    if (strstr(overlay_json, "\"mesh_overlays\"") == NULL ||
        strstr(overlay_json, "boundary_flow_emitter") == NULL ||
        strstr(overlay_json, "\"Flow") != NULL) {
        free(overlay_json);
        return false;
    }
    if (!runtime_scene_bridge_writeback_physics_overlay_json(runtime_json,
                                                             overlay_json,
                                                             &merged_json,
                                                             diagnostics,
                                                             sizeof(diagnostics))) {
        free(overlay_json);
        free(merged_json);
        return false;
    }
    merged_root = json_tokener_parse(merged_json);
    if (merged_root && json_object_is_type(merged_root, json_type_object) &&
        json_object_object_get_ex(merged_root, "objects", &objects) &&
        json_object_is_type(objects, json_type_array) &&
        json_object_array_length(objects) == 1u) {
        object = json_object_array_get_idx(objects, 0u);
        extensions = test_json_object_child(object, "extensions");
        line_drawing = test_json_object_child(extensions, "line_drawing");
        physics_sim = test_json_object_child(extensions, "physics_sim");
        emitter = test_json_object_child(physics_sim, "emitter");
        fluid_behavior = test_json_string_child(physics_sim, "fluid_behavior");
        type = test_json_string_child(emitter, "type");
        ok = line_drawing != NULL &&
             test_json_string_child(line_drawing, "runtime_mesh_path") != NULL &&
             fluid_behavior &&
             strcmp(fluid_behavior, "boundary_flow_emitter") == 0 &&
             test_json_bool_child(physics_sim, "fluid_obstacle", &fluid_obstacle) &&
             !fluid_obstacle &&
             emitter != NULL &&
             type &&
             strcmp(type, "Jet") == 0 &&
             test_json_double_child(emitter, "strength", &strength) &&
             fabs(strength - 44.0) <= 1e-9 &&
             test_json_double_child(emitter, "radius", &radius) &&
             fabs(radius - 0.22) <= 1e-6;
    }

    if (merged_root) json_object_put(merged_root);
    free(overlay_json);
    free(merged_json);
    return ok;
}

bool test_scene_editor_session_tracks_legacy_selection(void) {
    PhysicsSimEditorSession session = {0};
    char summary[128];

    physics_sim_editor_session_init(&session, NULL, NULL);
    physics_sim_editor_session_set_legacy_selection(&session, SELECTION_OBJECT, -1, 4, -1);
    if (session.legacy_selection.kind != SELECTION_OBJECT) return false;
    if (session.legacy_selection.object_index != 4) return false;
    if (strstr(physics_sim_editor_session_legacy_selection_summary(&session,
                                                                   summary,
                                                                   sizeof(summary)),
               "object=4") == NULL) {
        return false;
    }

    physics_sim_editor_session_set_legacy_selection(&session, SELECTION_EMITTER, 2, 4, -1);
    if (session.legacy_selection.kind != SELECTION_EMITTER) return false;
    if (session.legacy_selection.emitter_index != 2) return false;
    if (strstr(physics_sim_editor_session_legacy_selection_summary(&session,
                                                                   summary,
                                                                   sizeof(summary)),
               "emitter=2") == NULL) {
        return false;
    }

    physics_sim_editor_session_set_legacy_selection(&session, SELECTION_NONE, -1, -1, -1);
    if (strcmp(physics_sim_editor_session_legacy_selection_summary(&session,
                                                                   summary,
                                                                   sizeof(summary)),
               "Legacy Selection: none") != 0) {
        return false;
    }

    return true;
}
