#include "app/editor/scene_editor_session.h"

#include <json-c/json.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static void session_diag(char *out_diagnostics, size_t out_diagnostics_size, const char *message) {
    if (!out_diagnostics || out_diagnostics_size == 0 || !message) return;
    snprintf(out_diagnostics, out_diagnostics_size, "%s", message);
}

static void physics_sim_editor_session_seed_default_overlay(PhysicsSimEditorSession *session) {
    int i = 0;
    if (!session || !session->has_retained_scene) return;

    memset(&session->physics_overlay, 0, sizeof(session->physics_overlay));
    session->has_physics_overlay = true;
    session->physics_overlay.active = true;
    session->physics_overlay.derived_defaults = true;
    session->physics_overlay.logical_clock = 0;

    for (i = 0; i < session->retained_scene.retained_object_count; ++i) {
        const CoreSceneObjectContract *object = &session->retained_scene.objects[i];
        PhysicsSimObjectOverlay *overlay = &session->physics_overlay.object_overlays[session->physics_overlay.object_overlay_count];
        overlay->active = true;
        overlay->retained_object_index = i;
        snprintf(overlay->object_id, sizeof(overlay->object_id), "%s", object->object.object_id);
        overlay->motion_mode = object->object.flags.locked
                                   ? PHYSICS_SIM_OVERLAY_MOTION_STATIC
                                   : PHYSICS_SIM_OVERLAY_MOTION_DYNAMIC;
        overlay->initial_velocity = (CoreObjectVec3){0};
        session->physics_overlay.object_overlay_count++;
    }
}

static PhysicsSimObjectOverlay *physics_sim_editor_session_selected_overlay_mut(PhysicsSimEditorSession *session) {
    if (!session || !session->has_physics_overlay) return NULL;
    if (session->selection.retained_object_index < 0 ||
        session->selection.retained_object_index >= session->physics_overlay.object_overlay_count) {
        return NULL;
    }
    return &session->physics_overlay.object_overlays[session->selection.retained_object_index];
}

static PhysicsSimObjectOverlay *physics_sim_editor_session_find_overlay_by_object_id_mut(PhysicsSimEditorSession *session,
                                                                                          const char *object_id) {
    int i = 0;
    if (!session || !session->has_physics_overlay || !object_id || !object_id[0]) return NULL;
    for (i = 0; i < session->physics_overlay.object_overlay_count; ++i) {
        PhysicsSimObjectOverlay *overlay = &session->physics_overlay.object_overlays[i];
        if (!overlay->active) continue;
        if (strcmp(overlay->object_id, object_id) == 0) {
            return overlay;
        }
    }
    return NULL;
}

static void physics_sim_editor_session_clear(PhysicsSimEditorSession *session) {
    if (!session) return;
    memset(session, 0, sizeof(*session));
    session->selection.retained_object_index = -1;
    session->selection.retained_object_id[0] = '\0';
    session->legacy_selection.kind = SELECTION_NONE;
    session->legacy_selection.emitter_index = -1;
    session->legacy_selection.object_index = -1;
    session->legacy_selection.import_index = -1;
}

void physics_sim_editor_session_init(PhysicsSimEditorSession *session,
                                     FluidScenePreset *working_preset,
                                     const SceneEditorBootstrap *bootstrap) {
    if (!session) return;

    physics_sim_editor_session_clear(session);
    session->using_legacy_preset_adapter = (working_preset != NULL);
    session->working_preset = working_preset;

    if (!bootstrap || !bootstrap->has_retained_scene) {
        return;
    }
    if (!bootstrap->retained_scene.valid_contract) {
        return;
    }

    session->retained_scene = bootstrap->retained_scene;
    session->has_retained_scene = true;
    physics_sim_editor_session_seed_default_overlay(session);
    physics_sim_editor_session_select_retained_index(session, 0);
}

void physics_sim_editor_session_select_retained_index(PhysicsSimEditorSession *session, int index) {
    const CoreSceneObjectContract *object = NULL;
    if (!session || !session->has_retained_scene) return;

    session->selection.retained_object_index = -1;
    session->selection.retained_object_id[0] = '\0';

    if (index < 0 || index >= session->retained_scene.retained_object_count) {
        return;
    }

    object = &session->retained_scene.objects[index];
    session->selection.retained_object_index = index;
    snprintf(session->selection.retained_object_id,
             sizeof(session->selection.retained_object_id),
             "%s",
             object->object.object_id);
}

