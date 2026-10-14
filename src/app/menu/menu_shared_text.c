#include "app/menu/menu_shared_text.h"
#include <math.h>
#include <string.h>
#include "app/menu/menu_types.h"
#include "app/menu/menu_input.h"
#include "kit_ui_text_edit_sdl.h"
#include "kit_ui_native_text_sdl.h"
#include "render/text_draw.h"
#include "app/menu/menu_text_presentation.h"

static TextInputField *active_field(SceneMenuInteraction *ctx) {
    TextInputField *fields[] = {&ctx->rename_input, &ctx->headless_frames_input,
        &ctx->inflow_input, &ctx->viscosity_input, &ctx->input_root_input,
        &ctx->output_root_input, &ctx->warm_start_input};
    for (size_t i = 0; i < sizeof(fields)/sizeof(fields[0]); ++i)
        if (fields[i]->active) return fields[i];
    return NULL;
}

static char *storage(SceneMenuInteraction *ctx, const TextInputField *field, size_t *capacity) {
    PhysicsSimMenuText *text = &ctx->shared_text;
    if (field == &ctx->input_root_input) { *capacity=text->path_capacity[0]; return text->input_root; }
    if (field == &ctx->output_root_input) { *capacity=text->path_capacity[1]; return text->output_root; }
    if (field == &ctx->warm_start_input) { *capacity=text->path_capacity[2]; return text->warm_start; }
    *capacity=field->max_length+1;
    return (char *)field->buffer;
}
const char *menu_shared_text_value(SceneMenuInteraction *ctx, const TextInputField *field) {
    if (!ctx || !field) return "";
    size_t capacity = 0;
    return storage(ctx, field, &capacity);
}
static void update_legacy_shadow(SceneMenuInteraction *ctx) {
    TextInputField *field = ctx->shared_text.owner;
    if (!field) return;
    size_t capacity = 0;
    char *buffer = storage(ctx, field, &capacity);
    if (buffer != field->buffer)
        menu_text_copy_utf8(field->buffer, sizeof(field->buffer), buffer);
    field->length = strlen(field->buffer);
}
void menu_shared_text_insert(SceneMenuInteraction *ctx, const char *run) {
    if (!ctx || !run || ctx->headless_running) return;
    menu_shared_text_sync(ctx);
    if (!ctx->shared_text.owner) return;
    if (kit_ui_text_insert(&ctx->shared_text.edit, run).code == CORE_OK) {
        update_legacy_shadow(ctx);
        ctx->shared_text.owner->caret_visible=true;
        ctx->shared_text.owner->caret_timer=0;
    }
}
void menu_shared_text_key(SceneMenuInteraction *ctx, SDL_Keycode key, SDL_Keymod mod) {
    SDL_Event event; SDL_zero(event);
    event.type=SDL_KEYDOWN;event.key.keysym.sym=key;event.key.keysym.mod=mod;
    (void)menu_shared_text_event(ctx, &event);
}

void menu_shared_text_cancel_transient(SceneMenuInteraction *ctx) {
    if (!ctx) return;
    PhysicsSimMenuText *text = &ctx->shared_text;
    text->dragging = false;
    text->view_ready = false;
    kit_ui_text_cancel_composition(&text->edit);
    if (text->native_session) SDL_StopTextInput();
    text->native_session = false;
}

void menu_shared_text_begin(SceneMenuInteraction *ctx, TextInputField *field,
                            const char *initial, size_t limit) {
    if (!ctx || !field) return;
    menu_shared_text_cancel_transient(ctx);
    text_input_begin(field, initial, limit);
    PhysicsSimMenuText *text = &ctx->shared_text;
    if (field == &ctx->input_root_input) text->path_capacity[0] = sizeof(text->input_root);
    if (field == &ctx->output_root_input) text->path_capacity[1] = sizeof(text->output_root);
    if (field == &ctx->warm_start_input) text->path_capacity[2] = sizeof(text->warm_start);
    size_t capacity = 0;
    char *buffer = storage(ctx, field, &capacity);
    if (buffer != field->buffer) {
        if (limit && limit < capacity-1) capacity=limit+1;
        if (field == &ctx->input_root_input) text->path_capacity[0]=capacity;
        if (field == &ctx->output_root_input) text->path_capacity[1]=capacity;
        if (field == &ctx->warm_start_input) text->path_capacity[2]=capacity;
        menu_text_copy_utf8(buffer, capacity, initial);
    } else menu_text_copy_utf8(buffer, capacity, initial);
    memset(&text->edit, 0, sizeof(text->edit));
    (void)kit_ui_text_bind(&text->edit, buffer, capacity, KIT_UI_TEXT_SINGLE_LINE);
    ctx->shared_text.owner = field;
    ctx->shared_text.native_session = true; /* text_input_begin started it. */
    update_legacy_shadow(ctx);
    menu_shared_controls_cancel(ctx);
}

