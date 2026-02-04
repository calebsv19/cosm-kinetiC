#include "app/structural/structural_editor.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static float snap_value(float v, float grid) {
    if (grid <= 0.0f) return v;
    float snapped = roundf(v / grid) * grid;
    return snapped;
}

static void editor_set_status(StructuralEditor *editor, const char *msg) {
    if (!editor) return;
    if (!msg) {
        editor->status_message[0] = '\0';
        return;
    }
    snprintf(editor->status_message, sizeof(editor->status_message), "%s", msg);
}

void structural_editor_set_status(StructuralEditor *editor, const char *msg) {
    editor_set_status(editor, msg);
}

static void editor_mark_dirty(StructuralEditor *editor) {
    if (!editor || !editor->scene) return;
    structural_scene_clear_solution(editor->scene);
}

static void editor_apply_snap(StructuralEditor *editor, float *x, float *y) {
    if (!editor || !x || !y) return;
    if (!editor->snap_to_grid) return;
    *x = snap_value(*x, editor->grid_size);
    *y = snap_value(*y, editor->grid_size);
}

static void editor_clear_selection(StructuralScene *scene) {
    if (!scene) return;
    for (size_t i = 0; i < scene->node_count; ++i) {
        scene->nodes[i].selected = false;
    }
    for (size_t i = 0; i < scene->edge_count; ++i) {
        scene->edges[i].selected = false;
    }
}

static void editor_set_selected_support(StructuralScene *scene,
                                        bool fix_x,
                                        bool fix_y,
                                        bool fix_theta,
                                        const char *status,
                                        StructuralEditor *editor) {
    if (!scene) return;
    bool changed = false;
    for (size_t i = 0; i < scene->node_count; ++i) {
        StructNode *node = &scene->nodes[i];
        if (!node->selected) continue;
        node->fixed_x = fix_x;
        node->fixed_y = fix_y;
        node->fixed_theta = fix_theta;
        changed = true;
    }
    if (changed) {
        structural_scene_clear_solution(scene);
        if (editor && status) {
            editor_set_status(editor, status);
        }
    }
}

static void editor_select_nodes_in_box(StructuralScene *scene,
                                       int min_x,
                                       int min_y,
                                       int max_x,
                                       int max_y,
                                       bool additive) {
    if (!scene) return;
    if (!additive) {
        editor_clear_selection(scene);
    }
    for (size_t i = 0; i < scene->node_count; ++i) {
        StructNode *node = &scene->nodes[i];
        if ((int)node->x >= min_x && (int)node->x <= max_x &&
            (int)node->y >= min_y && (int)node->y <= max_y) {
            node->selected = true;
        }
    }
}

static int editor_first_selected_node(const StructuralScene *scene) {
    if (!scene) return -1;
    for (size_t i = 0; i < scene->node_count; ++i) {
        if (scene->nodes[i].selected) return scene->nodes[i].id;
    }
    return -1;
}

static int editor_first_selected_edge(const StructuralScene *scene) {
    if (!scene) return -1;
    for (size_t i = 0; i < scene->edge_count; ++i) {
        if (scene->edges[i].selected) return scene->edges[i].id;
    }
    return -1;
}

