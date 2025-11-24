#include "app/editor/scene_editor_canvas.h"

#include <math.h>
#include <string.h>

static SDL_Color COLOR_CANVAS    = {12, 14, 18, 255};
static SDL_Color COLOR_SOURCE    = {252, 163, 17, 255};
static SDL_Color COLOR_JET       = {64, 201, 255, 255};
static SDL_Color COLOR_SINK      = {200, 80, 255, 255};
static SDL_Color COLOR_TEXT      = {245, 247, 250, 255};
static SDL_Color COLOR_SELECTED  = {255, 255, 255, 255};
static SDL_Color COLOR_GRID_LINE = {32, 36, 40, 255};
static SDL_Color COLOR_BOUNDARY_DISABLED = {45, 50, 58, 180};

static SDL_Color emitter_color(const FluidEmitter *em) {
    switch (em->type) {
    case EMITTER_DENSITY_SOURCE: return COLOR_SOURCE;
    case EMITTER_VELOCITY_JET:   return COLOR_JET;
    case EMITTER_SINK:           return COLOR_SINK;
    default:                     return COLOR_JET;
    }
}

static SDL_Color lighten_color(SDL_Color color, float factor) {
    if (factor < 0.0f) factor = 0.0f;
    if (factor > 1.0f) factor = 1.0f;
    SDL_Color result = color;
    result.r = (Uint8)(color.r + (Uint8)((255 - color.r) * factor));
    result.g = (Uint8)(color.g + (Uint8)((255 - color.g) * factor));
    result.b = (Uint8)(color.b + (Uint8)((255 - color.b) * factor));
    return result;
}

float scene_editor_canvas_object_visual_radius_px(const PresetObject *obj, int canvas_w) {
    if (!obj) return (float)SCENE_EDITOR_OBJECT_MIN_RADIUS_PX;
    float radius = obj->size_x * (float)canvas_w;
    if (radius < (float)SCENE_EDITOR_OBJECT_MIN_RADIUS_PX) {
        radius = (float)SCENE_EDITOR_OBJECT_MIN_RADIUS_PX;
    }
    return radius;
}

void scene_editor_canvas_object_visual_half_sizes_px(const PresetObject *obj,
                                                     int canvas_w,
                                                     int canvas_h,
                                                     float *out_half_w_px,
                                                     float *out_half_h_px) {
    if (!obj) return;
    float half_w = obj->size_x * (float)canvas_w;
    float half_h = obj->size_y * (float)canvas_h;
    if (half_w < (float)SCENE_EDITOR_OBJECT_MIN_HALF_PX) half_w = (float)SCENE_EDITOR_OBJECT_MIN_HALF_PX;
    if (half_h < (float)SCENE_EDITOR_OBJECT_MIN_HALF_PX) half_h = (float)SCENE_EDITOR_OBJECT_MIN_HALF_PX;
    if (out_half_w_px) *out_half_w_px = half_w;
    if (out_half_h_px) *out_half_h_px = half_h;
}

float scene_editor_canvas_object_handle_length_px(const PresetObject *obj,
                                                  int canvas_w,
                                                  int canvas_h) {
    if (!obj) return 0.0f;
    if (obj->type == PRESET_OBJECT_CIRCLE) {
        return scene_editor_canvas_object_visual_radius_px(obj, canvas_w);
    }
    float half_w = 0.0f, half_h = 0.0f;
    scene_editor_canvas_object_visual_half_sizes_px(obj, canvas_w, canvas_h, &half_w, &half_h);
    (void)half_h; // currently unused; handle follows the box's local +X axis.
    return half_w;
}

void scene_editor_canvas_project(int canvas_x,
                                 int canvas_y,
                                 int canvas_w,
                                 int canvas_h,
                                 float px,
                                 float py,
                                 int *out_x,
                                 int *out_y) {
    if (!out_x || !out_y) return;
    *out_x = canvas_x + (int)lroundf(px * (float)canvas_w);
    *out_y = canvas_y + (int)lroundf(py * (float)canvas_h);
}

