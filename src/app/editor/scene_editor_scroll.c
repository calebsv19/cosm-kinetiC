#include "app/editor/scene_editor_scroll.h"

void editor_list_view_init(EditorListView *lv, SDL_Rect track, int row_height) {
    if (!lv) return;
    lv->scroll.track = track;
    lv->scroll.content_height = 0.0f;
    lv->scroll.view_height = (float)track.h;
    lv->scroll.offset = 0.0f;
    lv->scroll.thumb_height = (float)track.h;
    lv->dragging_thumb = false;
    lv->row_height = row_height > 0 ? row_height : 24;
    lv->row_count = 0;
}

void editor_list_view_set_rows(EditorListView *lv, int row_count) {
    if (!lv) return;
    lv->row_count = (row_count < 0) ? 0 : row_count;
    editor_scroll_set_content(&lv->scroll, (float)lv->row_count * (float)lv->row_height);
}

void editor_list_view_draw(SDL_Renderer *r,
                           const EditorListView *lv,
                           SDL_Color track_color,
                           SDL_Color thumb_color) {
    if (!lv) return;
    editor_scroll_draw(r, &lv->scroll, track_color, thumb_color);
}

void editor_list_view_handle_wheel(EditorListView *lv, int mx, int my, float wheel_y) {
    if (!lv) return;
    if (editor_scroll_handle_wheel(&lv->scroll, mx, my, wheel_y)) {
        lv->dragging_thumb = false;
    }
}

bool editor_list_view_pointer_down(EditorListView *lv, int mx, int my) {
    if (!lv) return false;
    bool drag = editor_scroll_handle_pointer_down(&lv->scroll, mx, my);
    lv->dragging_thumb = drag;
    return drag;
}

void editor_list_view_pointer_move(EditorListView *lv, int mx, int my) {
    if (!lv || !lv->dragging_thumb) return;
    editor_scroll_handle_pointer_move(&lv->scroll, mx, my);
}

void editor_list_view_pointer_up(EditorListView *lv) {
    if (!lv) return;
    lv->dragging_thumb = false;
    editor_scroll_handle_pointer_up(&lv->scroll);
}

int editor_list_view_row_at(const EditorListView *lv, int mx, int my, int list_x, int list_y, int list_w, int list_h) {
    if (!lv) return -1;
    if (mx < list_x || mx > list_x + list_w) return -1;
    if (my < list_y || my > list_y + list_h) return -1;
    float local_y = (float)(my - list_y) + editor_scroll_offset(&lv->scroll);
    int row = (int)(local_y / (float)lv->row_height);
    if (row < 0 || row >= lv->row_count) return -1;
    return row;
}

float editor_list_view_offset(const EditorListView *lv) {
    return lv ? editor_scroll_offset(&lv->scroll) : 0.0f;
}

void editor_list_view_scroll_to_row(EditorListView *lv, int row_index) {
    if (!lv) return;
    editor_scroll_scroll_to_row(&lv->scroll, row_index, lv->row_height);
}