static void editor_weld_selected_nodes(StructuralEditor *editor) {
    if (!editor || !editor->scene) return;
    StructuralScene *scene = editor->scene;
    int target_id = editor_first_selected_node(scene);
    if (target_id < 0) return;
    StructNode *target = structural_scene_get_node(scene, target_id);
    if (!target) return;

    float sum_x = 0.0f;
    float sum_y = 0.0f;
    int count = 0;
    for (size_t i = 0; i < scene->node_count; ++i) {
        StructNode *node = &scene->nodes[i];
        if (!node->selected) continue;
        sum_x += node->x;
        sum_y += node->y;
        count++;
    }
    if (count < 2) return;

    target->x = sum_x / (float)count;
    target->y = sum_y / (float)count;

    for (size_t i = 0; i < scene->edge_count; ++i) {
        StructEdge *edge = &scene->edges[i];
        bool changed = false;
        if (structural_scene_get_node(scene, edge->a_id) &&
            structural_scene_get_node(scene, edge->b_id)) {
            for (size_t n = 0; n < scene->node_count; ++n) {
                StructNode *node = &scene->nodes[n];
                if (!node->selected) continue;
                if (node->id == target_id) continue;
                if (edge->a_id == node->id) {
                    edge->a_id = target_id;
                    changed = true;
                }
                if (edge->b_id == node->id) {
                    edge->b_id = target_id;
                    changed = true;
                }
            }
        }
        if (changed && edge->a_id == edge->b_id) {
            structural_scene_remove_edge(scene, edge->id);
            --i;
        }
    }

    for (size_t i = 0; i < scene->node_count;) {
        StructNode *node = &scene->nodes[i];
        if (node->selected && node->id != target_id) {
            structural_scene_remove_node(scene, node->id);
            continue;
        }
        ++i;
    }

    editor_clear_selection(scene);
    target->selected = true;
    editor_set_status(editor, "Welded selected nodes.");
}

static void editor_split_selected_edge(StructuralEditor *editor) {
    if (!editor || !editor->scene) return;
    StructuralScene *scene = editor->scene;
    int edge_id = editor_first_selected_edge(scene);
    if (edge_id < 0) return;
    StructEdge *edge = structural_scene_get_edge(scene, edge_id);
    if (!edge) return;
    StructNode *a = structural_scene_get_node(scene, edge->a_id);
    StructNode *b = structural_scene_get_node(scene, edge->b_id);
    if (!a || !b) return;

    float mid_x = 0.5f * (a->x + b->x);
    float mid_y = 0.5f * (a->y + b->y);
    int new_node_id = structural_scene_add_node(scene, mid_x, mid_y);
    if (new_node_id < 0) return;

    int old_material = edge->material_index;
    structural_scene_remove_edge(scene, edge_id);
    int e1 = structural_scene_add_edge(scene, a->id, new_node_id);
    int e2 = structural_scene_add_edge(scene, new_node_id, b->id);
    StructEdge *edge1 = structural_scene_get_edge(scene, e1);
    StructEdge *edge2 = structural_scene_get_edge(scene, e2);
    if (edge1) edge1->material_index = old_material;
    if (edge2) edge2->material_index = old_material;

    editor_clear_selection(scene);
    StructNode *new_node = structural_scene_get_node(scene, new_node_id);
    if (new_node) new_node->selected = true;
    editor_set_status(editor, "Split edge at midpoint.");
}

static void editor_duplicate_selection(StructuralEditor *editor) {
    if (!editor || !editor->scene) return;
    StructuralScene *scene = editor->scene;

    int selected_nodes[STRUCT_MAX_NODES];
    int selected_count = 0;
    for (size_t i = 0; i < scene->node_count; ++i) {
        if (scene->nodes[i].selected && selected_count < STRUCT_MAX_NODES) {
            selected_nodes[selected_count++] = scene->nodes[i].id;
        }
    }
    if (selected_count == 0) return;

    int new_ids[STRUCT_MAX_NODES];
    for (int i = 0; i < selected_count; ++i) {
        StructNode *node = structural_scene_get_node(scene, selected_nodes[i]);
        if (!node) {
            new_ids[i] = -1;
            continue;
        }
        int new_id = structural_scene_add_node(scene, node->x + 20.0f, node->y + 20.0f);
        StructNode *new_node = structural_scene_get_node(scene, new_id);
        if (new_node) {
            new_node->fixed_x = node->fixed_x;
            new_node->fixed_y = node->fixed_y;
        }
        new_ids[i] = new_id;
    }

    for (size_t i = 0; i < scene->edge_count; ++i) {
        StructEdge *edge = &scene->edges[i];
        int idx_a = -1;
        int idx_b = -1;
        for (int n = 0; n < selected_count; ++n) {
            if (edge->a_id == selected_nodes[n]) idx_a = n;
            if (edge->b_id == selected_nodes[n]) idx_b = n;
        }
        if (idx_a >= 0 && idx_b >= 0) {
            int new_edge_id = structural_scene_add_edge(scene, new_ids[idx_a], new_ids[idx_b]);
            StructEdge *new_edge = structural_scene_get_edge(scene, new_edge_id);
            if (new_edge) new_edge->material_index = edge->material_index;
        }
    }

    editor_clear_selection(scene);
    for (int i = 0; i < selected_count; ++i) {
        StructNode *node = structural_scene_get_node(scene, new_ids[i]);
        if (node) node->selected = true;
    }
    editor_set_status(editor, "Duplicated selection.");
}

