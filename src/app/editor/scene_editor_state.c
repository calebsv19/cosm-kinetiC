#include "app/editor/scene_editor_internal.h"
#include "app/editor/scene_editor_input_hit_helpers.h"
#include "app/editor/scene_editor_model.h"
#include "app/data_paths.h"
#include "core_io.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define EDITOR_SHELL_PAD_X 0
#define EDITOR_SHELL_PAD_TOP 0
#define EDITOR_SHELL_PAD_BOTTOM 0
#define EDITOR_CENTER_PANE_PAD 18
#define EDITOR_VIEWPORT_TITLE_Y 12
#define EDITOR_VIEWPORT_NAME_Y 40
#define EDITOR_VIEWPORT_SUMMARY_Y 66
#define EDITOR_VIEWPORT_TOP_RESERVE 110
#define EDITOR_VIEWPORT_BOTTOM_PAD 18
#define EDITOR_LEGACY_CANVAS_INSET 12

static SDL_Rect rect_from_core_pane(CorePaneRect rect) {
    SDL_Rect out = {0};
    out.x = (int)lroundf(rect.x);
    out.y = (int)lroundf(rect.y);
    out.w = (int)lroundf(rect.width);
    out.h = (int)lroundf(rect.height);
    if (out.w < 0) out.w = 0;
    if (out.h < 0) out.h = 0;
    return out;
}

static bool editor_store_retained_runtime_scene_json(SceneEditorState *state,
                                                     const char *json_text,
                                                     size_t json_size) {
    char *copy = NULL;
    if (!state || !json_text || json_size == 0) return false;
    copy = (char *)malloc(json_size + 1u);
    if (!copy) return false;
    memcpy(copy, json_text, json_size);
    copy[json_size] = '\0';
    free(state->retained_runtime_scene_json);
    state->retained_runtime_scene_json = copy;
    return true;
}

static void editor_refresh_scene_library(SceneEditorState *state) {
    if (!state) return;
    physics_sim_editor_scene_library_refresh(&state->scene_library,
                                             &state->working,
                                             &state->session,
                                             physics_sim_default_runtime_scene_sample_dir(),
                                             state->retained_runtime_scene_path);
}

static const char *editor_filename_from_path(const char *path) {
    const char *filename = NULL;
    if (!path || !path[0]) return "scene.json";
    filename = strrchr(path, '/');
    return filename ? filename + 1 : path;
}

static void editor_refresh_retained_save_diagnostics(SceneEditorState *state, const char *prefix) {
    char target_path[512];
    const char *runtime_dir = physics_sim_default_runtime_scene_user_dir();
    const char *target_name = NULL;
    bool reuses_saved_path = false;
    if (!state || !physics_sim_editor_session_has_retained_scene(&state->session)) return;
    if (!scene_editor_retained_document_resolve_save_path(runtime_dir,
                                                          state->retained_runtime_scene_path,
                                                          state->name_edit_ptr,
                                                          state->retained_scene_provenance_id,
                                                          target_path,
                                                          sizeof(target_path))) {
        snprintf(state->save_scene_diagnostics,
                 sizeof(state->save_scene_diagnostics),
                 "%s",
                 (prefix && prefix[0]) ? prefix : "Scene save target unresolved.");
        return;
    }
    target_name = editor_filename_from_path(target_path);
    reuses_saved_path = scene_editor_retained_document_is_runtime_user_path(runtime_dir,
                                                                            state->retained_runtime_scene_path);
    if (prefix && prefix[0]) {
        snprintf(state->save_scene_diagnostics,
                 sizeof(state->save_scene_diagnostics),
                 "%s %s",
                 prefix,
                 target_name);
        return;
    }
    if (reuses_saved_path) {
        snprintf(state->save_scene_diagnostics,
                 sizeof(state->save_scene_diagnostics),
                 "Save target: %s",
                 target_name);
    } else {
        snprintf(state->save_scene_diagnostics,
                 sizeof(state->save_scene_diagnostics),
                 "First save target: %s",
                 target_name);
    }
}

