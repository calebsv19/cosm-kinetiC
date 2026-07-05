#include "app/editor/scene_editor_panel_internal.h"

#include "app/editor/scene_editor_model.h"

#include <stdio.h>

#include "render/text_draw.h"

static const char *editor_space_mode_label(SpaceMode mode) {
    if (mode == SPACE_MODE_3D) {
        return "3D (Scaffold)";
    }
    return "2D";
}

const char *scene_editor_inspector_view_label(SceneEditorInspectorView view) {
    switch (view) {
        case SCENE_EDITOR_INSPECTOR_OBJECT_PHYSICS: return "Object Physics";
        case SCENE_EDITOR_INSPECTOR_SOURCE_EMITTER: return "Source / Emitter";
        case SCENE_EDITOR_INSPECTOR_DIAGNOSTICS: return "Diagnostics";
        case SCENE_EDITOR_INSPECTOR_SCENE_PHYSICS:
        default: return "Scene Physics";
    }
}

static SceneEditorInspectorView scene_editor_panel_resolved_inspector_view(
    const SceneEditorState *state) {
    if (!state) return SCENE_EDITOR_INSPECTOR_SCENE_PHYSICS;
    if (state->inspector_view >= SCENE_EDITOR_INSPECTOR_SCENE_PHYSICS &&
        state->inspector_view <= SCENE_EDITOR_INSPECTOR_DIAGNOSTICS) {
        return state->inspector_view;
    }
    return SCENE_EDITOR_INSPECTOR_SCENE_PHYSICS;
}

static void draw_inspector_section_label(SceneEditorState *state,
                                         const char *label,
                                         int x,
                                         int y) {
    if (!state || !state->renderer || !label) return;
    draw_text(state->renderer, state->font_small, label, x, y, COLOR_TEXT_DIM);
}

static void draw_dimension_field(SceneEditorState *state,
                                 const SDL_Rect *rect,
                                 const char *label,
                                 float value,
                                 bool editing,
                                 const TextInputField *input) {
    if (!state || !rect) return;
    SDL_Renderer *renderer = state->renderer;
    TTF_Font *font = state->font_small ? state->font_small : state->font_main;
    if (!renderer || !font) return;
    SDL_Color bg = editing ? COLOR_FIELD_ACTIVE : COLOR_PANEL;
    SDL_SetRenderDrawColor(renderer, bg.r, bg.g, bg.b, 255);
    SDL_RenderFillRect(renderer, rect);
    SDL_SetRenderDrawColor(renderer, COLOR_FIELD_BORDER.r, COLOR_FIELD_BORDER.g, COLOR_FIELD_BORDER.b,
                           editing ? 255 : 140);
    SDL_RenderDrawRect(renderer, rect);
    int label_height = panel_font_height(renderer, font, 16);
    int label_y = rect->y - label_height - 2;
    char label_fit[64];
    char value_fit[64];
    const char *draw_label = label ? label : "";
    const char *draw_value = NULL;
    int max_text_w = rect->w - 8;
    if (max_text_w < 8) max_text_w = 8;
    if (label_y < 0) label_y = 0;
    fit_text_to_width(renderer, font, draw_label, max_text_w, label_fit, sizeof(label_fit));
    draw_text(renderer,
              font,
              label_fit[0] ? label_fit : draw_label,
              rect->x + 2,
              label_y,
              COLOR_TEXT_DIM);
    char value_buf[32];
    const char *value_text = value_buf;
    if (editing && input) {
        value_text = text_input_value(input);
        if (!value_text) value_text = "";
    } else {
        snprintf(value_buf, sizeof(value_buf), "%.2f", value);
    }
    fit_text_to_width(renderer, font, value_text, rect->w - 12, value_fit, sizeof(value_fit));
    draw_value = value_fit[0] ? value_fit : value_text;
    int text_w = 0;
    int text_h = 0;
    if (physics_sim_text_measure_utf8(renderer, font, draw_value, &text_w, &text_h)) {
        int text_y = rect->y + rect->h / 2 - text_h / 2 + 2;
        SDL_Rect dst = {rect->x + 6, text_y, text_w, text_h};
        (void)physics_sim_text_draw_utf8(renderer, font, draw_value, COLOR_TEXT, &dst);
    }
    if (editing && input && input->caret_visible) {
        int caret_x = rect->x + 6 + text_w + 2;
        int caret_y0 = rect->y + 4;
        int caret_y1 = rect->y + rect->h - 4;
        SDL_SetRenderDrawColor(renderer, COLOR_FIELD_BORDER.r, COLOR_FIELD_BORDER.g, COLOR_FIELD_BORDER.b, 255);
        SDL_RenderDrawLine(renderer, caret_x, caret_y0, caret_x, caret_y1);
    }
}