static void editor_delete_selection(StructuralEditor *editor) {
    if (!editor || !editor->scene) return;
    StructuralScene *scene = editor->scene;

    for (size_t i = 0; i < scene->edge_count;) {
        if (scene->edges[i].selected) {
            structural_scene_remove_edge(scene, scene->edges[i].id);
            continue;
        }
        ++i;
    }

    for (size_t i = 0; i < scene->node_count;) {
        if (scene->nodes[i].selected) {
            structural_scene_remove_node(scene, scene->nodes[i].id);
            continue;
        }
        ++i;
    }

    editor_set_status(editor, "Deleted selection.");
}

static void editor_cycle_material(StructuralEditor *editor) {
    if (!editor || !editor->scene) return;
    StructuralScene *scene = editor->scene;
    if (scene->material_count == 0) return;

    int next_material = (editor->active_material + 1) % (int)scene->material_count;
    editor->active_material = next_material;

    for (size_t i = 0; i < scene->edge_count; ++i) {
        if (scene->edges[i].selected) {
            scene->edges[i].material_index = editor->active_material;
        }
    }
    editor_set_status(editor, "Material updated.");
}

static void editor_cycle_load_case(StructuralEditor *editor, int direction) {
    if (!editor || !editor->scene) return;
    StructuralScene *scene = editor->scene;
    if (scene->load_case_count <= 0) return;
    int next = scene->active_load_case + direction;
    if (next < 0) next = (int)scene->load_case_count - 1;
    if (next >= (int)scene->load_case_count) next = 0;
    scene->active_load_case = next;
    editor_set_status(editor, "Switched load case.");
}

void structural_editor_init(StructuralEditor *editor, StructuralScene *scene) {
    if (!editor) return;
    memset(editor, 0, sizeof(*editor));
    editor->scene = scene;
    editor->tool = STRUCT_TOOL_SELECT;
    editor->snap_to_grid = false;
    editor->grid_size = 16.0f;
    editor->edge_start_node_id = -1;
    editor->drag_node_id = -1;
    editor->load_node_id = -1;
    editor->moment_node_id = -1;
    editor->active_material = 0;
    editor->show_ids = false;
    editor->show_constraints = true;
    editor->show_loads = true;
    editor->show_deformed = true;
    editor->show_stress = true;
    editor->show_bending = false;
    editor->show_shear = false;
    editor->show_combined = false;
    editor->scale_use_percentile = true;
    editor->scale_freeze = false;
    editor->scale_thickness = true;
    editor->scale_gamma = 0.6f;
    editor->scale_percentile = 0.95f;
    editor->thickness_gain = 0.6f;
    editor->deform_scale = 10.0f;
}

