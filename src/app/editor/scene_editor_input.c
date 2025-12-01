#include "app/editor/scene_editor_input.h"
#include "app/editor/scene_editor_internal.h"
#include "app/editor/scene_editor_canvas.h"
#include "app/editor/scene_editor_import.h"
#include "app/editor/scene_editor_model.h"
#include "app/editor/scene_editor_precision.h"
#include "app/shape_lookup.h"
#include "import/shape_import.h"
#include "geo/shape_asset.h"

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static bool point_in_rect(const SDL_Rect *rect, int x, int y) {
    if (!rect) return false;
    return x >= rect->x && x < rect->x + rect->w &&
           y >= rect->y && y < rect->y + rect->h;
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
    state->applied = false;
    state->running = false;
}

static void remove_import_at(SceneEditorState *state, int index) {
    if (!state || index < 0 || index >= (int)state->working.import_shape_count) return;
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

void editor_pointer_down(void *user, const InputPointerState *ptr) {
    SceneEditorState *state = (SceneEditorState *)user;
    if (!state || !ptr) return;
    int x = ptr->x;
    int y = ptr->y;

    state->pointer_x = x;
    state->pointer_y = y;
    SDL_Rect canvas_rect = {state->canvas_x, state->canvas_y, state->canvas_width, state->canvas_height};
    bool in_canvas = point_in_rect(&canvas_rect, x, y);

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
            } else if (btn == &state->btn_add_source) {
                if (state->selected_object >= 0) {
                    state->selected_emitter = ensure_emitter_for_object(state, state->selected_object, EMITTER_DENSITY_SOURCE, true);
                } else {
                    add_emitter(state, EMITTER_DENSITY_SOURCE);
                }
            } else if (btn == &state->btn_add_jet) {
                if (state->selected_object >= 0) {
                    state->selected_emitter = ensure_emitter_for_object(state, state->selected_object, EMITTER_VELOCITY_JET, true);
                } else {
                    add_emitter(state, EMITTER_VELOCITY_JET);
                }
            } else if (btn == &state->btn_add_sink) {
                if (state->selected_object >= 0) {
                    state->selected_emitter = ensure_emitter_for_object(state, state->selected_object, EMITTER_SINK, true);
                } else {
                    add_emitter(state, EMITTER_SINK);
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
                const char *selected_path = state->import_files[row];
                char asset_path[512] = {0};
                const char *store_path = selected_path;
                if (path_starts_with(selected_path, "import/")) {
                    if (convert_import_to_asset(selected_path, asset_path, sizeof(asset_path))) {
                        store_path = asset_path;
                        scene_editor_refresh_import_files(state);
                        fprintf(stderr, "[editor] Converted legacy import %s -> %s\n",
                                selected_path, store_path);
                    } else {
                        store_path = selected_path; // fallback to raw
                        fprintf(stderr, "[editor] Using raw import path (conversion failed): %s\n",
                                selected_path);
                    }
                }
                // If it already exists in the preset, just select it.
                bool exists = false;
                for (size_t i = 0; i < state->working.import_shape_count; ++i) {
                    if (strcmp(state->working.import_shapes[i].path, store_path) == 0) {
                        exists = true;
                        state->selected_row = (int)i;
                        state->selection_kind = SELECTION_IMPORT;
                        fprintf(stderr, "[editor] Import already present, selecting row %zu: %s\n",
                                i, store_path);
                        break;
                    }
                }
                if (!exists && state->working.import_shape_count < MAX_IMPORTED_SHAPES) {
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
            } else {
                state->dragging_import_new = true;
                state->dragging_import_index = row;
            }
            return;
        }
    }

    if (!state->showing_import_picker &&
        point_in_rect(&state->list_rect, x, y)) {
        state->selection_kind = SELECTION_NONE;
        state->selected_object = -1;
        state->selected_emitter = -1;
        int row = editor_list_view_row_at(&state->list_view,
                                          x, y,
                                          state->list_rect.x, state->list_rect.y,
                                          state->list_rect.w, state->list_rect.h);
        if (row >= 0 && row < (int)state->working.import_shape_count) {
            state->selected_row = row;
            state->selection_kind = SELECTION_IMPORT;
            Uint32 now = SDL_GetTicks();
            bool double_click = (now - state->last_import_click) <= DOUBLE_CLICK_MS;
            state->last_import_click = now;
            if (double_click) {
                ImportedShape *imp = &state->working.import_shapes[row];
                imp->enabled = true;
                resolve_import_shape_id(state, imp);
                // Snap to center if still at default to ensure visible.
                if (imp->position_x == 0.0f && imp->position_y == 0.0f) {
                    imp->position_x = 0.5f;
                    imp->position_y = 0.5f;
                }
                fprintf(stderr, "[editor] Activated import row %d: %s (shape_id=%d)\n",
                        row, imp->path, imp->shape_id);
                set_dirty(state);
            }
            return;
        }
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
            return;
        }
    }

    if (!point_in_rect(&state->panel_rect, x, y)) {
        // Hit test import handles first for rotate/scale.
        if (state->shape_library) {
            int handle_hit = -1;
            int hx = 0, hy = 0;
            float hit_radius_px = scene_editor_canvas_handle_size_px(state->canvas_width,
                                                                     state->canvas_height) * 0.6f;
            for (int i = (int)state->working.import_shape_count - 1; i >= 0; --i) {
                const ImportedShape *imp = &state->working.import_shapes[i];
                if (!imp->enabled) continue;
                if (!scene_editor_canvas_import_handle_point(state->canvas_x,
                                                             state->canvas_y,
                                                             state->canvas_width,
                                                             state->canvas_height,
                                                             imp,
                                                             &hx,
                                                             &hy)) {
                    continue;
                }
                float dx = (float)x - (float)hx;
                float dy = (float)y - (float)hy;
                if ((dx * dx + dy * dy) <= hit_radius_px * hit_radius_px) {
                    handle_hit = i;
                    break;
                }
            }
            if (handle_hit >= 0) {
                state->selection_kind = SELECTION_IMPORT;
                state->selected_row = handle_hit;
                state->selected_object = -1;
                state->selected_emitter = -1;
                ImportedShape *imp = &state->working.import_shapes[handle_hit];
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
                return;
            }
        }

        // Hit test imports next so clicking them in-canvas selects them.
        if (state->shape_library) {
            int import_hit = scene_editor_canvas_hit_import(&state->working,
                                                            state->shape_library,
                                                            state->canvas_x,
                                                            state->canvas_y,
                                                            state->canvas_width,
                                                            state->canvas_height,
                                                            x,
                                                            y);
            if (import_hit >= 0) {
                state->selection_kind = SELECTION_IMPORT;
                state->selected_row = import_hit;
                state->selected_object = -1;
                state->selected_emitter = -1;
                return;
            }
        }

        int handle_hit = scene_editor_canvas_hit_object_handle(&state->working,
                                                               state->canvas_x,
                                                               state->canvas_y,
                                                               state->canvas_width,
                                                               state->canvas_height,
                                                               x,
                                                               y);
        if (handle_hit >= 0) {
            state->selected_object = handle_hit;
            state->selected_emitter = -1;
            state->selection_kind = SELECTION_OBJECT;
            state->dragging_object_handle = true;
            PresetObject *obj = &state->working.objects[handle_hit];
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
            return;
        }

        EditorDragMode mode = DRAG_NONE;
        int obj_hit = scene_editor_canvas_hit_object(&state->working,
                                                     state->canvas_x,
                                                     state->canvas_y,
                                                     state->canvas_width,
                                                     state->canvas_height,
                                                     x,
                                                     y);
        if (obj_hit >= 0) {
            state->selected_object = obj_hit;
            state->selected_emitter = -1;
            state->selection_kind = SELECTION_OBJECT;
            state->dragging_object = true;
            state->dragging_object_handle = false;
            state->handle_resize_started = false;
            PresetObject *obj = &state->working.objects[obj_hit];
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
            return;
        }

        int hit = scene_editor_canvas_hit_test(&state->working,
                                               state->canvas_x,
                                               state->canvas_y,
                                               state->canvas_width,
                                               state->canvas_height,
                                               x,
                                               y,
                                               &mode,
                                               state->emitter_object_map);
        state->hover_emitter = hit;
        if (hit < 0) {
            state->dragging = false;
            state->drag_mode = DRAG_NONE;
            state->selection_kind = SELECTION_IMPORT;
            return;
        }
        state->selection_kind = SELECTION_EMITTER;
        state->selected_emitter = hit;
        state->selected_object = -1;
        state->drag_mode = mode;
        state->dragging = true;
        state->dragging_object_handle = false;
        state->handle_resize_started = false;
        state->drag_offset_x = 0.0f;
        state->drag_offset_y = 0.0f;
        if (mode == DRAG_POSITION) {
            FluidEmitter *em = &state->working.emitters[hit];
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
        }
    }
}

void editor_pointer_up(void *user, const InputPointerState *ptr) {
    (void)ptr;
    SceneEditorState *state = (SceneEditorState *)user;
    if (!state) return;
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

}

void editor_pointer_move(void *user, const InputPointerState *ptr) {
    SceneEditorState *state = (SceneEditorState *)user;
    if (!state || !ptr) return;
    state->pointer_x = ptr->x;
    state->pointer_y = ptr->y;
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
    }
    if (state->dragging_object_handle &&
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
    if (state->dragging_object && state->selected_object >= 0 &&
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
    if (state->dragging && state->selected_emitter >= 0 &&
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
                                                            state->emitter_object_map);
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
        finish_and_apply(state);
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
    default:
        break;
    }
}

void editor_key_up(void *user, SDL_Keycode key, SDL_Keymod mod) {
    (void)user;
    (void)key;
    (void)mod;
}