static void editor_seed_retained_document_identity(SceneEditorState *state,
                                                   const char *runtime_scene_path,
                                                   const char *scene_id) {
    if (!state) return;
    if (runtime_scene_path && runtime_scene_path[0]) {
        snprintf(state->retained_runtime_scene_source_path,
                 sizeof(state->retained_runtime_scene_source_path),
                 "%s",
                 runtime_scene_path);
        snprintf(state->retained_runtime_scene_path,
                 sizeof(state->retained_runtime_scene_path),
                 "%s",
                 runtime_scene_path);
    } else {
        state->retained_runtime_scene_source_path[0] = '\0';
        state->retained_runtime_scene_path[0] = '\0';
    }
    if (scene_id && scene_id[0]) {
        snprintf(state->retained_scene_provenance_id,
                 sizeof(state->retained_scene_provenance_id),
                 "%s",
                 scene_id);
    } else {
        state->retained_scene_provenance_id[0] = '\0';
    }
    if (state->name_edit_ptr && state->name_edit_capacity > 0) {
        scene_editor_retained_document_name_from_path(runtime_scene_path,
                                                      state->retained_scene_provenance_id,
                                                      state->name_edit_ptr,
                                                      state->name_edit_capacity);
        state->working.name = state->name_edit_ptr;
    }
}

void set_dirty(SceneEditorState *state) {
    if (!state) return;
    state->dirty = true;
    if (physics_sim_editor_session_has_retained_scene(&state->session)) {
        state->save_scene_success = false;
        snprintf(state->save_scene_diagnostics,
                 sizeof(state->save_scene_diagnostics),
                 "Scene has unapplied overlay changes.");
    }
}

float sanitize_domain_dimension(float value) {
    if (!isfinite(value) || value <= 0.1f) {
        return 0.1f;
    }
    if (value > 64.0f) {
        return 64.0f;
    }
    return value;
}

void editor_update_dimension_rects(SceneEditorState *state) {
    if (!state) return;
    int rect_h = 26;
    int gap = 12;
    int inset = 16;
    int available_w = state->right_panel_rect.w - inset * 2 - gap;
    if (available_w < 80) available_w = 80;
    int field_w = available_w / 2;
    if (field_w < 60) field_w = 60;
    int rect_y = state->right_panel_rect.y + 52;
    int start_x = state->right_panel_rect.x + inset;
    state->width_rect = (SDL_Rect){start_x, rect_y, field_w, rect_h};
    state->height_rect = (SDL_Rect){start_x + field_w + gap, rect_y, field_w, rect_h};
}