void scene_editor_canvas_to_normalized(int canvas_x,
                                       int canvas_y,
                                       int canvas_w,
                                       int canvas_h,
                                       int sx,
                                       int sy,
                                       float *out_x,
                                       float *out_y) {
    if (!out_x || !out_y) return;
    float nx = (float)(sx - canvas_x) / (float)canvas_w;
    float ny = (float)(sy - canvas_y) / (float)canvas_h;
    if (nx < 0.0f) nx = 0.0f;
    if (nx > 1.0f) nx = 1.0f;
    if (ny < 0.0f) ny = 0.0f;
    if (ny > 1.0f) ny = 1.0f;
    *out_x = nx;
    *out_y = ny;
}

void scene_editor_canvas_draw_background(SDL_Renderer *renderer,
                                         int canvas_x,
                                         int canvas_y,
                                         int canvas_w,
                                         int canvas_h) {
    if (!renderer || canvas_w <= 0 || canvas_h <= 0) return;
    SDL_Rect rect = {canvas_x, canvas_y, canvas_w, canvas_h};
    SDL_SetRenderDrawColor(renderer,
                           COLOR_CANVAS.r,
                           COLOR_CANVAS.g,
                           COLOR_CANVAS.b,
                           COLOR_CANVAS.a);
    SDL_RenderFillRect(renderer, &rect);

    int grid_steps = 10;
    if (grid_steps < 1) grid_steps = 1;
    SDL_SetRenderDrawColor(renderer,
                           COLOR_GRID_LINE.r,
                           COLOR_GRID_LINE.g,
                           COLOR_GRID_LINE.b,
                           255);
    for (int i = 1; i < grid_steps; ++i) {
        int offset_x = (int)((float)canvas_w / (float)grid_steps * (float)i);
        int offset_y = (int)((float)canvas_h / (float)grid_steps * (float)i);
        SDL_RenderDrawLine(renderer,
                           canvas_x + offset_x,
                           canvas_y,
                           canvas_x + offset_x,
                           canvas_y + canvas_h);
        SDL_RenderDrawLine(renderer,
                           canvas_x,
                           canvas_y + offset_y,
                           canvas_x + canvas_w,
                           canvas_y + offset_y);
    }

    SDL_SetRenderDrawColor(renderer,
                           COLOR_SELECTED.r,
                           COLOR_SELECTED.g,
                           COLOR_SELECTED.b,
                           60);
    SDL_RenderDrawRect(renderer, &rect);
}

int scene_editor_canvas_hit_test(const FluidScenePreset *preset,
                                 int canvas_x,
                                 int canvas_y,
                                 int canvas_w,
                                 int canvas_h,
                                 int px,
                                 int py,
                                 EditorDragMode *mode) {
    if (!preset) return -1;
    int closest = -1;
    float best_dist = 1e9f;

    for (size_t i = 0; i < preset->emitter_count; ++i) {
        const FluidEmitter *em = &preset->emitters[i];
        int cx, cy;
        scene_editor_canvas_project(canvas_x, canvas_y, canvas_w, canvas_h,
                                    em->position_x, em->position_y,
                                    &cx, &cy);
        int radius_px = (int)(em->radius * (float)fmin(canvas_w, canvas_h));
        if (radius_px < 4) radius_px = 4;
        float dx = (float)px - (float)cx;
        float dy = (float)py - (float)cy;
        float dist = sqrtf(dx * dx + dy * dy);
        if (dist <= (float)radius_px) {
            if (dist < best_dist) {
                closest = (int)i;
                best_dist = dist;
                if (mode) *mode = DRAG_POSITION;
            }
        } else if (em->type != EMITTER_DENSITY_SOURCE) {
            int arrow_len = radius_px + 40;
            int hx = cx + (int)(em->dir_x * arrow_len);
            int hy = cy + (int)(em->dir_y * arrow_len);
            float adx = (float)px - (float)hx;
            float ady = (float)py - (float)hy;
            float adist = sqrtf(adx * adx + ady * ady);
            if (adist <= 18.0f && adist < best_dist) {
                closest = (int)i;
                best_dist = adist;
                if (mode) *mode = DRAG_DIRECTION;
            }
        }
    }
    return closest;
}

