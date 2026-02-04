#include "app/editor/scene_editor_input.h"
#include "app/editor/scene_editor_internal.h"
#include "app/editor/scene_editor_canvas.h"
#include "app/editor/scene_editor_import.h"
#include "app/editor/scene_editor_model.h"
#include "app/editor/scene_editor_precision.h"
#include "app/shape_lookup.h"
#include "import/shape_import.h"
#include "geo/shape_asset.h"
#include "vk_renderer.h"
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static const int DRAG_THRESHOLD_PX = 4;
static void clear_drag_flags(SceneEditorState *state);

static bool point_in_rect(const SDL_Rect *rect, int x, int y) {
    if (!rect) return false;
    return x >= rect->x && x < rect->x + rect->w &&
           y >= rect->y && y < rect->y + rect->h;
}

// Helper to trace and choose which object/import (if any) should receive an emitter when
// clicking Source/Jet/Sink. Logs selection state for debugging.
static void pick_target_for_emitter(SceneEditorState *state, int *out_obj, int *out_imp) {
    if (out_obj) *out_obj = -1;
    if (out_imp) *out_imp = -1;
    if (!state) return;

    int target_obj = state->selected_object;
    int target_imp = (state->selection_kind == SELECTION_IMPORT) ? state->selected_row : -1;
    int attached_obj = -1;
    int attached_imp = -1;
    if (state->selected_emitter >= 0 &&
        state->selected_emitter < (int)state->working.emitter_count) {
        attached_obj = state->emitter_object_map[state->selected_emitter];
        if (attached_obj < 0) attached_obj = state->working.emitters[state->selected_emitter].attached_object;
        attached_imp = state->emitter_import_map[state->selected_emitter];
        if (attached_imp < 0) attached_imp = state->working.emitters[state->selected_emitter].attached_import;
        if (target_obj < 0 && attached_obj >= 0) target_obj = attached_obj;
        if (target_imp < 0 && attached_imp >= 0) target_imp = attached_imp;
    }
    if (target_obj < 0) target_obj = state->hover_object;
    if (target_imp < 0) target_imp = state->hover_import_row;
    if (target_obj < 0 && state->working.object_count == 1) target_obj = 0;
    if (target_imp < 0 && state->working.import_shape_count == 1) target_imp = 0;

    SDL_Log("editor: emitter button target pick sel_kind=%d sel_obj=%d sel_em=%d attached_obj=%d attached_imp=%d hover_obj=%d hover_imp=%d obj_count=%zu imp_count=%zu -> target_obj=%d target_imp=%d",
            state->selection_kind,
            state->selected_object,
            state->selected_emitter,
            attached_obj,
            attached_imp,
            state->hover_object,
            state->hover_import_row,
            state->working.object_count,
            state->working.import_shape_count,
            target_obj,
            target_imp);

    if (out_obj) *out_obj = target_obj;
    if (out_imp) *out_imp = target_imp;
}

static void finish_and_apply(SceneEditorState *state) {
    if (!state) return;
    commit_field_edit(state);
    if (state->editing_width) {
        editor_finish_dimension_edit(state, true, true);
    }
    if (state->editing_height) {
        editor_finish_dimension_edit(state, false, true);
    }
    state->applied = true;
    state->running = false;
}

static void cancel_and_close(SceneEditorState *state) {
    if (!state) return;
    if (state->editing_width) {
        editor_finish_dimension_edit(state, true, false);
    }
    if (state->editing_height) {
        editor_finish_dimension_edit(state, false, false);
    }
    clear_drag_flags(state);
    state->pointer_down_in_canvas = false;
    state->pointer_drag_started = false;
    state->dirty = false;
    state->applied = false;
    state->running = false;
}

static void remove_import_at(SceneEditorState *state, int index) {
    if (!state || index < 0 || index >= (int)state->working.import_shape_count) return;
    // Remove any emitter attached to this import.
    int em_idx = emitter_index_for_import(state, index);
    if (em_idx >= 0) {
        remove_emitter_at(state, em_idx);
    }
    // Shift any mappings above the removed import.
    for (size_t ei = 0; ei < state->working.emitter_count; ++ei) {
        if (state->emitter_import_map[ei] > index) {
            state->emitter_import_map[ei]--;
        } else if (state->emitter_import_map[ei] == index) {
            state->emitter_import_map[ei] = -1;
            state->working.emitters[ei].attached_import = -1;
        }
    }
    for (int i = index; i + 1 < (int)state->working.import_shape_count; ++i) {
        state->working.import_shapes[i] = state->working.import_shapes[i + 1];
    }
    state->working.import_shape_count--;
    if (state->selected_row >= (int)state->working.import_shape_count) {
        state->selected_row = (int)state->working.import_shape_count - 1;
    }
    set_dirty(state);
}

static void resolve_import_shape_id(SceneEditorState *state, ImportedShape *imp) {
    if (!state || !imp || !state->shape_library) return;
    const ShapeAsset *asset = shape_lookup_from_path(state->shape_library, imp->path);
    if (!asset) {
        fprintf(stderr, "[editor] No asset match for import path: %s\n", imp->path);
        imp->shape_id = -1;
        return;
    }
    for (size_t si = 0; si < state->shape_library->count; ++si) {
        if (&state->shape_library->assets[si] == asset) {
            imp->shape_id = (int)si;
            fprintf(stderr, "[editor] Resolved import '%s' to shape_id=%d (name=%s)\n",
                    imp->path, imp->shape_id, asset->name ? asset->name : "(unnamed)");
            return;
        }
    }
    imp->shape_id = -1;
}

static bool ensure_shape_loaded(SceneEditorState *state, const char *asset_path) {
    if (!state || !asset_path || !state->shape_library) return false;
    if (shape_lookup_from_path(state->shape_library, asset_path)) {
        return true;
    }
    // Append into the shared library so the newly converted asset is available immediately.
    ShapeAsset asset = {0};
    if (!shape_asset_load_file(asset_path, &asset)) {
        fprintf(stderr, "[editor] Failed to load asset %s\n", asset_path);
        return false;
    }
    ShapeAssetLibrary *lib = (ShapeAssetLibrary *)state->shape_library; // safe: editor owns the lifetime
    ShapeAsset *tmp = (ShapeAsset *)realloc(lib->assets, (lib->count + 1) * sizeof(ShapeAsset));
    if (!tmp) {
        shape_asset_free(&asset);
        fprintf(stderr, "[editor] Failed to grow shape library for %s\n", asset_path);
        return false;
    }
    lib->assets = tmp;
    lib->assets[lib->count] = asset;
    lib->count += 1;
    fprintf(stderr, "[editor] Appended asset to library: %s (count=%zu)\n", asset_path, lib->count);
    return true;
}

static bool path_starts_with(const char *s, const char *prefix) {
    if (!s || !prefix) return false;
    size_t len = strlen(prefix);
    return strncmp(s, prefix, len) == 0;
}