void editor_update_canvas_layout(SceneEditorState *state) {
    if (!state || !state->window) return;
    int winW = 0, winH = 0;
    SDL_GetWindowSize(state->window, &winW, &winH);
    int shell_w = winW - EDITOR_SHELL_PAD_X * 2;
    int shell_h = winH - EDITOR_SHELL_PAD_TOP - EDITOR_SHELL_PAD_BOTTOM;
    int canvas_max_w = 0;
    int canvas_max_h = 0;
    float w_units = sanitize_domain_dimension(state->working.domain_width);
    float h_units = sanitize_domain_dimension(state->working.domain_height);
    float aspect = w_units / h_units;
    CorePaneRect left_rect = {0};
    CorePaneRect center_rect = {0};
    CorePaneRect right_rect = {0};
    bool pane_ok = false;
    int canvas_w = 0;
    int canvas_h = 0;
    int center_inner_x = 0;
    int center_inner_y = 0;
    int viewport_x = 0;
    int viewport_y = 0;
    int viewport_w = 0;
    int viewport_h = 0;

    if (shell_w < 760) shell_w = 760;
    if (shell_h < 420) shell_h = 420;
    if (aspect < 0.2f) aspect = 0.2f;
    if (aspect > 10.0f) aspect = 10.0f;

    if (!state->pane_host.initialized) {
        pane_ok = scene_editor_pane_host_init(&state->pane_host, (float)shell_w, (float)shell_h);
    } else {
        pane_ok = scene_editor_pane_host_rebuild(&state->pane_host, (float)shell_w, (float)shell_h);
    }
    if (pane_ok) {
        pane_ok = scene_editor_pane_host_get_rect_for_role(&state->pane_host,
                                                           SCENE_EDITOR_PANE_LEFT,
                                                           &left_rect) &&
                  scene_editor_pane_host_get_rect_for_role(&state->pane_host,
                                                           SCENE_EDITOR_PANE_CENTER,
                                                           &center_rect) &&
                  scene_editor_pane_host_get_rect_for_role(&state->pane_host,
                                                           SCENE_EDITOR_PANE_RIGHT,
                                                           &right_rect);
    }
    if (!pane_ok) {
        left_rect = (CorePaneRect){0.0f, 0.0f, 284.0f, (float)shell_h};
        center_rect = (CorePaneRect){296.0f, 0.0f, (float)(shell_w - 592), (float)shell_h};
        right_rect = (CorePaneRect){(float)(shell_w - 284), 0.0f, 284.0f, (float)shell_h};
    }

    state->panel_rect = rect_from_core_pane(left_rect);
    state->center_pane_rect = rect_from_core_pane(center_rect);
    state->right_panel_rect = rect_from_core_pane(right_rect);
    state->panel_rect.x += EDITOR_SHELL_PAD_X;
    state->panel_rect.y += EDITOR_SHELL_PAD_TOP;
    state->center_pane_rect.x += EDITOR_SHELL_PAD_X;
    state->center_pane_rect.y += EDITOR_SHELL_PAD_TOP;
    state->right_panel_rect.x += EDITOR_SHELL_PAD_X;
    state->right_panel_rect.y += EDITOR_SHELL_PAD_TOP;

    viewport_x = state->center_pane_rect.x + EDITOR_CENTER_PANE_PAD;
    viewport_y = state->center_pane_rect.y + EDITOR_VIEWPORT_TOP_RESERVE;
    viewport_w = state->center_pane_rect.w - EDITOR_CENTER_PANE_PAD * 2;
    viewport_h = state->center_pane_rect.h - EDITOR_VIEWPORT_TOP_RESERVE - EDITOR_VIEWPORT_BOTTOM_PAD;
    if (viewport_w < 220) viewport_w = 220;
    if (viewport_h < 180) viewport_h = 180;
    state->viewport_surface_rect = (SDL_Rect){viewport_x, viewport_y, viewport_w, viewport_h};

    state->center_title_rect = (SDL_Rect){
        state->center_pane_rect.x + 12,
        state->center_pane_rect.y + EDITOR_VIEWPORT_TITLE_Y,
        state->center_pane_rect.w - 24,
        22
    };
    state->center_name_rect = (SDL_Rect){
        state->center_pane_rect.x + 12,
        state->center_pane_rect.y + EDITOR_VIEWPORT_NAME_Y,
        state->center_pane_rect.w - 24,
        24
    };
    state->center_summary_rect = (SDL_Rect){
        state->center_pane_rect.x + 12,
        state->center_pane_rect.y + EDITOR_VIEWPORT_SUMMARY_Y,
        state->center_pane_rect.w - 24,
        34
    };

    center_inner_x = state->viewport_surface_rect.x + EDITOR_LEGACY_CANVAS_INSET;
    center_inner_y = state->viewport_surface_rect.y + EDITOR_LEGACY_CANVAS_INSET;
    canvas_max_w = state->viewport_surface_rect.w - EDITOR_LEGACY_CANVAS_INSET * 2;
    canvas_max_h = state->viewport_surface_rect.h - EDITOR_LEGACY_CANVAS_INSET * 2;
    if (canvas_max_w < 220) canvas_max_w = 220;
    if (canvas_max_h < 180) canvas_max_h = 180;

    canvas_w = canvas_max_w;
    canvas_h = (int)lroundf((float)canvas_w / aspect);
    if (canvas_h > canvas_max_h) {
        canvas_h = canvas_max_h;
        canvas_w = (int)lroundf((float)canvas_h * aspect);
    }
    const int min_canvas_w = 220;
    const int min_canvas_h = 180;
    if (canvas_w < min_canvas_w) {
        canvas_w = min_canvas_w;
        canvas_h = (int)lroundf((float)canvas_w / aspect);
    }
    if (canvas_h < min_canvas_h) {
        canvas_h = min_canvas_h;
        canvas_w = (int)lroundf((float)canvas_h * aspect);
        if (canvas_w > canvas_max_w) {
            canvas_w = canvas_max_w;
            canvas_h = (int)lroundf((float)canvas_w / aspect);
        }
    }

    state->canvas_y = center_inner_y + (canvas_max_h - canvas_h) / 2;
    state->canvas_x = center_inner_x + (canvas_max_w - canvas_w) / 2;
    state->canvas_width = canvas_w;
    state->canvas_height = canvas_h;
    editor_update_dimension_rects(state);
}