static void draw_circle(SDL_Renderer *renderer, int cx, int cy, int radius, SDL_Color color) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    for (int dy = -radius; dy <= radius; ++dy) {
        for (int dx = -radius; dx <= radius; ++dx) {
            if (dx * dx + dy * dy <= radius * radius) {
                SDL_RenderDrawPoint(renderer, cx + dx, cy + dy);
            }
        }
    }
}

static void draw_line(SDL_Renderer *renderer, int x0, int y0, int x1, int y1, SDL_Color color) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_RenderDrawLine(renderer, x0, y0, x1, y1);
}

void scene_editor_canvas_draw_emitters(SDL_Renderer *renderer,
                                       int canvas_x,
                                       int canvas_y,
                                       int canvas_w,
                                       int canvas_h,
                                       const FluidScenePreset *preset,
                                       int selected_emitter,
                                       int hover_emitter,
                                       TTF_Font *font_small) {
    if (!renderer || !preset) return;

    for (size_t i = 0; i < preset->emitter_count; ++i) {
        const FluidEmitter *em = &preset->emitters[i];
        int cx, cy;
        scene_editor_canvas_project(canvas_x, canvas_y, canvas_w, canvas_h,
                                    em->position_x, em->position_y,
                                    &cx, &cy);
        int radius_px = (int)(em->radius * (float)fmin(canvas_w, canvas_h));
        if (radius_px < 4) radius_px = 4;

        SDL_Color color = emitter_color(em);
        if ((int)i == selected_emitter) {
            SDL_Color outline = lighten_color(color, SCENE_EDITOR_SELECT_HIGHLIGHT_FACTOR);
            draw_circle(renderer, cx, cy, radius_px + 3, outline);
            color = outline;
        } else if ((int)i == hover_emitter) {
            color = lighten_color(color, 0.15f);
        }
        draw_circle(renderer, cx, cy, radius_px, color);

        if (em->type != EMITTER_DENSITY_SOURCE) {
            int arrow_len = radius_px + 40;
            int hx = cx + (int)(em->dir_x * arrow_len);
            int hy = cy + (int)(em->dir_y * arrow_len);
            draw_line(renderer, cx, cy, hx, hy, COLOR_SELECTED);
            draw_circle(renderer, hx, hy, 5, COLOR_SELECTED);
        }

        if ((int)i == hover_emitter && font_small) {
            char tooltip_pos[64];
            snprintf(tooltip_pos, sizeof(tooltip_pos), "Radius: %.2f", em->radius);
            (void)tooltip_pos;
            // Canvas-specific tooltip drawing can be added later if needed.
        }
    }
}

static float edge_function(const SDL_FPoint *a, const SDL_FPoint *b, float px, float py) {
    return (px - a->x) * (b->y - a->y) - (py - a->y) * (b->x - a->x);
}

static void draw_filled_triangle(SDL_Renderer *renderer,
                                 SDL_FPoint p0,
                                 SDL_FPoint p1,
                                 SDL_FPoint p2,
                                 SDL_Color color) {
    float area = edge_function(&p0, &p1, p2.x, p2.y);
    if (fabsf(area) < 1e-6f) return;
    float min_x = floorf(fminf(p0.x, fminf(p1.x, p2.x)));
    float max_x = ceilf(fmaxf(p0.x, fmaxf(p1.x, p2.x)));
    float min_y = floorf(fminf(p0.y, fminf(p1.y, p2.y)));
    float max_y = ceilf(fmaxf(p0.y, fmaxf(p1.y, p2.y)));
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    for (int y = (int)min_y; y <= (int)max_y; ++y) {
        for (int x = (int)min_x; x <= (int)max_x; ++x) {
            float px = (float)x + 0.5f;
            float py = (float)y + 0.5f;
            float w0 = edge_function(&p0, &p1, px, py);
            float w1 = edge_function(&p1, &p2, px, py);
            float w2 = edge_function(&p2, &p0, px, py);
            bool all_positive = (w0 >= 0.0f && w1 >= 0.0f && w2 >= 0.0f);
            bool all_negative = (w0 <= 0.0f && w1 <= 0.0f && w2 <= 0.0f);
            if (all_positive || all_negative) {
                SDL_RenderDrawPoint(renderer, x, y);
            }
        }
    }
}

