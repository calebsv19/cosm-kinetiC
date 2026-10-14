#include "app/menu/menu_text_presentation.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "render/text_upload_policy.h"
void menu_text_copy_utf8(char *out, size_t capacity, const char *source) {
    if (!out || !capacity) return;
    snprintf(out, capacity, "%s", source ? source : "");
    KitUiTextEdit edit = {0};
    size_t n = strlen(out);
    while (kit_ui_text_bind(&edit, out, capacity, KIT_UI_TEXT_SINGLE_LINE).code != CORE_OK && n)
        out[--n] = 0;
}

CoreResult menu_text_fit_measured(const char *source, float width, char *out,
    size_t capacity, KitUiTextMeasure measure, void *user) {
    if (!out || !capacity || !measure || !isfinite(width))
        return (CoreResult){CORE_ERR_INVALID_ARG, "invalid menu text fit"};
    out[0] = 0;
    if (!source || width <= 0) return core_result_ok();
    char work[KIT_UI_TEXT_PRESENTATION_BYTES];
    if (strlen(source) >= sizeof(work))
        return (CoreResult){CORE_ERR_INVALID_ARG, "menu label exceeds presentation capacity"};
    menu_text_copy_utf8(work, sizeof(work), source);
    for (size_t i = 0; work[i]; ++i) if (work[i] == '\n' || work[i] == '\r') work[i] = ' ';
    KitUiTextEdit edit = {0};
    CoreResult result = kit_ui_text_bind(&edit, work, sizeof(work), KIT_UI_TEXT_SINGLE_LINE);
    if (result.code != CORE_OK) return result;
    float measured = 0;
    result = measure(user, work, &measured);
    if (result.code != CORE_OK) return result;
    if (measured <= width && strlen(work) < capacity) {
        memcpy(out, work, strlen(work)+1);
        return core_result_ok();
    }
    if (capacity < 4) return core_result_ok();
    for (;;) {
        if (strlen(work)+4 <= capacity) {
            snprintf(out, capacity, "%s...", work);
            result = measure(user, out, &measured);
            if (result.code != CORE_OK) { out[0]=0; return result; }
            if (measured <= width) return core_result_ok();
        }
        if (!work[0]) { out[0]=0; return core_result_ok(); }
        result = kit_ui_text_command(&edit, KIT_UI_TEXT_BACKSPACE, 0);
        if (result.code != CORE_OK) { out[0]=0; return result; }
    }
}


typedef struct MenuFitMeasure { SDL_Renderer *renderer; TTF_Font *font; } MenuFitMeasure;
static CoreResult measure(void *user, const char *run, float *width) {
    MenuFitMeasure *context = user;
    int w = 0;
    if (!context->font || TTF_SizeUTF8(context->font, run, &w, NULL) != 0)
        return (CoreResult){CORE_ERR_INVALID_ARG, "menu label measurement failed"};
    *width = physics_sim_text_logical_pixels(context->renderer, w);
    return core_result_ok();
}
void menu_text_fit(SDL_Renderer *renderer, TTF_Font *font, const char *text,
    int width, char *out, size_t capacity) {
    MenuFitMeasure context = {renderer, font};
    (void)menu_text_fit_measured(text, width, out, capacity, measure, &context);
}

CoreResult menu_text_label_build(KitUiTextPresentation *view, char *text, size_t capacity,
    const SDL_Rect *rect, float height, int alignment, KitUiTextMeasure measure, void *user) {
    if (!rect || alignment < 0 || alignment > 2)
        return (CoreResult){CORE_ERR_INVALID_ARG, "invalid menu label geometry"};
    KitUiTextEdit edit = {0};
    CoreResult result = kit_ui_text_bind(&edit, text, capacity, KIT_UI_TEXT_SINGLE_LINE);
    if (result.code != CORE_OK) return result;
    KitUiTextPresentationOptions options = {
        .viewport = {rect->x, rect->y, rect->w, rect->h}, .line_height = height, .caret_width = 1};
    result = kit_ui_text_presentation_build(view, &edit, &options, measure, user);
    if (result.code != CORE_OK) return result;
    for (size_t i = 0; i < view->count; ++i) {
        if (alignment == 1) view->rows[i].origin.x += (rect->w-view->rows[i].width)/2;
        if (alignment == 2) view->rows[i].origin.x += rect->w-view->rows[i].width;
        view->rows[i].origin.y += (rect->h-height)/2;
    }
    return core_result_ok();
}
