#include "app/menu/menu_text_presentation.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "app/menu/menu_types.h"
#include "app/menu/menu_render.h"
#include "render/text_draw.h"

typedef struct MenuTextPaint {
    SDL_Renderer *renderer;
    TTF_Font *font;
    SDL_Color color;
} MenuTextPaint;
static CoreResult measure(void *user, const char *run, float *width) {
    MenuTextPaint *paint = user;
    int w = 0;
    if (!physics_sim_text_measure_utf8(paint->renderer, paint->font, run, &w, NULL))
        return (CoreResult){CORE_ERR_INVALID_ARG, "menu label measurement failed"};
    *width = w;
    return core_result_ok();
}
static CoreResult paint_text(void *user, KitRenderVec2 origin, const char *run) {
    MenuTextPaint *paint = user;
    if (!run[0]) return core_result_ok();
    return physics_sim_text_draw_utf8_at(paint->renderer, paint->font, run,
        (int)origin.x, (int)origin.y, paint->color) ? core_result_ok() :
        (CoreResult){CORE_ERR_INVALID_ARG, "menu label paint failed"};
}
static CoreResult no_rect(void *user, KitRenderRect rect, KitRenderColor color) {
    (void)user; (void)rect; (void)color;
    return core_result_ok();
}
void menu_draw_contained_text(SDL_Renderer *renderer, TTF_Font *font, const char *text,
    const SDL_Rect *rect, SDL_Color color, int alignment) {
    if (!renderer || !font || !text || !rect || rect->w <= 0 || rect->h <= 0) return;
    char fitted[KIT_UI_TEXT_PRESENTATION_BYTES];
    MenuTextPaint paint = {renderer, font, color};
    if (menu_text_fit_measured(text, rect->w, fitted, sizeof(fitted), measure, &paint).code != CORE_OK)
        return;
    int h = 12;
    (void)physics_sim_text_measure_utf8(renderer, font, "Mg", NULL, &h);
    KitUiTextPresentation view;
    if (menu_text_label_build(&view, fitted, sizeof(fitted), rect, h, alignment, measure, &paint).code != CORE_OK)
        return;
    SDL_Rect old, clip = *rect;
    bool had_clip = SDL_RenderIsClipEnabled(renderer);
    SDL_RenderGetClipRect(renderer, &old);
    if (had_clip) SDL_IntersectRect(&old, rect, &clip);
    SDL_RenderSetClipRect(renderer, &clip);
    KitUiTextPainter painter = {&paint, no_rect, paint_text};
    KitUiTextPresentationColors colors = {0};
    /* External font drawing rasterizes into cached textures here; queued Vulkan
     * commands retain texture resources, not these stack text/presentation pointers. */
    (void)kit_ui_text_presentation_paint(&view, &colors, &painter);
    SDL_RenderSetClipRect(renderer, had_clip ? &old : NULL);
}
SDL_Rect menu_status_text_rect(const SceneMenuInteraction *ctx, int width, int height, int line_height) {
    if (!ctx || width < 1 || height < 1 || line_height < 1) return (SDL_Rect){0};
    int x = ctx->data_io_panel_rect.x + 12;
    if (x < 20) x = 20;
    int right = ctx->data_io_panel_rect.x + ctx->data_io_panel_rect.w - 12;
    if (right > width-20) right = width-20;
    int top = ctx->config_panel_rect.y + ctx->config_panel_rect.h + 4;
    int bottom = ctx->data_io_panel_rect.y - 4;
    if (top < 20) top = 20;
    if (bottom > height-20) bottom = height-20;
    int h = line_height;
    if (h > bottom-top) h = bottom-top;
    if (right <= x || h <= 0) return (SDL_Rect){0};
    return (SDL_Rect){x, bottom-h, right-x, h};
}
void menu_draw_status_text(SceneMenuInteraction *ctx, int width, int height) {
    if (!ctx || !ctx->status_visible || !ctx->status_text[0]) return;
    TTF_Font *font = ctx->font_small ? ctx->font_small : ctx->font;
    int h = 12;
    (void)physics_sim_text_measure_utf8(ctx->renderer, font, "Mg", NULL, &h);
    SDL_Rect rect = menu_status_text_rect(ctx, width, height, h);
    menu_draw_contained_text(ctx->renderer, font, ctx->status_text, &rect,
        menu_color_status_text(ctx->status_wait_ack), 0);
}
