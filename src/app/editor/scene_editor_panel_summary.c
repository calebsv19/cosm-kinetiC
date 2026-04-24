#include "app/editor/scene_editor_panel_internal.h"

#include "app/editor/scene_editor_model.h"

#include <stdio.h>

static void draw_status_card(SceneEditorState *state,
                             int x,
                             int y,
                             int w,
                             int card_bottom_y) {
    char overlay_line[192];
    char save_line[192];
    char overlay_wrap[2][192];
    char save_wrap[2][192];
    SDL_Color overlay_color = COLOR_TEXT_DIM;
    SDL_Color save_color = COLOR_TEXT_DIM;
    SDL_Rect card_rect;
    int overlay_lines = 0;
    int save_lines = 0;
    int line_step = 0;
    int line_h = 0;
    int card_h = 0;
    int current_y = 0;
    if (!state || !state->renderer || !physics_sim_editor_session_has_retained_scene(&state->session)) return;

    line_h = panel_font_height(state->renderer, state->font_small, 15);
    if (line_h < 14) line_h = 14;
    line_step = line_h + 8;

    SDL_Color card_fill = lighten_color(COLOR_PANEL, 0.05f);
    if (state->dirty) {
        snprintf(overlay_line, sizeof(overlay_line), "Overlay: unapplied changes");
        overlay_color = COLOR_STATUS_WARN;
    } else if (state->overlay_apply_diagnostics[0]) {
        snprintf(overlay_line, sizeof(overlay_line), "Overlay: %s", state->overlay_apply_diagnostics);
        overlay_color = state->overlay_apply_success ? COLOR_STATUS_OK : COLOR_STATUS_ERR;
    } else {
        snprintf(overlay_line, sizeof(overlay_line), "Overlay: no local changes");
    }

    if (state->save_scene_diagnostics[0]) {
        snprintf(save_line, sizeof(save_line), "Scene Save: %s", state->save_scene_diagnostics);
        save_color = state->save_scene_success ? COLOR_STATUS_OK : COLOR_STATUS_ERR;
        if (state->dirty) {
            save_color = COLOR_TEXT_DIM;
        }
    } else {
        snprintf(save_line, sizeof(save_line), "Scene Save: not yet saved");
        save_color = COLOR_STATUS_WARN;
    }

    wrap_text_lines(state->renderer,
                    state->font_small,
                    overlay_line,
                    w - 16,
                    2,
                    overlay_wrap,
                    &overlay_lines);
    wrap_text_lines(state->renderer,
                    state->font_small,
                    save_line,
                    w - 16,
                    2,
                    save_wrap,
                    &save_lines);
    if (overlay_lines < 1) overlay_lines = 1;
    if (save_lines < 1) save_lines = 1;

    card_h = 16 + line_step * (1 + overlay_lines + save_lines);
    if (card_bottom_y > y && card_h > (card_bottom_y - y)) {
        card_h = card_bottom_y - y;
    }
    if (card_h < line_step * 3) {
        card_h = line_step * 3;
    }
    card_rect = (SDL_Rect){x, y, w, card_h};

    SDL_SetRenderDrawColor(state->renderer, card_fill.r, card_fill.g, card_fill.b, 255);
    SDL_RenderFillRect(state->renderer, &card_rect);
    SDL_SetRenderDrawColor(state->renderer, COLOR_BG.r, COLOR_BG.g, COLOR_BG.b, 220);
    SDL_RenderDrawRect(state->renderer, &card_rect);

    draw_text(state->renderer, state->font_small, "Status", x + 8, y + 8, COLOR_TEXT);
    current_y = y + 8 + line_step;
    for (int i = 0; i < overlay_lines; ++i) {
        if (!overlay_wrap[i][0]) continue;
        draw_text(state->renderer, state->font_small, overlay_wrap[i], x + 8, current_y, overlay_color);
        current_y += line_step;
    }
    for (int i = 0; i < save_lines; ++i) {
        if (!save_wrap[i][0]) continue;
        draw_text(state->renderer, state->font_small, save_wrap[i], x + 8, current_y, save_color);
        current_y += line_step;
    }
}

void draw_center_pane_summary(SceneEditorState *state) {
    char line_a[160];
    char line_b[160];
    int x = 0;
    int y = 0;
    if (!state || !state->renderer) return;

    x = state->center_summary_rect.x;
    y = state->center_summary_rect.y;
    if (state->viewport.requested_mode == SPACE_MODE_3D) {
        snprintf(line_a,
                 sizeof(line_a),
                 "3D Scaffold  Dist: %.2f  Orbit: %.0f/%.0f  Focus: %.2f, %.2f",
                 state->viewport.orbit_distance,
                 state->viewport.orbit_yaw_deg,
                 state->viewport.orbit_pitch_deg,
                 state->viewport.center_x,
                 state->viewport.center_y);
        snprintf(line_b,
                 sizeof(line_b),
                 "Proj: 2D lane  Alt+LMB orbit  MMB pan  Wheel dolly  F frame");
    } else {
        snprintf(line_a,
                 sizeof(line_a),
                 "2D Ortho  Zoom: %.2fx  Focus: %.2f, %.2f",
                 scene_editor_viewport_active_zoom(&state->viewport),
                 state->viewport.center_x,
                 state->viewport.center_y);
        snprintf(line_b,
                 sizeof(line_b),
                 "MMB pan  Wheel zoom  F frame");
    }
    draw_text(state->renderer, state->font_small, line_a, x, y, COLOR_TEXT_DIM);
    draw_text(state->renderer, state->font_small, line_b, x, y + 18, COLOR_TEXT_DIM);
}

