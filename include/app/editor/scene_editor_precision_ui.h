#ifndef SCENE_EDITOR_PRECISION_UI_H
#define SCENE_EDITOR_PRECISION_UI_H
#include <stdbool.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
typedef struct PrecisionUiTransient {
    bool *pointer_down, *drag_started, *object, *import_body, *import_handle,
        *handle, *emitter, *handle_started;
    int *hit_count, *hit_base;
    int *hover_object, *hover_import, *hover_emitter, *hover_edge, *pointer_x, *pointer_y;
} PrecisionUiTransient;
void scene_editor_precision_cancel_transient(void *);
void scene_editor_precision_draw_hint(SDL_Renderer *,TTF_Font *,int,int);
#endif