void structural_editor_handle_pointer_down(StructuralEditor *editor,
                                           const InputPointerState *state,
                                           SDL_Keymod mod) {
    if (!editor || !editor->scene || !state) return;
    if (state->button != SDL_BUTTON_LEFT) return;
    StructuralScene *scene = editor->scene;
    float x = (float)state->x;
    float y = (float)state->y;

    if (editor->tool == STRUCT_TOOL_ADD_NODE) {
        editor_apply_snap(editor, &x, &y);
        structural_scene_add_node(scene, x, y);
        editor_mark_dirty(editor);
        editor_set_status(editor, "Added node.");
        return;
    }

    if (editor->tool == STRUCT_TOOL_ADD_EDGE) {
        int node_id = structural_scene_find_node_at(scene, x, y, 10.0f);
        if (node_id < 0) return;
        if (editor->edge_start_node_id < 0) {
            editor->edge_start_node_id = node_id;
            editor_set_status(editor, "Edge start set.");
            return;
        }
        if (editor->edge_start_node_id == node_id) return;
        int edge_id = structural_scene_add_edge(scene, editor->edge_start_node_id, node_id);
        StructEdge *edge = structural_scene_get_edge(scene, edge_id);
        if (edge) edge->material_index = editor->active_material;
        editor->edge_start_node_id = -1;
        editor_mark_dirty(editor);
        editor_set_status(editor, "Added edge.");
        return;
    }

    if (editor->tool == STRUCT_TOOL_ADD_LOAD) {
        int node_id = structural_scene_find_node_at(scene, x, y, 10.0f);
        if (node_id < 0) return;
        editor->load_dragging = true;
        editor->load_node_id = node_id;
        editor->load_start_x = x;
        editor->load_start_y = y;
        return;
    }

    if (editor->tool == STRUCT_TOOL_ADD_MOMENT) {
        int node_id = structural_scene_find_node_at(scene, x, y, 10.0f);
        if (node_id < 0) return;
        editor->moment_dragging = true;
        editor->moment_node_id = node_id;
        editor->moment_start_x = x;
        editor->moment_start_y = y;
        return;
    }

    if (editor->tool == STRUCT_TOOL_SELECT) {
        int node_id = structural_scene_find_node_at(scene, x, y, 10.0f);
        if (node_id >= 0) {
            StructNode *node = structural_scene_get_node(scene, node_id);
            if (!node) return;
            if (mod & KMOD_SHIFT) {
                node->selected = !node->selected;
            } else {
                editor_clear_selection(scene);
                node->selected = true;
            }
            editor->drag_node_id = node_id;
            editor->dragging = true;
            editor->drag_offset_x = node->x - x;
            editor->drag_offset_y = node->y - y;
            return;
        }

        int edge_id = structural_scene_find_edge_at(scene, x, y, 8.0f);
        if (edge_id >= 0) {
            StructEdge *edge = structural_scene_get_edge(scene, edge_id);
            if (!edge) return;
            if (!(mod & KMOD_SHIFT)) {
                editor_clear_selection(scene);
            }
            edge->selected = !edge->selected;
            return;
        }

        if (mod & KMOD_SHIFT) {
            editor->box_selecting = true;
            editor->box_start_x = state->x;
            editor->box_start_y = state->y;
            editor->box_end_x = state->x;
            editor->box_end_y = state->y;
        } else {
            editor_clear_selection(scene);
        }
    }
}

void structural_editor_handle_pointer_up(StructuralEditor *editor,
                                         const InputPointerState *state,
                                         SDL_Keymod mod) {
    if (!editor || !editor->scene || !state) return;
    if (state->button != SDL_BUTTON_LEFT) return;
    StructuralScene *scene = editor->scene;
    float x = (float)state->x;
    float y = (float)state->y;

    if (editor->load_dragging && editor->load_node_id >= 0) {
        float dx = x - editor->load_start_x;
        float dy = y - editor->load_start_y;
        float scale = 0.1f;
        structural_scene_add_load(scene,
                                  editor->load_node_id,
                                  dx * scale,
                                  dy * scale,
                                  0.0f,
                                  scene->active_load_case);
        editor->load_dragging = false;
        editor->load_node_id = -1;
        editor_mark_dirty(editor);
        editor_set_status(editor, "Added load.");
        return;
    }

    if (editor->moment_dragging && editor->moment_node_id >= 0) {
        float dx = x - editor->moment_start_x;
        float scale = 0.1f;
        structural_scene_add_load(scene,
                                  editor->moment_node_id,
                                  0.0f,
                                  0.0f,
                                  dx * scale,
                                  scene->active_load_case);
        editor->moment_dragging = false;
        editor->moment_node_id = -1;
        editor_mark_dirty(editor);
        editor_set_status(editor, "Added moment.");
        return;
    }

    if (editor->box_selecting) {
        editor->box_selecting = false;
        int min_x = editor->box_start_x < editor->box_end_x ? editor->box_start_x : editor->box_end_x;
        int max_x = editor->box_start_x > editor->box_end_x ? editor->box_start_x : editor->box_end_x;
        int min_y = editor->box_start_y < editor->box_end_y ? editor->box_start_y : editor->box_end_y;
        int max_y = editor->box_start_y > editor->box_end_y ? editor->box_start_y : editor->box_end_y;
        editor_select_nodes_in_box(scene, min_x, min_y, max_x, max_y, (mod & KMOD_SHIFT) != 0);
        return;
    }

    editor->dragging = false;
    editor->drag_node_id = -1;
}

