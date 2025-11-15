#ifndef UI_SCROLLBAR_H
#define UI_SCROLLBAR_H

#include <SDL2/SDL.h>
#include <stdbool.h>

typedef struct ScrollBar {
    SDL_Rect track;
    float    content_size;
    float    view_size;
    float    scroll_offset;
    bool     dragging;
    int      drag_start_y;
    float    drag_start_offset;
} ScrollBar;

void scrollbar_init(ScrollBar *bar);
void scrollbar_set_track(ScrollBar *bar, SDL_Rect rect);
void scrollbar_set_content(ScrollBar *bar, float content_size, float view_size);
void scrollbar_set_offset(ScrollBar *bar, float offset);
void scrollbar_scroll(ScrollBar *bar, float delta);
bool scrollbar_handle_pointer_down(ScrollBar *bar, int x, int y);
void scrollbar_handle_pointer_up(ScrollBar *bar);
void scrollbar_handle_pointer_move(ScrollBar *bar, int x, int y);
void scrollbar_handle_wheel(ScrollBar *bar, int wheel_y);
float scrollbar_offset(const ScrollBar *bar);
void scrollbar_draw(SDL_Renderer *renderer, const ScrollBar *bar);

#endif // UI_SCROLLBAR_H