SDL_Rect editor_name_rect(const SceneEditorState *state) {
    if (!state) {
        SDL_Rect rect = {0};
        return rect;
    }
    return state->center_name_rect;
}

SDL_Rect editor_active_viewport_rect(const SceneEditorState *state) {
    SDL_Rect rect = {0};
    if (!state) return rect;
    if (physics_sim_editor_session_has_retained_scene(&state->session) &&
        state->viewport_surface_rect.w > 0 &&
        state->viewport_surface_rect.h > 0) {
        return state->viewport_surface_rect;
    }
    rect.x = state->canvas_x;
    rect.y = state->canvas_y;
    rect.w = state->canvas_width;
    rect.h = state->canvas_height;
    return rect;
}

void editor_begin_name_edit(SceneEditorState *state) {
    if (!state || !state->name_edit_ptr || state->name_edit_capacity == 0) return;
    text_input_begin(&state->name_input,
                     state->name_edit_ptr,
                     state->name_edit_capacity - 1);
    state->renaming_name = true;
}

void editor_finish_name_edit(SceneEditorState *state, bool apply) {
    if (!state || !state->renaming_name) return;
    if (apply && state->name_edit_ptr) {
        const char *value = text_input_value(&state->name_input);
        if (!value || value[0] == '\0') {
            state->name_edit_ptr[0] = '\0';
        } else {
            snprintf(state->name_edit_ptr, state->name_edit_capacity, "%s", value);
        }
        state->working.name = state->name_edit_ptr;
        if (state->name_buffer && state->name_buffer != state->name_edit_ptr) {
            snprintf(state->name_buffer, state->name_capacity, "%s", state->name_edit_ptr);
        }
        if (physics_sim_editor_session_has_retained_scene(&state->session)) {
            editor_refresh_retained_save_diagnostics(state, NULL);
        }
    }
    text_input_end(&state->name_input);
    state->renaming_name = false;
}

void editor_begin_dimension_edit(SceneEditorState *state, bool editing_width) {
    if (!state) return;
    if (editing_width) {
        if (state->editing_height) {
            editor_finish_dimension_edit(state, false, false);
        }
        char buffer[32];
        snprintf(buffer, sizeof(buffer), "%.2f", state->working.domain_width);
        text_input_begin(&state->width_input, buffer, 10);
        state->editing_width = true;
    } else {
        if (state->editing_width) {
            editor_finish_dimension_edit(state, true, false);
        }
        char buffer[32];
        snprintf(buffer, sizeof(buffer), "%.2f", state->working.domain_height);
        text_input_begin(&state->height_input, buffer, 10);
        state->editing_height = true;
    }
}

void editor_finish_dimension_edit(SceneEditorState *state,
                                  bool editing_width,
                                  bool apply) {
    if (!state) return;
    TextInputField *field = editing_width ? &state->width_input : &state->height_input;
    if (!field->active) {
        if (editing_width) state->editing_width = false;
        else state->editing_height = false;
        return;
    }
    if (apply) {
        const char *text = text_input_value(field);
        if (text && text[0]) {
            float value = (float)atof(text);
            value = sanitize_domain_dimension(value);
            if (editing_width) {
                if (fabsf(state->working.domain_width - value) > 1e-3f) {
                    state->working.domain_width = value;
                    set_dirty(state);
                }
            } else {
                if (fabsf(state->working.domain_height - value) > 1e-3f) {
                    state->working.domain_height = value;
                    set_dirty(state);
                }
            }
            editor_update_canvas_layout(state);
        }
    }
    text_input_end(field);
    if (editing_width) state->editing_width = false;
    else state->editing_height = false;
}