static void to_asset_basename(const char *import_path, char *out_name, size_t out_sz) {
    if (!out_name || out_sz == 0) return;
    out_name[0] = '\0';
    if (!import_path) return;
    const char *base = strrchr(import_path, '/');
    base = base ? base + 1 : import_path;
    const char *dot = strrchr(base, '.');
    size_t len = dot && dot > base ? (size_t)(dot - base) : strlen(base);
    if (len >= out_sz) len = out_sz - 1;
    memcpy(out_name, base, len);
    out_name[len] = '\0';
}

static bool convert_import_to_asset(const char *import_path,
                                    char *out_asset_path,
                                    size_t out_sz) {
    if (!import_path || !out_asset_path || out_sz == 0) return false;
    out_asset_path[0] = '\0';

    char name[256];
    to_asset_basename(import_path, name, sizeof(name));
    if (name[0] == '\0') return false;

    char asset_path[512];
    snprintf(asset_path, sizeof(asset_path), "config/objects/%s.asset.json", name);

    // If asset already exists, reuse it.
    FILE *f = fopen(asset_path, "rb");
    if (f) {
        fclose(f);
        snprintf(out_asset_path, out_sz, "%s", asset_path);
        return true;
    }

    ShapeDocument doc;
    if (!shape_import_load(import_path, &doc) || doc.shapeCount == 0) {
        return false;
    }
    ShapeAsset asset;
    bool ok = shape_asset_from_shapelib_shape(&doc.shapes[0], 0.5f, &asset);
    if (ok) {
        if (asset.name) free(asset.name);
        asset.name = (char *)malloc(strlen(name) + 1);
        if (asset.name) {
            memcpy(asset.name, name, strlen(name) + 1);
        }
        ok = shape_asset_save_file(&asset, asset_path);
    }
    shape_asset_free(&asset);
    ShapeDocument_Free(&doc);
    if (ok) {
        snprintf(out_asset_path, out_sz, "%s", asset_path);
    }
    return ok;
}

static bool add_import_from_picker(SceneEditorState *state, int row) {
    if (!state || row < 0 || row >= state->import_file_count) return false;
    const char *selected_path = state->import_files[row];
    char asset_path[512] = {0};
    const char *store_path = selected_path;
    if (path_starts_with(selected_path, "import/")) {
        if (convert_import_to_asset(selected_path, asset_path, sizeof(asset_path))) {
            store_path = asset_path;
            scene_editor_refresh_import_files(state);
        }
    }
    ensure_shape_loaded(state, store_path);
    if (state->working.import_shape_count < MAX_IMPORTED_SHAPES) {
        ImportedShape *imp = &state->working.import_shapes[state->working.import_shape_count++];
        memset(imp, 0, sizeof(*imp));
        snprintf(imp->path, sizeof(imp->path), "%s", store_path);
        imp->shape_id = -1;
        imp->position_x = 0.5f;
        imp->position_y = 0.5f;
        imp->scale = 1.0f;
        imp->rotation_deg = 0.0f;
        imp->density = 1.0f;
        imp->friction = 0.2f;
        imp->is_static = true;
        imp->enabled = true;
        state->selected_row = (int)state->working.import_shape_count - 1;
        state->selection_kind = SELECTION_IMPORT;
        fprintf(stderr, "[editor] Added import row %zu: %s\n",
                state->working.import_shape_count - 1, store_path);
        resolve_import_shape_id(state, imp);
        set_dirty(state);
    }
    state->showing_import_picker = false;
    return true;
}

static bool add_import_from_existing(SceneEditorState *state, int row) {
    if (!state || row < 0 || row >= (int)state->working.import_shape_count) return false;
    if (state->working.import_shape_count >= MAX_IMPORTED_SHAPES) return false;
    const ImportedShape *src = &state->working.import_shapes[row];
    ImportedShape *imp = &state->working.import_shapes[state->working.import_shape_count++];
    memset(imp, 0, sizeof(*imp));
    snprintf(imp->path, sizeof(imp->path), "%s", src->path);
    ensure_shape_loaded(state, imp->path);
    imp->shape_id = -1;
    imp->position_x = 0.5f;
    imp->position_y = 0.5f;
    imp->scale = 1.0f;
    imp->rotation_deg = 0.0f;
    imp->density = 1.0f;
    imp->friction = 0.2f;
    imp->is_static = true;
    imp->enabled = true;
    state->selected_row = (int)state->working.import_shape_count - 1;
    state->selection_kind = SELECTION_IMPORT;
    fprintf(stderr, "[editor] Duplicated import %d -> %d: %s\n",
            row, state->selected_row, imp->path);
    resolve_import_shape_id(state, imp);
    set_dirty(state);
    return true;
}

__attribute__((unused))
static void scene_editor_input_keep_helpers(void) {
    (void)add_import_from_existing;
}

static bool hit_matches_selection(const SceneEditorState *state, const SceneEditorHit *hit) {
    if (!state || !hit) return false;
    switch (hit->kind) {
    case HIT_EMITTER:
        return state->selection_kind == SELECTION_EMITTER && state->selected_emitter == hit->index;
    case HIT_OBJECT:
    case HIT_OBJECT_HANDLE:
        return state->selection_kind == SELECTION_OBJECT && state->selected_object == hit->index;
    case HIT_IMPORT:
    case HIT_IMPORT_HANDLE:
        return state->selection_kind == SELECTION_IMPORT && state->selected_row == hit->index;
    case HIT_BOUNDARY_EDGE:
        return state->boundary_selected_edge == hit->boundary_edge;
    default:
        return false;
    }
}

static void clear_drag_flags(SceneEditorState *state) {
    if (!state) return;
    state->dragging_import_handle = false;
    state->dragging_import_body = false;
    state->dragging = false;
    state->drag_mode = DRAG_NONE;
    state->dragging_object = false;
    state->dragging_object_handle = false;
    state->handle_resize_started = false;
}

static void select_from_hit(SceneEditorState *state, const SceneEditorHit *hit) {
    if (!state || !hit) return;
    switch (hit->kind) {
    case HIT_EMITTER:
        state->selection_kind = SELECTION_EMITTER;
        state->selected_emitter = hit->index;
        state->selected_object = -1;
        state->selected_row = -1;
        break;
    case HIT_OBJECT:
    case HIT_OBJECT_HANDLE:
        state->selection_kind = SELECTION_OBJECT;
        state->selected_object = hit->index;
        state->selected_emitter = -1;
        state->selected_row = -1;
        break;
    case HIT_IMPORT:
    case HIT_IMPORT_HANDLE:
        state->selection_kind = SELECTION_IMPORT;
        state->selected_row = hit->index;
        state->selected_object = -1;
        state->selected_emitter = -1;
        break;
    case HIT_BOUNDARY_EDGE:
        state->boundary_selected_edge = hit->boundary_edge;
        break;
    default:
        state->selection_kind = SELECTION_NONE;
        state->selected_emitter = -1;
        state->selected_object = -1;
        state->selected_row = -1;
        break;
    }
}