void menu_shared_text_sync(SceneMenuInteraction *ctx) {
    if (!ctx) return;
    PhysicsSimMenuText *text = &ctx->shared_text;
    TextInputField *field = active_field(ctx);
    if (text->owner != field) {
        menu_shared_text_cancel_transient(ctx);
        memset(&text->edit, 0, sizeof(text->edit));
        text->owner = field;
    }
    if (field) {
        size_t capacity = 0;
        char *buffer = storage(ctx, field, &capacity);
        (void)kit_ui_text_bind(&text->edit, buffer, capacity, KIT_UI_TEXT_SINGLE_LINE);
    }
    bool eligible = field && (text->view_ready || text->native_session) && !ctx->headless_running &&
        (!ctx->window || (ctx->window_lifecycle.observed.presentable &&
                         (ctx->window_lifecycle.observed.flags & SDL_WINDOW_INPUT_FOCUS)));
    if (eligible && !text->native_session) SDL_StartTextInput();
    if (!eligible && text->native_session) SDL_StopTextInput();
    text->native_session = eligible;
}

static CoreResult measure(void *user, const char *run, float *width) {
    SceneMenuInteraction *ctx = user;
    int w = 0;
    if (!physics_sim_text_measure_utf8(ctx->renderer, ctx->shared_text.font, run, &w, NULL))
        return (CoreResult){CORE_ERR_INVALID_ARG, "menu text measurement failed"};
    *width = (float)w;
    return core_result_ok();
}

bool menu_shared_text_event(SceneMenuInteraction *ctx, const SDL_Event *event) {
    if (!ctx || !event) return false;
    menu_shared_text_sync(ctx);
    PhysicsSimMenuText *text = &ctx->shared_text;
    if (!text->owner || ctx->headless_running) return false;
    if (text->owner == &ctx->rename_input &&
        (event->type == SDL_MOUSEWHEEL || ctx->scrollbar_dragging))
        menu_shared_text_cancel_transient(ctx);
    if (event->type == SDL_MOUSEBUTTONDOWN && event->button.button == SDL_BUTTON_LEFT) {
        SDL_Rect v = text->hit_clip;
        float x = event->button.x, y = event->button.y;
        if (text->view_ready && x >= v.x && y >= v.y && x < v.x+v.w && y < v.y+v.h) {
            size_t at = 0;
            if (kit_ui_text_presentation_hit(&text->view, x, y, measure, ctx, &at).code == CORE_OK) {
                size_t anchor = (SDL_GetModState() & KMOD_SHIFT) ? text->edit.anchor : at;
                (void)kit_ui_text_position(&text->edit, at, anchor);
                text->dragging = true;
                menu_shared_controls_cancel(ctx);
                text->owner->caret_visible = true;
                text->owner->caret_timer = 0;
                return true;
            }
        }
        text->dragging = false;
    }
    if (text->dragging && (event->type == SDL_MOUSEMOTION ||
        (event->type == SDL_MOUSEBUTTONUP && event->button.button == SDL_BUTTON_LEFT))) {
        float x = event->type == SDL_MOUSEMOTION ? event->motion.x : event->button.x;
        float y = event->type == SDL_MOUSEMOTION ? event->motion.y : event->button.y;
        size_t at = 0;
        if (text->view_ready && kit_ui_text_presentation_hit(&text->view, x, y, measure, ctx, &at).code == CORE_OK)
            (void)kit_ui_text_position(&text->edit, at, text->edit.anchor);
        if (event->type == SDL_MOUSEBUTTONUP) text->dragging = false;
        return true;
    }
    KitUiTextEventResult result = kit_ui_text_event_sdl(&text->edit, event);
    update_legacy_shadow(ctx);
    if (result.consumed) {
        text->owner->caret_timer = 0;
        text->owner->caret_visible = true;
    }
    if (result.submit || result.cancel) {
        /* Only the existing owner may validate, commit/cancel or persist. */
        menu_key_down(ctx, result.submit ? SDLK_RETURN : SDLK_ESCAPE, KMOD_NONE);
        menu_shared_text_sync(ctx);
    }
    /* Text ownership suppresses unrelated menu shortcuts, including theme cycling. */
    return result.consumed || event->type == SDL_KEYDOWN || event->type == SDL_KEYUP;
}

