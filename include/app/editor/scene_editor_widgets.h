#ifndef SCENE_EDITOR_WIDGETS_H
#define SCENE_EDITOR_WIDGETS_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdbool.h>

#include "app/scene_presets.h"

typedef struct EditorButton {
    SDL_Rect rect;
    const char *label;
    bool enabled;
} EditorButton;

typedef enum EditorFieldTarget {
    FIELD_NONE = 0,
    FIELD_RADIUS,
    FIELD_STRENGTH
} EditorFieldTarget;

typedef struct NumericField {
    SDL_Rect rect;
    const char *label;
    EditorFieldTarget target;
    bool editing;
    char buffer[32];
} NumericField;

void scene_editor_draw_button(SDL_Renderer *renderer,
                              const EditorButton *button,
                              TTF_Font *font);

void scene_editor_draw_numeric_field(SDL_Renderer *renderer,
                                     TTF_Font *font,
                                     const NumericField *field,
                                     const FluidEmitter *selected_emitter);

#endif // SCENE_EDITOR_WIDGETS_H