static void prepare_drag_for_hit(SceneEditorState *state, const SceneEditorHit *hit, int x, int y) {
    if (!state || !hit) return;
    clear_drag_flags(state);
    switch (hit->kind) {
    case HIT_IMPORT_HANDLE:
        if (state->shape_library && hit->index >= 0 &&
            hit->index < (int)state->working.import_shape_count) {
            ImportedShape *imp = &state->working.import_shapes[hit->index];
            float nx = 0.0f, ny = 0.0f;
            scene_editor_canvas_to_import_normalized(state->canvas_x,
                                                     state->canvas_y,
                                                     state->canvas_width,
                                                     state->canvas_height,
                                                     x,
                                                     y,
                                                     &nx,
                                                     &ny);
            float dxn = nx - imp->position_x;
            float dyn = ny - imp->position_y;
            float dist = sqrtf(dxn * dxn + dyn * dyn);
            if (dist < 0.0001f) dist = 0.0001f;
            state->dragging_import_handle = true;
            state->import_handle_start_dist = dist;
            state->import_handle_start_scale = imp->scale;
        }
        break;
    case HIT_IMPORT:
        if (hit->index >= 0 &&
            hit->index < (int)state->working.import_shape_count) {
            state->dragging_import_body = true;
            state->pointer_drag_started = true;
            float nx = 0.0f, ny = 0.0f;
            scene_editor_canvas_to_import_normalized(state->canvas_x,
                                                     state->canvas_y,
                                                     state->canvas_width,
                                                     state->canvas_height,
                                                     x,
                                                     y,
                                                     &nx,
                                                     &ny);
            ImportedShape *imp = &state->working.import_shapes[hit->index];
            state->import_body_drag_off_x = nx - imp->position_x;
            state->import_body_drag_off_y = ny - imp->position_y;
        }
        break;
    case HIT_OBJECT_HANDLE:
        if (hit->index >= 0 && hit->index < (int)state->working.object_count) {
            state->dragging_object_handle = true;
            state->pointer_drag_started = true;
            PresetObject *obj = &state->working.objects[hit->index];
            float half_w_px = 0.0f, half_h_px = 0.0f;
            scene_editor_canvas_object_visual_half_sizes_px(obj,
                                                            state->canvas_width,
                                                            state->canvas_height,
                                                            &half_w_px,
                                                            &half_h_px);
            if (obj->type == PRESET_OBJECT_CIRCLE) {
                state->object_handle_ratio = 1.0f;
            } else {
                state->object_handle_ratio = (half_w_px > 0.0001f)
                                                 ? (half_h_px / half_w_px)
                                                 : 1.0f;
                if (state->object_handle_ratio <= 0.0f) state->object_handle_ratio = 1.0f;
            }
            int cx = 0, cy = 0;
            scene_editor_canvas_project(state->canvas_x,
                                        state->canvas_y,
                                        state->canvas_width,
                                        state->canvas_height,
                                        obj->position_x,
                                        obj->position_y,
                                        &cx,
                                        &cy);
            float dx_px = (float)x - (float)cx;
            float dy_px = (float)y - (float)cy;
            float len_px = sqrtf(dx_px * dx_px + dy_px * dy_px);
            float min_len_px = (obj->type == PRESET_OBJECT_BOX)
                                   ? (float)SCENE_EDITOR_OBJECT_MIN_HALF_PX
                                   : (float)SCENE_EDITOR_OBJECT_MIN_RADIUS_PX;
            float adjusted_px = len_px - (float)SCENE_EDITOR_OBJECT_HANDLE_MARGIN_PX;
            if (adjusted_px < min_len_px) adjusted_px = min_len_px;
            state->handle_initial_length = adjusted_px;
            state->handle_resize_started = false;
        }
        break;
    case HIT_OBJECT:
        if (hit->index >= 0 && hit->index < (int)state->working.object_count) {
            state->dragging_object = true;
            state->pointer_drag_started = true;
            state->dragging_object_handle = false;
            state->handle_resize_started = false;
            PresetObject *obj = &state->working.objects[hit->index];
            float nx, ny;
            scene_editor_canvas_to_normalized(state->canvas_x,
                                              state->canvas_y,
                                              state->canvas_width,
                                              state->canvas_height,
                                              x,
                                              y,
                                              &nx,
                                              &ny);
            state->object_drag_offset_x = nx - obj->position_x;
            state->object_drag_offset_y = ny - obj->position_y;
        }
        break;
    case HIT_EMITTER:
        if (hit->index >= 0 && hit->index < (int)state->working.emitter_count) {
            int attached_obj = state->emitter_object_map[hit->index];
            int attached_imp = state->emitter_import_map[hit->index];
            if (attached_obj < 0) attached_obj = state->working.emitters[hit->index].attached_object;
            if (attached_imp < 0) attached_imp = state->working.emitters[hit->index].attached_import;
            if (hit->drag_mode == DRAG_POSITION && attached_obj >= 0 &&
                attached_obj < (int)state->working.object_count) {
                // Dragging the body of an attached emitter moves the object (and keeps emitter in sync).
                state->selected_object = attached_obj;
                state->dragging_object = true;
                state->pointer_drag_started = true;
                PresetObject *obj = &state->working.objects[attached_obj];
                float nx, ny;
                scene_editor_canvas_to_normalized(state->canvas_x,
                                                  state->canvas_y,
                                                  state->canvas_width,
                                                  state->canvas_height,
                                                  x,
                                                  y,
                                                  &nx,
                                                  &ny);
                state->object_drag_offset_x = nx - obj->position_x;
                state->object_drag_offset_y = ny - obj->position_y;
                state->selected_emitter = hit->index;
                state->selection_kind = SELECTION_EMITTER;
            } else if (hit->drag_mode == DRAG_POSITION && attached_imp >= 0 &&
                       attached_imp < (int)state->working.import_shape_count) {
                state->selected_row = attached_imp;
                state->selection_kind = SELECTION_IMPORT;
                state->dragging_import_body = true;
                state->pointer_drag_started = true;
                float nx = 0.0f, ny = 0.0f;
                scene_editor_canvas_to_import_normalized(state->canvas_x,
                                                         state->canvas_y,
                                                         state->canvas_width,
                                                         state->canvas_height,
                                                         x,
                                                         y,
                                                         &nx,
                                                         &ny);
                ImportedShape *imp = &state->working.import_shapes[attached_imp];
                state->import_body_drag_off_x = nx - imp->position_x;
                state->import_body_drag_off_y = ny - imp->position_y;
                state->selected_emitter = hit->index;
            } else {
                state->drag_mode = hit->drag_mode;
                state->dragging = true;
                state->pointer_drag_started = true;
                state->drag_offset_x = 0.0f;
                state->drag_offset_y = 0.0f;
                if (hit->drag_mode == DRAG_POSITION) {
                    FluidEmitter *em = &state->working.emitters[hit->index];
                    float nx, ny;
                    scene_editor_canvas_to_normalized(state->canvas_x,
                                                      state->canvas_y,
                                                      state->canvas_width,
                                                      state->canvas_height,
                                                      x,
                                                      y,
                                                      &nx,
                                                      &ny);
                    state->drag_offset_x = nx - em->position_x;
                    state->drag_offset_y = ny - em->position_y;
                    state->emitter_handle_offset_px = 0.0f;
                } else {
                    // Capture handle grab offset so radius won't jump on initial drag.
                    int cx = 0, cy = 0;
                    FluidEmitter *em = &state->working.emitters[hit->index];
                    scene_editor_canvas_project(state->canvas_x,
                                                state->canvas_y,
                                                state->canvas_width,
                                                state->canvas_height,
                                                em->position_x,
                                                em->position_y,
                                                &cx,
                                                &cy);
                    float dx = (float)x - (float)cx;
                    float dy = (float)y - (float)cy;
                    float len = sqrtf(dx * dx + dy * dy);
                    float min_dim = (float)((state->canvas_width < state->canvas_height) ? state->canvas_width : state->canvas_height);
                    float radius_px = (float)em->radius * min_dim;
                    state->emitter_handle_offset_px = len - radius_px;
                    if (state->emitter_handle_offset_px < 0.0f) state->emitter_handle_offset_px = 0.0f;
                }
            }
        }
        break;
    case HIT_BOUNDARY_EDGE:
        state->boundary_selected_edge = hit->boundary_edge;
        if (state->boundary_mode &&
            hit->boundary_edge >= 0 &&
            hit->boundary_edge < BOUNDARY_EDGE_COUNT &&
            state->working.boundary_flows[hit->boundary_edge].mode == BOUNDARY_FLOW_DISABLED) {
            cycle_boundary_emitter(state, hit->boundary_edge);
        }
        break;
    default:
        break;
    }
}