void physics_sim_editor_session_set_legacy_selection(PhysicsSimEditorSession *session,
                                                     EditorSelectionKind kind,
                                                     int emitter_index,
                                                     int object_index,
                                                     int import_index) {
    if (!session) return;

    session->legacy_selection.kind = kind;
    session->legacy_selection.emitter_index = emitter_index;
    session->legacy_selection.object_index = object_index;
    session->legacy_selection.import_index = import_index;
}

bool physics_sim_editor_session_has_retained_scene(const PhysicsSimEditorSession *session) {
    return session && session->has_retained_scene;
}

bool physics_sim_editor_session_has_physics_overlay(const PhysicsSimEditorSession *session) {
    return session && session->has_physics_overlay && session->physics_overlay.active;
}

int physics_sim_editor_session_retained_object_count(const PhysicsSimEditorSession *session) {
    if (!session || !session->has_retained_scene) return 0;
    return session->retained_scene.retained_object_count;
}

const char *physics_sim_editor_session_scene_id(const PhysicsSimEditorSession *session) {
    if (!session || !session->has_retained_scene) return "";
    return session->retained_scene.root.scene_id;
}

const CoreSceneObjectContract *physics_sim_editor_session_object_at(const PhysicsSimEditorSession *session,
                                                                    int index) {
    if (!session || !session->has_retained_scene) return NULL;
    if (index < 0 || index >= session->retained_scene.retained_object_count) return NULL;
    return &session->retained_scene.objects[index];
}

const CoreSceneObjectContract *physics_sim_editor_session_selected_object(const PhysicsSimEditorSession *session) {
    if (!session) return NULL;
    return physics_sim_editor_session_object_at(session, session->selection.retained_object_index);
}

const PhysicsSimObjectOverlay *physics_sim_editor_session_object_overlay_at(const PhysicsSimEditorSession *session,
                                                                            int index) {
    if (!session || !session->has_physics_overlay) return NULL;
    if (index < 0 || index >= session->physics_overlay.object_overlay_count) return NULL;
    return &session->physics_overlay.object_overlays[index];
}

const PhysicsSimObjectOverlay *physics_sim_editor_session_selected_object_overlay(const PhysicsSimEditorSession *session) {
    if (!session) return NULL;
    return physics_sim_editor_session_object_overlay_at(session, session->selection.retained_object_index);
}

bool physics_sim_editor_session_set_selected_motion_mode(PhysicsSimEditorSession *session,
                                                         PhysicsSimOverlayMotionMode mode) {
    PhysicsSimObjectOverlay *overlay = physics_sim_editor_session_selected_overlay_mut(session);
    if (!overlay) return false;
    overlay->motion_mode = mode;
    session->physics_overlay.derived_defaults = false;
    return true;
}

bool physics_sim_editor_session_nudge_selected_velocity(PhysicsSimEditorSession *session,
                                                        double dx,
                                                        double dy,
                                                        double dz) {
    PhysicsSimObjectOverlay *overlay = physics_sim_editor_session_selected_overlay_mut(session);
    if (!overlay) return false;
    overlay->initial_velocity.x += dx;
    overlay->initial_velocity.y += dy;
    overlay->initial_velocity.z += dz;
    session->physics_overlay.derived_defaults = false;
    return true;
}

bool physics_sim_editor_session_reset_selected_velocity(PhysicsSimEditorSession *session) {
    PhysicsSimObjectOverlay *overlay = physics_sim_editor_session_selected_overlay_mut(session);
    if (!overlay) return false;
    overlay->initial_velocity = (CoreObjectVec3){0};
    session->physics_overlay.derived_defaults = false;
    return true;
}