float clamp01(float v) {
    if (v < 0.0f) return 0.0f;
    if (v > 1.0f) return 1.0f;
    return v;
}

static void editor_refresh_emitter_attachment_maps(SceneEditorState *state) {
    int i = 0;
    if (!state) return;
    for (i = 0; i < MAX_FLUID_EMITTERS; ++i) {
        state->emitter_object_map[i] = -1;
        state->emitter_import_map[i] = -1;
    }
    for (i = 0; i < (int)state->working.emitter_count && i < MAX_FLUID_EMITTERS; ++i) {
        FluidEmitter *em = &state->working.emitters[i];
        if (em->attached_object < 0 || em->attached_object >= (int)state->working.object_count) {
            em->attached_object = -1;
        }
        if (em->attached_import < 0 || em->attached_import >= (int)state->working.import_shape_count) {
            em->attached_import = -1;
        }
        state->emitter_object_map[i] = em->attached_object;
        state->emitter_import_map[i] = em->attached_import;
    }
}

static bool editor_retained_scene_bounds(const SceneEditorState *state,
                                         float *out_min_x,
                                         float *out_min_y,
                                         float *out_min_z,
                                         float *out_max_x,
                                         float *out_max_y,
                                         float *out_max_z) {
    CoreObjectVec3 corners[8];
    const PhysicsSimRetainedRuntimeScene *retained = NULL;
    int i = 0;
    bool have = false;
    if (!state || !physics_sim_editor_session_has_retained_scene(&state->session)) return false;
    retained = &state->session.retained_scene;
    if (retained->has_line_drawing_scene3d && retained->bounds.enabled) {
        *out_min_x = (float)retained->bounds.min.x;
        *out_min_y = (float)retained->bounds.min.y;
        *out_min_z = (float)retained->bounds.min.z;
        *out_max_x = (float)retained->bounds.max.x;
        *out_max_y = (float)retained->bounds.max.y;
        *out_max_z = (float)retained->bounds.max.z;
        return true;
    }

    for (i = 0; i < retained->retained_object_count; ++i) {
        const CoreSceneObjectContract *object = &retained->objects[i];
        int corner_count = 0;
        if (object->has_plane_primitive) {
            const CoreScenePlanePrimitive *plane = &object->plane_primitive;
            const double half_width = plane->width * 0.5;
            const double half_height = plane->height * 0.5;
            CoreObjectVec3 origin = plane->frame.origin;
            CoreObjectVec3 u_plus = origin;
            CoreObjectVec3 u_minus = origin;
            u_plus.x += plane->frame.axis_u.x * half_width;
            u_plus.y += plane->frame.axis_u.y * half_width;
            u_plus.z += plane->frame.axis_u.z * half_width;
            u_minus.x -= plane->frame.axis_u.x * half_width;
            u_minus.y -= plane->frame.axis_u.y * half_width;
            u_minus.z -= plane->frame.axis_u.z * half_width;
            corners[0] = u_minus;
            corners[0].x -= plane->frame.axis_v.x * half_height;
            corners[0].y -= plane->frame.axis_v.y * half_height;
            corners[0].z -= plane->frame.axis_v.z * half_height;
            corners[1] = u_plus;
            corners[1].x -= plane->frame.axis_v.x * half_height;
            corners[1].y -= plane->frame.axis_v.y * half_height;
            corners[1].z -= plane->frame.axis_v.z * half_height;
            corners[2] = u_plus;
            corners[2].x += plane->frame.axis_v.x * half_height;
            corners[2].y += plane->frame.axis_v.y * half_height;
            corners[2].z += plane->frame.axis_v.z * half_height;
            corners[3] = u_minus;
            corners[3].x += plane->frame.axis_v.x * half_height;
            corners[3].y += plane->frame.axis_v.y * half_height;
            corners[3].z += plane->frame.axis_v.z * half_height;
            corner_count = 4;
        } else if (object->has_rect_prism_primitive) {
            const CoreSceneRectPrismPrimitive *prism = &object->rect_prism_primitive;
            const double half_width = prism->width * 0.5;
            const double half_height = prism->height * 0.5;
            const double half_depth = prism->depth * 0.5;
            int corner_index = 0;
            for (int sx = -1; sx <= 1; sx += 2) {
                for (int sy = -1; sy <= 1; sy += 2) {
                    for (int sz = -1; sz <= 1; sz += 2) {
                        CoreObjectVec3 corner = prism->frame.origin;
                        corner.x += prism->frame.axis_u.x * half_width * (double)sx;
                        corner.y += prism->frame.axis_u.y * half_width * (double)sx;
                        corner.z += prism->frame.axis_u.z * half_width * (double)sx;
                        corner.x += prism->frame.axis_v.x * half_height * (double)sy;
                        corner.y += prism->frame.axis_v.y * half_height * (double)sy;
                        corner.z += prism->frame.axis_v.z * half_height * (double)sy;
                        corner.x += prism->frame.normal.x * half_depth * (double)sz;
                        corner.y += prism->frame.normal.y * half_depth * (double)sz;
                        corner.z += prism->frame.normal.z * half_depth * (double)sz;
                        corners[corner_index++] = corner;
                    }
                }
            }
            corner_count = 8;
        } else {
            corners[0] = object->object.transform.position;
            corner_count = 1;
        }

        for (int c = 0; c < corner_count; ++c) {
            float px = (float)corners[c].x;
            float py = (float)corners[c].y;
            float pz = (float)corners[c].z;
            if (!have) {
                *out_min_x = *out_max_x = px;
                *out_min_y = *out_max_y = py;
                *out_min_z = *out_max_z = pz;
                have = true;
            } else {
                if (px < *out_min_x) *out_min_x = px;
                if (py < *out_min_y) *out_min_y = py;
                if (pz < *out_min_z) *out_min_z = pz;
                if (px > *out_max_x) *out_max_x = px;
                if (py > *out_max_y) *out_max_y = py;
                if (pz > *out_max_z) *out_max_z = pz;
            }
        }
    }
    return have;
}