void editor_pointer_down(void *user, const InputPointerState *ptr) {
    SceneEditorState *state = (SceneEditorState *)user;
    if (!state || !ptr) return;
    if (ptr->button != SDL_BUTTON_LEFT) return;
    int x = ptr->x;
    int y = ptr->y;

    state->pointer_x = x;
    state->pointer_y = y;
    SDL_Rect canvas_rect = {state->canvas_x, state->canvas_y, state->canvas_width, state->canvas_height};
    bool in_canvas = point_in_rect(&canvas_rect, x, y);
    state->pointer_down_in_canvas = in_canvas;
    state->pointer_drag_started = false;
    state->hit_stack_count = 0;
    state->hit_stack_base = 0;
    if (in_canvas) {
        state->pointer_down_x = x;
        state->pointer_down_y = y;
    }

    if (state->name_edit_ptr) {
        SDL_Rect name_rect = editor_name_rect(state);
        if (point_in_rect(&name_rect, x, y)) {
            Uint32 now = SDL_GetTicks();
            bool double_click =
                (now - state->last_name_click) <= DOUBLE_CLICK_MS;
            state->last_name_click = now;
            if (double_click) {
                editor_begin_name_edit(state);
                return;
            }
        } else if (state->renaming_name) {
            editor_finish_name_edit(state, false);
        }
    }

    if (state->active_field &&
        !point_in_rect(&state->active_field->rect, x, y)) {
        commit_field_edit(state);
    }

    if (state->width_rect.w > 0 && state->width_rect.h > 0) {
        if (point_in_rect(&state->width_rect, x, y)) {
            Uint32 now = SDL_GetTicks();
            bool double_click = (now - state->last_width_click) <= DOUBLE_CLICK_MS;
            state->last_width_click = now;
            if (double_click) {
                editor_begin_dimension_edit(state, true);
                return;
            }
        } else if (state->editing_width) {
            editor_finish_dimension_edit(state, true, false);
        }
    }

    if (state->height_rect.w > 0 && state->height_rect.h > 0) {
        if (point_in_rect(&state->height_rect, x, y)) {
            Uint32 now = SDL_GetTicks();
            bool double_click = (now - state->last_height_click) <= DOUBLE_CLICK_MS;
            state->last_height_click = now;
            if (double_click) {
                editor_begin_dimension_edit(state, false);
                return;
            }
        } else if (state->editing_height) {
            editor_finish_dimension_edit(state, false, false);
        }
    }

    EditorButton *buttons[] = {
        &state->btn_save,
        &state->btn_cancel,
        &state->btn_add_source,
        &state->btn_add_jet,
        &state->btn_add_sink,
        &state->btn_add_import,
        &state->btn_import_back,
        &state->btn_import_delete,
        &state->btn_boundary
    };
    for (size_t i = 0; i < sizeof(buttons) / sizeof(buttons[0]); ++i) {
        EditorButton *btn = buttons[i];
        if (!btn->enabled) continue;
        if (point_in_rect(&btn->rect, x, y)) {
            if (btn == &state->btn_save) {
                finish_and_apply(state);
            } else if (btn == &state->btn_cancel) {
                cancel_and_close(state);
            } else if (btn == &state->btn_add_source ||
                       btn == &state->btn_add_jet ||
                       btn == &state->btn_add_sink) {
                int target_obj = -1;
                int target_imp = -1;
                pick_target_for_emitter(state, &target_obj, &target_imp);

                FluidEmitterType type = EMITTER_DENSITY_SOURCE;
                if (btn == &state->btn_add_jet) {
                    type = EMITTER_VELOCITY_JET;
                } else if (btn == &state->btn_add_sink) {
                    type = EMITTER_SINK;
                }

                if (target_obj >= 0 && target_obj < (int)state->working.object_count) {
                    int em = ensure_emitter_for_object(state,
                                                       target_obj,
                                                       type,
                                                       true);
                    state->selected_emitter = em;
                    state->selection_kind = (em >= 0) ? SELECTION_EMITTER : SELECTION_OBJECT;
                    // Keep object selection so the user still sees the association.
                    state->selected_object = target_obj;
                } else if (target_imp >= 0 && target_imp < (int)state->working.import_shape_count) {
                    int em = ensure_emitter_for_import(state,
                                                       target_imp,
                                                       type,
                                                       true);
                    state->selected_emitter = em;
                    state->selection_kind = (em >= 0) ? SELECTION_EMITTER : SELECTION_IMPORT;
                    state->selected_row = target_imp;
                } else {
                    add_emitter(state, type);
                }
            } else if (btn == &state->btn_add_import) {
                scene_editor_refresh_import_files(state);
                state->showing_import_picker = true;
                state->hover_import_row = -1;
                state->selected_import_row = -1;
            } else if (btn == &state->btn_import_back) {
                state->showing_import_picker = false;
            } else if (btn == &state->btn_import_delete) {
                if (state->selected_row >= 0 &&
                    state->selected_row < (int)state->working.import_shape_count) {
                    int idx = state->selected_row;
                    for (int j = idx; j + 1 < (int)state->working.import_shape_count; ++j) {
                        state->working.import_shapes[j] = state->working.import_shapes[j + 1];
                    }
                    state->working.import_shape_count--;
                    if (state->selected_row >= (int)state->working.import_shape_count) {
                        state->selected_row = (int)state->working.import_shape_count - 1;
                    }
                    set_dirty(state);
                }
            } else if (btn == &state->btn_boundary) {
                state->boundary_mode = !state->boundary_mode;
                state->boundary_selected_edge = -1;
                state->boundary_hover_edge = -1;
            }
            return;
        }
    }

    if (state->showing_import_picker &&
        point_in_rect(&state->import_rect, x, y)) {
        state->selection_kind = SELECTION_NONE;
        state->selected_object = -1;
        state->selected_emitter = -1;
        int row = editor_list_view_row_at(&state->import_view,
                                          x, y,
                                          state->import_rect.x, state->import_rect.y,
                                          state->import_rect.w, state->import_rect.h);
        if (row >= 0 && row < state->import_file_count) {
            Uint32 now = SDL_GetTicks();
            bool double_click = (now - state->last_import_click) <= DOUBLE_CLICK_MS;
            state->last_import_click = now;
            state->selected_import_row = row;
            if (double_click) {
                add_import_from_picker(state, row);
            } else {
                state->dragging_import_new = true;
                state->dragging_import_index = row;
            }
            return;
        }
    }

    if (!state->showing_import_picker &&
        point_in_rect(&state->list_rect, x, y)) {
        state->selection_kind = SELECTION_OBJECT;
        state->selected_emitter = -1;
        int row = editor_list_view_row_at(&state->list_view,
                                          x, y,
                                          state->list_rect.x, state->list_rect.y,
                                          state->list_rect.w, state->list_rect.h);
        if (row >= 0 && row < (int)state->working.object_count) {
            state->selected_object = row;
            state->hover_object = row;
        }
        return;
    }

    NumericField *fields[] = {&state->radius_field, &state->strength_field};
    for (size_t i = 0; i < sizeof(fields) / sizeof(fields[0]); ++i) {
        if (point_in_rect(&fields[i]->rect, x, y)) {
            begin_field_edit(state, fields[i]);
            return;
        }
    }

    int edge_hit = scene_editor_canvas_hit_edge(state->canvas_x,
                                                state->canvas_y,
                                                state->canvas_width,
                                                state->canvas_height,
                                                x,
                                                y);
    if (edge_hit >= 0) {
        state->boundary_selected_edge = edge_hit;
        if (state->boundary_mode) {
            if (state->working.boundary_flows[edge_hit].mode == BOUNDARY_FLOW_DISABLED) {
                cycle_boundary_emitter(state, edge_hit);
            }
        }
        return;
    }

    if (in_canvas) {
        Uint32 now = SDL_GetTicks();
        bool double_click = (now - state->last_canvas_click) <= DOUBLE_CLICK_MS;
        state->last_canvas_click = now;
        if (double_click) {
            bool precision_dirty = false;
            int selected_obj = state->selected_object;
            int selected_imp = (state->selection_kind == SELECTION_IMPORT) ? state->selected_row : -1;
#if defined(__APPLE__)
            if (state->renderer) {
                vk_renderer_wait_idle((VkRenderer *)state->renderer);
            }
            SDL_HideWindow(state->window);
            SDL_PumpEvents();
            SDL_Delay(80);
#endif
            if (scene_editor_run_precision(&state->cfg,
                                           &state->working,
                                           &selected_obj,
                                           &selected_imp,
                                           state->shape_library,
                                           state->font_small,
                                           state->font_main,
                                           &precision_dirty)) {
                state->selected_object = selected_obj;
                if (selected_imp >= 0) {
                    state->selection_kind = SELECTION_IMPORT;
                    state->selected_row = selected_imp;
                } else if (selected_obj >= 0) {
                    state->selection_kind = SELECTION_OBJECT;
                }
                if (precision_dirty) set_dirty(state);
                editor_update_canvas_layout(state);
            }
#if defined(__APPLE__)
            SDL_ShowWindow(state->window);
            SDL_RaiseWindow(state->window);
#endif
            return;
        }
    }

    if (!point_in_rect(&state->panel_rect, x, y)) {
        if (in_canvas) {
            int max_hits = (int)(sizeof(state->hit_stack) / sizeof(state->hit_stack[0]));
            state->hit_stack_count = scene_editor_canvas_collect_hits(&state->working,
                                                                      state->shape_library,
                                                                      state->canvas_x,
                                                                      state->canvas_y,
                                                                      state->canvas_width,
                                                                      state->canvas_height,
                                                                      x,
                                                                      y,
                                                                      state->emitter_object_map,
                                                                      state->emitter_import_map,
                                                                      state->hit_stack,
                                                                      max_hits);
            state->hit_stack_base = 0;
            // Prefer the top-most hit (emitter handle first) unless we have no hit or a same-priority match.
            for (int i = 0; i < state->hit_stack_count; ++i) {
                if (hit_matches_selection(state, &state->hit_stack[i])) {
                    // Only override the first entry if the first entry is not an emitter handle.
                    if (!(state->hit_stack[0].kind == HIT_EMITTER &&
                          state->hit_stack[0].drag_mode == DRAG_DIRECTION)) {
                        state->hit_stack_base = i;
                    }
                    break;
                }
            }
            if (state->hit_stack_count > 0) {
                SceneEditorHit anchor = state->hit_stack[state->hit_stack_base];
                select_from_hit(state, &anchor);
                prepare_drag_for_hit(state, &anchor, x, y);
            } else {
                clear_drag_flags(state);
            }
        }
    }
}

