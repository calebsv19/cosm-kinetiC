#include "app/editor/scene_editor_panel_internal.h"

#include <stdio.h>
#include <string.h>

#include "app/editor/scene_editor_model.h"
#include "app/menu/menu_render.h"

void scene_editor_panel_draw_object_list(SceneEditorState *state) {
    if (!state) return;
    refresh_panel_theme();
    int row_h = state->list_view.row_height;
    int x = state->list_rect.x + 6;
    int y_start = state->list_rect.y - (int)editor_list_view_offset(&state->list_view);
    int text_h = panel_font_height(state->renderer, state->font_small, 16);
    if (text_h < 12) text_h = 12;
    SDL_Color text = COLOR_TEXT;
    int total_rows = (int)state->working.object_count;
    editor_list_view_set_rows(&state->list_view, total_rows);
    for (int i = 0; i < total_rows; ++i) {
        int y = y_start + i * row_h;
        if (y + row_h < state->list_rect.y || y > state->list_rect.y + state->list_rect.h) continue;
        SDL_Color bg = {0, 0, 0, 0};
        if (i == state->selected_object) {
            SDL_Color accent = menu_color_accent();
            bg = (SDL_Color){accent.r, accent.g, accent.b, 80};
        } else if (i == state->hover_object) {
            SDL_Color hover = lighten_color(COLOR_PANEL, 0.12f);
            bg = (SDL_Color){hover.r, hover.g, hover.b, 90};
        }
        if (bg.a > 0) {
            SDL_Rect r = {state->list_rect.x + 2, y + 2, state->list_rect.w - 14, row_h - 4};
            SDL_SetRenderDrawColor(state->renderer, bg.r, bg.g, bg.b, bg.a);
            SDL_RenderFillRect(state->renderer, &r);
        }
        const PresetObject *obj = &state->working.objects[i];
        const char *label = (obj->type == PRESET_OBJECT_BOX) ? "Box" : "Circle";
        int em_idx = emitter_index_for_object(state, i);
        SDL_Color col = text;
        char line[64];
        char line_fit[64];
        snprintf(line, sizeof(line), "%s %s%s",
                 label,
                 obj->gravity_enabled ? "[G]" : "[ ]",
                 (em_idx >= 0) ? " (Emitter)" : "");
        fit_text_to_width(state->renderer,
                          state->font_small,
                          line,
                          state->list_rect.w - 26,
                          line_fit,
                          sizeof(line_fit));
        draw_text(state->renderer,
                  state->font_small,
                  line_fit[0] ? line_fit : line,
                  x,
                  y + (row_h - text_h) / 2,
                  col);
    }
    SDL_Color track = COLOR_BG;
    track.a = 180;
    SDL_Color thumb = menu_color_accent();
    thumb.a = 220;
    editor_list_view_draw(state->renderer, &state->list_view, track, thumb);
}

void scene_editor_panel_draw_scene_library_summary(SceneEditorState *state) {
    char line[256];
    const PhysicsSimSceneLibraryEntry *entry = NULL;
    int main_h = 0;
    int small_h = 0;
    int text_y = 0;
    if (!state) return;
    if (state->scene_library.mode == PHYSICS_SIM_SCENE_LIBRARY_MODE_3D) {
        entry = physics_sim_editor_scene_library_selected_retained(&state->scene_library);
        snprintf(line,
                 sizeof(line),
                 "3D catalog: %s (%d)",
                 entry && entry->display_name[0] ? entry->display_name : "none",
                 state->scene_library.retained_scenes.count);
    } else {
        entry = physics_sim_editor_scene_library_selected_legacy(&state->scene_library);
        snprintf(line,
                 sizeof(line),
                 "2D catalog: %s",
                 entry && entry->display_name[0] ? entry->display_name : "none");
    }
    main_h = panel_font_height(state->renderer, state->font_main, 24);
    if (main_h < 20) main_h = 20;
    small_h = panel_font_height(state->renderer, state->font_small, 16);
    if (small_h < 14) small_h = 14;
    text_y = state->panel_rect.y + 12 + main_h + 6;
    draw_text(state->renderer,
              state->font_small,
              line,
              state->panel_rect.x + 12,
              text_y,
              COLOR_TEXT_DIM);
}