bool physics_sim_editor_session_build_overlay_json(const PhysicsSimEditorSession *session,
                                                   char **out_overlay_json,
                                                   char *out_diagnostics,
                                                   size_t out_diagnostics_size) {
    json_object *root = NULL;
    json_object *overlay_meta = NULL;
    json_object *extensions = NULL;
    json_object *physics_ext = NULL;
    json_object *object_overlays = NULL;
    const char *serialized = NULL;
    char *out = NULL;
    size_t out_len = 0;
    int i = 0;

    if (out_overlay_json) *out_overlay_json = NULL;
    session_diag(out_diagnostics, out_diagnostics_size, "invalid input");
    if (!session || !out_overlay_json) return false;
    if (!session->has_retained_scene || !session->has_physics_overlay || !session->physics_overlay.active) {
        session_diag(out_diagnostics, out_diagnostics_size, "physics overlay unavailable");
        return false;
    }

    root = json_object_new_object();
    overlay_meta = json_object_new_object();
    extensions = json_object_new_object();
    physics_ext = json_object_new_object();
    object_overlays = json_object_new_array();
    if (!root || !overlay_meta || !extensions || !physics_ext || !object_overlays) {
        session_diag(out_diagnostics, out_diagnostics_size, "out of memory");
        if (root) json_object_put(root);
        if (overlay_meta) json_object_put(overlay_meta);
        if (extensions) json_object_put(extensions);
        if (physics_ext) json_object_put(physics_ext);
        if (object_overlays) json_object_put(object_overlays);
        return false;
    }

    json_object_object_add(overlay_meta, "producer", json_object_new_string("physics_sim"));
    json_object_object_add(overlay_meta,
                           "logical_clock",
                           json_object_new_int(session->physics_overlay.logical_clock + 1));
    json_object_object_add(root, "overlay_meta", overlay_meta);

    json_object_object_add(physics_ext,
                           "overlay_variant",
                           json_object_new_string("retained_object_overlay_v1"));
    json_object_object_add(physics_ext,
                           "scene_id",
                           json_object_new_string(session->retained_scene.root.scene_id));
    json_object_object_add(physics_ext,
                           "derived_defaults",
                           json_object_new_boolean(session->physics_overlay.derived_defaults ? 1 : 0));

    for (i = 0; i < session->physics_overlay.object_overlay_count; ++i) {
        const PhysicsSimObjectOverlay *overlay = &session->physics_overlay.object_overlays[i];
        json_object *overlay_obj = NULL;
        json_object *velocity = NULL;
        if (!overlay->active) continue;

        overlay_obj = json_object_new_object();
        velocity = json_object_new_object();
        if (!overlay_obj || !velocity) {
            session_diag(out_diagnostics, out_diagnostics_size, "out of memory");
            if (overlay_obj) json_object_put(overlay_obj);
            if (velocity) json_object_put(velocity);
            json_object_put(root);
            return false;
        }

        json_object_object_add(overlay_obj, "object_id", json_object_new_string(overlay->object_id));
        json_object_object_add(overlay_obj,
                               "motion_mode",
                               json_object_new_string(physics_sim_editor_session_motion_mode_label(overlay->motion_mode)));
        json_object_object_add(velocity, "x", json_object_new_double(overlay->initial_velocity.x));
        json_object_object_add(velocity, "y", json_object_new_double(overlay->initial_velocity.y));
        json_object_object_add(velocity, "z", json_object_new_double(overlay->initial_velocity.z));
        json_object_object_add(overlay_obj, "initial_velocity", velocity);
        json_object_array_add(object_overlays, overlay_obj);
    }

    json_object_object_add(physics_ext, "object_overlays", object_overlays);
    json_object_object_add(extensions, "physics_sim", physics_ext);
    json_object_object_add(root, "extensions", extensions);

    serialized = json_object_to_json_string_ext(root,
                                                JSON_C_TO_STRING_PRETTY | JSON_C_TO_STRING_NOSLASHESCAPE);
    if (!serialized) {
        session_diag(out_diagnostics, out_diagnostics_size, "failed to serialize overlay json");
        json_object_put(root);
        return false;
    }

    out_len = strlen(serialized);
    out = (char *)malloc(out_len + 1u);
    if (!out) {
        session_diag(out_diagnostics, out_diagnostics_size, "out of memory");
        json_object_put(root);
        return false;
    }
    memcpy(out, serialized, out_len + 1u);
    *out_overlay_json = out;
    session_diag(out_diagnostics, out_diagnostics_size, "ok");
    json_object_put(root);
    return true;
}