void editor_pointer_up(void *user, const InputPointerState *ptr) {
    SceneEditorState *state = (SceneEditorState *)user;
    if (!state || !ptr) return;
    if (ptr->button != SDL_BUTTON_LEFT) return;
    bool cycle_selection = false;
    SceneEditorHit next_hit = {0};
    if (state->pointer_down_in_canvas &&
        !state->pointer_drag_started &&
        state->hit_stack_count > 1) {
        int next = (state->hit_stack_base + 1) % state->hit_stack_count;
        if (next != state->hit_stack_base) {
            next_hit = state->hit_stack[next];
            cycle_selection = true;
        }
    }
    if (state->dragging_import_new) {
        state->dragging_import_new = false;
        if (state->dragging_import_index >= 0 &&
            state->dragging_import_index < state->import_file_count &&
            point_in_rect(&(SDL_Rect){state->canvas_x, state->canvas_y, state->canvas_width, state->canvas_height},
                          state->pointer_x, state->pointer_y)) {
            char full_path[256];
            snprintf(full_path, sizeof(full_path), "%s", state->import_files[state->dragging_import_index]);
            bool exists = false;
            for (size_t i = 0; i < state->working.import_shape_count; ++i) {
                if (strcmp(state->working.import_shapes[i].path, full_path) == 0) {
                    exists = true;
                    state->selected_row = (int)i;
                    break;
                }
            }
            if (!exists && state->working.import_shape_count < MAX_IMPORTED_SHAPES) {
                const char *selected_path = full_path;
                char asset_path[512] = {0};
                if (path_starts_with(selected_path, "import/")) {
                    if (convert_import_to_asset(selected_path, asset_path, sizeof(asset_path))) {
                        selected_path = asset_path;
                        scene_editor_refresh_import_files(state);
                    }
                }
                ImportedShape *imp = &state->working.import_shapes[state->working.import_shape_count++];
                memset(imp, 0, sizeof(*imp));
                snprintf(imp->path, sizeof(imp->path), "%s", selected_path);
                imp->shape_id = -1;
                imp->position_x = state->import_drag_pos_x;
                imp->position_y = state->import_drag_pos_y;
                imp->scale = 1.0f;
                imp->rotation_deg = 0.0f;
                imp->density = 1.0f;
                imp->friction = 0.2f;
                imp->is_static = true;
                imp->enabled = true;
                state->selected_row = (int)state->working.import_shape_count - 1;
                state->selection_kind = SELECTION_IMPORT;
                set_dirty(state);
            }
        }
        state->dragging_import_index = -1;
    }
    state->dragging_import_handle = false;
    state->dragging = false;
    state->drag_mode = DRAG_NONE;
    state->dragging_object = false;
    state->dragging_object_handle = false;
    state->handle_resize_started = false;
    state->dragging_import_body = false;
    if (cycle_selection) {
        select_from_hit(state, &next_hit);
    }
    state->pointer_down_in_canvas = false;
    state->pointer_drag_started = false;
    state->hit_stack_count = 0;
    state->hit_stack_base = 0;
}

