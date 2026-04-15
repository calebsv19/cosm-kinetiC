#include "app/editor/scene_editor_panel.h"
#include "app/editor/scene_editor_canvas.h"
#include "app/editor/scene_editor_input_common.h"
#include "app/editor/scene_editor_model.h"
#include "app/editor/scene_editor_session.h"
#include "app/menu/menu_render.h"

#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include <string.h>

#include "render/text_upload_policy.h"
#include "vk_renderer.h"

static SDL_Color COLOR_BG        = {20, 22, 26, 255};
static SDL_Color COLOR_PANEL     = {32, 36, 40, 255};
static SDL_Color COLOR_TEXT      = {245, 247, 250, 255};
static SDL_Color COLOR_TEXT_DIM  = {190, 198, 209, 255};
static SDL_Color COLOR_FIELD_ACTIVE = {24, 26, 33, 255};
static SDL_Color COLOR_FIELD_BORDER = {90, 170, 255, 255};
static SDL_Color COLOR_STATUS_OK   = {120, 210, 150, 255};
static SDL_Color COLOR_STATUS_WARN = {235, 198, 110, 255};
static SDL_Color COLOR_STATUS_ERR  = {235, 130, 130, 255};

static SDL_Rect scene_editor_draw_surface_rect(const SceneEditorState *state) {
    return editor_active_viewport_rect(state);
}

static const char *editor_space_mode_label(SpaceMode mode) {
    if (mode == SPACE_MODE_3D) {
        return "3D (Scaffold)";
    }
    return "2D";
}

static SDL_Color lighten_color(SDL_Color color, float factor) {
    if (factor < 0.0f) factor = 0.0f;
    if (factor > 1.0f) factor = 1.0f;
    SDL_Color result = color;
    result.r = (Uint8)(color.r + (Uint8)((255 - color.r) * factor));
    result.g = (Uint8)(color.g + (Uint8)((255 - color.g) * factor));
    result.b = (Uint8)(color.b + (Uint8)((255 - color.b) * factor));
    return result;
}

static void refresh_panel_theme(void) {
    COLOR_BG = menu_color_bg();
    COLOR_PANEL = menu_color_panel();
    COLOR_TEXT = menu_color_text();
    COLOR_TEXT_DIM = menu_color_text_dim();
    COLOR_FIELD_BORDER = menu_color_accent();
    COLOR_FIELD_ACTIVE = lighten_color(COLOR_PANEL, 0.08f);
}

static void draw_text(SDL_Renderer *renderer,
                      TTF_Font *font,
                      const char *text,
                      int x,
                      int y,
                      SDL_Color color) {
    if (!font || !text) return;
    SDL_Surface *surf = TTF_RenderUTF8_Blended(font, text, color);
    if (!surf) return;
    SDL_Rect dst = {
        x,
        y,
        physics_sim_text_logical_pixels(renderer, surf->w),
        physics_sim_text_logical_pixels(renderer, surf->h)
    };
    VkRendererTexture tex = {0};
    if (vk_renderer_upload_sdl_surface_with_filter((VkRenderer *)renderer,
                                                   surf,
                                                   &tex,
                                                   physics_sim_text_upload_filter(renderer)) == VK_SUCCESS) {
        vk_renderer_draw_texture((VkRenderer *)renderer, &tex, NULL, &dst);
        vk_renderer_queue_texture_destroy((VkRenderer *)renderer, &tex);
    }
    SDL_FreeSurface(surf);
}

static void fit_text_to_width(SDL_Renderer *renderer,
                              TTF_Font *font,
                              const char *text,
                              int max_width,
                              char *out,
                              size_t out_size) {
    int w = 0;
    size_t len = 0;
    if (!out || out_size == 0) return;
    out[0] = '\0';
    if (!text) return;
    snprintf(out, out_size, "%s", text);
    if (!font || max_width <= 0) return;
    if (TTF_SizeUTF8(font, out, &w, NULL) == 0) {
        w = physics_sim_text_logical_pixels(renderer, w);
        if (w <= max_width) return;
    }

    len = strlen(out);
    while (len > 0) {
        --len;
        out[len] = '\0';
        {
            char candidate[256];
            snprintf(candidate, sizeof(candidate), "%s...", out);
            if (TTF_SizeUTF8(font, candidate, &w, NULL) == 0) {
                w = physics_sim_text_logical_pixels(renderer, w);
                if (w <= max_width) {
                    snprintf(out, out_size, "%s", candidate);
                    return;
                }
            }
        }
    }
    snprintf(out, out_size, "...");
}