static void draw_rotated_box(SDL_Renderer *renderer,
                             float cx,
                             float cy,
                             float half_w,
                             float half_h,
                             float angle,
                             SDL_Color color) {
    float cos_a = cosf(angle);
    float sin_a = sinf(angle);
    SDL_FPoint corners[4];
    float local_x[4] = {-half_w, half_w, half_w, -half_w};
    float local_y[4] = {-half_h, -half_h, half_h, half_h};
    for (int i = 0; i < 4; ++i) {
        float lx = local_x[i];
        float ly = local_y[i];
        float rx = lx * cos_a - ly * sin_a;
        float ry = lx * sin_a + ly * cos_a;
        corners[i].x = cx + rx;
        corners[i].y = cy + ry;
    }

    draw_filled_triangle(renderer, corners[0], corners[1], corners[2], color);
    draw_filled_triangle(renderer, corners[0], corners[2], corners[3], color);

    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    for (int i = 0; i < 4; ++i) {
        SDL_FPoint a = corners[i];
        SDL_FPoint b = corners[(i + 1) % 4];
        SDL_RenderDrawLine(renderer, (int)lroundf(a.x), (int)lroundf(a.y),
                           (int)lroundf(b.x), (int)lroundf(b.y));
    }
}

int scene_editor_canvas_hit_object(const FluidScenePreset *preset,
                                   int canvas_x,
                                   int canvas_y,
                                   int canvas_w,
                                   int canvas_h,
                                   int px,
                                   int py) {
    if (!preset) return -1;
    for (size_t i = 0; i < preset->object_count; ++i) {
        const PresetObject *obj = &preset->objects[i];
        int cx, cy;
        scene_editor_canvas_project(canvas_x, canvas_y, canvas_w, canvas_h,
                                    obj->position_x, obj->position_y,
                                    &cx, &cy);
        if (obj->type == PRESET_OBJECT_CIRCLE) {
            int radius = (int)lroundf(scene_editor_canvas_object_visual_radius_px(obj, canvas_w));
            float dx = (float)px - (float)cx;
            float dy = (float)py - (float)cy;
            if (dx * dx + dy * dy <= (float)(radius * radius)) {
                return (int)i;
            }
        } else {
            float half_w = 0.0f, half_h = 0.0f;
            scene_editor_canvas_object_visual_half_sizes_px(obj, canvas_w, canvas_h, &half_w, &half_h);
            float dx = (float)px - (float)cx;
            float dy = (float)py - (float)cy;
            float cos_a = cosf(obj->angle);
            float sin_a = sinf(obj->angle);
            float local_x = dx * cos_a + dy * sin_a;
            float local_y = -dx * sin_a + dy * cos_a;
            if (fabsf(local_x) <= half_w && fabsf(local_y) <= half_h) {
                return (int)i;
            }
        }
    }
    return -1;
}

bool scene_editor_canvas_object_handle_point(const FluidScenePreset *preset,
                                             int canvas_x,
                                             int canvas_y,
                                             int canvas_w,
                                             int canvas_h,
                                             int object_index,
                                             int *out_x,
                                             int *out_y) {
    if (!preset || object_index < 0 || object_index >= (int)preset->object_count) return false;
    if (!out_x || !out_y) return false;
    const PresetObject *obj = &preset->objects[object_index];
    int cx, cy;
    scene_editor_canvas_project(canvas_x,
                                canvas_y,
                                canvas_w,
                                canvas_h,
                                obj->position_x,
                                obj->position_y,
                                &cx,
                                &cy);
    float handle_len_px = scene_editor_canvas_object_handle_length_px(obj, canvas_w, canvas_h)
                          + (float)SCENE_EDITOR_OBJECT_HANDLE_MARGIN_PX;
    float angle = obj->angle;
    *out_x = cx + (int)lroundf(cosf(angle) * handle_len_px);
    *out_y = cy + (int)lroundf(sinf(angle) * handle_len_px);
    return true;
}