void structural_editor_handle_pointer_move(StructuralEditor *editor,
                                           const InputPointerState *state,
                                           SDL_Keymod mod) {
    (void)mod;
    if (!editor || !editor->scene || !state) return;
    StructuralScene *scene = editor->scene;

    if (editor->dragging && editor->drag_node_id >= 0) {
        StructNode *node = structural_scene_get_node(scene, editor->drag_node_id);
        if (!node) return;
        float x = (float)state->x + editor->drag_offset_x;
        float y = (float)state->y + editor->drag_offset_y;
        editor_apply_snap(editor, &x, &y);
        node->x = x;
        node->y = y;
        editor_mark_dirty(editor);
        return;
    }

    if (editor->box_selecting) {
        editor->box_end_x = state->x;
        editor->box_end_y = state->y;
    }
}

static void editor_toggle_edge_release(StructuralEditor *editor,
                                       bool toggle_a,
                                       bool toggle_b) {
    if (!editor || !editor->scene) return;
    StructuralScene *scene = editor->scene;
    bool changed = false;
    for (size_t i = 0; i < scene->edge_count; ++i) {
        StructEdge *edge = &scene->edges[i];
        if (!edge->selected) continue;
        if (toggle_a) edge->release_a = !edge->release_a;
        if (toggle_b) edge->release_b = !edge->release_b;
        changed = true;
    }
    if (changed) {
        structural_scene_clear_solution(scene);
        editor_set_status(editor, "Toggled joint releases.");
    }
}

