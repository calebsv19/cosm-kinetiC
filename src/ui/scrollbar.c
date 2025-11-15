#include "ui/scrollbar.h"

#include <math.h>

static const float SCROLL_STEP = 50.0f;
static const int   MIN_THUMB_PIXELS = 24;

static bool scrollbar_has_overflow(const ScrollBar *bar) {
    return bar && bar->content_size > bar->view_size + 0.5f;
}

static float scrollbar_max_offset(const ScrollBar *bar) {
    if (!bar) return 0.0f;
    float max_offset = bar->content_size - bar->view_size;
    return (max_offset > 0.0f) ? max_offset : 0.0f;
}

static SDL_Rect scrollbar_thumb_rect(const ScrollBar *bar) {
    SDL_Rect thumb = bar->track;
    if (!scrollbar_has_overflow(bar)) {
        thumb.h = bar->track.h;
        thumb.w = bar->track.w;
        return thumb;
    }

    float view_ratio = bar->view_size / bar->content_size;
    if (view_ratio < 0.0f) view_ratio = 0.0f;
    if (view_ratio > 1.0f) view_ratio = 1.0f;
    int thumb_h = (int)lroundf(view_ratio * (float)bar->track.h);
    if (thumb_h < MIN_THUMB_PIXELS) thumb_h = MIN_THUMB_PIXELS;
    if (thumb_h > bar->track.h) thumb_h = bar->track.h;
    thumb.h = thumb_h;

    float track_range = (float)(bar->track.h - thumb_h);
    float max_off = scrollbar_max_offset(bar);
    float normalized = (max_off > 0.0f) ? (bar->scroll_offset / max_off) : 0.0f;
    if (normalized < 0.0f) normalized = 0.0f;
    if (normalized > 1.0f) normalized = 1.0f;
    thumb.y = bar->track.y + (int)lroundf(track_range * normalized);
    return thumb;
}

static void clamp_offset(ScrollBar *bar) {
    if (!bar) return;
    float max_off = scrollbar_max_offset(bar);
    if (bar->scroll_offset < 0.0f) bar->scroll_offset = 0.0f;
    if (bar->scroll_offset > max_off) bar->scroll_offset = max_off;
}

void scrollbar_init(ScrollBar *bar) {
    if (!bar) return;
    bar->track = (SDL_Rect){0, 0, 0, 0};
    bar->content_size = 0.0f;
    bar->view_size = 0.0f;
    bar->scroll_offset = 0.0f;
    bar->dragging = false;
    bar->drag_start_y = 0;
    bar->drag_start_offset = 0.0f;
}

void scrollbar_set_track(ScrollBar *bar, SDL_Rect rect) {
    if (!bar) return;
    bar->track = rect;
}

void scrollbar_set_content(ScrollBar *bar, float content_size, float view_size) {
    if (!bar) return;
    bar->content_size = (content_size >= 0.0f) ? content_size : 0.0f;
    bar->view_size = (view_size >= 0.0f) ? view_size : 0.0f;
    clamp_offset(bar);
}

void scrollbar_set_offset(ScrollBar *bar, float offset) {
    if (!bar) return;
    bar->scroll_offset = offset;
    clamp_offset(bar);
}

void scrollbar_scroll(ScrollBar *bar, float delta) {
    if (!bar) return;
    if (!scrollbar_has_overflow(bar)) {
        bar->scroll_offset = 0.0f;
        return;
    }
    bar->scroll_offset += delta;
    clamp_offset(bar);
}

bool scrollbar_handle_pointer_down(ScrollBar *bar, int x, int y) {
    if (!bar || !scrollbar_has_overflow(bar)) return false;
    SDL_Rect thumb = scrollbar_thumb_rect(bar);
    bool inside_thumb = x >= thumb.x && x < thumb.x + thumb.w &&
                        y >= thumb.y && y < thumb.y + thumb.h;
    if (inside_thumb) {
        bar->dragging = true;
        bar->drag_start_y = y;
        bar->drag_start_offset = bar->scroll_offset;
        return true;
    }

    if (x >= bar->track.x && x < bar->track.x + bar->track.w &&
        y >= bar->track.y && y < bar->track.y + bar->track.h) {
        float local = (float)(y - bar->track.y) / (float)bar->track.h;
        if (local < 0.0f) local = 0.0f;
        if (local > 1.0f) local = 1.0f;
        bar->scroll_offset = scrollbar_max_offset(bar) * local;
        clamp_offset(bar);
        return true;
    }
    return false;
}

void scrollbar_handle_pointer_up(ScrollBar *bar) {
    if (!bar) return;
    bar->dragging = false;
}

void scrollbar_handle_pointer_move(ScrollBar *bar, int x, int y) {
    (void)x;
    if (!bar || !bar->dragging) return;
    SDL_Rect thumb = scrollbar_thumb_rect(bar);
    float track_range = (float)(bar->track.h - thumb.h);
    if (track_range <= 0.0f) return;
    float dy = (float)(y - bar->drag_start_y);
    float ratio = scrollbar_max_offset(bar) / track_range;
    bar->scroll_offset = bar->drag_start_offset + dy * ratio;
    clamp_offset(bar);
}

void scrollbar_handle_wheel(ScrollBar *bar, int wheel_y) {
    if (!bar || wheel_y == 0) return;
    // SDL wheel: positive Y = scroll up
    float delta = -wheel_y * SCROLL_STEP;
    scrollbar_scroll(bar, delta);
}

float scrollbar_offset(const ScrollBar *bar) {
    if (!bar) return 0.0f;
    return bar->scroll_offset;
}

void scrollbar_draw(SDL_Renderer *renderer, const ScrollBar *bar) {
    if (!renderer || !bar) return;
    if (!scrollbar_has_overflow(bar)) return;

    SDL_Color track_color = {40, 44, 52, 255};
    SDL_Color thumb_color = {95, 105, 125, 255};

    SDL_SetRenderDrawColor(renderer, track_color.r, track_color.g, track_color.b, track_color.a);
    SDL_RenderFillRect(renderer, &bar->track);

    SDL_Rect thumb = scrollbar_thumb_rect(bar);
    SDL_SetRenderDrawColor(renderer, thumb_color.r, thumb_color.g, thumb_color.b, thumb_color.a);
    SDL_RenderFillRect(renderer, &thumb);
}