int scene_editor_canvas_hit_object_handle(const FluidScenePreset *preset,
                                          int canvas_x,
                                          int canvas_y,
                                          int canvas_w,
                                          int canvas_h,
                                          int px,
                                          int py) {
    if (!preset) return -1;
    const int handle_radius = SCENE_EDITOR_OBJECT_HANDLE_HIT_RADIUS_PX;
    for (size_t i = 0; i < preset->object_count; ++i) {
        int hx = 0, hy = 0;
        if (!scene_editor_canvas_object_handle_point(preset,
                                                     canvas_x,
                                                     canvas_y,
                                                     canvas_w,
                                                     canvas_h,
                                                     (int)i,
                                                     &hx,
                                                     &hy)) {
            continue;
        }
        float dx = (float)px - (float)hx;
        float dy = (float)py - (float)hy;
        if ((dx * dx + dy * dy) <= (float)(handle_radius * handle_radius)) {
            return (int)i;
        }
    }
    return -1;
}

int scene_editor_canvas_hit_edge(int canvas_x,
                                 int canvas_y,
                                 int canvas_w,
                                 int canvas_h,
                                 int px,
                                 int py) {
    int canvas_right = canvas_x + canvas_w;
    int canvas_bottom = canvas_y + canvas_h;
    int margin = SCENE_EDITOR_BOUNDARY_HIT_MARGIN;

    bool inside_x = (px >= canvas_x - margin) && (px <= canvas_right + margin);
    bool inside_y = (py >= canvas_y - margin) && (py <= canvas_bottom + margin);
    if (!inside_x || !inside_y) return -1;

    if (py >= canvas_y - margin && py <= canvas_y + margin) {
        return BOUNDARY_EDGE_TOP;
    }
    if (py >= canvas_bottom - margin && py <= canvas_bottom + margin) {
        return BOUNDARY_EDGE_BOTTOM;
    }
    if (px >= canvas_x - margin && px <= canvas_x + margin) {
        return BOUNDARY_EDGE_LEFT;
    }
    if (px >= canvas_right - margin && px <= canvas_right + margin) {
        return BOUNDARY_EDGE_RIGHT;
    }
    return -1;
}

void scene_editor_canvas_draw_objects(SDL_Renderer *renderer,
                                      int canvas_x,
                                      int canvas_y,
                                      int canvas_w,
                                      int canvas_h,
                                      const FluidScenePreset *preset,
                                      int selected_object,
                                      int hover_object) {
    if (!renderer || !preset) return;

    for (size_t i = 0; i < preset->object_count; ++i) {
        const PresetObject *obj = &preset->objects[i];
        int cx, cy;
        scene_editor_canvas_project(canvas_x, canvas_y, canvas_w, canvas_h,
                                    obj->position_x, obj->position_y,
                                    &cx, &cy);
        SDL_Color base = (obj->type == PRESET_OBJECT_BOX)
                             ? (SDL_Color){170, 120, 80, 255}
                             : (SDL_Color){255, 80, 80, 255};
        if ((int)i == selected_object) {
            base = lighten_color(base, SCENE_EDITOR_SELECT_HIGHLIGHT_FACTOR);
        } else if ((int)i == hover_object) {
            base = lighten_color(base, 0.15f);
        }
        if (obj->type == PRESET_OBJECT_CIRCLE) {
            int radius = (int)lroundf(scene_editor_canvas_object_visual_radius_px(obj, canvas_w));
            draw_circle(renderer, cx, cy, radius, base);
        } else {
            float half_w = 0.0f, half_h = 0.0f;
            scene_editor_canvas_object_visual_half_sizes_px(obj, canvas_w, canvas_h, &half_w, &half_h);
            draw_rotated_box(renderer,
                             (float)cx,
                             (float)cy,
                             half_w,
                             half_h,
                             obj->angle,
                             base);
        }

        if ((int)i == selected_object) {
            int hx = 0, hy = 0;
            if (scene_editor_canvas_object_handle_point(preset,
                                                        canvas_x,
                                                        canvas_y,
                                                        canvas_w,
                                                        canvas_h,
                                                        (int)i,
                                                        &hx,
                                                        &hy)) {
                SDL_SetRenderDrawColor(renderer,
                                       COLOR_SELECTED.r,
                                       COLOR_SELECTED.g,
                                       COLOR_SELECTED.b,
                                       255);
                SDL_RenderDrawLine(renderer, cx, cy, hx, hy);
                draw_circle(renderer, hx, hy, 6, COLOR_SELECTED);
            }
        }
    }
}

