#include "app/scene_menu_layout_helpers.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include <string.h>

#include "app/menu/menu_settings_layout.h"
#include "app/menu/menu_state.h"
#include "render/text_upload_policy.h"

static bool rect_has_area(SDL_Rect rect) {
    return rect.w > 0 && rect.h > 0;
}

static bool rect_contains_rect(SDL_Rect outer, SDL_Rect inner) {
    if (!rect_has_area(outer) || !rect_has_area(inner)) return false;
    return inner.x >= outer.x &&
           inner.y >= outer.y &&
           inner.x + inner.w <= outer.x + outer.w &&
           inner.y + inner.h <= outer.y + outer.h;
}

static bool rects_overlap(SDL_Rect a, SDL_Rect b) {
    if (!rect_has_area(a) || !rect_has_area(b)) return false;
    return a.x < b.x + b.w &&
           a.x + a.w > b.x &&
           a.y < b.y + b.h &&
           a.y + a.h > b.y;
}

static bool layout_uses_scene_project_cache_panel(const SceneMenuInteraction *ctx) {
    return ctx && ctx->scene_project_cache_status.is_scene_project;
}

static void layout_error(char *error, size_t error_size, const char *message) {
    if (!error || error_size == 0u) return;
    snprintf(error, error_size, "%s", message ? message : "layout error");
}

int scene_menu_font_height(SDL_Renderer *renderer, TTF_Font *font, int fallback) {
    if (!font) return fallback;
    {
        int h = TTF_FontHeight(font);
        if (h > 0) {
            return physics_sim_text_logical_pixels(renderer, h);
        }
    }
    return fallback;
}