static void draw_dimension_fields(SceneEditorState *state) {
    double domain_width = 0.0;
    double domain_height = 0.0;
    double domain_depth = 0.0;
    if (!state) return;
    if (physics_sim_editor_session_has_retained_scene(&state->session)) {
        physics_sim_editor_session_scene_domain_dimensions(&state->session,
                                                           &domain_width,
                                                           &domain_height,
                                                           &domain_depth);
        draw_dimension_field(state,
                             &state->width_rect,
                             "Width",
                             (float)domain_width,
                             state->editing_width,
                             &state->width_input);
        draw_dimension_field(state,
                             &state->height_rect,
                             "Height",
                             (float)domain_height,
                             state->editing_height,
                             &state->height_input);
        draw_dimension_field(state,
                             &state->depth_rect,
                             "Depth",
                             (float)domain_depth,
                             state->editing_depth,
                             &state->depth_input);
        return;
    }
    draw_dimension_field(state,
                         &state->width_rect,
                         "Width",
                         state->working.domain_width,
                         state->editing_width,
                         &state->width_input);
    draw_dimension_field(state,
                         &state->height_rect,
                         "Height",
                         state->working.domain_height,
                         state->editing_height,
                         &state->height_input);
}

static void draw_scene_physics_section(SceneEditorState *state) {
    int x = 0;
    int y = 0;
    char mode_line[64];
    if (!state || !state->renderer) return;
    x = state->right_panel_rect.x + 12;
    y = state->right_panel_rect.y + 40;
    draw_inspector_section_label(state,
                                 scene_editor_inspector_view_label(
                                     SCENE_EDITOR_INSPECTOR_SCENE_PHYSICS),
                                 x,
                                 y);
    snprintf(mode_line,
             sizeof(mode_line),
             "Mode: %s",
             editor_space_mode_label(state->cfg.space_mode));
    draw_text(state->renderer,
              state->font_small,
              mode_line,
              x,
              y + 20,
              COLOR_TEXT);
    draw_dimension_fields(state);
}

static void draw_source_emitter_fields(SceneEditorState *state,
                                       const FluidEmitter *selected_em,
                                       const PhysicsSimEmitterOverlay *selected_retained_em) {
    int label_h = 0;
    if (!state || !state->renderer) return;
    if (physics_sim_editor_session_has_retained_scene(&state->session)) {
        label_h = panel_font_height(state->renderer, state->font_small, 16);
        draw_inspector_section_label(state,
                                     scene_editor_inspector_view_label(
                                         SCENE_EDITOR_INSPECTOR_SOURCE_EMITTER),
                                     state->btn_add_source.rect.x,
                                     state->btn_add_source.rect.y - label_h - 6);
        scene_editor_draw_button(state->renderer, &state->btn_add_source, state->font_small);
        scene_editor_draw_button(state->renderer, &state->btn_add_jet, state->font_small);
        scene_editor_draw_button(state->renderer, &state->btn_add_sink, state->font_small);
    }
    if (!physics_sim_editor_session_has_retained_scene(&state->session)) {
        scene_editor_draw_numeric_field(state->renderer, state->font_small,
                                        &state->radius_field, selected_em, NULL);
    } else {
        scene_editor_draw_numeric_field(state->renderer, state->font_small,
                                        &state->radius_field, NULL, selected_retained_em);
    }
    scene_editor_draw_numeric_field(state->renderer, state->font_small,
                                    &state->strength_field, selected_em, selected_retained_em);
    if (physics_sim_editor_session_has_retained_scene(&state->session)) {
        scene_editor_draw_numeric_field(state->renderer, state->font_small,
                                        &state->thermal_buoyancy_field, NULL, selected_retained_em);
    }
}

