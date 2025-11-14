#include "app/editor/scene_editor_widgets.h"

#include <SDL2/SDL_ttf.h>

static SDL_Color COLOR_PANEL     = {32, 36, 40, 255};
static SDL_Color COLOR_PANEL_ACC = {45, 50, 58, 255};
static SDL_Color COLOR_TEXT      = {245, 247, 250, 255};
static SDL_Color COLOR_TEXT_DIM  = {190, 198, 209, 255};

void scene_editor_draw_button(SDL_Renderer *renderer,
                              const EditorButton *button,
                              TTF_Font *font) {
    if (!renderer || !button || !font) return;
    SDL_Color fill = button->enabled ? COLOR_PANEL : (SDL_Color){25, 28, 32, 255};
    SDL_SetRenderDrawColor(renderer, fill.r, fill.g, fill.b, fill.a);
    SDL_RenderFillRect(renderer, &button->rect);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 200);
    SDL_RenderDrawRect(renderer, &button->rect);
    SDL_Color text = button->enabled ? COLOR_TEXT : COLOR_TEXT_DIM;
    SDL_Surface *surf = TTF_RenderUTF8_Blended(font, button->label, text);
    if (!surf) return;
    SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer, surf);
    SDL_Rect dst = {
        .x = button->rect.x + 12,
        .y = button->rect.y + (button->rect.h / 2) - surf->h / 2,
        .w = surf->w,
        .h = surf->h
    };
    SDL_RenderCopy(renderer, tex, NULL, &dst);
    SDL_DestroyTexture(tex);
    SDL_FreeSurface(surf);
}

void scene_editor_draw_numeric_field(SDL_Renderer *renderer,
                                     TTF_Font *font,
                                     const NumericField *field,
                                     const FluidEmitter *selected_emitter) {
    if (!renderer || !font || !field) return;
    SDL_SetRenderDrawColor(renderer, COLOR_PANEL.r, COLOR_PANEL.g, COLOR_PANEL.b, 255);
    SDL_RenderFillRect(renderer, &field->rect);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 200);
    SDL_RenderDrawRect(renderer, &field->rect);
    SDL_Surface *label = TTF_RenderUTF8_Blended(font, field->label, COLOR_TEXT_DIM);
    if (label) {
        SDL_Texture *label_tex = SDL_CreateTextureFromSurface(renderer, label);
        SDL_Rect dst = {field->rect.x, field->rect.y - 20, label->w, label->h};
        SDL_RenderCopy(renderer, label_tex, NULL, &dst);
        SDL_DestroyTexture(label_tex);
        SDL_FreeSurface(label);
    }

    char display[32] = {0};
    if (field->editing) {
        snprintf(display, sizeof(display), "%s", field->buffer);
    } else if (selected_emitter) {
        float value = (field->target == FIELD_RADIUS) ? selected_emitter->radius : selected_emitter->strength;
        snprintf(display, sizeof(display), "%.3f", value);
    } else {
        snprintf(display, sizeof(display), "--");
    }
    SDL_Surface *value = TTF_RenderUTF8_Blended(font, display, COLOR_TEXT);
    if (value) {
        SDL_Texture *value_tex = SDL_CreateTextureFromSurface(renderer, value);
        SDL_Rect dst = {field->rect.x + 8, field->rect.y + 8, value->w, value->h};
        SDL_RenderCopy(renderer, value_tex, NULL, &dst);
        SDL_DestroyTexture(value_tex);
        SDL_FreeSurface(value);
    }
}