static SDL_Color boundary_color(const BoundaryFlow *flow) {
    if (!flow || flow->mode == BOUNDARY_FLOW_DISABLED) {
        return COLOR_BOUNDARY_DISABLED;
    }
    if (flow->mode == BOUNDARY_FLOW_RECEIVE) {
        return COLOR_SINK;
    }
    return COLOR_SOURCE;
}

void scene_editor_canvas_draw_boundary_flows(SDL_Renderer *renderer,
                                             int canvas_x,
                                             int canvas_y,
                                             int canvas_w,
                                             int canvas_h,
                                             const BoundaryFlow flows[BOUNDARY_EDGE_COUNT],
                                             int hover_edge,
                                             int selected_edge,
                                             bool edit_mode) {
    if (!renderer || !flows) return;
    SDL_Rect rects[BOUNDARY_EDGE_COUNT];
    rects[BOUNDARY_EDGE_TOP] = (SDL_Rect){canvas_x, canvas_y, canvas_w, 12};
    rects[BOUNDARY_EDGE_BOTTOM] = (SDL_Rect){canvas_x, canvas_y + canvas_h - 12, canvas_w, 12};
    rects[BOUNDARY_EDGE_LEFT] = (SDL_Rect){canvas_x, canvas_y, 12, canvas_h};
    rects[BOUNDARY_EDGE_RIGHT] = (SDL_Rect){canvas_x + canvas_w - 12, canvas_y, 12, canvas_h};

    for (int edge = 0; edge < BOUNDARY_EDGE_COUNT; ++edge) {
        SDL_Color color = boundary_color(&flows[edge]);
        if (selected_edge == edge) {
            color = lighten_color(color, 0.4f);
        } else if (hover_edge == edge) {
            color = lighten_color(color, edit_mode ? 0.3f : 0.15f);
        }
        SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
        SDL_RenderFillRect(renderer, &rects[edge]);
    }
}

