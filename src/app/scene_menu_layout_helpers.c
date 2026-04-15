#include "app/scene_menu_layout_helpers.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include <string.h>

#include "render/text_upload_policy.h"

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
    int control_h = 40;
    int compact_h = 34;
    int left_margin = 28;
    int right_margin = 28;
    int list_top = PRESET_LIST_MARGIN_Y;
    int panel_x = 420;
    int panel_w = 360;
    int panel_y = PRESET_LIST_MARGIN_Y;
    int panel_h = 320;
    int ui_gap = 12;
    int config_pad = 12;
    int icon_w = 36;
    int action_w = 190;
    int section_gap = 12;
    int output_button_w = 84;
    int output_buttons_total_w = 0;
    int list_w = 0;
    int footer_y = 0;
    int top_controls_y = 0;
    int top_controls_gap = 8;
    int top_controls_w = 0;
    int top_hint_h = 0;
    int io_rows_h = 0;
    int io_panel_h = 0;
    int io_panel_y = 0;
    int row_y = 0;
    int row_gap = 8;
    if (!ctx) return;

    title_h = scene_menu_font_height(ctx->renderer, ctx->font_title, 32);
    body_h = scene_menu_font_height(ctx->renderer, ctx->font, 22);
    small_h = scene_menu_font_height(ctx->renderer, ctx->font_small ? ctx->font_small : ctx->font, 18);
    control_h = body_h + 16;
    if (control_h < 38) control_h = 38;
    compact_h = small_h + 14;
    if (compact_h < 32) compact_h = 32;
    icon_w = compact_h;
    section_gap = small_h / 2 + 8;
    if (section_gap < 10) section_gap = 10;

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

    action_w = (panel_w - ui_gap) / 2;
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
    if (ctx->edit_button.rect.x < panel_x) {
        ctx->edit_button.rect.x = panel_x;
        ctx->edit_button.rect.w = ctx->start_button.rect.x - ui_gap - ctx->edit_button.rect.x;
        if (ctx->edit_button.rect.w < 110) ctx->edit_button.rect.w = 110;
    }

    ctx->quit_button.rect.w = 120;
    ctx->quit_button.rect.h = compact_h;
    ctx->quit_button.rect.x = left_margin;
    ctx->quit_button.rect.y = footer_y + (control_h - compact_h) / 2;

    io_rows_h = compact_h * 6 + row_gap * 5;
    io_panel_h = small_h + 8 + io_rows_h + config_pad * 2;
    io_panel_y = ctx->start_button.rect.y - io_panel_h - 12;
    if (io_panel_y < list_top + compact_h + top_hint_h + 56) {
        io_panel_y = list_top + compact_h + top_hint_h + 56;
    }
    row_y = io_panel_y + config_pad + small_h + 8;

    ctx->output_root_rect = (SDL_Rect){
        panel_x + config_pad,
        row_y,
        panel_w - config_pad * 2,
        compact_h
    };
    row_y += compact_h + row_gap;
    ctx->input_root_rect = (SDL_Rect){
        ctx->output_root_rect.x,
        row_y,
        ctx->output_root_rect.w,
        compact_h
    };
    row_y += compact_h + row_gap;
    ctx->inflow_rect = (SDL_Rect){
        ctx->output_root_rect.x,
        row_y,
        ctx->output_root_rect.w,
        compact_h
    };
    row_y += compact_h + row_gap;
    ctx->viscosity_rect = (SDL_Rect){
        ctx->output_root_rect.x,
        row_y,
        ctx->output_root_rect.w,
        compact_h
    };
    row_y += compact_h + row_gap;
    ctx->headless_frames_rect = (SDL_Rect){
        ctx->output_root_rect.x,
        row_y,
        ctx->output_root_rect.w,
        compact_h
    };
    row_y += compact_h + row_gap;
    ctx->headless_toggle_button.rect = (SDL_Rect){
        ctx->output_root_rect.x,
        row_y,
        ctx->output_root_rect.w,
        compact_h
    };

    panel_y = list_top + compact_h + top_hint_h + 16;
    panel_h = io_panel_y - panel_y - 12;
    if (panel_h < 180) panel_h = 180;
    ctx->config_panel_rect = (SDL_Rect){panel_x, panel_y, panel_w, panel_h};

    ctx->grid_dec_button.rect = (SDL_Rect){
        panel_x + panel_w - config_pad - icon_w * 2 - 8,
        panel_y + config_pad + small_h + 6,
        icon_w,
        compact_h
    };
    ctx->grid_inc_button.rect = (SDL_Rect){
        ctx->grid_dec_button.rect.x + icon_w + 8,
        ctx->grid_dec_button.rect.y,
        icon_w,
        compact_h
    };

    ctx->quality_prev_button.rect = (SDL_Rect){
        panel_x + config_pad,
        ctx->grid_dec_button.rect.y + compact_h + section_gap,
        icon_w,
        compact_h
    };
    ctx->quality_next_button.rect = (SDL_Rect){
        panel_x + panel_w - config_pad - icon_w,
        ctx->quality_prev_button.rect.y,
        icon_w,
        compact_h
    };

    ctx->volume_toggle_rect = (SDL_Rect){
        panel_x + config_pad,
        ctx->quality_prev_button.rect.y + compact_h + section_gap,
        panel_w - config_pad * 2,
        compact_h
    };
    ctx->render_toggle_rect = (SDL_Rect){
        panel_x + config_pad,
        ctx->volume_toggle_rect.y + compact_h + 8,
        panel_w - config_pad * 2,
        compact_h
    };

    output_buttons_total_w = output_button_w * 2 + 8;
    if (output_buttons_total_w > ctx->output_root_rect.w - 40) {
        output_button_w = (ctx->output_root_rect.w - 48) / 2;
        if (output_button_w < 56) output_button_w = 56;
        output_buttons_total_w = output_button_w * 2 + 8;
    }
    ctx->output_root_folder_button.rect = (SDL_Rect){
        ctx->output_root_rect.x + ctx->output_root_rect.w - output_button_w,
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

    {
        int controls_bottom = ctx->render_toggle_rect.y + ctx->render_toggle_rect.h + config_pad;
        int needed_h = controls_bottom - panel_y;
        int max_h = io_panel_y - panel_y - 12;
        if (max_h < 120) max_h = 120;
        if (panel_h < needed_h) panel_h = needed_h;
        if (panel_h > max_h) panel_h = max_h;
        if (panel_h < 120) panel_h = 120;
        ctx->config_panel_rect.h = panel_h;
    }
}
