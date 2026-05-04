#include "app/menu/workspace_authoring/physics_sim_workspace_authoring_overlay.h"

#include <stdio.h>

#include "app/menu/menu_render.h"
#include "app/menu/workspace_authoring/physics_sim_workspace_authoring_overlay_model.h"
#include "kit_workspace_authoring_ui.h"

enum {
    PHYSICS_SIM_AUTHORING_PANE_ROW_CAP = 3
};

static SDL_Rect physics_sim_authoring_sdl_rect(CorePaneRect rect) {
    SDL_Rect out;
    out.x = (int)rect.x;
    out.y = (int)rect.y;
    out.w = (int)rect.width;
    out.h = (int)rect.height;
    if (out.w < 0) out.w = 0;
    if (out.h < 0) out.h = 0;
    return out;
}

static SDL_Color physics_sim_authoring_alpha(SDL_Color color, Uint8 alpha) {
    color.a = alpha;
    return color;
}

static void physics_sim_authoring_draw_button(
    SDL_Renderer *renderer,
    TTF_Font *font,
    const KitWorkspaceAuthoringOverlayButton *button) {
    SDL_Rect rect;
    SDL_Color fill;
    SDL_Color border;
    if (!renderer || !font || !button || !button->visible) return;
    rect = physics_sim_authoring_sdl_rect(button->rect);
    fill = physics_sim_authoring_alpha(menu_color_button_bg(), button->enabled ? 238u : 140u);
    border = physics_sim_authoring_alpha(menu_color_accent(), button->enabled ? 245u : 150u);
    SDL_SetRenderDrawColor(renderer, fill.r, fill.g, fill.b, fill.a);
    SDL_RenderFillRect(renderer, &rect);
    SDL_SetRenderDrawColor(renderer, border.r, border.g, border.b, border.a);
    SDL_RenderDrawRect(renderer, &rect);
    menu_draw_text(renderer,
                   font,
                   button->label ? button->label : "",
                   rect.x + 8,
                   rect.y + 5,
                   menu_color_text());
}

static void physics_sim_authoring_draw_controls(SceneMenuInteraction *ctx, int width) {
    KitWorkspaceAuthoringOverlayButton buttons[4];
    uint32_t count = 0u;
    uint32_t i = 0u;
    if (!ctx || !ctx->renderer || width <= 0) return;
    count = kit_workspace_authoring_ui_build_overlay_buttons(
        width,
        physics_sim_workspace_authoring_host_active(&ctx->workspace_authoring),
        physics_sim_workspace_authoring_host_pane_overlay_active(&ctx->workspace_authoring),
        buttons,
        (uint32_t)(sizeof(buttons) / sizeof(buttons[0])));
    for (i = 0u; i < count; ++i) {
        physics_sim_authoring_draw_button(ctx->renderer,
                                          ctx->font_small ? ctx->font_small : ctx->font,
                                          &buttons[i]);
    }
}

static void physics_sim_authoring_draw_pane_rows(SceneMenuInteraction *ctx,
                                                 int width,
                                                 int height) {
    PhysicsSimWorkspaceAuthoringPaneRow rows[PHYSICS_SIM_AUTHORING_PANE_ROW_CAP];
    uint32_t count = 0u;
    uint32_t i = 0u;
    SDL_Color pane_border = physics_sim_authoring_alpha(menu_color_accent(), 210u);
    SDL_Color label_fill = physics_sim_authoring_alpha(menu_color_panel(), 232u);
    SDL_Color text = menu_color_text();
    SDL_Color muted = menu_color_text_dim();
    TTF_Font *font = ctx->font_small ? ctx->font_small : ctx->font;
    if (!ctx || !ctx->renderer || width <= 0 || height <= 0) return;

    count = physics_sim_workspace_authoring_overlay_build_pane_rows(
        width,
        height,
        rows,
        (uint32_t)(sizeof(rows) / sizeof(rows[0])));
    for (i = 0u; i < count; ++i) {
        SDL_Rect pane_rect = physics_sim_authoring_sdl_rect(rows[i].pane_rect);
        SDL_Rect label_rect;
        char detail[192];
        if (pane_rect.w <= 0 || pane_rect.h <= 0) continue;

        SDL_SetRenderDrawColor(ctx->renderer,
                               pane_border.r,
                               pane_border.g,
                               pane_border.b,
                               pane_border.a);
        SDL_RenderDrawRect(ctx->renderer, &pane_rect);
        label_rect = (SDL_Rect){ pane_rect.x + 8, pane_rect.y + 8, 240, 24 };
        if (label_rect.x + label_rect.w > pane_rect.x + pane_rect.w - 8) {
            label_rect.w = (pane_rect.x + pane_rect.w - 8) - label_rect.x;
        }
        if (label_rect.w < 96) label_rect.w = pane_rect.w - 16;
        if (label_rect.w > 0) {
            SDL_SetRenderDrawColor(ctx->renderer,
                                   label_fill.r,
                                   label_fill.g,
                                   label_fill.b,
                                   label_fill.a);
            SDL_RenderFillRect(ctx->renderer, &label_rect);
            SDL_SetRenderDrawColor(ctx->renderer,
                                   pane_border.r,
                                   pane_border.g,
                                   pane_border.b,
                                   pane_border.a);
            SDL_RenderDrawRect(ctx->renderer, &label_rect);
            menu_draw_text(ctx->renderer,
                           font,
                           rows[i].pane_label ? rows[i].pane_label : "Pane",
                           label_rect.x + 6,
                           label_rect.y + 4,
                           text);
        }

        if (pane_rect.w > 260 && pane_rect.h > 96) {
            snprintf(detail,
                     sizeof(detail),
                     "%s / %s",
                     rows[i].module_key ? rows[i].module_key : "unbound",
                     rows[i].module_label ? rows[i].module_label : "Unbound");
            menu_draw_text(ctx->renderer,
                           font,
                           detail,
                           pane_rect.x + 14,
                           pane_rect.y + 42,
                           muted);
        }
    }
}

void physics_sim_workspace_authoring_overlay_draw(SceneMenuInteraction *ctx,
                                                  int width,
                                                  int height) {
    if (!ctx || !ctx->renderer ||
        !physics_sim_workspace_authoring_host_active(&ctx->workspace_authoring)) {
        return;
    }

    if (physics_sim_workspace_authoring_host_pane_overlay_active(&ctx->workspace_authoring)) {
        physics_sim_authoring_draw_pane_rows(ctx, width, height);
    }
    physics_sim_authoring_draw_controls(ctx, width);
}