static int panel_font_height(SDL_Renderer *renderer, TTF_Font *font, int fallback) {
    int font_h = 0;
    if (!font) return fallback;
    font_h = TTF_FontHeight(font);
    if (font_h <= 0) return fallback;
    return physics_sim_text_logical_pixels(renderer, font_h);
}

static void draw_object_list(SceneEditorState *state) {
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

static void draw_scene_library_summary(SceneEditorState *state) {
    char line[256];
    const PhysicsSimSceneLibraryEntry *entry = NULL;
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
    draw_text(state->renderer,
              state->font_small,
              line,
              state->panel_rect.x + 12,
              state->panel_rect.y + 30,
              COLOR_TEXT_DIM);
}

static void draw_retained_object_list(SceneEditorState *state) {
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

static void draw_left_object_info_card(SceneEditorState *state) {
    const CoreSceneObjectContract *selected_retained = NULL;
    const PhysicsSimObjectOverlay *selected_overlay = NULL;
    SDL_Rect rect = {0};
    SDL_Color fill = {0};
    char line_a[160];
    char line_b[160];
    char line_c[160];
    char line_d[160];
    char line_e[160];
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

    fill = lighten_color(COLOR_PANEL, 0.05f);
    SDL_SetRenderDrawColor(state->renderer, fill.r, fill.g, fill.b, 255);
    SDL_RenderFillRect(state->renderer, &rect);
    SDL_SetRenderDrawColor(state->renderer, COLOR_BG.r, COLOR_BG.g, COLOR_BG.b, 255);
    SDL_RenderDrawRect(state->renderer, &rect);

    selected_retained = physics_sim_editor_session_selected_object(&state->session);
    selected_overlay = physics_sim_editor_session_selected_object_overlay(&state->session);
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
        snprintf(line_e,
                 sizeof(line_e),
                 "Vel %.2f, %.2f, %.2f",
                 selected_overlay->initial_velocity.x,
                 selected_overlay->initial_velocity.y,
                 selected_overlay->initial_velocity.z);
    } else {
        line_c[0] = '\0';
        line_e[0] = '\0';
    }

    if (selected_retained->has_plane_primitive) {
        CoreObjectVec3 position = selected_retained->plane_primitive.frame.origin;
        snprintf(line_d,
                 sizeof(line_d),
                 "Size %.2f x %.2f  Pos %.2f, %.2f, %.2f",
                 selected_retained->plane_primitive.width,
                 selected_retained->plane_primitive.height,
                 position.x,
                 position.y,
                 position.z);
    } else if (selected_retained->has_rect_prism_primitive) {
        CoreObjectVec3 position = selected_retained->rect_prism_primitive.frame.origin;
        snprintf(line_d,
                 sizeof(line_d),
                 "Size %.2f x %.2f x %.2f",
                 selected_retained->rect_prism_primitive.width,
                 selected_retained->rect_prism_primitive.height,
                 selected_retained->rect_prism_primitive.depth);
        snprintf(line_e,
                 sizeof(line_e),
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
        snprintf(line_d,
                 sizeof(line_d),
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
    }
}

static void draw_hover_tooltip(SceneEditorState *state) {
    SDL_Rect viewport_rect = {0};
    if (!state || !state->renderer || !state->font_small) return;
    if (state->pointer_x < 0 || state->pointer_y < 0) return;
    viewport_rect = editor_active_viewport_rect(state);
    if (!scene_editor_input_point_in_rect(&viewport_rect, state->pointer_x, state->pointer_y)) return;

    const char *lines[3] = {0};
    char buf1[64];
    char buf2[64];
    char buf3[64];
    int count = 0;

    if (state->hover_emitter >= 0 &&
        state->hover_emitter < (int)state->working.emitter_count) {
        const FluidEmitter *em = &state->working.emitters[state->hover_emitter];
        snprintf(buf1, sizeof(buf1), "%s emitter", emitter_type_name(em->type));
        snprintf(buf2, sizeof(buf2), "Radius %.2f  Strength %.1f", em->radius, em->strength);
        snprintf(buf3, sizeof(buf3), "Pos %.2f, %.2f", em->position_x, em->position_y);
        lines[0] = buf1;
        lines[1] = buf2;
        lines[2] = buf3;
        count = 3;
    } else if (state->hover_object >= 0 &&
               state->hover_object < (int)state->working.object_count) {
        const PresetObject *obj = &state->working.objects[state->hover_object];
        const char *type = (obj->type == PRESET_OBJECT_BOX) ? "Box" : "Circle";
        snprintf(buf1, sizeof(buf1), "Object: %s", type);
        snprintf(buf2, sizeof(buf2), "Size %.2f x %.2f", obj->size_x, obj->size_y);
        snprintf(buf3, sizeof(buf3), "Pos %.2f, %.2f", obj->position_x, obj->position_y);
        lines[0] = buf1;
        lines[1] = buf2;
        lines[2] = buf3;
        count = 3;
        int em_idx = emitter_index_for_object(state, state->hover_object);
        if (em_idx >= 0 && em_idx < (int)state->working.emitter_count) {
            const FluidEmitter *em = &state->working.emitters[em_idx];
            snprintf(buf3, sizeof(buf3), "Emitter: %s (r=%.2f, s=%.1f)",
                     emitter_type_name(em->type), em->radius, em->strength);
            lines[2] = buf3;
            count = 3;
        }
    } else if (state->boundary_mode && state->boundary_hover_edge >= 0 &&
               state->boundary_hover_edge < BOUNDARY_EDGE_COUNT) {
        int edge = state->boundary_hover_edge;
        const BoundaryFlow *flow = &state->working.boundary_flows[edge];
        snprintf(buf1, sizeof(buf1), "Edge: %s", boundary_edge_name(edge));
        snprintf(buf2, sizeof(buf2), "Mode: %s", boundary_mode_label(flow->mode));
        lines[0] = buf1;
        lines[1] = buf2;
        count = 2;
        if (flow->mode != BOUNDARY_FLOW_DISABLED) {
            snprintf(buf3, sizeof(buf3), "Strength %.1f", flow->strength);
            lines[2] = buf3;
            count = 3;
        }
    }

    if (count > 0) {
        scene_editor_canvas_draw_tooltip(state->renderer,
                                         state->font_small,
                                         state->pointer_x,
                                         state->pointer_y,
                                         lines,
                                         count);
    }
}

static void draw_import_list(SceneEditorState *state) {
    if (!state) return;
    refresh_panel_theme();
    int row_h = state->import_view.row_height;
    int x = state->import_rect.x + 6;
    int y_start = state->import_rect.y - (int)editor_list_view_offset(&state->import_view);
    int text_h = panel_font_height(state->renderer, state->font_small, 16);
    if (text_h < 12) text_h = 12;
    SDL_Color text = COLOR_TEXT;
    editor_list_view_set_rows(&state->import_view, state->import_file_count);
    for (int i = 0; i < state->import_file_count; ++i) {
        int y = y_start + i * row_h;
        if (y + row_h < state->import_rect.y || y > state->import_rect.y + state->import_rect.h) continue;
        SDL_Color bg = {0, 0, 0, 0};
        if (i == state->selected_import_row) {
            SDL_Color accent = menu_color_accent();
            bg = (SDL_Color){accent.r, accent.g, accent.b, 80};
        } else if (i == state->hover_import_row) {
            SDL_Color hover = lighten_color(COLOR_PANEL, 0.12f);
            bg = (SDL_Color){hover.r, hover.g, hover.b, 90};
        }
        if (bg.a > 0) {
            SDL_Rect r = {state->import_rect.x + 2, y + 2, state->import_rect.w - 14, row_h - 4};
            SDL_SetRenderDrawColor(state->renderer, bg.r, bg.g, bg.b, bg.a);
            SDL_RenderFillRect(state->renderer, &r);
        }
        const char *label = state->import_files[i][0] ? state->import_files[i] : "(empty)";
        char label_fit[256];
        fit_text_to_width(state->renderer,
                          state->font_small,
                          label,
                          state->import_rect.w - 24,
                          label_fit,
                          sizeof(label_fit));
        draw_text(state->renderer,
                  state->font_small,
                  label_fit,
                  x,
                  y + (row_h - text_h) / 2,
                  text);
    }
    SDL_Color track = COLOR_BG;
    track.a = 180;
    SDL_Color thumb = menu_color_accent();
    thumb.a = 220;
    editor_list_view_draw(state->renderer, &state->import_view, track, thumb);
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
    SDL_Surface *surf = TTF_RenderUTF8_Blended(font, draw_value, COLOR_TEXT);
    int text_w = 0;
    int text_h = 0;
    if (surf) {
        text_w = physics_sim_text_logical_pixels(renderer, surf->w);
        text_h = physics_sim_text_logical_pixels(renderer, surf->h);
        int text_y = rect->y + rect->h / 2 - text_h / 2 + 2;
        SDL_Rect dst = {rect->x + 6, text_y, text_w, text_h};
        VkRendererTexture vk_tex = {0};
        if (vk_renderer_upload_sdl_surface_with_filter((VkRenderer *)renderer,
                                                       surf,
                                                       &vk_tex,
                                                       physics_sim_text_upload_filter(renderer)) == VK_SUCCESS) {
            vk_renderer_draw_texture((VkRenderer *)renderer, &vk_tex, NULL, &dst);
            vk_renderer_queue_texture_destroy((VkRenderer *)renderer, &vk_tex);
        }
        SDL_FreeSurface(surf);
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
    if (!state) return;
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

static void draw_center_pane_summary(SceneEditorState *state) {
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

static void draw_status_card(SceneEditorState *state,
                             int x,
                             int y,
                             int w,
                             int card_bottom_y) {
    char overlay_line[192];
    char save_line[192];
    char overlay_fit[192];
    char save_fit[192];
    SDL_Color overlay_color = COLOR_TEXT_DIM;
    SDL_Color save_color = COLOR_TEXT_DIM;
    SDL_Rect card_rect;
    int line_step = 0;
    int line_h = 0;
    int card_h = 0;
    if (!state || !state->renderer || !physics_sim_editor_session_has_retained_scene(&state->session)) return;

    line_h = panel_font_height(state->renderer, state->font_small, 15);
    if (line_h < 14) line_h = 14;
    line_step = line_h + 8;
    card_h = line_step * 3 + 18;
    if (card_bottom_y > y && card_h > (card_bottom_y - y)) {
        card_h = card_bottom_y - y;
    }
    if (card_h < line_step * 3) {
        card_h = line_step * 3;
    }
    card_rect = (SDL_Rect){x, y, w, card_h};

    SDL_Color card_fill = lighten_color(COLOR_PANEL, 0.05f);
    SDL_SetRenderDrawColor(state->renderer, card_fill.r, card_fill.g, card_fill.b, 255);
    SDL_RenderFillRect(state->renderer, &card_rect);
    SDL_SetRenderDrawColor(state->renderer, COLOR_BG.r, COLOR_BG.g, COLOR_BG.b, 220);
    SDL_RenderDrawRect(state->renderer, &card_rect);

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

    fit_text_to_width(state->renderer, state->font_small, overlay_line, w - 16, overlay_fit, sizeof(overlay_fit));
    fit_text_to_width(state->renderer, state->font_small, save_line, w - 16, save_fit, sizeof(save_fit));

    draw_text(state->renderer, state->font_small, "Status", x + 8, y + 8, COLOR_TEXT);
    draw_text(state->renderer,
              state->font_small,
              overlay_fit[0] ? overlay_fit : overlay_line,
              x + 8,
              y + 8 + line_step,
              overlay_color);
    draw_text(state->renderer,
              state->font_small,
              save_fit[0] ? save_fit : save_line,
              x + 8,
              y + 8 + line_step * 2,
              save_color);
}

static void draw_right_panel_summary(SceneEditorState *state) {
    char selected_line_a[160];
    char selected_line_b[160];
    const CoreSceneObjectContract *selected_retained = NULL;
    const PhysicsSimObjectOverlay *selected_overlay = NULL;
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

    draw_text(state->renderer, state->font_small, selected_line_a, x, line_y, COLOR_TEXT);
    if (selected_line_b[0]) {
        draw_text(state->renderer, state->font_small, selected_line_b, x, line_y + line_step, COLOR_TEXT_DIM);
    }
    summary_bottom = physics_sim_editor_session_has_retained_scene(&state->session)
                         ? state->btn_apply_overlay.rect.y - 12
                         : state->btn_save.rect.y - 12;
    draw_status_card(state, x, line_y + line_step * 2, state->overlay_summary_rect.w, summary_bottom);
}

void scene_editor_panel_draw(SceneEditorState *state) {
    if (!state || !state->renderer) return;
    refresh_panel_theme();
    SDL_Renderer *renderer = state->renderer;
    SDL_SetRenderDrawColor(renderer, COLOR_BG.r, COLOR_BG.g, COLOR_BG.b, 255);
    int win_w = 0;
    int win_h = 0;
    if (state->window) {
        SDL_GetWindowSize(state->window, &win_w, &win_h);
    }
    if (win_w <= 0 || win_h <= 0) {
        win_w = state->right_panel_rect.x + state->right_panel_rect.w;
        win_h = state->right_panel_rect.y + state->right_panel_rect.h;
    }
    SDL_Rect clear_rect = {0, 0, win_w, win_h};
    SDL_RenderFillRect(renderer, &clear_rect);

    SDL_SetRenderDrawColor(renderer, COLOR_PANEL.r, COLOR_PANEL.g, COLOR_PANEL.b, 255);
    SDL_RenderFillRect(renderer, &state->panel_rect);
    SDL_RenderFillRect(renderer, &state->center_pane_rect);
    SDL_RenderFillRect(renderer, &state->right_panel_rect);
    SDL_SetRenderDrawColor(renderer, COLOR_BG.r, COLOR_BG.g, COLOR_BG.b, 200);
    SDL_RenderDrawRect(renderer, &state->panel_rect);
    SDL_RenderDrawRect(renderer, &state->center_pane_rect);
    SDL_RenderDrawRect(renderer, &state->right_panel_rect);

    SDL_Rect draw_surface = scene_editor_draw_surface_rect(state);
    scene_editor_canvas_draw_background(renderer,
                                        draw_surface.x,
                                        draw_surface.y,
                                        draw_surface.w,
                                        draw_surface.h,
                                        false,
                                        0.0f,
                                        0.0f,
                                        !physics_sim_editor_session_has_retained_scene(&state->session));

    const char *title = state->name_edit_ptr
                            ? state->name_edit_ptr
                            : (state->working.name ? state->working.name : "Untitled Preset");
    scene_editor_canvas_draw_name(renderer,
                                  &state->center_name_rect,
                                  state->font_main ? state->font_main : state->font_small,
                                  state->font_small,
                                  title,
                                  state->renaming_name,
                                  &state->name_input);

    SDL_RenderSetClipRect(renderer, &draw_surface);

    if (physics_sim_editor_session_has_retained_scene(&state->session)) {
        scene_editor_canvas_draw_retained_scene(renderer, state);
    } else {
        scene_editor_canvas_draw_boundary_flows(renderer,
                                                state->canvas_x,
                                                state->canvas_y,
                                                state->canvas_width,
                                                state->canvas_height,
                                                state->working.boundary_flows,
                                                state->boundary_hover_edge,
                                                state->boundary_selected_edge,
                                                state->boundary_mode);

        scene_editor_canvas_draw_imports(renderer, state);

        scene_editor_canvas_draw_objects(renderer,
                                         state->canvas_x,
                                         state->canvas_y,
                                         state->canvas_width,
                                         state->canvas_height,
                                         &state->working,
                                         state->selected_object,
                                         state->hover_object,
                                         state->emitter_object_map);

        scene_editor_canvas_draw_emitters(renderer,
                                          state->canvas_x,
                                          state->canvas_y,
                                          state->canvas_width,
                                          state->canvas_height,
                                          &state->working,
                                          state->selected_emitter,
                                          state->hover_emitter,
                                          state->font_small,
                                          state->emitter_object_map,
                                          state->emitter_import_map);
    }

    SDL_RenderSetClipRect(renderer, NULL);

    draw_hover_tooltip(state);
    draw_dimension_fields(state);

    draw_text(renderer, state->font_main,
              "Scene Controls",
              state->panel_rect.x + 12,
              state->panel_rect.y + 12,
              COLOR_TEXT);
    draw_scene_library_summary(state);
    draw_text(renderer, state->font_main,
              "Viewport",
              state->center_title_rect.x,
              state->center_title_rect.y,
              COLOR_TEXT);
    draw_center_pane_summary(state);
    draw_text(renderer, state->font_main,
              "Inspector",
              state->right_panel_rect.x + 12,
              state->right_panel_rect.y + 12,
              COLOR_TEXT);
    draw_text(renderer, state->font_small,
              "Space Mode",
              state->right_panel_rect.x + 12,
              state->right_panel_rect.y + 28,
              COLOR_TEXT_DIM);
    draw_text(renderer, state->font_small,
              editor_space_mode_label(state->cfg.space_mode),
              state->right_panel_rect.x + 12,
              state->right_panel_rect.y + 48,
              COLOR_TEXT);
    draw_text(renderer, state->font_small,
              "Domain",
              state->right_panel_rect.x + 12,
              state->right_panel_rect.y + 78,
              COLOR_TEXT_DIM);

    const FluidEmitter *selected_em = NULL;
    if (state->selected_emitter >= 0 &&
        state->selected_emitter < (int)state->working.emitter_count) {
        selected_em = &state->working.emitters[state->selected_emitter];
    }
    scene_editor_draw_numeric_field(renderer, state->font_small,
                                    &state->radius_field, selected_em);
    scene_editor_draw_numeric_field(renderer, state->font_small,
                                    &state->strength_field, selected_em);
    if (physics_sim_editor_session_has_physics_overlay(&state->session)) {
        int label_h = panel_font_height(renderer, state->font_small, 16);
        draw_text(renderer,
                  state->font_small,
                  "Physics Overlay",
                  state->btn_overlay_dynamic.rect.x,
                  state->btn_overlay_dynamic.rect.y - label_h - 6,
                  COLOR_TEXT_DIM);
        scene_editor_draw_button(renderer, &state->btn_overlay_dynamic, state->font_small);
        scene_editor_draw_button(renderer, &state->btn_overlay_static, state->font_small);
        scene_editor_draw_button(renderer, &state->btn_overlay_vel_x_neg, state->font_small);
        scene_editor_draw_button(renderer, &state->btn_overlay_vel_x_pos, state->font_small);
        scene_editor_draw_button(renderer, &state->btn_overlay_vel_y_neg, state->font_small);
        scene_editor_draw_button(renderer, &state->btn_overlay_vel_y_pos, state->font_small);
        scene_editor_draw_button(renderer, &state->btn_overlay_vel_z_neg, state->font_small);
        scene_editor_draw_button(renderer, &state->btn_overlay_vel_z_pos, state->font_small);
        scene_editor_draw_button(renderer, &state->btn_overlay_vel_reset, state->font_small);
    }

    scene_editor_draw_button(renderer, &state->btn_add_source, state->font_small);
    scene_editor_draw_button(renderer, &state->btn_add_jet, state->font_small);
    scene_editor_draw_button(renderer, &state->btn_add_sink, state->font_small);
    if (state->showing_import_picker) {
        scene_editor_draw_button(renderer, &state->btn_import_back, state->font_small);
    } else {
        scene_editor_draw_button(renderer, &state->btn_add_import, state->font_small);
    }
    scene_editor_draw_button(renderer, &state->btn_boundary, state->font_small);

    SDL_Rect list_rect = state->list_rect;
    SDL_Rect import_rect = state->import_rect;
    SDL_Color list_fill = lighten_color(COLOR_PANEL, 0.04f);
    SDL_SetRenderDrawColor(renderer, list_fill.r, list_fill.g, list_fill.b, 255);
    SDL_RenderFillRect(renderer, &list_rect);
    SDL_SetRenderDrawColor(renderer, COLOR_BG.r, COLOR_BG.g, COLOR_BG.b, 255);
    SDL_RenderDrawRect(renderer, &list_rect);

    if (state->showing_import_picker) {
        SDL_Color import_fill = lighten_color(COLOR_PANEL, 0.08f);
        SDL_SetRenderDrawColor(renderer, import_fill.r, import_fill.g, import_fill.b, 255);
        SDL_RenderFillRect(renderer, &import_rect);
        SDL_SetRenderDrawColor(renderer, COLOR_BG.r, COLOR_BG.g, COLOR_BG.b, 255);
        SDL_RenderDrawRect(renderer, &import_rect);
        {
            int label_h = panel_font_height(renderer, state->font_small, 16);
            draw_text(renderer, state->font_small, "Import files", import_rect.x, import_rect.y - label_h - 6, COLOR_TEXT_DIM);
        }
        draw_import_list(state);
    } else {
        {
            int label_h = panel_font_height(renderer, state->font_small, 16);
            draw_text(renderer,
                      state->font_small,
                      physics_sim_editor_session_has_retained_scene(&state->session) ? "3D Scene Objects (Read-Only)" : "2D Preset Objects",
                      list_rect.x,
                      list_rect.y - label_h - 6,
                      COLOR_TEXT_DIM);
        }
        if (physics_sim_editor_session_has_retained_scene(&state->session)) {
            draw_retained_object_list(state);
            draw_left_object_info_card(state);
        } else {
            draw_object_list(state);
        }
    }

    draw_right_panel_summary(state);
    if (physics_sim_editor_session_has_retained_scene(&state->session)) {
        scene_editor_draw_button(renderer, &state->btn_apply_overlay, state->font_small);
    }
    scene_editor_draw_button(renderer, &state->btn_save, state->font_small);
    scene_editor_draw_button(renderer, &state->btn_cancel, state->font_small);
}