static CoreResult paint_rect(void *user, KitRenderRect rect, KitRenderColor color) {
    SceneMenuInteraction *ctx = user;
    SDL_Rect r = {(int)floorf(rect.x), (int)floorf(rect.y),
                  (int)ceilf(rect.width), (int)ceilf(rect.height)};
    SDL_SetRenderDrawColor(ctx->renderer, color.r, color.g, color.b, color.a);
    SDL_RenderFillRect(ctx->renderer, &r);
    return core_result_ok();
}

typedef struct TextPaint { SceneMenuInteraction *ctx; SDL_Color color; } TextPaint;
static CoreResult paint_text(void *user, KitRenderVec2 origin, const char *run) {
    TextPaint *paint = user;
    if (!run[0]) return core_result_ok();
    return physics_sim_text_draw_utf8_at(paint->ctx->renderer, paint->ctx->shared_text.font,
             run, (int)origin.x, (int)origin.y, paint->color) ? core_result_ok() :
             (CoreResult){CORE_ERR_INVALID_ARG, "menu text painting failed"};
}
static CoreResult paint_text_rect(void *user, KitRenderRect r, KitRenderColor c) {
    return paint_rect(((TextPaint *)user)->ctx, r, c);
}
static KitRenderColor color(SDL_Color c) {
    return (KitRenderColor){c.r, c.g, c.b, c.a};
}

void menu_draw_shared_text_input(SceneMenuInteraction *ctx, TTF_Font *font,
    const SDL_Rect *rect, TextInputField *field, SDL_Color fill, SDL_Color border, SDL_Color ink) {
    if (!ctx || !ctx->renderer || !font || !rect || !field) return;
    menu_shared_text_sync(ctx);
    PhysicsSimMenuText *text = &ctx->shared_text;
    SDL_SetRenderDrawColor(ctx->renderer, fill.r, fill.g, fill.b, 250);
    SDL_RenderFillRect(ctx->renderer, rect);
    SDL_SetRenderDrawColor(ctx->renderer, border.r, border.g, border.b, 255);
    SDL_RenderDrawRect(ctx->renderer, rect);
    if (text->owner != field) return;
    text->font = font;
    int h = 12;
    (void)physics_sim_text_measure_utf8(ctx->renderer, font, "Mg", NULL, &h);
    KitUiTextPresentationOptions options = {
        .viewport = {rect->x+8, rect->y+(rect->h-h)/2, rect->w-16, h},
        .line_height = h, .caret_width = 1, .active = 1, .reveal_caret = 1};
    if (options.viewport.width < 1) options.viewport.width = 1;
    text->view_ready = kit_ui_text_presentation_build(&text->view, &text->edit,
                          &options, measure, ctx).code == CORE_OK;
    if (!text->view_ready) return;
    SDL_Rect old_clip; SDL_RenderGetClipRect(ctx->renderer, &old_clip);
    bool had_clip = SDL_RenderIsClipEnabled(ctx->renderer);
    SDL_Rect viewport = {(int)options.viewport.x, (int)options.viewport.y,
                         (int)options.viewport.width, (int)options.viewport.height};
    SDL_Rect clip = viewport;
    if (had_clip) SDL_IntersectRect(&old_clip, &viewport, &clip);
    text->hit_clip = clip;
    text->view_ready = clip.w > 0 && clip.h > 0;
    menu_shared_text_sync(ctx);
    SDL_RenderSetClipRect(ctx->renderer, &clip);
    TextPaint paint = {ctx, ink};
    KitUiTextPainter painter = {&paint, paint_text_rect, paint_text};
    KitUiTextPresentationColors colors = {color(border), color(ink), color(ink)};
    colors.selection.a = 115;
    if (!field->caret_visible || !text->native_session) colors.caret.a = 0;
    (void)kit_ui_text_presentation_paint(&text->view, &colors, &painter);
    SDL_RenderSetClipRect(ctx->renderer, had_clip ? &old_clip : NULL);
    if (text->native_session && text->view_ready && ctx->window) {
        int w = 0, h_window = 0;
        SDL_GetWindowSize(ctx->window, &w, &h_window);
        KitUiTextPresentationOptions saved = text->view.options;
        text->view.options.viewport = (KitRenderRect){clip.x, clip.y, clip.w, clip.h};
        (void)kit_ui_native_text_rect_sdl(ctx->window, &text->view, w, h_window, NULL);
        text->view.options = saved;
    }
}