bool physics_sim_editor_session_hydrate_overlay_from_runtime_scene_json(PhysicsSimEditorSession *session,
                                                                        const char *runtime_scene_json,
                                                                        char *out_diagnostics,
                                                                        size_t out_diagnostics_size) {
    json_object *root = NULL;
    json_object *extensions = NULL;
    json_object *physics_ext = NULL;
    json_object *overlay_merge = NULL;
    json_object *producer_clocks = NULL;
    json_object *overlay_meta = NULL;
    json_object *object_overlays = NULL;
    json_object *logical_clock = NULL;
    json_object *derived_defaults = NULL;
    int i = 0;
    bool found_any = false;
    bool has_derived_defaults = false;

    session_diag(out_diagnostics, out_diagnostics_size, "invalid input");
    if (!session || !runtime_scene_json) return false;
    if (!session->has_retained_scene || !session->has_physics_overlay || !session->physics_overlay.active) {
        session_diag(out_diagnostics, out_diagnostics_size, "physics overlay unavailable");
        return false;
    }

    root = json_tokener_parse(runtime_scene_json);
    if (!root || !json_object_is_type(root, json_type_object)) {
        if (root) json_object_put(root);
        session_diag(out_diagnostics, out_diagnostics_size, "failed to parse runtime scene json");
        return false;
    }

    if (!json_object_object_get_ex(root, "extensions", &extensions) ||
        !json_object_is_type(extensions, json_type_object) ||
        !json_object_object_get_ex(extensions, "physics_sim", &physics_ext) ||
        !json_object_is_type(physics_ext, json_type_object)) {
        session_diag(out_diagnostics, out_diagnostics_size, "ok");
        json_object_put(root);
        return true;
    }

    if (json_object_object_get_ex(root, "overlay_meta", &overlay_meta) &&
        json_object_is_type(overlay_meta, json_type_object) &&
        json_object_object_get_ex(overlay_meta, "logical_clock", &logical_clock) &&
        json_object_is_type(logical_clock, json_type_int)) {
        session->physics_overlay.logical_clock = json_object_get_int(logical_clock);
    } else if (json_object_object_get_ex(extensions, "overlay_merge", &overlay_merge) &&
               json_object_is_type(overlay_merge, json_type_object) &&
               json_object_object_get_ex(overlay_merge, "producer_clocks", &producer_clocks) &&
               json_object_is_type(producer_clocks, json_type_object) &&
               json_object_object_get_ex(producer_clocks, "physics_sim", &logical_clock) &&
               json_object_is_type(logical_clock, json_type_int)) {
        session->physics_overlay.logical_clock = json_object_get_int(logical_clock);
    }
    if (json_object_object_get_ex(physics_ext, "derived_defaults", &derived_defaults) &&
        json_object_is_type(derived_defaults, json_type_boolean)) {
        session->physics_overlay.derived_defaults = json_object_get_boolean(derived_defaults) ? true : false;
        has_derived_defaults = true;
    }

    if (json_object_object_get_ex(physics_ext, "object_overlays", &object_overlays) &&
        json_object_is_type(object_overlays, json_type_array)) {
        int overlay_count = json_object_array_length(object_overlays);
        for (i = 0; i < overlay_count; ++i) {
            json_object *overlay_obj = json_object_array_get_idx(object_overlays, i);
            json_object *object_id = NULL;
            json_object *motion_mode = NULL;
            json_object *initial_velocity = NULL;
            PhysicsSimObjectOverlay *overlay = NULL;
            if (!overlay_obj || !json_object_is_type(overlay_obj, json_type_object)) continue;
            if (!json_object_object_get_ex(overlay_obj, "object_id", &object_id) ||
                !json_object_is_type(object_id, json_type_string)) {
                continue;
            }
            overlay = physics_sim_editor_session_find_overlay_by_object_id_mut(session,
                                                                               json_object_get_string(object_id));
            if (!overlay) continue;
            if (json_object_object_get_ex(overlay_obj, "motion_mode", &motion_mode) &&
                json_object_is_type(motion_mode, json_type_string)) {
                const char *mode_text = json_object_get_string(motion_mode);
                overlay->motion_mode = (mode_text && strcmp(mode_text, "Static") == 0)
                                           ? PHYSICS_SIM_OVERLAY_MOTION_STATIC
                                           : PHYSICS_SIM_OVERLAY_MOTION_DYNAMIC;
            }
            if (json_object_object_get_ex(overlay_obj, "initial_velocity", &initial_velocity) &&
                json_object_is_type(initial_velocity, json_type_object)) {
                json_object *vx = NULL;
                json_object *vy = NULL;
                json_object *vz = NULL;
                if (json_object_object_get_ex(initial_velocity, "x", &vx) &&
                    (json_object_is_type(vx, json_type_double) || json_object_is_type(vx, json_type_int))) {
                    overlay->initial_velocity.x = json_object_get_double(vx);
                }
                if (json_object_object_get_ex(initial_velocity, "y", &vy) &&
                    (json_object_is_type(vy, json_type_double) || json_object_is_type(vy, json_type_int))) {
                    overlay->initial_velocity.y = json_object_get_double(vy);
                }
                if (json_object_object_get_ex(initial_velocity, "z", &vz) &&
                    (json_object_is_type(vz, json_type_double) || json_object_is_type(vz, json_type_int))) {
                    overlay->initial_velocity.z = json_object_get_double(vz);
                }
            }
            found_any = true;
        }
    }

    if (found_any && !has_derived_defaults) {
        session->physics_overlay.derived_defaults = false;
    }

    session_diag(out_diagnostics, out_diagnostics_size, "ok");
    json_object_put(root);
    return true;
}