void editor_pointer_move(void *user, const InputPointerState *ptr) {
    SceneEditorState *state = (SceneEditorState *)user;
    if (!state || !ptr) return;
    state->pointer_x = ptr->x;
    state->pointer_y = ptr->y;
    if (state->pointer_down_in_canvas && !state->pointer_drag_started) {
        int dx = ptr->x - state->pointer_down_x;
        int dy = ptr->y - state->pointer_down_y;
        if ((dx * dx + dy * dy) > (DRAG_THRESHOLD_PX * DRAG_THRESHOLD_PX)) {
            state->pointer_drag_started = true;
        }
    }
    bool allow_drag = (!state->pointer_down_in_canvas) || state->pointer_drag_started;
    if (state->dragging && state->selected_emitter >= 0 &&
        state->selected_emitter < (int)state->working.emitter_count) {
        allow_drag = true; // emitters should respond immediately once drag flag is set
    }
    if (!allow_drag && state->pointer_down_in_canvas &&
        (state->dragging_object || state->dragging_object_handle ||
         state->dragging_import_handle || state->dragging_import_body ||
         state->dragging_import_new || state->dragging)) {
        int dx = ptr->x - state->pointer_down_x;
        int dy = ptr->y - state->pointer_down_y;
        if ((dx * dx + dy * dy) > (DRAG_THRESHOLD_PX * DRAG_THRESHOLD_PX)) {
            state->pointer_drag_started = true;
            allow_drag = true;
        }
    }
    if (state->showing_import_picker) {
        state->hover_import_row = editor_list_view_row_at(&state->import_view,
                                                          ptr->x, ptr->y,
                                                          state->import_rect.x, state->import_rect.y,
                                                          state->import_rect.w, state->import_rect.h);
    } else {
        state->hover_row = editor_list_view_row_at(&state->list_view,
                                                   ptr->x, ptr->y,
                                                   state->list_rect.x, state->list_rect.y,
                                                   state->list_rect.w, state->list_rect.h);
        state->hover_object = state->hover_row;
    }
    if (state->dragging_object_handle &&
        allow_drag &&
        state->selected_object >= 0 &&
        state->selected_object < (int)state->working.object_count) {
        PresetObject *obj = &state->working.objects[state->selected_object];
        int cx = 0, cy = 0;
        scene_editor_canvas_project(state->canvas_x,
                                    state->canvas_y,
                                    state->canvas_width,
                                    state->canvas_height,
                                    obj->position_x,
                                    obj->position_y,
                                    &cx,
                                    &cy);
        float dx_px = (float)(ptr->x - cx);
        float dy_px = (float)(ptr->y - cy);
        float len_px = sqrtf(dx_px * dx_px + dy_px * dy_px);
        float min_len_px = (obj->type == PRESET_OBJECT_BOX)
                               ? (float)SCENE_EDITOR_OBJECT_MIN_HALF_PX
                               : (float)SCENE_EDITOR_OBJECT_MIN_RADIUS_PX;
        float adjusted_px = len_px - (float)SCENE_EDITOR_OBJECT_HANDLE_MARGIN_PX;
        if (adjusted_px < min_len_px) {
            adjusted_px = min_len_px;
        }
        if (!state->handle_resize_started) {
            if (fabsf(adjusted_px - state->handle_initial_length) <= 1.0f) {
                return;
            }
            state->handle_resize_started = true;
        }
        obj->angle = atan2f(dy_px, dx_px);
        if (obj->type == PRESET_OBJECT_BOX) {
            float ratio_px = state->object_handle_ratio;
            if (ratio_px <= 0.01f) ratio_px = 1.0f;
            float half_w_px = adjusted_px;
            float half_h_px = adjusted_px * ratio_px;
            if (half_w_px < (float)SCENE_EDITOR_OBJECT_MIN_HALF_PX) {
                half_w_px = (float)SCENE_EDITOR_OBJECT_MIN_HALF_PX;
            }
            if (half_h_px < (float)SCENE_EDITOR_OBJECT_MIN_HALF_PX) {
                half_h_px = (float)SCENE_EDITOR_OBJECT_MIN_HALF_PX;
            }
            obj->size_x = half_w_px / (float)state->canvas_width;
            obj->size_y = half_h_px / (float)state->canvas_height;
        } else {
            obj->size_x = adjusted_px / (float)state->canvas_width;
            obj->size_y = adjusted_px / (float)state->canvas_width;
        }
        clamp_object(obj);
        sync_emitter_to_object(state, state->selected_object);
        set_dirty(state);
        return;
    }
    if (state->dragging_object && allow_drag && state->selected_object >= 0 &&
        state->selected_object < (int)state->working.object_count) {
        PresetObject *obj = &state->working.objects[state->selected_object];
        float nx = 0.0f, ny = 0.0f;
        canvas_to_normalized_unclamped(state, ptr->x, ptr->y, &nx, &ny);
        nx -= state->object_drag_offset_x;
        ny -= state->object_drag_offset_y;
        if (object_is_outside(nx, ny)) {
            remove_selected_object(state);
            return;
        }
        obj->position_x = clamp01(nx);
        obj->position_y = clamp01(ny);
        clamp_object(obj);
        sync_emitter_to_object(state, state->selected_object);
        set_dirty(state);
        return;
    }
    if (state->dragging_import_handle &&
        allow_drag &&
        state->selected_row >= 0 &&
        state->selected_row < (int)state->working.import_shape_count &&
        state->selection_kind == SELECTION_IMPORT) {
        ImportedShape *imp = &state->working.import_shapes[state->selected_row];
        float nx = 0.0f, ny = 0.0f;
        scene_editor_canvas_to_import_normalized(state->canvas_x,
                                                 state->canvas_y,
                                                 state->canvas_width,
                                                 state->canvas_height,
                                                 ptr->x,
                                                 ptr->y,
                                                 &nx,
                                                 &ny);
        float dx = nx - imp->position_x;
        float dy = ny - imp->position_y;
        float dist = sqrtf(dx * dx + dy * dy);
        if (dist < 0.0001f) dist = 0.0001f;
        float ratio = dist / state->import_handle_start_dist;
        imp->scale = state->import_handle_start_scale * ratio;
        if (imp->scale < 0.01f) imp->scale = 0.01f;
        imp->rotation_deg = atan2f(dy, dx) * 180.0f / (float)M_PI;
        sync_emitter_to_import(state, state->selected_row);
        set_dirty(state);
        return;
    }
    if (state->dragging_import_body &&
        allow_drag &&
        state->selected_row >= 0 &&
        state->selected_row < (int)state->working.import_shape_count &&
        state->selection_kind == SELECTION_IMPORT) {
        ImportedShape *imp = &state->working.import_shapes[state->selected_row];
        float nx = 0.0f, ny = 0.0f;
        scene_editor_canvas_to_import_normalized(state->canvas_x,
                                                 state->canvas_y,
                                                 state->canvas_width,
                                                 state->canvas_height,
                                                 ptr->x,
                                                 ptr->y,
                                                 &nx,
                                                 &ny);
        imp->position_x = nx - state->import_body_drag_off_x;
        imp->position_y = ny - state->import_body_drag_off_y;
        float min_dim = (float)((state->canvas_width < state->canvas_height) ? state->canvas_width : state->canvas_height);
        float span_x = 0.5f * ((float)state->canvas_width / min_dim);
        float span_y = 0.5f * ((float)state->canvas_height / min_dim);
        float min_x = 0.5f - span_x;
        float max_x = 0.5f + span_x;
        float min_y = 0.5f - span_y;
        float max_y = 0.5f + span_y;
        if (imp->position_x < min_x) imp->position_x = min_x;
        if (imp->position_x > max_x) imp->position_x = max_x;
        if (imp->position_y < min_y) imp->position_y = min_y;
        if (imp->position_y > max_y) imp->position_y = max_y;
        // Keep attached emitter in sync.
        sync_emitter_to_import(state, state->selected_row);
        set_dirty(state);
        return;
    }
    if (state->dragging_import_new) {
        if (point_in_rect(&(SDL_Rect){state->canvas_x, state->canvas_y, state->canvas_width, state->canvas_height},
                          ptr->x, ptr->y)) {
            float nx, ny;
            canvas_to_normalized_unclamped(state, ptr->x, ptr->y, &nx, &ny);
            state->import_drag_pos_x = clamp01(nx);
            state->import_drag_pos_y = clamp01(ny);
        }
    }
    if (state->dragging && allow_drag && state->selected_emitter >= 0 &&
        state->selected_emitter < (int)state->working.emitter_count) {
        FluidEmitter *em = &state->working.emitters[state->selected_emitter];
        if (state->drag_mode == DRAG_POSITION) {
            float nx, ny;
            scene_editor_canvas_to_normalized(state->canvas_x,
                                              state->canvas_y,
                                              state->canvas_width,
                                              state->canvas_height,
                                              ptr->x,
                                              ptr->y,
                                              &nx,
                                              &ny);
            nx -= state->drag_offset_x;
            ny -= state->drag_offset_y;
            em->position_x = clamp01(nx);
            em->position_y = clamp01(ny);
            set_dirty(state);
        } else if (state->drag_mode == DRAG_DIRECTION) {
            int cx, cy;
            scene_editor_canvas_project(state->canvas_x,
                                        state->canvas_y,
                                        state->canvas_width,
                                        state->canvas_height,
                                        em->position_x,
                                        em->position_y,
                                        &cx,
                                        &cy);
            float dx = (float)(ptr->x - cx);
            float dy = (float)(ptr->y - cy);
            float len = sqrtf(dx * dx + dy * dy);
            if (len > 0.0001f) {
                em->dir_x = dx / len;
                em->dir_y = dy / len;
                float min_dim = (float)((state->canvas_width < state->canvas_height) ? state->canvas_width : state->canvas_height);
                if (min_dim > 0.0f) {
                    float adj_len = len - state->emitter_handle_offset_px;
                    if (adj_len < 0.0f) adj_len = 0.0f;
                    float new_radius = adj_len / min_dim;
                    if (new_radius < 0.02f) new_radius = 0.02f;
                    if (new_radius > 0.6f) new_radius = 0.6f;
                    float ratio = (em->radius > 0.0001f) ? (new_radius / em->radius) : 1.0f;
                    em->radius = new_radius;
                    em->strength *= ratio;
                }
                set_dirty(state);
            }
        }
    } else {
        EditorDragMode mode = DRAG_NONE;
        state->hover_emitter = scene_editor_canvas_hit_test(&state->working,
                                                            state->canvas_x,
                                                            state->canvas_y,
                                                            state->canvas_width,
                                                            state->canvas_height,
                                                            ptr->x,
                                                            ptr->y,
                                                            &mode,
                                                            state->emitter_object_map,
                                                            state->emitter_import_map);
    }
    state->hover_object = scene_editor_canvas_hit_object(&state->working,
                                                         state->canvas_x,
                                                         state->canvas_y,
                                                         state->canvas_width,
                                                         state->canvas_height,
                                                         ptr->x,
                                                         ptr->y);
    if (state->hover_object < 0) {
        int handle_hover = scene_editor_canvas_hit_object_handle(&state->working,
                                                                 state->canvas_x,
                                                                 state->canvas_y,
                                                                 state->canvas_width,
                                                                 state->canvas_height,
                                                                 ptr->x,
                                                                 ptr->y);
        if (handle_hover >= 0) {
            state->hover_object = handle_hover;
        }
    }

    state->boundary_hover_edge = scene_editor_canvas_hit_edge(state->canvas_x,
                                                              state->canvas_y,
                                                              state->canvas_width,
                                                              state->canvas_height,
                                                              ptr->x,
                                                              ptr->y);
}