static void draw_object_physics_controls(SceneEditorState *state) {
    int label_h = 0;
    if (!state || !state->renderer) return;
    if (!physics_sim_editor_session_has_physics_overlay(&state->session)) return;
    label_h = panel_font_height(state->renderer, state->font_small, 16);
    if (state->btn_mesh_role_solid.rect.w > 0) {
        draw_inspector_section_label(state,
                                     scene_editor_inspector_view_label(
                                         SCENE_EDITOR_INSPECTOR_OBJECT_PHYSICS),
                                     state->btn_mesh_role_solid.rect.x,
                                     state->btn_mesh_role_solid.rect.y - label_h - 6);
        scene_editor_draw_button(state->renderer, &state->btn_mesh_role_solid, state->font_small);
        scene_editor_draw_button(state->renderer, &state->btn_mesh_role_visual, state->font_small);
        scene_editor_draw_button(state->renderer, &state->btn_mesh_role_surface, state->font_small);
        scene_editor_draw_button(state->renderer, &state->btn_mesh_role_heat, state->font_small);
        scene_editor_draw_button(state->renderer, &state->btn_mesh_role_flow, state->font_small);
    }
    draw_inspector_section_label(state,
                                 "Motion / Overlay",
                                 state->btn_overlay_dynamic.rect.x,
                                 state->btn_overlay_dynamic.rect.y - label_h - 6);
    scene_editor_draw_button(state->renderer, &state->btn_overlay_dynamic, state->font_small);
    scene_editor_draw_button(state->renderer, &state->btn_overlay_static, state->font_small);
    scene_editor_draw_button(state->renderer, &state->btn_overlay_mode_3d, state->font_small);
    scene_editor_draw_button(state->renderer, &state->btn_overlay_surface_3d, state->font_small);
    scene_editor_draw_button(state->renderer, &state->btn_overlay_obstacle_3d, state->font_small);
    scene_editor_draw_button(state->renderer, &state->btn_overlay_vel_x_neg, state->font_small);
    scene_editor_draw_button(state->renderer, &state->btn_overlay_vel_x_pos, state->font_small);
    scene_editor_draw_button(state->renderer, &state->btn_overlay_vel_y_neg, state->font_small);
    scene_editor_draw_button(state->renderer, &state->btn_overlay_vel_y_pos, state->font_small);
    scene_editor_draw_button(state->renderer, &state->btn_overlay_vel_z_neg, state->font_small);
    scene_editor_draw_button(state->renderer, &state->btn_overlay_vel_z_pos, state->font_small);
    scene_editor_draw_button(state->renderer, &state->btn_overlay_vel_reset, state->font_small);
}

void scene_editor_panel_draw_right_inspector(SceneEditorState *state) {
    const FluidEmitter *selected_em = NULL;
    const PhysicsSimEmitterOverlay *selected_retained_object_em = NULL;
    const PhysicsSimEmitterOverlay *selected_retained_mesh_em = NULL;
    const PhysicsSimEmitterOverlay *selected_retained_em = NULL;
    SceneEditorInspectorView active_view = SCENE_EDITOR_INSPECTOR_SCENE_PHYSICS;
    if (!state || !state->renderer) return;

    active_view = scene_editor_panel_resolved_inspector_view(state);
    (void)active_view;

    selected_retained_object_em =
        physics_sim_editor_session_selected_object_emitter(&state->session);
    selected_retained_mesh_em =
        physics_sim_editor_session_selected_runtime_mesh_emitter(&state->session);
    selected_retained_em =
        selected_retained_object_em ? selected_retained_object_em : selected_retained_mesh_em;
    if (state->selected_emitter >= 0 &&
        state->selected_emitter < (int)state->working.emitter_count) {
        selected_em = &state->working.emitters[state->selected_emitter];
    }

    draw_text(state->renderer, state->font_main,
              "Inspector",
              state->right_panel_rect.x + 12,
              state->right_panel_rect.y + 12,
              COLOR_TEXT);
    draw_scene_physics_section(state);
    draw_source_emitter_fields(state, selected_em, selected_retained_em);
    draw_object_physics_controls(state);
    draw_right_panel_summary(state);
}
