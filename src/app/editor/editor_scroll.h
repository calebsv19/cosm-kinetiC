#pragma once

#include <SDL2/SDL.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct {
    SDL_Rect track;
    float content_height;
    float offset;
    float view_height;
    float thumb_height;
} EditorScroll;

void editor_scroll_init(EditorScroll *s, SDL_Rect track);
void editor_scroll_set_content(EditorScroll *s, float content_height);
void editor_scroll_set_view(EditorScroll *s, float view_height);
float editor_scroll_offset(const EditorScroll *s);
void editor_scroll_set_offset(EditorScroll *s, float offset);
void editor_scroll_draw(SDL_Renderer *r, const EditorScroll *s, SDL_Color track_color, SDL_Color thumb_color);
bool editor_scroll_handle_wheel(EditorScroll *s, int mx, int my, float wheel_y);
bool editor_scroll_handle_pointer_down(EditorScroll *s, int mx, int my);
void editor_scroll_handle_pointer_move(EditorScroll *s, int mx, int my);
void editor_scroll_handle_pointer_up(EditorScroll *s);
void editor_scroll_scroll_to_row(EditorScroll *s, int row_index, int row_height);