void scene_editor_canvas_draw_tooltip(SDL_Renderer *renderer,
                                      TTF_Font *font,
                                      int x,
                                      int y,
                                      const char *lines[],
                                      int line_count) {
    if (!renderer || !font || !lines || line_count <= 0) return;
    SDL_Color bg = {20, 22, 28, 230};
    SDL_Color border = {255, 255, 255, 60};
    SDL_Color text = COLOR_TEXT;

    int padding = 6;
    int max_w = 0;
    int total_h = 0;
    SDL_Texture *textures[8] = {0};
    SDL_Surface *surfaces[8] = {0};
    if (line_count > 8) line_count = 8;

    for (int i = 0; i < line_count; ++i) {
        if (!lines[i]) continue;
        SDL_Surface *surf = TTF_RenderUTF8_Blended(font, lines[i], text);
        if (!surf) continue;
        SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer, surf);
        if (!tex) {
            SDL_FreeSurface(surf);
            continue;
        }
        textures[i] = tex;
        surfaces[i] = surf;
        if (surf->w > max_w) max_w = surf->w;
        total_h += surf->h;
    }
    if (max_w == 0 || total_h == 0) {
        for (int i = 0; i < line_count; ++i) {
            if (textures[i]) SDL_DestroyTexture(textures[i]);
            if (surfaces[i]) SDL_FreeSurface(surfaces[i]);
        }
        return;
    }
    int spacing = 2;
    total_h += spacing * (line_count - 1);

    SDL_Rect rect = {
        .x = x + 16,
        .y = y + 16,
        .w = max_w + padding * 2,
        .h = total_h + padding * 2
    };

    SDL_SetRenderDrawColor(renderer, bg.r, bg.g, bg.b, bg.a);
    SDL_RenderFillRect(renderer, &rect);
    SDL_SetRenderDrawColor(renderer, border.r, border.g, border.b, border.a);
    SDL_RenderDrawRect(renderer, &rect);

    int y_cursor = rect.y + padding;
    for (int i = 0; i < line_count; ++i) {
        SDL_Texture *tex = textures[i];
        SDL_Surface *surf = surfaces[i];
        if (!tex || !surf) continue;
        SDL_Rect dst = {rect.x + padding, y_cursor, surf->w, surf->h};
        SDL_RenderCopy(renderer, tex, NULL, &dst);
        y_cursor += surf->h + spacing;
    }

    for (int i = 0; i < line_count; ++i) {
        if (textures[i]) SDL_DestroyTexture(textures[i]);
        if (surfaces[i]) SDL_FreeSurface(surfaces[i]);
    }
}

void scene_editor_canvas_draw_name(SDL_Renderer *renderer,
                                   int canvas_x,
                                   int canvas_y,
                                   int canvas_w,
                                   int canvas_h,
                                   TTF_Font *font_main,
                                   TTF_Font *font_small,
                                   const char *name,
                                   bool renaming,
                                   const TextInputField *input) {
    if (!renderer) return;
    (void)canvas_h;
    SDL_Rect rect = {
        .x = canvas_x,
        .y = canvas_y - 50,
        .w = canvas_w,
        .h = 36
    };
    if (rect.y < 20) rect.y = 20;

    if (renaming && input && font_main) {
        SDL_SetRenderDrawColor(renderer, COLOR_CANVAS.r, COLOR_CANVAS.g, COLOR_CANVAS.b, 240);
        SDL_RenderFillRect(renderer, &rect);
        SDL_SetRenderDrawColor(renderer, COLOR_SELECTED.r, COLOR_SELECTED.g, COLOR_SELECTED.b, 255);
        SDL_RenderDrawRect(renderer, &rect);
        const char *text = text_input_value(input);
        SDL_Surface *surf = TTF_RenderUTF8_Blended(font_main, text, COLOR_TEXT);
        int text_w = 0;
        if (surf) {
            SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer, surf);
            text_w = surf->w;
            SDL_Rect dst = {rect.x + 8, rect.y + rect.h / 2 - surf->h / 2,
                            surf->w, surf->h};
            SDL_RenderCopy(renderer, tex, NULL, &dst);
            SDL_DestroyTexture(tex);
            SDL_FreeSurface(surf);
        }
        if (input->caret_visible) {
            int caret_x = rect.x + 8 + text_w + 2;
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            SDL_RenderDrawLine(renderer, caret_x, rect.y + 6,
                               caret_x, rect.y + rect.h - 6);
        }
    } else if (font_main) {
        const char *title = (name && name[0]) ? name : "Untitled Preset";
        SDL_Surface *surf = TTF_RenderUTF8_Blended(font_main, title, COLOR_TEXT);
        if (surf) {
            SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer, surf);
            SDL_Rect dst = {rect.x, rect.y, surf->w, surf->h};
            SDL_RenderCopy(renderer, tex, NULL, &dst);
            SDL_DestroyTexture(tex);
            SDL_FreeSurface(surf);
        }
    }
    (void)font_small;
}
