#include "render/debug_draw_objects.h"

#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define OBJECT_BORDER_THICKNESS 2

static float safe_scale(int target, int source) {
    if (source <= 0) return 1.0f;
    if (target <= 0) return 1.0f;
    return (float)target / (float)source;
}

static void draw_rotated_box_outline(const SceneObject *obj,
                                     float scale_x,
                                     float scale_y,
                                     SDL_Renderer *renderer,
                                     SDL_Color color) {
    if (!obj || !renderer) return;
    float cos_a = cosf(obj->body.angle);
    float sin_a = sinf(obj->body.angle);
    SDL_SetRenderDrawColor(renderer,
                           color.r,
                           color.g,
                           color.b,
                           color.a);

    for (int t = 0; t < OBJECT_BORDER_THICKNESS; ++t) {
        float shrink_x = (scale_x > 0.0f) ? (float)t / scale_x : 0.0f;
        float shrink_y = (scale_y > 0.0f) ? (float)t / scale_y : 0.0f;
        float hx = obj->body.half_extents.x - shrink_x;
        float hy = obj->body.half_extents.y - shrink_y;
        if (hx <= 0.0f || hy <= 0.0f) break;

        SDL_Point pts[5];
        const float corner_x[4] = {-hx,  hx,  hx, -hx};
        const float corner_y[4] = {-hy, -hy,  hy,  hy};
        for (int i = 0; i < 4; ++i) {
            float lx = corner_x[i];
            float ly = corner_y[i];
            float rx = lx * cos_a - ly * sin_a;
            float ry = lx * sin_a + ly * cos_a;
            float world_x = obj->body.position.x + rx;
            float world_y = obj->body.position.y + ry;
            pts[i].x = (int)lroundf(world_x * scale_x);
            pts[i].y = (int)lroundf(world_y * scale_y);
        }
        pts[4] = pts[0];
        SDL_RenderDrawLines(renderer, pts, 5);
    }
}

void debug_draw_object_borders(const SceneState *scene,
                               SDL_Renderer *renderer,
                               int window_w,
                               int window_h) {
#if OBJECT_BORDER_THICKNESS <= 0
    (void)scene;
    (void)renderer;
    (void)window_w;
    (void)window_h;
    return;
#else
    if (!scene || !scene->config || !renderer) return;
    const ObjectManager *objects = &scene->objects;
    if (!objects || objects->count == 0) return;

    float scale_x = safe_scale(window_w, scene->config->window_w);
    float scale_y = safe_scale(window_h, scene->config->window_h);

    SDL_Color circle_color = {255, 80, 80, 255};
    SDL_Color box_color    = {170, 120, 80, 255};
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    const int segments = 48;
    for (int i = 0; i < objects->count; ++i) {
        const SceneObject *obj = &objects->objects[i];
        int cx = (int)lroundf(obj->body.position.x * scale_x);
        int cy = (int)lroundf(obj->body.position.y * scale_y);

        if (obj->type == SCENE_OBJECT_CIRCLE) {
            float radius_scale = (scale_x + scale_y) * 0.5f;
            if (radius_scale <= 0.0f) radius_scale = scale_x > 0.0f ? scale_x : 1.0f;
            int radius = (int)lroundf(obj->body.radius * radius_scale);
            if (radius < 2) radius = 2;
            SDL_SetRenderDrawColor(renderer,
                                   circle_color.r,
                                   circle_color.g,
                                   circle_color.b,
                                   circle_color.a);
            for (int t = 0; t < OBJECT_BORDER_THICKNESS; ++t) {
                int r = radius - t;
                if (r <= 0) break;
                float prev_x = (float)(cx + r);
                float prev_y = (float)cy;
                for (int seg = 1; seg <= segments; ++seg) {
                    float theta = (float)seg / (float)segments * 2.0f * (float)M_PI;
                    float cur_x = (float)cx + cosf(theta) * (float)r;
                    float cur_y = (float)cy + sinf(theta) * (float)r;
                    SDL_RenderDrawLine(renderer,
                                       (int)lroundf(prev_x),
                                       (int)lroundf(prev_y),
                                       (int)lroundf(cur_x),
                                       (int)lroundf(cur_y));
                    prev_x = cur_x;
                    prev_y = cur_y;
                }
            }
        } else if (obj->type == SCENE_OBJECT_BOX) {
            draw_rotated_box_outline(obj, scale_x, scale_y, renderer, box_color);
        }
    }
#endif
}