void editor_on_wheel(void *user, const InputWheelState *wheel) {
    SceneEditorState *state = (SceneEditorState *)user;
    if (!state || !wheel) return;
    if (state->showing_import_picker) {
        editor_list_view_handle_wheel(&state->import_view,
                                      state->pointer_x, state->pointer_y,
                                      (float)wheel->y);
    } else {
        editor_list_view_handle_wheel(&state->list_view,
                                      state->pointer_x, state->pointer_y,
                                      (float)wheel->y);
    }
}

void editor_text_input(void *user, const char *text) {
    SceneEditorState *state = (SceneEditorState *)user;
    if (!state || !text) return;
    if (state->renaming_name) {
        text_input_handle_text(&state->name_input, text);
        return;
    }
    if (state->editing_width) {
        text_input_handle_text(&state->width_input, text);
        return;
    }
    if (state->editing_height) {
        text_input_handle_text(&state->height_input, text);
        return;
    }
}

void editor_key_down(void *user, SDL_Keycode key, SDL_Keymod mod) {
    SceneEditorState *state = (SceneEditorState *)user;
    if (!state) return;
    if (state->renaming_name) {
        if (key == SDLK_RETURN || key == SDLK_KP_ENTER) {
            editor_finish_name_edit(state, true);
        } else if (key == SDLK_ESCAPE) {
            editor_finish_name_edit(state, false);
        } else {
            text_input_handle_key(&state->name_input, key);
        }
        return;
    }
    if (state->editing_width) {
        if (key == SDLK_RETURN || key == SDLK_KP_ENTER) {
            editor_finish_dimension_edit(state, true, true);
        } else if (key == SDLK_ESCAPE) {
            editor_finish_dimension_edit(state, true, false);
        } else {
            text_input_handle_key(&state->width_input, key);
        }
        return;
    }
    if (state->editing_height) {
        if (key == SDLK_RETURN || key == SDLK_KP_ENTER) {
            editor_finish_dimension_edit(state, false, true);
        } else if (key == SDLK_ESCAPE) {
            editor_finish_dimension_edit(state, false, false);
        } else {
            text_input_handle_key(&state->height_input, key);
        }
        return;
    }
    if (key == SDLK_ESCAPE) {
        state->selected_object = -1;
        state->selected_emitter = -1;
        state->hover_object = -1;
        state->hover_emitter = -1;
        state->selected_row = -1;
        state->selected_import_row = -1;
        state->dragging = false;
        state->dragging_object = false;
        state->dragging_object_handle = false;
        state->dragging_import_new = false;
        state->showing_import_picker = false;
        return;
    }

    if (key == SDLK_e && state->selected_object >= 0) {
        int em_idx = emitter_index_for_object(state, state->selected_object);
        if (em_idx >= 0) {
            size_t count = state->working.emitter_count;
            for (size_t i = (size_t)em_idx; i + 1 < count; ++i) {
                state->working.emitters[i] = state->working.emitters[i + 1];
            }
            state->working.emitter_count--;
            state->selected_emitter = -1;
            set_dirty(state);
        }
        return;
    }
    if (field_handle_key(state, key)) {
        return;
    }

    if (state->boundary_selected_edge >= 0) {
        switch (key) {
        case SDLK_e:
            cycle_boundary_emitter(state, state->boundary_selected_edge);
            return;
        case SDLK_r:
            set_boundary_receiver(state, state->boundary_selected_edge);
            return;
        case SDLK_x:
            clear_boundary(state, state->boundary_selected_edge);
            return;
        default:
            break;
        }
    }
    switch (key) {
    case SDLK_RETURN:
    case SDLK_KP_ENTER:
        if (state->showing_import_picker && state->selected_import_row >= 0) {
            add_import_from_picker(state, state->selected_import_row);
        } else {
            finish_and_apply(state);
        }
        break;
    case SDLK_ESCAPE:
        cancel_and_close(state);
        break;
    case SDLK_TAB: {
        if (state->working.emitter_count == 0) break;
        if (state->selected_emitter < 0) state->selected_emitter = 0;
        int dir = (mod & KMOD_SHIFT) ? -1 : 1;
        int next = state->selected_emitter + dir;
        if (next < 0) next = (int)state->working.emitter_count - 1;
        if (next >= (int)state->working.emitter_count) next = 0;
        state->selected_emitter = next;
        state->selection_kind = SELECTION_EMITTER;
        state->selected_object = -1;
        break;
    }
    case SDLK_DELETE:
    case SDLK_BACKSPACE:
        if (state->showing_import_picker) {
            if (state->selected_import_row >= 0 &&
                state->selected_import_row < state->import_file_count) {
                // no-op; picker list is read-only
            }
        } else if (state->selected_row >= 0 &&
                   state->selected_row < (int)state->working.import_shape_count) {
            remove_import_at(state, state->selected_row);
        } else if (state->selection_kind == SELECTION_OBJECT) {
            remove_selected_object(state);
        } else {
            remove_selected(state);
        }
        break;
    case SDLK_PLUS:
    case SDLK_EQUALS:
    case SDLK_KP_PLUS:
        if (state->selection_kind == SELECTION_OBJECT &&
            state->selected_object >= 0 &&
            state->selected_object < (int)state->working.object_count) {
            PresetObject *obj = &state->working.objects[state->selected_object];
            obj->size_x *= 1.1f;
            obj->size_y *= 1.1f;
            clamp_object(obj);
            set_dirty(state);
        } else if (state->selected_emitter >= 0 &&
                   state->selected_emitter < (int)state->working.emitter_count) {
            adjust_emitter_radius(&state->working.emitters[state->selected_emitter], 1.1f);
            set_dirty(state);
        }
        break;
    case SDLK_MINUS:
    case SDLK_UNDERSCORE:
    case SDLK_KP_MINUS:
        if (state->selection_kind == SELECTION_OBJECT &&
            state->selected_object >= 0 &&
            state->selected_object < (int)state->working.object_count) {
            PresetObject *obj = &state->working.objects[state->selected_object];
            obj->size_x *= 0.9f;
            obj->size_y *= 0.9f;
            clamp_object(obj);
            set_dirty(state);
        } else if (state->selected_emitter >= 0 &&
                   state->selected_emitter < (int)state->working.emitter_count) {
            adjust_emitter_radius(&state->working.emitters[state->selected_emitter], 0.9f);
            set_dirty(state);
        }
        break;
    case SDLK_UP:
        nudge_selected(state, 0.0f, -0.01f);
        break;
    case SDLK_DOWN:
        nudge_selected(state, 0.0f, 0.01f);
        break;
    case SDLK_LEFT:
        nudge_selected(state, -0.01f, 0.0f);
        break;
    case SDLK_RIGHT:
        nudge_selected(state, 0.01f, 0.0f);
        break;
    case SDLK_g:
        if (state->selection_kind == SELECTION_OBJECT &&
            state->selected_object >= 0 &&
            state->selected_object < (int)state->working.object_count) {
            int em_idx = emitter_index_for_object(state, state->selected_object);
            if (em_idx < 0) { // do not toggle gravity on emitter-bound objects
                PresetObject *obj = &state->working.objects[state->selected_object];
                obj->gravity_enabled = !obj->gravity_enabled;
                fprintf(stderr, "[editor] G pressed: toggled gravity on obj %d -> %d\n",
                        state->selected_object, obj->gravity_enabled ? 1 : 0);
                set_dirty(state);
            } else {
                fprintf(stderr, "[editor] G pressed on emitter-bound obj %d (ignored)\n",
                        state->selected_object);
            }
        } else if (state->selection_kind == SELECTION_IMPORT &&
                   state->selected_row >= 0 &&
                   state->selected_row < (int)state->working.import_shape_count) {
            int imp_idx = state->selected_row;
            int em_idx = emitter_index_for_import(state, imp_idx);
            if (em_idx < 0) {
                ImportedShape *imp = &state->working.import_shapes[imp_idx];
                imp->gravity_enabled = !imp->gravity_enabled;
                fprintf(stderr, "[editor] G pressed: toggled gravity on import %d -> %d\n",
                        imp_idx, imp->gravity_enabled ? 1 : 0);
                set_dirty(state);
            } else {
                fprintf(stderr, "[editor] G on emitter-bound import %d (ignored)\n", imp_idx);
            }
        } else {
            fprintf(stderr, "[editor] G pressed with no object selection (kind=%d, obj=%d)\n",
                    state->selection_kind, state->selected_object);
        }
        break;
    default:
        break;
    }
}

void editor_key_up(void *user, SDL_Keycode key, SDL_Keymod mod) {
    (void)user;
    (void)key;
    (void)mod;
}