void editor_frame_viewport_to_scene(SceneEditorState *state) {
    float min_x = 0.0f;
    float min_y = 0.0f;
    float min_z = 0.0f;
    float max_x = 1.0f;
    float max_y = 1.0f;
    float max_z = 0.0f;
    SDL_Rect rect = {0};
    if (!state) return;
    rect = editor_active_viewport_rect(state);
    if (editor_retained_scene_bounds(state,
                                     &min_x,
                                     &min_y,
                                     &min_z,
                                     &max_x,
                                     &max_y,
                                     &max_z)) {
        scene_editor_viewport_frame_bounds(&state->viewport,
                                           rect.w,
                                           rect.h,
                                           min_x,
                                           min_y,
                                           min_z,
                                           max_x,
                                           max_y,
                                           max_z);
    } else {
        scene_editor_viewport_frame(&state->viewport);
    }
    scene_editor_canvas_set_viewport_state(&state->viewport);
}

bool editor_load_runtime_scene_fixture(SceneEditorState *state,
                                       const char *runtime_scene_path,
                                       char *out_diagnostics,
                                       size_t out_diagnostics_size) {
    RuntimeSceneBridgePreflight summary = {0};
    PhysicsSimRetainedRuntimeScene retained = {0};
    SceneEditorBootstrap bootstrap = {0};
    const char *scene_id = NULL;
    CoreBuffer file_data = {0};
    CoreResult io_result = {0};
    bool stored_runtime_json = false;
    if (!state || !runtime_scene_path || !runtime_scene_path[0]) {
        if (out_diagnostics && out_diagnostics_size > 0) {
            snprintf(out_diagnostics, out_diagnostics_size, "runtime scene path missing");
        }
        return false;
    }
    io_result = core_io_read_all(runtime_scene_path, &file_data);
    if (io_result.code != CORE_OK || !file_data.data || file_data.size == 0) {
        if (out_diagnostics && out_diagnostics_size > 0) {
            snprintf(out_diagnostics, out_diagnostics_size, "failed to read runtime scene fixture");
        }
        return false;
    }
    if (!editor_store_retained_runtime_scene_json(state, (const char *)file_data.data, file_data.size)) {
        core_io_buffer_free(&file_data);
        if (out_diagnostics && out_diagnostics_size > 0) {
            snprintf(out_diagnostics, out_diagnostics_size, "failed to cache runtime scene source");
        }
        return false;
    }
    stored_runtime_json = true;
    core_io_buffer_free(&file_data);

    if (!runtime_scene_bridge_apply_json(state->retained_runtime_scene_json,
                                         &state->cfg,
                                         &state->working,
                                         &summary)) {
        if (out_diagnostics && out_diagnostics_size > 0) {
            snprintf(out_diagnostics, out_diagnostics_size, "%s", summary.diagnostics);
        }
        if (stored_runtime_json) {
            free(state->retained_runtime_scene_json);
            state->retained_runtime_scene_json = NULL;
        }
        return false;
    }
    runtime_scene_bridge_get_last_retained_scene(&retained);
    if (!retained.valid_contract) {
        if (out_diagnostics && out_diagnostics_size > 0) {
            snprintf(out_diagnostics, out_diagnostics_size, "%s", retained.diagnostics);
        }
        return false;
    }

    bootstrap.has_retained_scene = true;
    bootstrap.retained_scene = retained;
    physics_sim_editor_session_init(&state->session, &state->working, &bootstrap);
    if (!physics_sim_editor_session_hydrate_overlay_from_runtime_scene_json(&state->session,
                                                                            state->retained_runtime_scene_json,
                                                                            NULL,
                                                                            0)) {
        fprintf(stderr, "[editor] Retained scene overlay hydrate failed; continuing with defaults\n");
    }
    editor_refresh_scene_library(state);
    editor_refresh_emitter_attachment_maps(state);
    state->showing_import_picker = false;
    state->selected_import_row = -1;
    state->hover_import_row = -1;
    state->hover_row = -1;
    state->hover_object = -1;
    state->hover_emitter = -1;
    scene_editor_input_clear_drag_flags(state);
    state->pointer_down_in_canvas = false;
    state->pointer_drag_started = false;
    state->dirty = false;
    state->overlay_apply_success = false;
    snprintf(state->overlay_apply_diagnostics,
             sizeof(state->overlay_apply_diagnostics),
             "Overlay not yet applied for this retained scene.");
    state->save_scene_success = false;
    free(state->last_applied_overlay_json);
    state->last_applied_overlay_json = NULL;

    scene_id = retained.root.scene_id[0] ? retained.root.scene_id : summary.scene_id;
    editor_seed_retained_document_identity(state, runtime_scene_path, scene_id);
    editor_refresh_retained_save_diagnostics(state, NULL);

    if (state->working.object_count > 0) {
        scene_editor_select_object(state, 0);
    } else {
        scene_editor_select_none(state);
    }
    editor_update_canvas_layout(state);
    editor_frame_viewport_to_scene(state);

    if (out_diagnostics && out_diagnostics_size > 0) {
        snprintf(out_diagnostics,
                 out_diagnostics_size,
                 "Loaded %s (%d retained objects)",
                 state->name_edit_ptr && state->name_edit_ptr[0] ? state->name_edit_ptr : retained.root.scene_id,
                 retained.retained_object_count);
    }
    return true;
}

void canvas_to_normalized_unclamped(const SceneEditorState *state,
                                    int sx,
                                    int sy,
                                    float *out_x,
                                    float *out_y) {
    if (!state || !out_x || !out_y ||
        state->canvas_width <= 0 || state->canvas_height <= 0) return;
    scene_editor_viewport_screen_to_world(&state->viewport,
                                          state->canvas_x,
                                          state->canvas_y,
                                          state->canvas_width,
                                          state->canvas_height,
                                          sx,
                                          sy,
                                          out_x,
                                          out_y);
}
