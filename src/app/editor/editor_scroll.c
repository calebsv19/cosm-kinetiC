#include "app/editor/editor_scroll.h"

#include <math.h>
#include <stdbool.h>

void editor_scroll_init(EditorScroll *s, SDL_Rect track) {
    if (!s) return;
    s->track = track;
    s->content_height = 0.0f;
    s->offset = 0.0f;
    s->view_height = (float)track.h;
    s->thumb_height = 0.0f;
}

void editor_scroll_set_content(EditorScroll *s, float content_height) {
    if (!s) return;
    s->content_height = content_height;
    float view = s->view_height > 0.0f ? s->view_height : 1.0f;
    float ratio = (content_height <= 0.0f) ? 1.0f : (view / content_height);
    if (ratio > 1.0f) ratio = 1.0f;
    s->thumb_height = ratio * (float)s->track.h;
    if (s->thumb_height < 8.0f) s->thumb_height = 8.0f;
    editor_scroll_set_offset(s, s->offset); // clamp
}

void editor_scroll_set_view(EditorScroll *s, float view_height) {
    if (!s) return;
    s->view_height = view_height > 1.0f ? view_height : 1.0f;
    editor_scroll_set_content(s, s->content_height);
}

float editor_scroll_offset(const EditorScroll *s) {
    return s ? s->offset : 0.0f;
}

void editor_scroll_set_offset(EditorScroll *s, float offset) {
    if (!s) return;
    float max_off = (s->content_height - s->view_height);
    if (max_off < 0.0f) max_off = 0.0f;
    if (offset < 0.0f) offset = 0.0f;
    if (offset > max_off) offset = max_off;
    s->offset = offset;
}

static SDL_Rect thumb_rect(const EditorScroll *s) {
    SDL_Rect r = s->track;
    float max_off = (s->content_height - s->view_height);
    if (max_off < 1e-4f) {
        r.h = (int)lroundf(s->thumb_height);
        return r;
    }
    float t = s->offset / max_off;
    float travel = (float)s->track.h - s->thumb_height;
    float y = (float)s->track.y + t * travel;
    r.y = (int)lroundf(y);
    r.h = (int)lroundf(s->thumb_height);
    return r;
}

void editor_scroll_draw(SDL_Renderer *r, const EditorScroll *s, SDL_Color track_color, SDL_Color thumb_color) {
    if (!r || !s) return;
    if (s->content_height <= s->view_height + 1.0f) return; // no need to draw
    SDL_SetRenderDrawColor(r, track_color.r, track_color.g, track_color.b, track_color.a);
    SDL_RenderFillRect(r, &s->track);
    SDL_Rect tr = thumb_rect(s);
    SDL_SetRenderDrawColor(r, thumb_color.r, thumb_color.g, thumb_color.b, thumb_color.a);
    SDL_RenderFillRect(r, &tr);
}

static bool thumb_hit(const EditorScroll *s, int mx, int my) {
    SDL_Rect tr = thumb_rect(s);
    return mx >= tr.x && mx < tr.x + tr.w && my >= tr.y && my < tr.y + tr.h;
}

bool editor_scroll_handle_wheel(EditorScroll *s, int mx, int my, float wheel_y) {
    if (!s) return false;
    (void)mx;
    if (my < s->track.y || my > s->track.y + s->track.h) {
        return false;
    }
    float step = s->view_height * 0.25f;
    editor_scroll_set_offset(s, s->offset - wheel_y * step);
    return true;
}

bool editor_scroll_handle_pointer_down(EditorScroll *s, int mx, int my) {
    if (!s) return false;
    if (thumb_hit(s, mx, my)) {
        return true;
    }
    // jump to position
    float ratio = (float)(my - s->track.y) / (float)(s->track.h > 0 ? s->track.h : 1);
    float max_off = (s->content_height - s->view_height);
    if (max_off < 0.0f) max_off = 0.0f;
    editor_scroll_set_offset(s, ratio * max_off);
    return false;
}

void editor_scroll_handle_pointer_move(EditorScroll *s, int mx, int my) {
    if (!s) return;
    (void)mx;
    float travel = (float)s->track.h - s->thumb_height;
    if (travel <= 1e-4f) return;
    float rel = (float)(my - s->track.y);
    if (rel < 0.0f) rel = 0.0f;
    if (rel > travel) rel = travel;
    float max_off = (s->content_height - s->view_height);
    if (max_off < 0.0f) max_off = 0.0f;
    float t = rel / travel;
    editor_scroll_set_offset(s, t * max_off);
}

void editor_scroll_handle_pointer_up(EditorScroll *s) {
    (void)s;
}

void editor_scroll_scroll_to_row(EditorScroll *s, int row_index, int row_height) {
    if (!s || row_height <= 0 || row_index < 0) return;
    float row_top = (float)row_index * (float)row_height;
    float row_bottom = row_top + (float)row_height;
    float view = s->view_height;
    float off = s->offset;
    if (row_top < off) {
        editor_scroll_set_offset(s, row_top);
    } else if (row_bottom > off + view) {
        editor_scroll_set_offset(s, row_bottom - view);
    }
}