void scene_menu_fit_text_to_width(SDL_Renderer *renderer,
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

void scene_menu_update_dynamic_layout(SceneMenuInteraction *ctx,
                                      int win_w,
                                      int win_h) {
    int title_h = 32;
    int body_h = 22;
    int small_h = 18;
    int control_h = 38;
    int compact_h = 30;
    int left_margin = 28;
    int right_margin = 28;
    int list_top = PRESET_LIST_MARGIN_Y;
    int panel_x = 420;
    int panel_w = 360;
    int panel_y = PRESET_LIST_MARGIN_Y;
    int panel_h = 320;
    int ui_gap = 10;
    int config_pad = 10;
    int action_w = 190;
    int output_button_w = 84;
    int output_buttons_total_w = 0;
    int list_w = 0;
    int footer_y = 0;
    int top_controls_y = 0;
    int top_controls_gap = 8;
    int top_controls_w = 0;
    bool retained_catalog = false;
    int top_hint_h = 0;
    int io_rows_h = 0;
    int io_panel_h = 0;
    int io_panel_y = 0;
    int io_header_h = 0;
    int row_y = 0;
    int row_gap = 6;
    int io_row_count = 4;
    int cache_button_gap = 8;
    int cache_button_w = 0;
    int headless_button_w = 0;
    int frames_w = 0;
    bool show_warm_start = false;
    bool scene_project_mode = false;
    bool compact_cache_actions = false;
    if (!ctx) return;
    retained_catalog = menu_showing_retained_catalog(ctx);
    show_warm_start = menu_warm_start_controls_visible(ctx);
    scene_project_mode = layout_uses_scene_project_cache_panel(ctx);

    title_h = scene_menu_font_height(ctx->renderer, ctx->font_title, 32);
    body_h = scene_menu_font_height(ctx->renderer, ctx->font, 22);
    small_h = scene_menu_font_height(ctx->renderer, ctx->font_small ? ctx->font_small : ctx->font, 18);
    control_h = body_h + 12;
    if (control_h < 34) control_h = 34;
    compact_h = small_h + 10;
    if (compact_h < 28) compact_h = 28;
    list_top = 34 + title_h + 20;
    if (list_top < 82) list_top = 82;
    if (list_top > win_h - 280) list_top = win_h - 280;

    list_w = (win_w * 40) / 100;
    if (list_w < 360) list_w = 360;
    if (list_w > 460) list_w = 460;
    if (list_w > win_w - left_margin - right_margin - 340) {
        list_w = win_w - left_margin - right_margin - 340;
    }
    if (list_w < 320) list_w = 320;

    footer_y = win_h - control_h - 20;

    ctx->list_rect.x = left_margin;
    ctx->list_rect.y = list_top;
    ctx->list_rect.w = list_w;
    ctx->list_rect.h = footer_y - list_top - 16;
    if (ctx->list_rect.h < 220) ctx->list_rect.h = 220;

    panel_x = ctx->list_rect.x + ctx->list_rect.w + 20;
    panel_w = win_w - panel_x - right_margin;
    if (panel_w < 330) {
        panel_w = 330;
        panel_x = win_w - right_margin - panel_w;
    }
    if (panel_x < ctx->list_rect.x + ctx->list_rect.w + 8) {
        panel_x = ctx->list_rect.x + ctx->list_rect.w + 8;
    }

    top_controls_y = list_top;
    top_hint_h = small_h + 8;
    top_controls_w = (panel_w - top_controls_gap) / 2;
    if (top_controls_w < 140) top_controls_w = 140;
    ctx->mode_toggle_button.rect.x = panel_x;
    ctx->mode_toggle_button.rect.y = top_controls_y;
    ctx->mode_toggle_button.rect.w = top_controls_w;
    ctx->mode_toggle_button.rect.h = compact_h;

    ctx->space_toggle_button.rect.x = ctx->mode_toggle_button.rect.x +
                                      ctx->mode_toggle_button.rect.w +
                                      top_controls_gap;
    ctx->space_toggle_button.rect.y = top_controls_y;
    ctx->space_toggle_button.rect.w = panel_x + panel_w - ctx->space_toggle_button.rect.x;
    ctx->space_toggle_button.rect.h = compact_h;

    action_w = retained_catalog ? (panel_w - ui_gap * 2) / 3 : (panel_w - ui_gap) / 2;
    if (action_w < 130) action_w = 130;
    if (action_w > 220) action_w = 220;
    ctx->start_button.rect.w = action_w;
    ctx->start_button.rect.h = control_h;
    ctx->start_button.rect.x = panel_x + panel_w - ctx->start_button.rect.w;
    ctx->start_button.rect.y = footer_y;

    ctx->edit_button.rect.w = action_w;
    ctx->edit_button.rect.h = control_h;
    ctx->edit_button.rect.x = ctx->start_button.rect.x - ctx->edit_button.rect.w - ui_gap;
    ctx->edit_button.rect.y = ctx->start_button.rect.y;
    if (!retained_catalog && ctx->edit_button.rect.x < panel_x) {
        ctx->edit_button.rect.x = panel_x;
        ctx->edit_button.rect.w = ctx->start_button.rect.x - ui_gap - ctx->edit_button.rect.x;
        if (ctx->edit_button.rect.w < 110) ctx->edit_button.rect.w = 110;
    }

    ctx->duplicate_button.rect.w = action_w;
    ctx->duplicate_button.rect.h = control_h;
    ctx->duplicate_button.rect.x = ctx->edit_button.rect.x - ctx->duplicate_button.rect.w - ui_gap;
    ctx->duplicate_button.rect.y = ctx->start_button.rect.y;
    if (!retained_catalog) {
        ctx->duplicate_button.rect.w = 0;
        ctx->duplicate_button.rect.h = 0;
    } else if (ctx->duplicate_button.rect.x < panel_x) {
        ctx->duplicate_button.rect.x = panel_x;
    }

    ctx->quit_button.rect.w = 120;
    ctx->quit_button.rect.h = compact_h;
    ctx->quit_button.rect.x = left_margin;
    ctx->quit_button.rect.y = footer_y + (control_h - compact_h) / 2;

    panel_y = list_top + compact_h + top_hint_h + 12;
    ctx->config_panel_rect = (SDL_Rect){panel_x, panel_y, panel_w, 0};
    panel_h = menu_settings_layout_panel_height(ctx);
    ctx->config_panel_rect.h = panel_h;
    menu_settings_layout_toggle_rects(ctx,
                                      &ctx->volume_toggle_rect,
                                      &ctx->render_toggle_rect,
                                      &ctx->blur_toggle_rect,
                                      &ctx->atmospheric_initial_state_toggle_rect);
    menu_settings_layout_action_rects(ctx,
                                      &ctx->settings_apply_button.rect,
                                      &ctx->settings_save_button.rect,
                                      &ctx->settings_reset_button.rect,
                                      &ctx->settings_restore_saved_button.rect,
                                      &ctx->settings_defaults_button.rect);
    ctx->grid_dec_button.rect = (SDL_Rect){0, 0, 0, 0};
    ctx->grid_inc_button.rect = (SDL_Rect){0, 0, 0, 0};
    ctx->substeps_dec_button.rect = (SDL_Rect){0, 0, 0, 0};
    ctx->substeps_inc_button.rect = (SDL_Rect){0, 0, 0, 0};
    ctx->solver_dec_button.rect = (SDL_Rect){0, 0, 0, 0};
    ctx->solver_inc_button.rect = (SDL_Rect){0, 0, 0, 0};
    ctx->quality_prev_button.rect = (SDL_Rect){0, 0, 0, 0};
    ctx->quality_next_button.rect = (SDL_Rect){0, 0, 0, 0};
    ctx->inflow_rect = (SDL_Rect){0, 0, 0, 0};
    ctx->viscosity_rect = (SDL_Rect){0, 0, 0, 0};

    compact_cache_actions = scene_project_mode &&
                            panel_w - config_pad * 2 < 560;
    io_row_count = scene_project_mode ? 4 : (show_warm_start ? 4 : 3);
    io_rows_h = compact_h * io_row_count + row_gap * (io_row_count - 1);
    io_header_h = scene_project_mode ? small_h + 8 : small_h * 2 + 12;
    io_panel_h = io_header_h + io_rows_h + config_pad * 2;
    io_panel_y = ctx->start_button.rect.y - io_panel_h - 10;
    if (io_panel_y < ctx->config_panel_rect.y + ctx->config_panel_rect.h + 10) {
        io_panel_y = ctx->config_panel_rect.y + ctx->config_panel_rect.h + 10;
    }
    ctx->data_io_panel_rect = (SDL_Rect){
        panel_x,
        io_panel_y,
        panel_w,
        io_panel_h
    };
    row_y = io_panel_y + config_pad + io_header_h;

    ctx->scene_project_root_rect = (SDL_Rect){0, 0, 0, 0};
    ctx->scene_project_cache_target_rect = (SDL_Rect){0, 0, 0, 0};
    ctx->scene_project_cache_status_rect = (SDL_Rect){0, 0, 0, 0};

    ctx->output_root_rect = (SDL_Rect){
        panel_x + config_pad,
        row_y,
        panel_w - config_pad * 2,
        compact_h
    };
    if (scene_project_mode) {
        ctx->scene_project_root_rect = ctx->output_root_rect;
        ctx->output_root_rect = (SDL_Rect){0, 0, 0, 0};
        ctx->output_root_edit_button.rect = (SDL_Rect){0, 0, 0, 0};
        ctx->output_root_folder_button.rect = (SDL_Rect){0, 0, 0, 0};
        ctx->output_root_show_button.rect = (SDL_Rect){0, 0, 0, 0};
        row_y += compact_h + row_gap;
        ctx->scene_project_cache_target_rect = (SDL_Rect){
            panel_x + config_pad,
            row_y,
            panel_w - config_pad * 2,
            compact_h
        };
        row_y += compact_h + row_gap;
        ctx->scene_project_cache_status_rect = (SDL_Rect){
            panel_x + config_pad,
            row_y,
            panel_w - config_pad * 2,
            compact_h
        };
        row_y += compact_h + row_gap;
        ctx->input_root_rect = (SDL_Rect){0, 0, 0, 0};
        ctx->input_root_edit_button.rect = (SDL_Rect){0, 0, 0, 0};
        ctx->input_root_folder_button.rect = (SDL_Rect){0, 0, 0, 0};
        ctx->warm_start_rect = (SDL_Rect){0, 0, 0, 0};
        ctx->warm_start_edit_button.rect = (SDL_Rect){0, 0, 0, 0};
        ctx->warm_start_file_button.rect = (SDL_Rect){0, 0, 0, 0};
    } else {
        row_y += compact_h + row_gap;
        ctx->input_root_rect = (SDL_Rect){
            ctx->output_root_rect.x,
            row_y,
            ctx->output_root_rect.w,
            compact_h
        };
        row_y += compact_h + row_gap;
        if (show_warm_start) {
            ctx->warm_start_rect = (SDL_Rect){
                ctx->output_root_rect.x,
                row_y,
                ctx->output_root_rect.w,
                compact_h
            };
            row_y += compact_h + row_gap;
        } else {
            ctx->warm_start_rect = (SDL_Rect){0, 0, 0, 0};
            ctx->warm_start_edit_button.rect = (SDL_Rect){0, 0, 0, 0};
            ctx->warm_start_file_button.rect = (SDL_Rect){0, 0, 0, 0};
        }
    }
    {
        int action_row_w = scene_project_mode
                               ? ctx->scene_project_root_rect.w
                               : ctx->output_root_rect.w;
        int action_row_x = scene_project_mode
                               ? ctx->scene_project_root_rect.x
                               : ctx->output_root_rect.x;
        frames_w = action_row_w / 4;
    if (compact_cache_actions) {
        frames_w = 96;
    } else if (frames_w < 150) {
        frames_w = 150;
    }
    if (frames_w > 220) frames_w = 220;
    cache_button_w = scene_project_mode
                         ? (action_row_w - frames_w - cache_button_gap * 3) / 5
                         : 0;
    if (cache_button_w < (compact_cache_actions ? 112 : 120)) {
        cache_button_w = compact_cache_actions ? 112 : 120;
    }
    if (cache_button_w > 180) cache_button_w = 180;
    if (!scene_project_mode) {
        cache_button_w = 0;
        cache_button_gap = 0;
    }
    headless_button_w = action_row_w - frames_w - cache_button_w * 2 - cache_button_gap * 3;
    if (headless_button_w < 150) {
        frames_w = action_row_w;
        headless_button_w = action_row_w;
        cache_button_w = 0;
        cache_button_gap = 0;
    }
    ctx->headless_frames_rect = (SDL_Rect){
        action_row_x,
        row_y,
        frames_w,
        compact_h
    };
    }
    ctx->headless_toggle_button.rect = (SDL_Rect){
        ctx->headless_frames_rect.x + ctx->headless_frames_rect.w + cache_button_gap,
        row_y,
        headless_button_w,
        compact_h
    };
    if (cache_button_w > 0) {
        ctx->cache_command_button.rect = (SDL_Rect){
            ctx->headless_toggle_button.rect.x + ctx->headless_toggle_button.rect.w + cache_button_gap,
            ctx->headless_toggle_button.rect.y,
            cache_button_w,
            ctx->headless_toggle_button.rect.h
        };
        ctx->cache_show_button.rect = (SDL_Rect){
            ctx->cache_command_button.rect.x + ctx->cache_command_button.rect.w + cache_button_gap,
            ctx->cache_command_button.rect.y,
            cache_button_w,
            ctx->cache_command_button.rect.h
        };
    } else {
        ctx->cache_command_button.rect = (SDL_Rect){0, 0, 0, 0};
        ctx->cache_show_button.rect = (SDL_Rect){0, 0, 0, 0};
    }

    output_buttons_total_w = output_button_w * 3 + 16;
    if (output_buttons_total_w > ctx->output_root_rect.w - 40) {
        output_button_w = (ctx->output_root_rect.w - 56) / 3;
        if (output_button_w < 56) output_button_w = 56;
        output_buttons_total_w = output_button_w * 3 + 16;
    }
    if (!scene_project_mode) {
        ctx->output_root_show_button.rect = (SDL_Rect){
            ctx->output_root_rect.x + ctx->output_root_rect.w - output_button_w,
            ctx->output_root_rect.y,
            output_button_w,
            ctx->output_root_rect.h
        };
        ctx->output_root_folder_button.rect = (SDL_Rect){
            ctx->output_root_show_button.rect.x - output_button_w - 8,
            ctx->output_root_rect.y,
            output_button_w,
            ctx->output_root_rect.h
        };
        ctx->output_root_edit_button.rect = (SDL_Rect){
            ctx->output_root_folder_button.rect.x - output_button_w - 8,
            ctx->output_root_rect.y,
            output_button_w,
            ctx->output_root_rect.h
        };
        ctx->input_root_folder_button.rect = (SDL_Rect){
            ctx->input_root_rect.x + ctx->input_root_rect.w - output_button_w,
            ctx->input_root_rect.y,
            output_button_w,
            ctx->input_root_rect.h
        };
        ctx->input_root_edit_button.rect = (SDL_Rect){
            ctx->input_root_folder_button.rect.x - output_button_w - 8,
            ctx->input_root_rect.y,
            output_button_w,
            ctx->input_root_rect.h
        };
    }
    if (!scene_project_mode && show_warm_start) {
        ctx->warm_start_file_button.rect = (SDL_Rect){
            ctx->warm_start_rect.x + ctx->warm_start_rect.w - output_button_w,
            ctx->warm_start_rect.y,
            output_button_w,
            ctx->warm_start_rect.h
        };
        ctx->warm_start_edit_button.rect = (SDL_Rect){
            ctx->warm_start_file_button.rect.x - output_button_w - 8,
            ctx->warm_start_rect.y,
            output_button_w,
            ctx->warm_start_rect.h
        };
    }
}

bool scene_menu_layout_validate_no_overlap(const SceneMenuInteraction *ctx,
                                           char *error,
                                           size_t error_size) {
    const SDL_Rect *io_children[] = {
        &ctx->output_root_rect,
        &ctx->input_root_rect,
        &ctx->scene_project_root_rect,
        &ctx->scene_project_cache_target_rect,
        &ctx->scene_project_cache_status_rect,
        &ctx->warm_start_rect,
        &ctx->headless_frames_rect,
        &ctx->headless_toggle_button.rect,
        &ctx->cache_command_button.rect,
        &ctx->cache_show_button.rect
    };
    const SDL_Rect *bottom_actions[] = {
        &ctx->duplicate_button.rect,
        &ctx->edit_button.rect,
        &ctx->start_button.rect
    };
    if (!ctx) {
        layout_error(error, error_size, "missing layout context");
        return false;
    }
    if (!rect_has_area(ctx->config_panel_rect) || !rect_has_area(ctx->data_io_panel_rect)) {
        layout_error(error, error_size, "missing primary panel rect");
        return false;
    }
    if (rects_overlap(ctx->config_panel_rect, ctx->data_io_panel_rect)) {
        layout_error(error, error_size, "settings panel overlaps data io panel");
        return false;
    }
    if (rects_overlap(ctx->data_io_panel_rect, ctx->start_button.rect) ||
        rects_overlap(ctx->data_io_panel_rect, ctx->edit_button.rect) ||
        rects_overlap(ctx->data_io_panel_rect, ctx->duplicate_button.rect)) {
        layout_error(error, error_size, "data io panel overlaps bottom actions");
        return false;
    }
    for (size_t i = 0u; i < sizeof(io_children) / sizeof(io_children[0]); ++i) {
        if (!rect_has_area(*io_children[i])) continue;
        if (!rect_contains_rect(ctx->data_io_panel_rect, *io_children[i])) {
            layout_error(error, error_size, "data io control escapes panel");
            return false;
        }
    }
    for (size_t i = 0u; i < sizeof(io_children) / sizeof(io_children[0]); ++i) {
        if (!rect_has_area(*io_children[i])) continue;
        for (size_t j = i + 1u; j < sizeof(io_children) / sizeof(io_children[0]); ++j) {
            if (!rect_has_area(*io_children[j])) continue;
            if (rects_overlap(*io_children[i], *io_children[j])) {
                layout_error(error, error_size, "data io controls overlap");
                return false;
            }
        }
    }
    for (size_t i = 0u; i < sizeof(bottom_actions) / sizeof(bottom_actions[0]); ++i) {
        if (!rect_has_area(*bottom_actions[i])) continue;
        for (size_t j = i + 1u; j < sizeof(bottom_actions) / sizeof(bottom_actions[0]); ++j) {
            if (!rect_has_area(*bottom_actions[j])) continue;
            if (rects_overlap(*bottom_actions[i], *bottom_actions[j])) {
                layout_error(error, error_size, "bottom actions overlap");
                return false;
            }
        }
    }
    if (rects_overlap(ctx->cache_command_button.rect, ctx->duplicate_button.rect) ||
        rects_overlap(ctx->cache_command_button.rect, ctx->edit_button.rect) ||
        rects_overlap(ctx->cache_command_button.rect, ctx->start_button.rect)) {
        layout_error(error, error_size, "cache command overlaps bottom actions");
        return false;
    }
    if (rects_overlap(ctx->cache_show_button.rect, ctx->duplicate_button.rect) ||
        rects_overlap(ctx->cache_show_button.rect, ctx->edit_button.rect) ||
        rects_overlap(ctx->cache_show_button.rect, ctx->start_button.rect)) {
        layout_error(error, error_size, "cache show overlaps bottom actions");
        return false;
    }
    if (error && error_size > 0u) error[0] = '\0';
    return true;
}
