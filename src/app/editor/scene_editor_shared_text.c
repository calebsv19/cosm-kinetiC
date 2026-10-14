#include "app/editor/scene_editor_internal.h"
#include "app/editor/scene_editor_model.h"
#include "kit_ui_text_edit_sdl.h"
#include "kit_ui_native_text_sdl.h"
#include "render/text_draw.h"
#include <math.h>
#include <string.h>

static TextInputField *metadata(SceneEditorState *s) {
    if (s->renaming_name) return &s->name_input;
    if (s->editing_width) return &s->width_input;
    if (s->editing_height) return &s->height_input;
    if (s->editing_depth) return &s->depth_input;
    return NULL;
}
void scene_editor_shared_text_reset(SceneEditorState *s) {
    SceneEditorSharedUi *ui = &s->shared_ui;
    ui->dragging = ui->view_ready = false;
    kit_ui_text_cancel_composition(&ui->edit);
    if (ui->native_session) SDL_StopTextInput();
    ui->native_session = false;
}
void scene_editor_shared_text_sync(SceneEditorState *s) {
    SceneEditorSharedUi *ui = &s->shared_ui;
    TextInputField *field = metadata(s);
    char *buffer = field ? field->buffer :
        (s->active_field && s->active_field->editing ? s->active_field->buffer : NULL);
    size_t capacity = field ? field->max_length+1 :
        (buffer ? sizeof(s->active_field->buffer) : 0);
    if (buffer != ui->buffer) {
        scene_editor_shared_text_reset(s);
        memset(&ui->edit, 0, sizeof(ui->edit));
        ui->buffer = buffer;
        /* Metadata begin already owns a native session. Adopt it without
         * stopping SDL here: StopTextInput flushes queued committed input. */
        ui->native_session = field && SDL_IsTextInputActive();
    }
    if (buffer) (void)kit_ui_text_bind(&ui->edit, buffer, capacity, KIT_UI_TEXT_SINGLE_LINE);
    bool eligible = buffer && (ui->view_ready || ui->native_session) && (!s->window ||
        (ui->window.observed.presentable && (ui->window.observed.flags & SDL_WINDOW_INPUT_FOCUS)));
    if (eligible && !ui->native_session) SDL_StartTextInput();
    if (!eligible && ui->native_session) SDL_StopTextInput();
    ui->native_session = eligible;
}
static CoreResult measure(void *user, const char *run, float *width) {
    SceneEditorState *s = user;
    int w = 0;
    if (!physics_sim_text_measure_utf8(s->renderer, s->shared_ui.font, run, &w, NULL))
        return (CoreResult){CORE_ERR_INVALID_ARG, "editor text measurement failed"};
    *width = w;
    return core_result_ok();
}
bool scene_editor_shared_text_event(SceneEditorState *s, const SDL_Event *event) {
    scene_editor_shared_text_sync(s);
    SceneEditorSharedUi *ui = &s->shared_ui;
    if (!ui->buffer) return false;
    if (event->type == SDL_MOUSEBUTTONDOWN && event->button.button == SDL_BUTTON_LEFT) {
        SDL_Rect r = ui->hit_clip;
        if (ui->view_ready && event->button.x >= r.x && event->button.x < r.x+r.w &&
            event->button.y >= r.y && event->button.y < r.y+r.h) {
            size_t at = 0;
            if (kit_ui_text_presentation_hit(&ui->view, event->button.x, event->button.y,
                                            measure, s, &at).code == CORE_OK) {
                (void)kit_ui_text_position(&ui->edit, at,
                    (SDL_GetModState() & KMOD_SHIFT) ? ui->edit.anchor : at);
                ui->dragging = true;
                return true;
            }
        }
        ui->dragging = false;
    }
    if (ui->dragging && (event->type == SDL_MOUSEMOTION ||
        (event->type == SDL_MOUSEBUTTONUP && event->button.button == SDL_BUTTON_LEFT))) {
        size_t at = 0;
        float x = event->type == SDL_MOUSEMOTION ? event->motion.x : event->button.x;
        float y = event->type == SDL_MOUSEMOTION ? event->motion.y : event->button.y;
        if (ui->view_ready && kit_ui_text_presentation_hit(&ui->view, x, y, measure, s, &at).code == CORE_OK)
            (void)kit_ui_text_position(&ui->edit, at, ui->edit.anchor);
        if (event->type == SDL_MOUSEBUTTONUP) ui->dragging = false;
        return true;
    }
    /* Numeric fields retain the legacy accepted character set, including paste.
     * Parsing, clamping and domain commit remain in commit_field_edit. */
    bool numeric = !metadata(s) && s->active_field && s->active_field->editing;
    char previous[sizeof(s->radius_field.buffer)] = {0};
    KitUiTextEdit saved = ui->edit;
    if (numeric) memcpy(previous, ui->buffer, sizeof(previous));
    KitUiTextEventResult result = kit_ui_text_event_sdl(&ui->edit, event);
    if (numeric) {
        for (const char *p = ui->buffer; *p; ++p) {
            if ((*p < '0' || *p > '9') && *p != '.' && *p != '-') {
                memcpy(ui->buffer, previous, sizeof(previous));
                ui->edit = saved;
                break;
            }
        }
    }
    if (result.consumed) {
        KitUiInteractionEvent cancel = {.type = KIT_UI_INTERACTION_CANCEL};
        KitUiInteractionResult cancelled;
        (void)kit_ui_surface_route(&ui->controls, &cancel, &cancelled);
    }
    TextInputField *field = metadata(s);
    if (field) {
        field->length = strlen(field->buffer);
        if (result.consumed) { field->caret_visible = true; field->caret_timer = 0; }
    }
    if (result.submit || result.cancel) {
        bool apply = result.submit != 0;
        if (s->renaming_name) editor_finish_name_edit(s, apply);
        else if (s->editing_width) editor_finish_dimension_edit(s, SCENE_EDITOR_DIMENSION_WIDTH, apply);
        else if (s->editing_height) editor_finish_dimension_edit(s, SCENE_EDITOR_DIMENSION_HEIGHT, apply);
        else if (s->editing_depth) editor_finish_dimension_edit(s, SCENE_EDITOR_DIMENSION_DEPTH, apply);
        else if (apply) commit_field_edit(s);
        else cancel_field_edit(s);
        scene_editor_shared_text_sync(s);
    }
    return result.consumed || event->type == SDL_KEYDOWN || event->type == SDL_KEYUP;
}
static CoreResult paint_rect(void *user, KitRenderRect r, KitRenderColor c) {
    SceneEditorState *s = user;
    SDL_Rect rect = {(int)floorf(r.x), (int)floorf(r.y), (int)ceilf(r.width), (int)ceilf(r.height)};
    SDL_SetRenderDrawColor(s->renderer, c.r, c.g, c.b, c.a);
    SDL_RenderFillRect(s->renderer, &rect);
    return core_result_ok();
}
typedef struct EditorTextPaint { SceneEditorState *state; SDL_Color ink; } EditorTextPaint;
static CoreResult paint_text(void *user, KitRenderVec2 origin, const char *run) {
    EditorTextPaint *p = user;
    if (!run[0]) return core_result_ok();
    return physics_sim_text_draw_utf8_at(p->state->renderer, p->state->shared_ui.font,
        run, (int)origin.x, (int)origin.y, p->ink) ? core_result_ok() :
        (CoreResult){CORE_ERR_INVALID_ARG, "editor text painting failed"};
}
static CoreResult paint_selection(void *user, KitRenderRect r, KitRenderColor c) {
    return paint_rect(((EditorTextPaint *)user)->state, r, c);
}
void scene_editor_shared_text_draw(SceneEditorState *s, TTF_Font *font,
    const SDL_Rect *rect, char *buffer, SDL_Color fill, SDL_Color border, SDL_Color ink) {
    scene_editor_shared_text_sync(s);
    SceneEditorSharedUi *ui = &s->shared_ui;
    SDL_SetRenderDrawColor(s->renderer, fill.r, fill.g, fill.b, fill.a);
    SDL_RenderFillRect(s->renderer, rect);
    SDL_SetRenderDrawColor(s->renderer, border.r, border.g, border.b, border.a);
    SDL_RenderDrawRect(s->renderer, rect);
    if (ui->buffer != buffer || !font) return;
    ui->font = font;
    int h = 12;
    (void)physics_sim_text_measure_utf8(s->renderer, font, "Mg", NULL, &h);
    KitUiTextPresentationOptions options = {.viewport = {rect->x+8, rect->y+(rect->h-h)/2,
        rect->w > 16 ? rect->w-16 : 1, h}, .line_height = h,
        .caret_width = 1, .active = 1, .reveal_caret = 1};
    ui->view_ready = kit_ui_text_presentation_build(&ui->view, &ui->edit, &options, measure, s).code == CORE_OK;
    if (!ui->view_ready) return;
    SDL_Rect old, clip = {(int)options.viewport.x, (int)options.viewport.y,
        (int)options.viewport.width, (int)options.viewport.height};
    bool inherited = SDL_RenderIsClipEnabled(s->renderer);
    SDL_RenderGetClipRect(s->renderer, &old);
    if (inherited) SDL_IntersectRect(&old, &clip, &clip);
    ui->hit_clip = clip;
    ui->view_ready = clip.w > 0 && clip.h > 0;
    if (!ui->view_ready) {scene_editor_shared_text_reset(s);return;}
    scene_editor_shared_text_sync(s);
    SDL_RenderSetClipRect(s->renderer, &clip);
    EditorTextPaint paint = {s, ink};
    KitUiTextPainter painter = {&paint, paint_selection, paint_text};
    TextInputField *field = metadata(s);
    KitRenderColor accent = {border.r, border.g, border.b, 115};
    KitRenderColor caret = {ink.r, ink.g, ink.b,
        ui->native_session && (!field || field->caret_visible) ? ink.a : 0};
    KitUiTextPresentationColors colors = {accent, caret, {ink.r, ink.g, ink.b, ink.a}};
    (void)kit_ui_text_presentation_paint(&ui->view, &colors, &painter);
    SDL_RenderSetClipRect(s->renderer, inherited ? &old : NULL);
    if (ui->native_session && s->window && ui->view_ready) {
        KitUiTextPresentationOptions saved = ui->view.options;
        ui->view.options.viewport = (KitRenderRect){clip.x, clip.y, clip.w, clip.h};
        (void)kit_ui_native_text_rect_sdl(s->window, &ui->view, s->layout_win_w, s->layout_win_h, NULL);
        ui->view.options = saved;
    }
}