void structural_editor_handle_key_down(StructuralEditor *editor,
                                       SDL_Keycode key,
                                       SDL_Keymod mod) {
    if (!editor || !editor->scene) return;
    StructuralScene *scene = editor->scene;

    switch (key) {
    case SDLK_1:
        editor->tool = STRUCT_TOOL_SELECT;
        editor_set_status(editor, "Tool: Select.");
        break;
    case SDLK_2:
        editor->tool = STRUCT_TOOL_ADD_NODE;
        editor_set_status(editor, "Tool: Add Node.");
        break;
    case SDLK_3:
        editor->tool = STRUCT_TOOL_ADD_EDGE;
        editor_set_status(editor, "Tool: Add Edge.");
        break;
    case SDLK_4:
        editor->tool = STRUCT_TOOL_ADD_LOAD;
        editor_set_status(editor, "Tool: Add Load.");
        break;
    case SDLK_5:
        editor->tool = STRUCT_TOOL_ADD_MOMENT;
        editor_set_status(editor, "Tool: Add Moment.");
        break;
    case SDLK_g:
        editor->snap_to_grid = !editor->snap_to_grid;
        editor_set_status(editor, editor->snap_to_grid ? "Snap enabled." : "Snap disabled.");
        break;
    case SDLK_8:
        editor_set_selected_support(scene, true, true, false, "Supports: pinned.", editor);
        editor_mark_dirty(editor);
        break;
    case SDLK_9:
        editor_set_selected_support(scene, true, true, true, "Supports: fixed.", editor);
        editor_mark_dirty(editor);
        break;
    case SDLK_0:
        editor_set_selected_support(scene, false, true, false, "Supports: roller.", editor);
        editor_mark_dirty(editor);
        break;
    case SDLK_x:
        for (size_t i = 0; i < scene->node_count; ++i) {
            if (scene->nodes[i].selected) {
                scene->nodes[i].fixed_x = !scene->nodes[i].fixed_x;
            }
        }
        editor_mark_dirty(editor);
        editor_set_status(editor, "Toggled X constraints.");
        break;
    case SDLK_y:
        for (size_t i = 0; i < scene->node_count; ++i) {
            if (scene->nodes[i].selected) {
                scene->nodes[i].fixed_y = !scene->nodes[i].fixed_y;
            }
        }
        editor_mark_dirty(editor);
        editor_set_status(editor, "Toggled Y constraints.");
        break;
    case SDLK_q:
        for (size_t i = 0; i < scene->node_count; ++i) {
            if (scene->nodes[i].selected) {
                scene->nodes[i].fixed_theta = !scene->nodes[i].fixed_theta;
            }
        }
        editor_mark_dirty(editor);
        editor_set_status(editor, "Toggled rotation constraints.");
        break;
    case SDLK_w:
        editor_weld_selected_nodes(editor);
        editor_mark_dirty(editor);
        break;
    case SDLK_k:
        editor_split_selected_edge(editor);
        editor_mark_dirty(editor);
        break;
    case SDLK_d:
        editor_duplicate_selection(editor);
        editor_mark_dirty(editor);
        break;
    case SDLK_h:
        if (mod & KMOD_SHIFT) {
            editor_toggle_edge_release(editor, true, false);
        } else if (mod & KMOD_ALT) {
            editor_toggle_edge_release(editor, false, true);
        } else {
            editor_toggle_edge_release(editor, true, true);
        }
        editor_mark_dirty(editor);
        break;
    case SDLK_DELETE:
    case SDLK_BACKSPACE:
        editor_delete_selection(editor);
        editor_mark_dirty(editor);
        break;
    case SDLK_m:
        if (mod & KMOD_SHIFT) {
            editor->active_material = (editor->active_material + 1) % (int)scene->material_count;
            editor_set_status(editor, "Active material changed.");
        } else {
            editor_cycle_material(editor);
        }
        editor_mark_dirty(editor);
        break;
    case SDLK_LEFTBRACKET:
        editor_cycle_load_case(editor, -1);
        editor_mark_dirty(editor);
        break;
    case SDLK_RIGHTBRACKET:
        editor_cycle_load_case(editor, 1);
        editor_mark_dirty(editor);
        break;
    case SDLK_MINUS:
        editor->deform_scale = fmaxf(0.0f, editor->deform_scale - 1.0f);
        break;
    case SDLK_EQUALS:
        editor->deform_scale += 1.0f;
        break;
    case SDLK_i:
        editor->show_ids = !editor->show_ids;
        break;
    case SDLK_c:
        editor->show_constraints = !editor->show_constraints;
        break;
    case SDLK_l:
        editor->show_loads = !editor->show_loads;
        break;
    case SDLK_o:
        editor->show_deformed = !editor->show_deformed;
        break;
    case SDLK_t:
        editor->show_stress = !editor->show_stress;
        break;
    case SDLK_b:
        editor->show_bending = !editor->show_bending;
        break;
    case SDLK_v:
        editor->show_shear = !editor->show_shear;
        break;
    default:
        break;
    }
}

void structural_editor_render_box(const StructuralEditor *editor,
                                  SDL_Renderer *renderer) {
    if (!editor || !renderer) return;
    if (!editor->box_selecting) return;
    int min_x = editor->box_start_x < editor->box_end_x ? editor->box_start_x : editor->box_end_x;
    int max_x = editor->box_start_x > editor->box_end_x ? editor->box_start_x : editor->box_end_x;
    int min_y = editor->box_start_y < editor->box_end_y ? editor->box_start_y : editor->box_end_y;
    int max_y = editor->box_start_y > editor->box_end_y ? editor->box_start_y : editor->box_end_y;
    SDL_Rect rect = {min_x, min_y, max_x - min_x, max_y - min_y};
    SDL_SetRenderDrawColor(renderer, 200, 200, 255, 80);
    SDL_RenderDrawRect(renderer, &rect);
}