void scene_editor_panel_draw_retained_object_list(SceneEditorState *state) {
    if (!state) return;
    refresh_panel_theme();
    int row_h = state->list_view.row_height;
    int x = state->list_rect.x + 6;
    int y_start = state->list_rect.y - (int)editor_list_view_offset(&state->list_view);
    int text_h = panel_font_height(state->renderer, state->font_small, 16);
    int total_rows = physics_sim_editor_session_retained_object_count(&state->session);
    if (text_h < 12) text_h = 12;
    editor_list_view_set_rows(&state->list_view, total_rows);
    for (int i = 0; i < total_rows; ++i) {
        const CoreSceneObjectContract *object = physics_sim_editor_session_object_at(&state->session, i);
        SDL_Color text = COLOR_TEXT;
        SDL_Color bg = (SDL_Color){0, 0, 0, 0};
        char line[128];
        char line_fit[128];
        const char *kind = "Unknown";
        const char *object_id = "(unnamed)";
        int y = y_start + i * row_h;
        if (y + row_h < state->list_rect.y || y > state->list_rect.y + state->list_rect.h) continue;
        if (!object) continue;

        if (i == state->session.selection.retained_object_index) {
            SDL_Color accent = menu_color_accent();
            bg = (SDL_Color){accent.r, accent.g, accent.b, 80};
        } else if (i == state->hover_row) {
            SDL_Color hover = lighten_color(COLOR_PANEL, 0.12f);
            bg = (SDL_Color){hover.r, hover.g, hover.b, 90};
        }
        if (bg.a > 0) {
            SDL_Rect r = {state->list_rect.x + 2, y + 2, state->list_rect.w - 14, row_h - 4};
            SDL_SetRenderDrawColor(state->renderer, bg.r, bg.g, bg.b, bg.a);
            SDL_RenderFillRect(state->renderer, &r);
        }

        kind = physics_sim_editor_session_object_kind_label(object->kind);
        if (strcmp(kind, "Plane Primitive") == 0) {
            kind = "Plane";
        } else if (strcmp(kind, "Rect Prism Primitive") == 0) {
            kind = "Prism";
        }
        if (object->object.object_id[0]) {
            object_id = object->object.object_id;
        }
        snprintf(line, sizeof(line), "%s [%s]", object_id, kind);
        fit_text_to_width(state->renderer,
                          state->font_small,
                          line,
                          state->list_rect.w - 26,
                          line_fit,
                          sizeof(line_fit));
        draw_text(state->renderer,
                  state->font_small,
                  line_fit[0] ? line_fit : line,
                  x,
                  y + (row_h - text_h) / 2,
                  text);
    }
    {
        SDL_Color track = COLOR_BG;
        SDL_Color thumb = menu_color_accent();
        track.a = 180;
        thumb.a = 220;
        editor_list_view_draw(state->renderer, &state->list_view, track, thumb);
    }
}

