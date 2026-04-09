#include "app/editor/scene_editor_input_hit_helpers.h"

#include "app/editor/scene_editor_canvas.h"
#include "app/editor/scene_editor_internal.h"
#include "app/editor/scene_editor_model.h"

#include <math.h>

bool scene_editor_input_hit_matches_selection(const SceneEditorState *state, const SceneEditorHit *hit) {
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

void scene_editor_input_clear_drag_flags(SceneEditorState *state) {
    if (!state) return;
    state->dragging_import_handle = false;
    state->dragging_import_body = false;
    state->dragging = false;
    state->drag_mode = DRAG_NONE;
    state->dragging_object = false;
    state->dragging_object_handle = false;
    state->handle_resize_started = false;
}

void scene_editor_input_select_from_hit(SceneEditorState *state, const SceneEditorHit *hit) {
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

void scene_editor_input_prepare_drag_for_hit(SceneEditorState *state,
                                             const SceneEditorHit *hit,
                                             int x,
                                             int y) {
    if (!state || !hit) return;
    scene_editor_input_clear_drag_flags(state);
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