bool physics_sim_editor_session_mark_overlay_applied(PhysicsSimEditorSession *session) {
    if (!session || !session->has_physics_overlay || !session->physics_overlay.active) return false;
    session->physics_overlay.logical_clock += 1;
    session->physics_overlay.derived_defaults = false;
    return true;
}

const char *physics_sim_editor_session_object_kind_label(CoreSceneObjectKind kind) {
    switch (kind) {
        case CORE_SCENE_OBJECT_KIND_PLANE_PRIMITIVE:
            return "Plane Primitive";
        case CORE_SCENE_OBJECT_KIND_RECT_PRISM_PRIMITIVE:
            return "Rect Prism Primitive";
        case CORE_SCENE_OBJECT_KIND_CURVE_PATH:
            return "Curve Path";
        case CORE_SCENE_OBJECT_KIND_POINT_SET:
            return "Point Set";
        case CORE_SCENE_OBJECT_KIND_EDGE_SET:
            return "Edge Set";
        case CORE_SCENE_OBJECT_KIND_UNKNOWN:
        default:
            return "Unknown";
    }
}

const char *physics_sim_editor_session_motion_mode_label(PhysicsSimOverlayMotionMode mode) {
    switch (mode) {
        case PHYSICS_SIM_OVERLAY_MOTION_STATIC:
            return "Static";
        case PHYSICS_SIM_OVERLAY_MOTION_DYNAMIC:
        default:
            return "Dynamic";
    }
}

const char *physics_sim_editor_session_legacy_selection_summary(const PhysicsSimEditorSession *session,
                                                                char *buffer,
                                                                size_t buffer_size) {
    const char *summary = "Legacy Selection: none";
    if (!buffer || buffer_size == 0) return "";
    buffer[0] = '\0';

    if (!session) {
        snprintf(buffer, buffer_size, "%s", summary);
        return buffer;
    }

    switch (session->legacy_selection.kind) {
        case SELECTION_EMITTER:
            snprintf(buffer,
                     buffer_size,
                     "Legacy Selection: emitter=%d object=%d import=%d",
                     session->legacy_selection.emitter_index,
                     session->legacy_selection.object_index,
                     session->legacy_selection.import_index);
            break;
        case SELECTION_OBJECT:
            snprintf(buffer,
                     buffer_size,
                     "Legacy Selection: object=%d emitter=%d",
                     session->legacy_selection.object_index,
                     session->legacy_selection.emitter_index);
            break;
        case SELECTION_IMPORT:
            snprintf(buffer,
                     buffer_size,
                     "Legacy Selection: import=%d emitter=%d",
                     session->legacy_selection.import_index,
                     session->legacy_selection.emitter_index);
            break;
        case SELECTION_NONE:
        default:
            snprintf(buffer, buffer_size, "%s", summary);
            break;
    }
    return buffer;
}