void scene_editor_panel_draw_left_object_info_card(SceneEditorState *state) {
    const CoreSceneObjectContract *selected_retained = NULL;
    const PhysicsSimObjectOverlay *selected_overlay = NULL;
    const PhysicsSimEmitterOverlay *selected_emitter = NULL;
    SDL_Rect rect = {0};
    SDL_Color fill = {0};
    char line_a[160];
    char line_b[160];
    char line_c[160];
    char line_d[160];
    char line_e[160];
    char line_f[160];
    char line_g[160];
    char runtime_wrap[2][192];
    char fit[192];
    int x = 0;
    int y = 0;
    int line_step = 0;

    if (!state || !state->renderer) return;
    rect = state->object_info_rect;
    if (rect.w <= 0 || rect.h <= 0) return;
    line_a[0] = '\0';
    line_b[0] = '\0';
    line_c[0] = '\0';
    line_d[0] = '\0';
    line_e[0] = '\0';
    line_f[0] = '\0';
    line_g[0] = '\0';

    fill = lighten_color(COLOR_PANEL, 0.05f);
    SDL_SetRenderDrawColor(state->renderer, fill.r, fill.g, fill.b, 255);
    SDL_RenderFillRect(state->renderer, &rect);
    SDL_SetRenderDrawColor(state->renderer, COLOR_BG.r, COLOR_BG.g, COLOR_BG.b, 255);
    SDL_RenderDrawRect(state->renderer, &rect);

    selected_retained = physics_sim_editor_session_selected_object(&state->session);
    selected_overlay = physics_sim_editor_session_selected_object_overlay(&state->session);
    selected_emitter = physics_sim_editor_session_selected_object_emitter(&state->session);
    x = rect.x + 8;
    y = rect.y + 8;
    line_step = panel_font_height(state->renderer, state->font_small, 15) + 6;
    if (line_step < 18) line_step = 18;

    draw_text(state->renderer, state->font_small, "Selected Object", x, y, COLOR_TEXT);
    y += line_step + 2;

    if (!selected_retained) {
        draw_text(state->renderer, state->font_small, "No retained object selected.", x, y, COLOR_TEXT_DIM);
        return;
    }

    snprintf(line_a,
             sizeof(line_a),
             "%s",
             selected_retained->object.object_id[0] ? selected_retained->object.object_id : "(unnamed)");
    snprintf(line_b,
             sizeof(line_b),
             "%s",
             physics_sim_editor_session_object_kind_label(selected_retained->kind));
    if (selected_overlay) {
        snprintf(line_c,
                 sizeof(line_c),
                 "Physics %s",
                 physics_sim_editor_session_motion_mode_label(selected_overlay->motion_mode));
        snprintf(line_f,
                 sizeof(line_f),
                 "Vel %.2f, %.2f, %.2f",
                 selected_overlay->initial_velocity.x,
                 selected_overlay->initial_velocity.y,
                 selected_overlay->initial_velocity.z);
        snprintf(line_g,
                 sizeof(line_g),
                 selected_emitter
                     ? "Runtime: motion + XY velocity active; emitter runtime pending"
                     : "Runtime: motion + XY velocity active; Z saved only");
    } else {
        line_c[0] = '\0';
        line_f[0] = '\0';
        line_g[0] = '\0';
    }

    if (selected_emitter) {
        snprintf(line_d,
                 sizeof(line_d),
                 "Emitter %s  r=%.2f  s=%.1f",
                 physics_sim_editor_session_emitter_type_label(selected_emitter->type),
                 selected_emitter->radius,
                 selected_emitter->strength);
        snprintf(line_g,
                 sizeof(line_g),
                 "3D %s  %s  %s  Thermal %.1f",
                 physics_sim_editor_session_emitter_source_mode_3d_label(selected_emitter->source_mode_3d),
                 physics_sim_editor_session_emitter_surface_3d_label(selected_emitter->surface_3d),
                 physics_sim_editor_session_emitter_obstacle_mode_3d_label(selected_emitter->obstacle_mode_3d),
                 selected_emitter->thermal_buoyancy_3d);
    } else {
        snprintf(line_d, sizeof(line_d), "Emitter none");
    }

    if (selected_retained->has_plane_primitive) {
        CoreObjectVec3 position = selected_retained->plane_primitive.frame.origin;
        snprintf(line_e,
                 sizeof(line_e),
                 "Size %.2f x %.2f  Pos %.2f, %.2f, %.2f",
                 selected_retained->plane_primitive.width,
                 selected_retained->plane_primitive.height,
                 position.x,
                 position.y,
                 position.z);
    } else if (selected_retained->has_rect_prism_primitive) {
        CoreObjectVec3 position = selected_retained->rect_prism_primitive.frame.origin;
        snprintf(line_e,
                 sizeof(line_e),
                 "Size %.2f x %.2f x %.2f",
                 selected_retained->rect_prism_primitive.width,
                 selected_retained->rect_prism_primitive.height,
                 selected_retained->rect_prism_primitive.depth);
        snprintf(line_f,
                 sizeof(line_f),
                 "Pos %.2f, %.2f, %.2f%s%.2f, %.2f, %.2f",
                 position.x,
                 position.y,
                 position.z,
                 selected_overlay ? "  Vel " : "",
                 selected_overlay ? selected_overlay->initial_velocity.x : 0.0,
                 selected_overlay ? selected_overlay->initial_velocity.y : 0.0,
                 selected_overlay ? selected_overlay->initial_velocity.z : 0.0);
    } else {
        CoreObjectVec3 position = selected_retained->object.transform.position;
        snprintf(line_e,
                 sizeof(line_e),
                 "Pos %.2f, %.2f, %.2f",
                 position.x,
                 position.y,
                 position.z);
    }

    fit_text_to_width(state->renderer, state->font_small, line_a, rect.w - 16, fit, sizeof(fit));
    draw_text(state->renderer, state->font_small, fit[0] ? fit : line_a, x, y, COLOR_TEXT);
    y += line_step;
    fit_text_to_width(state->renderer, state->font_small, line_b, rect.w - 16, fit, sizeof(fit));
    draw_text(state->renderer, state->font_small, fit[0] ? fit : line_b, x, y, COLOR_TEXT_DIM);
    y += line_step;
    if (line_c[0]) {
        fit_text_to_width(state->renderer, state->font_small, line_c, rect.w - 16, fit, sizeof(fit));
        draw_text(state->renderer, state->font_small, fit[0] ? fit : line_c, x, y, COLOR_TEXT_DIM);
        y += line_step;
    }
    if (line_d[0]) {
        fit_text_to_width(state->renderer, state->font_small, line_d, rect.w - 16, fit, sizeof(fit));
        draw_text(state->renderer, state->font_small, fit[0] ? fit : line_d, x, y, COLOR_TEXT_DIM);
        y += line_step;
    }
    if (line_e[0] && y + line_step <= rect.y + rect.h - 6) {
        fit_text_to_width(state->renderer, state->font_small, line_e, rect.w - 16, fit, sizeof(fit));
        draw_text(state->renderer, state->font_small, fit[0] ? fit : line_e, x, y, COLOR_TEXT_DIM);
        y += line_step;
    }
    if (line_f[0] && y + line_step <= rect.y + rect.h - 6) {
        fit_text_to_width(state->renderer, state->font_small, line_f, rect.w - 16, fit, sizeof(fit));
        draw_text(state->renderer, state->font_small, fit[0] ? fit : line_f, x, y, COLOR_TEXT_DIM);
        y += line_step;
    }
    if (line_g[0] && y + line_step <= rect.y + rect.h - 6) {
        int runtime_line_count = 0;
        wrap_text_lines(state->renderer,
                        state->font_small,
                        line_g,
                        rect.w - 16,
                        2,
                        runtime_wrap,
                        &runtime_line_count);
        for (int i = 0; i < runtime_line_count; ++i) {
            if (y + line_step > rect.y + rect.h - 6) break;
            draw_text(state->renderer, state->font_small, runtime_wrap[i], x, y, COLOR_TEXT_DIM);
            y += line_step;
        }
    }
}