void draw_right_panel_summary(SceneEditorState *state) {
    char selected_line_a[160];
    char selected_line_b[160];
    char domain_line[160];
    char domain_runtime_line[160];
    const CoreSceneObjectContract *selected_retained = NULL;
    const PhysicsSimObjectOverlay *selected_overlay = NULL;
    const PhysicsSimDomainOverlay *scene_domain = NULL;
    int line_y = 0;
    int line_step = 0;
    int x = 0;
    int summary_bottom = 0;
    if (!state || !state->renderer) return;

    x = state->right_panel_rect.x + 12;
    line_y = state->overlay_summary_rect.y;
    line_step = panel_font_height(state->renderer, state->font_small, 17) + 7;
    if (line_step < 22) line_step = 22;

    selected_line_a[0] = '\0';
    selected_line_b[0] = '\0';
    domain_line[0] = '\0';
    domain_runtime_line[0] = '\0';
    if (state->selection_kind == SELECTION_EMITTER &&
        state->selected_emitter >= 0 &&
        state->selected_emitter < (int)state->working.emitter_count) {
        const FluidEmitter *em = &state->working.emitters[state->selected_emitter];
        snprintf(selected_line_a, sizeof(selected_line_a),
                 "Selected: %s emitter #%d",
                 emitter_type_name(em->type),
                 state->selected_emitter);
        snprintf(selected_line_b, sizeof(selected_line_b),
                 "Radius %.2f  Strength %.1f  Pos %.2f, %.2f",
                 em->radius,
                 em->strength,
                 em->position_x,
                 em->position_y);
    } else if (state->selection_kind == SELECTION_OBJECT &&
               state->selected_object >= 0 &&
               state->selected_object < (int)state->working.object_count) {
        const PresetObject *obj = &state->working.objects[state->selected_object];
        snprintf(selected_line_a, sizeof(selected_line_a),
                 "Selected: %s object #%d",
                 (obj->type == PRESET_OBJECT_BOX) ? "Box" : "Circle",
                 state->selected_object);
        snprintf(selected_line_b, sizeof(selected_line_b),
                 "Size %.2f x %.2f x %.2f  Pos %.2f, %.2f",
                 obj->size_x,
                 obj->size_y,
                 obj->size_z,
                 obj->position_x,
                 obj->position_y);
    } else if (state->selection_kind == SELECTION_IMPORT &&
               state->selected_row >= 0 &&
               state->selected_row < (int)state->working.import_shape_count) {
        const ImportedShape *imp = &state->working.import_shapes[state->selected_row];
        snprintf(selected_line_a, sizeof(selected_line_a),
                 "Selected: import #%d",
                 state->selected_row);
        snprintf(selected_line_b, sizeof(selected_line_b),
                 "Scale %.2f  Rot %.1f  Pos %.2f, %.2f",
                 imp->scale,
                 imp->rotation_deg,
                 imp->position_x,
                 imp->position_y);
    } else {
        snprintf(selected_line_a, sizeof(selected_line_a), "Selected: none");
    }

    selected_retained = physics_sim_editor_session_selected_object(&state->session);
    selected_overlay = physics_sim_editor_session_selected_object_overlay(&state->session);
    if (selected_retained) {
        const char *object_id = selected_retained->object.object_id[0]
                                    ? selected_retained->object.object_id
                                    : "(unnamed)";
        snprintf(selected_line_a, sizeof(selected_line_a),
                 "Selected: %s",
                 object_id);
        snprintf(selected_line_b, sizeof(selected_line_b),
                 "%s",
                 physics_sim_editor_session_object_kind_label(selected_retained->kind));
        if (selected_overlay) {
            snprintf(selected_line_b,
                     sizeof(selected_line_b),
                     "%s  %s",
                     physics_sim_editor_session_object_kind_label(selected_retained->kind),
                     physics_sim_editor_session_motion_mode_label(selected_overlay->motion_mode));
        }
    }
    scene_domain = physics_sim_editor_session_scene_domain(&state->session);
    if (scene_domain) {
        double width = 0.0;
        double height = 0.0;
        double depth = 0.0;
        physics_sim_editor_session_scene_domain_dimensions(&state->session, &width, &height, &depth);
        snprintf(domain_line,
                 sizeof(domain_line),
                 "Domain: %s  %.2f x %.2f x %.2f",
                 scene_domain->seeded_from_retained_bounds ? "derived" : "manual",
                 width,
                 height,
                 depth);
        snprintf(domain_runtime_line,
                 sizeof(domain_runtime_line),
                 "Runtime domain: XY active; depth saved only");
    }

    draw_text(state->renderer, state->font_small, selected_line_a, x, line_y, COLOR_TEXT);
    if (selected_line_b[0]) {
        draw_text(state->renderer, state->font_small, selected_line_b, x, line_y + line_step, COLOR_TEXT_DIM);
    }
    if (domain_line[0]) {
        draw_text(state->renderer, state->font_small, domain_line, x, line_y + line_step * 2, COLOR_TEXT_DIM);
    }
    if (domain_runtime_line[0]) {
        draw_text(state->renderer,
                  state->font_small,
                  domain_runtime_line,
                  x,
                  line_y + line_step * 3,
                  COLOR_TEXT_DIM);
    }
    summary_bottom = physics_sim_editor_session_has_retained_scene(&state->session)
                         ? state->btn_apply_overlay.rect.y - 12
                         : state->btn_save.rect.y - 12;
    draw_status_card(state,
                     x,
                     line_y + line_step * (domain_runtime_line[0] ? 4 : (domain_line[0] ? 3 : 2)),
                     state->overlay_summary_rect.w,
                     summary_bottom);
}
