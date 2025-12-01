#pragma once

#include <SDL2/SDL.h>
#include <stdbool.h>
#include "app/editor/editor_scroll.h"

typedef struct {
    EditorScroll scroll;
    bool dragging_thumb;
    int row_height;
    int row_count;
} EditorListView;

void editor_list_view_init(EditorListView *lv, SDL_Rect track, int row_height);
void editor_list_view_set_rows(EditorListView *lv, int row_count);
void editor_list_view_draw(SDL_Renderer *r,
                           const EditorListView *lv,
                           SDL_Color track_color,
                           SDL_Color thumb_color);
void editor_list_view_handle_wheel(EditorListView *lv, int mx, int my, float wheel_y);
bool editor_list_view_pointer_down(EditorListView *lv, int mx, int my);
void editor_list_view_pointer_move(EditorListView *lv, int mx, int my);
void editor_list_view_pointer_up(EditorListView *lv);
int  editor_list_view_row_at(const EditorListView *lv, int mx, int my, int list_x, int list_y, int list_w, int list_h);
float editor_list_view_offset(const EditorListView *lv);
void editor_list_view_scroll_to_row(EditorListView *lv, int row_index);
