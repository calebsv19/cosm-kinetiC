#include "app/structural/structural_preset_editor.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_vulkan.h>

#include "app/editor/scene_editor_widgets.h"
#include "app/structural/structural_editor.h"
#include "input/input.h"
#include "physics/structural/structural_scene.h"
#include "physics/structural/structural_solver.h"
#include "app/structural/structural_render.h"
#include "vk_renderer.h"
#include "render/vk_shared_device.h"

typedef struct StructuralPresetEditor {
    SDL_Window   *window;
    SDL_Renderer *renderer;
    TTF_Font     *font_main;
    TTF_Font     *font_small;
    AppConfig     cfg;
    StructuralScene scene;
    StructuralEditor editor;
    StructuralSolveResult last_result;
    bool scale_initialized;
    float scale_stress;
    float scale_moment;
    float scale_shear;
    float scale_combined;
    bool solve_requested;
    bool running;
    bool applied;
    int pointer_x;
    int pointer_y;

    int canvas_x;
    int canvas_y;
    int canvas_w;
    int canvas_h;
    int panel_x;
    int panel_y;
    int panel_w;
    int panel_h;
    int preview_x;
    int preview_y;
    int preview_w;
    int preview_h;
    float scale;
    float ground_y;
    float ground_snap_dist;
    bool  ground_snap_enabled;

    EditorButton btn_save;
    EditorButton btn_cancel;
    EditorButton btn_ground;
    EditorButton btn_gravity;
    EditorButton btn_gravity_minus;
    EditorButton btn_gravity_plus;

    char preset_path[256];
} StructuralPresetEditor;

static SDL_Window *g_struct_window = NULL;
static VkRenderer g_struct_renderer_storage;
static SDL_Renderer *g_struct_renderer = NULL;
static bool g_struct_initialized = false;
static bool g_struct_use_shared_device = false;

static void struct_log_window_sizes(SDL_Window *window, const char *tag) {
    if (!window || !tag) return;
    int win_w = 0;
    int win_h = 0;
    int drawable_w = 0;
    int drawable_h = 0;
    SDL_GetWindowSize(window, &win_w, &win_h);
    SDL_Vulkan_GetDrawableSize(window, &drawable_w, &drawable_h);
    fprintf(stderr, "[struct-editor] %s win=%dx%d drawable=%dx%d\n",
            tag, win_w, win_h, drawable_w, drawable_h);
}

static bool struct_wait_for_drawable(SDL_Window *window, const char *tag) {
    if (!window) return false;
    for (int i = 0; i < 60; ++i) {
        int drawable_w = 0;
        int drawable_h = 0;
        SDL_PumpEvents();
        SDL_Vulkan_GetDrawableSize(window, &drawable_w, &drawable_h);
        if (drawable_w > 0 && drawable_h > 0) {
            return true;
        }
        SDL_Delay(16);
    }
    struct_log_window_sizes(window, tag);
    return false;
}

static void render_text(SDL_Renderer *renderer, TTF_Font *font,
                        int x, int y, SDL_Color color, const char *text) {
    if (!renderer || !font || !text) return;
    SDL_Surface *surface = TTF_RenderUTF8_Blended(font, text, color);
    if (!surface) return;
    SDL_Rect dst = {x, y, surface->w, surface->h};
    VkRendererTexture texture = {0};
    if (vk_renderer_upload_sdl_surface_with_filter((VkRenderer *)renderer,
                                                   surface,
                                                   &texture,
                                                   VK_FILTER_LINEAR) == VK_SUCCESS) {
        vk_renderer_draw_texture((VkRenderer *)renderer, &texture, NULL, &dst);
        vk_renderer_queue_texture_destroy((VkRenderer *)renderer, &texture);
    }
    SDL_FreeSurface(surface);
}

static void update_layout(StructuralPresetEditor *editor) {
    if (!editor || !editor->renderer) return;
    int w = 0;
    int h = 0;
    SDL_GetWindowSize(editor->window, &w, &h);

    int margin = 16;
    editor->panel_w = 320;
    editor->panel_x = w - editor->panel_w - margin;
    editor->panel_y = margin;
    editor->panel_h = h - margin * 2;

    editor->canvas_x = margin;
    editor->canvas_y = margin;
    editor->canvas_w = editor->panel_x - margin - editor->canvas_x;
    editor->canvas_h = h - margin * 2;
    if (editor->canvas_w < 50) editor->canvas_w = 50;
    if (editor->canvas_h < 50) editor->canvas_h = 50;

    float scale_x = (float)editor->canvas_w / (float)editor->cfg.window_w;
    float scale_y = (float)editor->canvas_h / (float)editor->cfg.window_h;
    editor->scale = (scale_x < scale_y) ? scale_x : scale_y;
    if (editor->scale < 0.01f) editor->scale = 0.01f;

    editor->preview_w = (int)(editor->cfg.window_w * editor->scale);
    editor->preview_h = (int)(editor->cfg.window_h * editor->scale);
    editor->preview_x = editor->canvas_x + (editor->canvas_w - editor->preview_w) / 2;
    editor->preview_y = editor->canvas_y + (editor->canvas_h - editor->preview_h) / 2;

    editor->ground_y = (float)editor->cfg.window_h - editor->scene.ground_offset;
    if (editor->ground_y < 0.0f) editor->ground_y = 0.0f;

    editor->btn_save.rect = (SDL_Rect){editor->panel_x + 16,
                                       editor->panel_y + editor->panel_h - 96,
                                       editor->panel_w - 32,
                                       38};
    editor->btn_save.label = "Save Preset";
    editor->btn_save.enabled = true;

    editor->btn_cancel.rect = (SDL_Rect){editor->panel_x + 16,
                                         editor->panel_y + editor->panel_h - 52,
                                         editor->panel_w - 32,
                                         34};
    editor->btn_cancel.label = "Cancel";
    editor->btn_cancel.enabled = true;

    editor->btn_ground.rect = (SDL_Rect){editor->panel_x + 16,
                                         editor->panel_y + editor->panel_h - 148,
                                         editor->panel_w - 32,
                                         34};
    editor->btn_ground.label = "Attach to Ground";
    editor->btn_ground.enabled = true;

    editor->btn_gravity.rect = (SDL_Rect){editor->panel_x + 16,
                                          editor->panel_y + editor->panel_h - 200,
                                          editor->panel_w - 32,
                                          34};
    editor->btn_gravity.label = editor->scene.gravity_enabled ? "Gravity: On" : "Gravity: Off";
    editor->btn_gravity.enabled = true;

    int half_w = (editor->panel_w - 40) / 2;
    editor->btn_gravity_minus.rect = (SDL_Rect){editor->panel_x + 16,
                                                editor->panel_y + editor->panel_h - 240,
                                                half_w,
                                                32};
    editor->btn_gravity_minus.label = "G-";
    editor->btn_gravity_minus.enabled = true;

    editor->btn_gravity_plus.rect = (SDL_Rect){editor->panel_x + 24 + half_w,
                                               editor->panel_y + editor->panel_h - 240,
                                               half_w,
                                               32};
    editor->btn_gravity_plus.label = "G+";
    editor->btn_gravity_plus.enabled = true;
}

static void apply_snap(const StructuralPresetEditor *editor, float *x, float *y) {
    if (!editor || !x || !y) return;
    if (!editor->editor.snap_to_grid) return;
    float grid = editor->editor.grid_size;
    if (grid <= 0.0f) return;
    *x = roundf(*x / grid) * grid;
    *y = roundf(*y / grid) * grid;
}

static void apply_ground_snap(const StructuralPresetEditor *editor, float *x, float *y) {
    if (!editor || !x || !y) return;
    if (!editor->ground_snap_enabled) return;
    if (fabsf(*y - editor->ground_y) <= editor->ground_snap_dist) {
        *y = editor->ground_y;
    }
}

static void draw_circle(SDL_Renderer *renderer, float cx, float cy, float radius) {
    const int segments = 16;
    for (int i = 0; i < segments; ++i) {
        float a0 = (float)i / (float)segments * 6.2831853f;
        float a1 = (float)(i + 1) / (float)segments * 6.2831853f;
        int x0 = (int)(cx + cosf(a0) * radius);
        int y0 = (int)(cy + sinf(a0) * radius);
        int x1 = (int)(cx + cosf(a1) * radius);
        int y1 = (int)(cy + sinf(a1) * radius);
        SDL_RenderDrawLine(renderer, x0, y0, x1, y1);
    }
}

static void draw_arrow(SDL_Renderer *renderer,
                       float x0, float y0,
                       float x1, float y1) {
    SDL_RenderDrawLine(renderer, (int)x0, (int)y0, (int)x1, (int)y1);
    float dx = x1 - x0;
    float dy = y1 - y0;
    float len = sqrtf(dx * dx + dy * dy);
    if (len < 1e-3f) return;
    float ux = dx / len;
    float uy = dy / len;
    float arrow = 6.0f;
    float left_x = x1 - ux * arrow - uy * arrow * 0.5f;
    float left_y = y1 - uy * arrow + ux * arrow * 0.5f;
    float right_x = x1 - ux * arrow + uy * arrow * 0.5f;
    float right_y = y1 - uy * arrow - ux * arrow * 0.5f;
    SDL_RenderDrawLine(renderer, (int)x1, (int)y1, (int)left_x, (int)left_y);
    SDL_RenderDrawLine(renderer, (int)x1, (int)y1, (int)right_x, (int)right_y);
}

static void draw_constraints(SDL_Renderer *renderer, const StructNode *node,
                             float sx, float sy, float scale) {
    if (!node) return;
    float size = 6.0f;
    if (node->fixed_x) {
        SDL_RenderDrawLine(renderer, (int)(sx - size), (int)(sy - size),
                           (int)(sx - size), (int)(sy + size));
    }
    if (node->fixed_y) {
        SDL_RenderDrawLine(renderer, (int)(sx - size), (int)(sy + size),
                           (int)(sx + size), (int)(sy + size));
    }
    if (node->fixed_theta) {
        draw_circle(renderer, sx, sy, 6.0f * fmaxf(0.5f, scale));
    }
}

static float max_abs_edge_stress(const StructuralScene *scene) {
    float max_val = 0.0f;
    if (!scene) return max_val;
    for (size_t i = 0; i < scene->edge_count; ++i) {
        float v = fabsf(scene->edges[i].axial_stress);
        if (v > max_val) max_val = v;
    }
    return max_val;
}

static float max_abs_edge_moment(const StructuralScene *scene) {
    float max_val = 0.0f;
    if (!scene) return max_val;
    for (size_t i = 0; i < scene->edge_count; ++i) {
        float m1 = fabsf(scene->edges[i].bending_moment_a);
        float m2 = fabsf(scene->edges[i].bending_moment_b);
        if (m1 > max_val) max_val = m1;
        if (m2 > max_val) max_val = m2;
    }
    return max_val;
}

static float max_abs_edge_shear(const StructuralScene *scene) {
    float max_val = 0.0f;
    if (!scene) return max_val;
    for (size_t i = 0; i < scene->edge_count; ++i) {
        float v1 = fabsf(scene->edges[i].shear_force_a);
        float v2 = fabsf(scene->edges[i].shear_force_b);
        if (v1 > max_val) max_val = v1;
        if (v2 > max_val) max_val = v2;
    }
    return max_val;
}

static float clamp01(float v) {
    if (v < 0.0f) return 0.0f;
    if (v > 1.0f) return 1.0f;
    return v;
}

static int compare_float_asc(const void *a, const void *b) {
    float fa = *(const float *)a;
    float fb = *(const float *)b;
    return (fa > fb) - (fa < fb);
}

static float compute_scale_from_values(float *values,
                                       size_t count,
                                       bool use_percentile,
                                       float percentile) {
    if (!values || count == 0) return 1.0f;
    qsort(values, count, sizeof(float), compare_float_asc);
    float p = clamp01(percentile);
    if (p < 0.5f) p = 0.5f;
    size_t idx = (size_t)lroundf((float)(count - 1) * p);
    if (idx >= count) idx = count - 1;
    float scale = use_percentile ? values[idx] : values[count - 1];
    if (scale < 1e-6f) scale = 1.0f;
    return scale;
}

static void compute_edge_scales(const StructuralScene *scene,
                                bool use_percentile,
                                float percentile,
                                float *out_stress,
                                float *out_moment,
                                float *out_shear,
                                float *out_combined) {
    if (!out_stress || !out_moment || !out_shear || !out_combined) return;
    if (!scene || scene->edge_count == 0) {
        *out_stress = 1.0f;
        *out_moment = 1.0f;
        *out_shear = 1.0f;
        *out_combined = 1.0f;
        return;
    }

    size_t count = scene->edge_count;
    float *stress_vals = (float *)malloc(sizeof(float) * count);
    float *moment_vals = (float *)malloc(sizeof(float) * count);
    float *shear_vals = (float *)malloc(sizeof(float) * count);
    float *combined_vals = (float *)malloc(sizeof(float) * count);
    if (!stress_vals || !moment_vals || !shear_vals || !combined_vals) {
        free(stress_vals);
        free(moment_vals);
        free(shear_vals);
        free(combined_vals);
        *out_stress = fmaxf(1.0f, max_abs_edge_stress(scene));
        *out_moment = fmaxf(1.0f, max_abs_edge_moment(scene));
        *out_shear = fmaxf(1.0f, max_abs_edge_shear(scene));
        *out_combined = fmaxf(1.0f, *out_stress);
        return;
    }

    for (size_t i = 0; i < count; ++i) {
        const StructEdge *edge = &scene->edges[i];
        float axial = fabsf(edge->axial_stress);
        float moment = fmaxf(fabsf(edge->bending_moment_a), fabsf(edge->bending_moment_b));
        float shear_avg = 0.5f * (edge->shear_force_a + edge->shear_force_b);
        float shear = fmaxf(fabsf(edge->shear_force_a), fabsf(edge->shear_force_b));
        stress_vals[i] = axial;
        moment_vals[i] = moment;
        shear_vals[i] = shear;
        combined_vals[i] = sqrtf(axial * axial + shear_avg * shear_avg);
    }

    *out_stress = compute_scale_from_values(stress_vals, count, use_percentile, percentile);
    *out_moment = compute_scale_from_values(moment_vals, count, use_percentile, percentile);
    *out_shear = compute_scale_from_values(shear_vals, count, use_percentile, percentile);
    *out_combined = compute_scale_from_values(combined_vals, count, use_percentile, percentile);

    free(stress_vals);
    free(moment_vals);
    free(shear_vals);
    free(combined_vals);
}

static SDL_Color stress_color_with_yield(const StructuralScene *scene,
                                         const StructEdge *edge,
                                         float stress_max,
                                         float gamma) {
    if (!scene || !edge) return (SDL_Color){110, 110, 120, 255};
    SDL_Color base = structural_render_color_diverging(-edge->axial_stress, stress_max, gamma);
    float sigma_y = 0.0f;
    if (edge->material_index >= 0 && edge->material_index < (int)scene->material_count) {
        sigma_y = scene->materials[edge->material_index].sigma_y;
    }
    if (sigma_y > 0.0f) {
        float over = fabsf(edge->axial_stress) - sigma_y;
        if (over > 0.0f) {
            float t = over / fmaxf(1e-6f, sigma_y * 0.5f);
            if (t > 1.0f) t = 1.0f;
            SDL_Color warn = {170, 90, 210, 255};
            SDL_Color out = base;
            out.r = (Uint8)((float)base.r + ((float)warn.r - (float)base.r) * t);
            out.g = (Uint8)((float)base.g + ((float)warn.g - (float)base.g) * t);
            out.b = (Uint8)((float)base.b + ((float)warn.b - (float)base.b) * t);
            return out;
        }
    }
    return base;
}

static void draw_moment_icon(SDL_Renderer *renderer, float cx, float cy, float radius, float moment) {
    if (fabsf(moment) < 1e-4f) return;
    draw_circle(renderer, cx, cy, radius);
    float tip_x = (moment >= 0.0f) ? cx + radius : cx - radius;
    float tip_y = cy;
    float sign = (moment >= 0.0f) ? -1.0f : 1.0f;
    SDL_RenderDrawLine(renderer,
                       (int)tip_x, (int)tip_y,
                       (int)(tip_x - sign * 4.0f), (int)(tip_y - 4.0f));
    SDL_RenderDrawLine(renderer,
                       (int)tip_x, (int)tip_y,
                       (int)(tip_x - sign * 4.0f), (int)(tip_y + 4.0f));
}

static void world_to_screen(const StructuralPresetEditor *editor,
                            float wx, float wy, float *sx, float *sy) {
    if (!editor || !sx || !sy) return;
    *sx = editor->preview_x + wx * editor->scale;
    *sy = editor->preview_y + wy * editor->scale;
}

static void screen_to_world(const StructuralPresetEditor *editor,
                            int sx, int sy, float *wx, float *wy) {
    if (!editor || !wx || !wy) return;
    *wx = ((float)sx - (float)editor->preview_x) / editor->scale;
    *wy = ((float)sy - (float)editor->preview_y) / editor->scale;
}

static float distance_to_segment(float px, float py,
                                 float ax, float ay,
                                 float bx, float by) {
    float dx = bx - ax;
    float dy = by - ay;
    float len2 = dx * dx + dy * dy;
    if (len2 < 1e-6f) {
        float ex = px - ax;
        float ey = py - ay;
        return sqrtf(ex * ex + ey * ey);
    }
    float t = ((px - ax) * dx + (py - ay) * dy) / len2;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    float cx = ax + t * dx;
    float cy = ay + t * dy;
    float ex = px - cx;
    float ey = py - cy;
    return sqrtf(ex * ex + ey * ey);
}

static bool point_in_preview(const StructuralPresetEditor *editor, int x, int y) {
    if (!editor) return false;
    return x >= editor->preview_x && x <= (editor->preview_x + editor->preview_w) &&
           y >= editor->preview_y && y <= (editor->preview_y + editor->preview_h);
}

static void render_scene(StructuralPresetEditor *editor) {
    if (!editor || !editor->renderer) return;
    StructuralScene *scene = &editor->scene;
    StructuralEditor *ed = &editor->editor;
    SDL_Renderer *renderer = editor->renderer;

    SDL_SetRenderDrawColor(renderer, 16, 18, 20, 255);
    SDL_Rect clear_rect = {0, 0, editor->panel_x + editor->panel_w, editor->panel_y + editor->panel_h};
    if (editor->window) {
        SDL_GetWindowSize(editor->window, &clear_rect.w, &clear_rect.h);
    }
    SDL_RenderFillRect(renderer, &clear_rect);

    SDL_Rect panel = {editor->panel_x, editor->panel_y, editor->panel_w, editor->panel_h};
    SDL_SetRenderDrawColor(renderer, 32, 36, 40, 255);
    SDL_RenderFillRect(renderer, &panel);
    SDL_SetRenderDrawColor(renderer, 45, 50, 58, 255);
    SDL_RenderDrawRect(renderer, &panel);

    SDL_Rect preview = {editor->preview_x, editor->preview_y, editor->preview_w, editor->preview_h};
    SDL_SetRenderDrawColor(renderer, 60, 65, 72, 255);
    SDL_RenderDrawRect(renderer, &preview);

    float ground_sx = 0.0f;
    float ground_sy = 0.0f;
    world_to_screen(editor, 0.0f, editor->ground_y, &ground_sx, &ground_sy);
    SDL_SetRenderDrawColor(renderer, 90, 80, 70, 255);
    SDL_RenderDrawLine(renderer,
                       editor->preview_x,
                       (int)ground_sy,
                       editor->preview_x + editor->preview_w,
                       (int)ground_sy);

    if (ed->snap_to_grid && ed->grid_size > 1.0f) {
        SDL_SetRenderDrawColor(renderer, 30, 34, 38, 255);
        float step = ed->grid_size * editor->scale;
        if (step >= 4.0f) {
            for (float x = 0.0f; x <= editor->cfg.window_w; x += ed->grid_size) {
                float sx = editor->preview_x + x * editor->scale;
                for (float y = 0.0f; y <= editor->cfg.window_h; y += ed->grid_size) {
                    float sy = editor->preview_y + y * editor->scale;
                    SDL_Rect dot = {(int)sx, (int)sy, 1, 1};
                    SDL_RenderFillRect(renderer, &dot);
                }
            }
        }
    }

    if (!ed->scale_freeze || !editor->scale_initialized) {
        compute_edge_scales(scene,
                            ed->scale_use_percentile,
                            ed->scale_percentile,
                            &editor->scale_stress,
                            &editor->scale_moment,
                            &editor->scale_shear,
                            &editor->scale_combined);
        editor->scale_initialized = true;
    }
    float stress_scale = editor->scale_stress;
    float moment_scale = editor->scale_moment;
    float shear_scale = editor->scale_shear;
    float combined_scale = editor->scale_combined;

    for (size_t i = 0; i < scene->edge_count; ++i) {
        const StructEdge *edge = &scene->edges[i];
        const StructNode *a = structural_scene_get_node(scene, edge->a_id);
        const StructNode *b = structural_scene_get_node(scene, edge->b_id);
        if (!a || !b) continue;
        SDL_Color c0 = {110, 110, 120, 255};
        SDL_Color c1 = c0;
        float value_mag = 0.0f;
        if (scene->has_solution) {
            if (ed->show_bending) {
                c0 = structural_render_color_diverging(-edge->bending_moment_a, moment_scale,
                                                       ed->scale_gamma);
                c1 = structural_render_color_diverging(-edge->bending_moment_b, moment_scale,
                                                       ed->scale_gamma);
                value_mag = fmaxf(fabsf(edge->bending_moment_a), fabsf(edge->bending_moment_b));
            } else if (ed->show_shear) {
                float v = 0.5f * (edge->shear_force_a + edge->shear_force_b);
                SDL_Color base = (SDL_Color){90, 95, 110, 255};
                float mag = fabsf(v);
                float t = mag / fmaxf(1e-6f, shear_scale);
                if (t > 1.0f) t = 1.0f;
                t = powf(t, ed->scale_gamma);
                SDL_Color heat = structural_render_color_heat(mag, shear_scale, ed->scale_gamma);
                c0.r = (Uint8)((float)base.r + ((float)heat.r - (float)base.r) * t);
                c0.g = (Uint8)((float)base.g + ((float)heat.g - (float)base.g) * t);
                c0.b = (Uint8)((float)base.b + ((float)heat.b - (float)base.b) * t);
                c1 = c0;
                value_mag = fabsf(v);
            } else if (ed->show_stress) {
                if (ed->show_combined) {
                    float v = 0.5f * (edge->shear_force_a + edge->shear_force_b);
                    float combined = sqrtf(edge->axial_stress * edge->axial_stress + v * v);
                    c0 = structural_render_color_heat(combined, combined_scale, ed->scale_gamma);
                    c1 = c0;
                    value_mag = combined;
                } else {
                    c0 = stress_color_with_yield(scene, edge, stress_scale, ed->scale_gamma);
                    c1 = c0;
                    value_mag = fabsf(edge->axial_stress);
                }
            } else {
                int palette = edge->material_index % 6;
                SDL_Color colors[6] = {
                    {190, 170, 120, 255},
                    {140, 190, 210, 255},
                    {200, 150, 170, 255},
                    {170, 200, 140, 255},
                    {180, 160, 210, 255},
                    {210, 180, 120, 255}
                };
                c0 = colors[palette];
                c1 = c0;
            }
        } else {
            int palette = edge->material_index % 6;
            SDL_Color colors[6] = {
                {190, 170, 120, 255},
                {140, 190, 210, 255},
                {200, 150, 170, 255},
                {170, 200, 140, 255},
                {180, 160, 210, 255},
                {210, 180, 120, 255}
            };
            c0 = colors[palette];
            c1 = c0;
        }
        if (edge->selected) {
            c0 = (SDL_Color){255, 200, 80, 255};
            c1 = c0;
        }
        float ax = 0.0f, ay = 0.0f, bx = 0.0f, by = 0.0f;
        world_to_screen(editor, a->x, a->y, &ax, &ay);
        world_to_screen(editor, b->x, b->y, &bx, &by);
        float thickness = fmaxf(2.0f, 4.0f * editor->scale);
        if (ed->scale_thickness && scene->has_solution) {
            float ref = ed->show_bending ? moment_scale
                      : ed->show_shear ? shear_scale
                      : ed->show_combined ? combined_scale
                      : stress_scale;
            if (ref < 1e-6f) ref = 1.0f;
            float t = value_mag / ref;
            if (t < 0.0f) t = 0.0f;
            if (t > 1.0f) t = 1.0f;
            t = powf(t, ed->scale_gamma);
            thickness = thickness * (1.0f + ed->thickness_gain * t);
        }
        structural_render_draw_beam(renderer, ax, ay, bx, by, thickness, c0, c1);
        if (edge->release_a || edge->release_b) {
            SDL_SetRenderDrawColor(renderer, 200, 200, 220, 255);
            if (edge->release_a) {
                draw_circle(renderer, ax, ay, 5.0f);
            }
            if (edge->release_b) {
                draw_circle(renderer, bx, by, 5.0f);
            }
        }
    }

    if (editor->editor.edge_start_node_id >= 0 &&
        editor->pointer_x >= 0 &&
        editor->pointer_y >= 0 &&
        point_in_preview(editor, editor->pointer_x, editor->pointer_y)) {
        const StructNode *start = structural_scene_get_node(scene, editor->editor.edge_start_node_id);
        if (start) {
            float wx = 0.0f;
            float wy = 0.0f;
            screen_to_world(editor, editor->pointer_x, editor->pointer_y, &wx, &wy);
            apply_snap(editor, &wx, &wy);
            apply_ground_snap(editor, &wx, &wy);
            float sx0 = 0.0f;
            float sy0 = 0.0f;
            float sx1 = 0.0f;
            float sy1 = 0.0f;
            world_to_screen(editor, start->x, start->y, &sx0, &sy0);
            world_to_screen(editor, wx, wy, &sx1, &sy1);
            SDL_SetRenderDrawColor(renderer, 180, 180, 200, 180);
            SDL_RenderDrawLine(renderer, (int)sx0, (int)sy0, (int)sx1, (int)sy1);
        }
    }

    if (ed->show_deformed && scene->has_solution) {
        SDL_SetRenderDrawColor(renderer, 120, 200, 120, 180);
        for (size_t i = 0; i < scene->edge_count; ++i) {
            const StructEdge *edge = &scene->edges[i];
            const StructNode *a = structural_scene_get_node(scene, edge->a_id);
            const StructNode *b = structural_scene_get_node(scene, edge->b_id);
            if (!a || !b) continue;
            int idx_a = -1;
            int idx_b = -1;
            for (size_t n = 0; n < scene->node_count; ++n) {
                if (scene->nodes[n].id == edge->a_id) idx_a = (int)n;
                if (scene->nodes[n].id == edge->b_id) idx_b = (int)n;
            }
            if (idx_a < 0 || idx_b < 0) continue;
            float dx = b->x - a->x;
            float dy = b->y - a->y;
            float L = sqrtf(dx * dx + dy * dy);
            if (L < 1e-4f) continue;
            float c = dx / L;
            float s = dy / L;

            float scale = ed->deform_scale;
            float u1 = (c * scene->disp_x[idx_a] + s * scene->disp_y[idx_a]) * scale;
            float v1 = (-s * scene->disp_x[idx_a] + c * scene->disp_y[idx_a]) * scale;
            float u2 = (c * scene->disp_x[idx_b] + s * scene->disp_y[idx_b]) * scale;
            float v2 = (-s * scene->disp_x[idx_b] + c * scene->disp_y[idx_b]) * scale;
            float t1 = scene->disp_theta[idx_a] * scale;
            float t2 = scene->disp_theta[idx_b] * scale;

            float prev_x = a->x + (0.0f + u1) * c - v1 * s;
            float prev_y = a->y + (0.0f + u1) * s + v1 * c;
            float prev_sx = 0.0f;
            float prev_sy = 0.0f;
            world_to_screen(editor, prev_x, prev_y, &prev_sx, &prev_sy);

            const int segments = 12;
            for (int seg = 1; seg <= segments; ++seg) {
                float xi = (float)seg / (float)segments;
                float x_local = L * xi;
                float n1 = 1.0f - 3.0f * xi * xi + 2.0f * xi * xi * xi;
                float n2 = L * (xi - 2.0f * xi * xi + xi * xi * xi);
                float n3 = 3.0f * xi * xi - 2.0f * xi * xi * xi;
                float n4 = L * (-xi * xi + xi * xi * xi);

                float u = (1.0f - xi) * u1 + xi * u2;
                float v = n1 * v1 + n2 * t1 + n3 * v2 + n4 * t2;

                float gx = a->x + (x_local + u) * c - v * s;
                float gy = a->y + (x_local + u) * s + v * c;
                float sx = 0.0f;
                float sy = 0.0f;
                world_to_screen(editor, gx, gy, &sx, &sy);
                SDL_RenderDrawLine(renderer, (int)prev_sx, (int)prev_sy, (int)sx, (int)sy);
                prev_sx = sx;
                prev_sy = sy;
            }
        }
    }

    for (size_t i = 0; i < scene->node_count; ++i) {
        const StructNode *node = &scene->nodes[i];
        float sx = 0.0f, sy = 0.0f;
        world_to_screen(editor, node->x, node->y, &sx, &sy);
        if (node->selected) {
            SDL_SetRenderDrawColor(renderer, 255, 220, 120, 255);
        } else {
            SDL_SetRenderDrawColor(renderer, 230, 230, 240, 255);
        }
        float radius = 4.0f * fmaxf(0.5f, editor->scale);
        structural_render_draw_endcap(renderer, sx, sy, radius);

        if (ed->show_constraints) {
            SDL_SetRenderDrawColor(renderer, 200, 140, 120, 255);
            draw_constraints(renderer, node, sx, sy, editor->scale);
        }

        if (node->selected) {
            SDL_SetRenderDrawColor(renderer, 255, 210, 120, 200);
            draw_circle(renderer, sx, sy, radius + 4.0f);
        }
    }

    if (ed->show_loads) {
        for (size_t i = 0; i < scene->load_count; ++i) {
            const StructLoad *load = &scene->loads[i];
            if (load->case_id != scene->active_load_case) continue;
            const StructNode *node = structural_scene_get_node(scene, load->node_id);
            if (!node) continue;
            float base_x = 0.0f, base_y = 0.0f;
            world_to_screen(editor, node->x, node->y, &base_x, &base_y);
            float scale = 10.0f * editor->scale;
            if (fabsf(load->fx) > 1e-4f || fabsf(load->fy) > 1e-4f) {
                SDL_SetRenderDrawColor(renderer, 140, 200, 255, 255);
                draw_arrow(renderer,
                           base_x,
                           base_y,
                           base_x + load->fx * scale,
                           base_y + load->fy * scale);
            }
            if (fabsf(load->mz) > 1e-4f) {
                SDL_SetRenderDrawColor(renderer, 255, 180, 120, 255);
                float radius = (6.0f + fminf(6.0f, fabsf(load->mz) * 4.0f)) * editor->scale;
                draw_moment_icon(renderer, base_x, base_y, radius, load->mz);
            }
        }
    }

    if (ed->box_selecting) {
        float sx0 = 0.0f, sy0 = 0.0f, sx1 = 0.0f, sy1 = 0.0f;
        world_to_screen(editor, (float)ed->box_start_x, (float)ed->box_start_y, &sx0, &sy0);
        world_to_screen(editor, (float)ed->box_end_x, (float)ed->box_end_y, &sx1, &sy1);
        int min_x = (int)fminf(sx0, sx1);
        int max_x = (int)fmaxf(sx0, sx1);
        int min_y = (int)fminf(sy0, sy1);
        int max_y = (int)fmaxf(sy0, sy1);
        SDL_Rect rect = {min_x, min_y, max_x - min_x, max_y - min_y};
        SDL_SetRenderDrawColor(renderer, 200, 200, 255, 80);
        SDL_RenderDrawRect(renderer, &rect);
    }

    SDL_Color text = {230, 230, 240, 255};
    SDL_Color dim = {190, 198, 209, 255};
    int hud_x = editor->panel_x + 16;
    int hud_y = editor->panel_y + 16;
    if (editor->font_main) {
        render_text(renderer, editor->font_main, hud_x, hud_y, text, "Structural Preset");
        hud_y += 28;
    }

    const char *tool_label = "Select";
    switch (ed->tool) {
    case STRUCT_TOOL_ADD_NODE: tool_label = "Add Node"; break;
    case STRUCT_TOOL_ADD_EDGE: tool_label = "Add Edge"; break;
    case STRUCT_TOOL_ADD_LOAD: tool_label = "Add Load"; break;
    case STRUCT_TOOL_ADD_MOMENT: tool_label = "Add Moment"; break;
    default: break;
    }
    char line[128];
    snprintf(line, sizeof(line), "Tool: %s (1-5)", tool_label);
    render_text(renderer, editor->font_small, hud_x, hud_y, dim, line);
    hud_y += 18;
    snprintf(line, sizeof(line), "Nodes: %zu  Edges: %zu", scene->node_count, scene->edge_count);
    render_text(renderer, editor->font_small, hud_x, hud_y, dim, line);
    hud_y += 18;
    const char *mat_name = (scene->material_count > 0 && ed->active_material >= 0 &&
                            ed->active_material < (int)scene->material_count)
                               ? scene->materials[ed->active_material].name
                               : "None";
    snprintf(line, sizeof(line), "Material: %s (M)", mat_name);
    render_text(renderer, editor->font_small, hud_x, hud_y, dim, line);
    hud_y += 18;
    const char *case_name = (scene->load_case_count > 0 && scene->active_load_case >= 0 &&
                             scene->active_load_case < (int)scene->load_case_count)
                                ? scene->load_cases[scene->active_load_case].name
                                : "None";
    snprintf(line, sizeof(line), "Load case: %s ([ / ])", case_name);
    render_text(renderer, editor->font_small, hud_x, hud_y, dim, line);
    hud_y += 18;
    snprintf(line, sizeof(line), "Overlay: T axial | B bend | V shear | Q combined");
    render_text(renderer, editor->font_small, hud_x, hud_y, dim, line);
    hud_y += 18;
    snprintf(line, sizeof(line), "Scale: %s P%.0f gamma %.2f %s %s",
             ed->scale_use_percentile ? "Pct" : "Max",
             ed->scale_percentile * 100.0f,
             ed->scale_gamma,
             ed->scale_freeze ? "freeze" : "live",
             ed->scale_thickness ? "thick" : "flat");
    render_text(renderer, editor->font_small, hud_x, hud_y, dim, line);
    hud_y += 18;
    snprintf(line, sizeof(line), "Viz: Ctrl+Q combined | Ctrl+Y pct | Ctrl+G gamma");
    render_text(renderer, editor->font_small, hud_x, hud_y, dim, line);
    hud_y += 18;
    snprintf(line, sizeof(line), "Viz: Ctrl+K freeze | Ctrl+X thick");
    render_text(renderer, editor->font_small, hud_x, hud_y, dim, line);
    hud_y += 18;
    snprintf(line, sizeof(line), "Constraints: X/Y/Q");
    render_text(renderer, editor->font_small, hud_x, hud_y, dim, line);
    hud_y += 18;
    snprintf(line, sizeof(line), "Solve: Space | Reset: R | New case: N");
    render_text(renderer, editor->font_small, hud_x, hud_y, dim, line);
    hud_y += 18;
    snprintf(line, sizeof(line), "Snap: G | Deform scale: -/=");
    render_text(renderer, editor->font_small, hud_x, hud_y, dim, line);
    hud_y += 22;
    snprintf(line, sizeof(line), "Gravity: %s (g=%.2f)",
             editor->scene.gravity_enabled ? "On" : "Off",
             editor->scene.gravity_strength);
    render_text(renderer, editor->font_small, hud_x, hud_y, dim, line);
    hud_y += 22;

    if (editor->last_result.warning[0]) {
        SDL_Color warn = {255, 180, 100, 255};
        render_text(renderer, editor->font_small, hud_x, hud_y, warn, editor->last_result.warning);
        hud_y += 18;
    }
    if (ed->status_message[0]) {
        SDL_Color status = {160, 200, 160, 255};
        render_text(renderer, editor->font_small, hud_x, hud_y, status, ed->status_message);
    }

    if (editor->pointer_x >= 0 && editor->pointer_y >= 0 &&
        point_in_preview(editor, editor->pointer_x, editor->pointer_y) &&
        editor->font_small) {
        float best = 1e9f;
        const StructEdge *best_edge = NULL;
        const StructNode *best_a = NULL;
        const StructNode *best_b = NULL;
        for (size_t i = 0; i < scene->edge_count; ++i) {
            const StructEdge *edge = &scene->edges[i];
            const StructNode *a = structural_scene_get_node(scene, edge->a_id);
            const StructNode *b = structural_scene_get_node(scene, edge->b_id);
            if (!a || !b) continue;
            float ax = 0.0f, ay = 0.0f, bx = 0.0f, by = 0.0f;
            world_to_screen(editor, a->x, a->y, &ax, &ay);
            world_to_screen(editor, b->x, b->y, &bx, &by);
            float dist = distance_to_segment((float)editor->pointer_x,
                                             (float)editor->pointer_y,
                                             ax, ay, bx, by);
            if (dist < best) {
                best = dist;
                best_edge = edge;
                best_a = a;
                best_b = b;
            }
        }
        if (best_edge && best <= 12.0f) {
            char line[160];
            int tx = editor->pointer_x + 12;
            int ty = editor->pointer_y + 12;
            SDL_Color tip = {240, 240, 240, 255};
            SDL_Color dim2 = {180, 190, 200, 255};
            snprintf(line, sizeof(line), "Edge %d (A %d, B %d)",
                     best_edge->id, best_a->id, best_b->id);
            render_text(renderer, editor->font_small, tx, ty, tip, line);
            ty += 16;
            snprintf(line, sizeof(line), "Axial: %.3f  Shear: %.3f",
                     best_edge->axial_stress,
                     0.5f * (best_edge->shear_force_a + best_edge->shear_force_b));
            render_text(renderer, editor->font_small, tx, ty, dim2, line);
            ty += 16;
            snprintf(line, sizeof(line), "Moment A: %.3f  B: %.3f",
                     best_edge->bending_moment_a,
                     best_edge->bending_moment_b);
            render_text(renderer, editor->font_small, tx, ty, dim2, line);
        }
    }

    scene_editor_draw_button(renderer, &editor->btn_gravity, editor->font_small);
    scene_editor_draw_button(renderer, &editor->btn_gravity_minus, editor->font_small);
    scene_editor_draw_button(renderer, &editor->btn_gravity_plus, editor->font_small);
    scene_editor_draw_button(renderer, &editor->btn_ground, editor->font_small);
    scene_editor_draw_button(renderer, &editor->btn_save, editor->font_small);
    scene_editor_draw_button(renderer, &editor->btn_cancel, editor->font_small);

}

static void handle_solve(StructuralPresetEditor *editor) {
    if (!editor) return;
    StructuralScene *scene = &editor->scene;
    bool has_ground = false;
    for (size_t i = 0; i < scene->node_count; ++i) {
        const StructNode *node = &scene->nodes[i];
        if (node->fixed_y && fabsf(node->y - editor->ground_y) <= 1.0f) {
            has_ground = true;
            break;
        }
    }
    if (!has_ground) {
        structural_editor_set_status(&editor->editor, "Warning: no nodes attached to ground.");
    }

    StructuralSolveResult result = {0};
    bool ok = structural_solve_frame(scene, &result);
    editor->last_result = result;
    if (ok) {
        snprintf(editor->last_result.warning, sizeof(editor->last_result.warning),
                 "Solve ok (%d iters, r=%.3f).", result.iterations, result.residual);
    }
}

static void attach_selected_to_ground(StructuralPresetEditor *editor) {
    if (!editor) return;
    StructuralScene *scene = &editor->scene;
    bool changed = false;
    for (size_t i = 0; i < scene->node_count; ++i) {
        StructNode *node = &scene->nodes[i];
        if (!node->selected) continue;
        node->y = editor->ground_y;
        node->fixed_x = true;
        node->fixed_y = true;
        node->fixed_theta = true;
        changed = true;
    }
    if (changed) {
        structural_scene_clear_solution(scene);
        structural_editor_set_status(&editor->editor, "Attached selected nodes to ground.");
    }
}

static void editor_save(StructuralPresetEditor *editor) {
    if (!editor) return;
    if (structural_scene_save(&editor->scene, editor->preset_path)) {
        structural_editor_set_status(&editor->editor, "Saved structural preset.");
        editor->applied = true;
        editor->running = false;
    } else {
        structural_editor_set_status(&editor->editor, "Save failed.");
    }
}

static void editor_load(StructuralPresetEditor *editor) {
    if (!editor) return;
    if (structural_scene_load(&editor->scene, editor->preset_path)) {
        structural_editor_init(&editor->editor, &editor->scene);
        structural_editor_set_status(&editor->editor, "Loaded structural preset.");
    } else {
        structural_editor_set_status(&editor->editor, "Load failed.");
    }
}

static bool button_hit(const SDL_Rect *rect, int x, int y) {
    if (!rect) return false;
    return x >= rect->x && x <= rect->x + rect->w &&
           y >= rect->y && y <= rect->y + rect->h;
}

static void on_pointer_down(void *user, const InputPointerState *state) {
    StructuralPresetEditor *editor = (StructuralPresetEditor *)user;
    if (!editor || !state) return;
    if (button_hit(&editor->btn_ground.rect, state->x, state->y)) {
        attach_selected_to_ground(editor);
        return;
    }
    if (button_hit(&editor->btn_gravity.rect, state->x, state->y)) {
        editor->scene.gravity_enabled = !editor->scene.gravity_enabled;
        structural_editor_set_status(&editor->editor,
                                     editor->scene.gravity_enabled ? "Gravity enabled." : "Gravity disabled.");
        return;
    }
    if (button_hit(&editor->btn_gravity_minus.rect, state->x, state->y)) {
        editor->scene.gravity_strength = fmaxf(0.0f, editor->scene.gravity_strength - 1.0f);
        return;
    }
    if (button_hit(&editor->btn_gravity_plus.rect, state->x, state->y)) {
        editor->scene.gravity_strength = fminf(100.0f, editor->scene.gravity_strength + 1.0f);
        return;
    }
    if (button_hit(&editor->btn_save.rect, state->x, state->y)) {
        editor_save(editor);
        return;
    }
    if (button_hit(&editor->btn_cancel.rect, state->x, state->y)) {
        editor->running = false;
        return;
    }
    if (!point_in_preview(editor, state->x, state->y)) return;

    if (state->button == SDL_BUTTON_LEFT && state->down &&
        editor->editor.edge_start_node_id >= 0) {
        float wx = 0.0f;
        float wy = 0.0f;
        screen_to_world(editor, state->x, state->y, &wx, &wy);
        apply_snap(editor, &wx, &wy);
        apply_ground_snap(editor, &wx, &wy);
        int node_id = structural_scene_find_node_at(&editor->scene, wx, wy, 10.0f);
        if (node_id < 0) {
            editor->editor.edge_start_node_id = -1;
            structural_editor_set_status(&editor->editor, "Beam chain cancelled.");
            return;
        }
    }

    if (state->button == SDL_BUTTON_RIGHT && state->down) {
        float wx = 0.0f;
        float wy = 0.0f;
        screen_to_world(editor, state->x, state->y, &wx, &wy);
        apply_snap(editor, &wx, &wy);
        apply_ground_snap(editor, &wx, &wy);
        bool keep_chain = (SDL_GetModState() & KMOD_SHIFT) == 0;
        StructuralScene *scene = &editor->scene;
        int node_id = structural_scene_find_node_at(scene, wx, wy, 10.0f);
        if (node_id >= 0) {
            if (editor->editor.edge_start_node_id >= 0 &&
                editor->editor.edge_start_node_id != node_id) {
                int edge_id = structural_scene_add_edge(scene, editor->editor.edge_start_node_id, node_id);
                StructEdge *edge = structural_scene_get_edge(scene, edge_id);
                if (edge) edge->material_index = editor->editor.active_material;
                if (keep_chain) {
                    editor->editor.edge_start_node_id = node_id;
                }
                structural_scene_clear_solution(scene);
                structural_editor_set_status(&editor->editor, "Added beam.");
            } else {
                if (keep_chain) {
                    editor->editor.edge_start_node_id = node_id;
                    structural_editor_set_status(&editor->editor, "Beam start set.");
                } else {
                    structural_editor_set_status(&editor->editor, "Beam start unchanged.");
                }
            }
            return;
        }

        int new_id = structural_scene_add_node(scene, wx, wy);
        if (new_id < 0) return;
        if (editor->editor.edge_start_node_id >= 0) {
            int edge_id = structural_scene_add_edge(scene, editor->editor.edge_start_node_id, new_id);
            StructEdge *edge = structural_scene_get_edge(scene, edge_id);
            if (edge) edge->material_index = editor->editor.active_material;
            structural_editor_set_status(&editor->editor,
                                         keep_chain ? "Added node + beam." : "Added node + beam (chain paused).");
        } else {
            structural_editor_set_status(&editor->editor,
                                         keep_chain ? "Added node." : "Added node (chain paused).");
        }
        if (keep_chain) {
            editor->editor.edge_start_node_id = new_id;
        }
        structural_scene_clear_solution(scene);
        return;
    }

    InputPointerState local = *state;
    float wx = 0.0f;
    float wy = 0.0f;
    screen_to_world(editor, state->x, state->y, &wx, &wy);
    apply_ground_snap(editor, &wx, &wy);
    local.x = (int)wx;
    local.y = (int)wy;
    SDL_Keymod mod = SDL_GetModState();
    structural_editor_handle_pointer_down(&editor->editor, &local, mod);
}

static void on_pointer_up(void *user, const InputPointerState *state) {
    StructuralPresetEditor *editor = (StructuralPresetEditor *)user;
    if (!editor || !state) return;

    InputPointerState local = *state;
    float wx = 0.0f;
    float wy = 0.0f;
    screen_to_world(editor, state->x, state->y, &wx, &wy);
    apply_ground_snap(editor, &wx, &wy);
    local.x = (int)wx;
    local.y = (int)wy;
    SDL_Keymod mod = SDL_GetModState();
    structural_editor_handle_pointer_up(&editor->editor, &local, mod);
}

static void on_pointer_move(void *user, const InputPointerState *state) {
    StructuralPresetEditor *editor = (StructuralPresetEditor *)user;
    if (!editor || !state) return;
    editor->pointer_x = state->x;
    editor->pointer_y = state->y;

    InputPointerState local = *state;
    float wx = 0.0f;
    float wy = 0.0f;
    screen_to_world(editor, state->x, state->y, &wx, &wy);
    if (editor->editor.dragging) {
        apply_ground_snap(editor, &wx, &wy);
    }
    local.x = (int)wx;
    local.y = (int)wy;
    SDL_Keymod mod = SDL_GetModState();
    structural_editor_handle_pointer_move(&editor->editor, &local, mod);
}

static void on_key_down(void *user, SDL_Keycode key, SDL_Keymod mod) {
    StructuralPresetEditor *editor = (StructuralPresetEditor *)user;
    if (!editor) return;
    if ((mod & KMOD_CTRL) && key == SDLK_s) {
        editor_save(editor);
        return;
    }
    if ((mod & KMOD_CTRL) && key == SDLK_o) {
        editor_load(editor);
        return;
    }
    if (mod & KMOD_CTRL) {
        if (key == SDLK_q) {
            editor->editor.show_combined = !editor->editor.show_combined;
            structural_editor_set_status(&editor->editor,
                                         editor->editor.show_combined ? "Combined stress on." : "Combined stress off.");
            return;
        }
        if (key == SDLK_y) {
            editor->editor.scale_use_percentile = !editor->editor.scale_use_percentile;
            editor->scale_initialized = false;
            structural_editor_set_status(&editor->editor,
                                         editor->editor.scale_use_percentile ? "Percentile scale on." : "Percentile scale off.");
            return;
        }
        if (key == SDLK_k) {
            editor->editor.scale_freeze = !editor->editor.scale_freeze;
            editor->scale_initialized = false;
            structural_editor_set_status(&editor->editor,
                                         editor->editor.scale_freeze ? "Scale frozen." : "Scale live.");
            return;
        }
        if (key == SDLK_g) {
            if (editor->editor.scale_gamma > 0.9f) editor->editor.scale_gamma = 0.7f;
            else if (editor->editor.scale_gamma > 0.6f) editor->editor.scale_gamma = 0.5f;
            else if (editor->editor.scale_gamma > 0.4f) editor->editor.scale_gamma = 0.35f;
            else editor->editor.scale_gamma = 1.0f;
            editor->scale_initialized = false;
            structural_editor_set_status(&editor->editor, "Gamma changed.");
            return;
        }
        if (key == SDLK_x) {
            editor->editor.scale_thickness = !editor->editor.scale_thickness;
            structural_editor_set_status(&editor->editor,
                                         editor->editor.scale_thickness ? "Thickness scale on." : "Thickness scale off.");
            return;
        }
    }
    if (key == SDLK_ESCAPE) {
        editor->running = false;
        return;
    }
    if (key == SDLK_SPACE || key == SDLK_RETURN) {
        editor->solve_requested = true;
        return;
    }
    if (key == SDLK_r) {
        structural_scene_reset(&editor->scene);
        structural_editor_init(&editor->editor, &editor->scene);
        memset(&editor->last_result, 0, sizeof(editor->last_result));
        editor->scale_initialized = false;
        return;
    }
    if (key == SDLK_n) {
        int index = structural_scene_add_load_case(&editor->scene, NULL);
        if (index >= 0) {
            editor->scene.active_load_case = index;
            structural_editor_set_status(&editor->editor, "Added load case.");
        }
        return;
    }
    if (key == SDLK_f) {
        attach_selected_to_ground(editor);
        return;
    }
    structural_editor_handle_key_down(&editor->editor, key, mod);
    if (key == SDLK_y || key == SDLK_g || key == SDLK_k) {
        editor->scale_initialized = false;
    }
}

bool structural_preset_editor_run(SDL_Window *window,
                                  SDL_Renderer *renderer,
                                  TTF_Font *font_main,
                                  TTF_Font *font_small,
                                  const AppConfig *cfg,
                                  const char *preset_path,
                                  InputContextManager *ctx_mgr) {
    if (!cfg) return false;
    (void)renderer;

    int base_w = cfg->window_w > 0 ? cfg->window_w : 900;
    int base_h = cfg->window_h > 0 ? cfg->window_h : 700;
    if (window) {
        SDL_GetWindowSize(window, &base_w, &base_h);
    }

    int restart_attempts = 0;
retry:
    fprintf(stderr, "[struct-editor] run: base size=%dx%d\n", base_w, base_h);
    SDL_Window *local_window = g_struct_window;
    SDL_Renderer *local_renderer = g_struct_renderer;
    bool use_shared_device = g_struct_use_shared_device;
    for (int attempt = 0; attempt < 2; ++attempt) {
        if (!g_struct_initialized) {
            fprintf(stderr, "[struct-editor] init: creating window\n");
            local_window = SDL_CreateWindow(
                "Structural Preset Editor",
                SDL_WINDOWPOS_CENTERED,
                SDL_WINDOWPOS_CENTERED,
                base_w,
                base_h,
                SDL_WINDOW_SHOWN | SDL_WINDOW_VULKAN);
            if (!local_window) return false;

            struct_log_window_sizes(local_window, "after create");
            g_struct_window = local_window;
            g_struct_renderer = (SDL_Renderer *)&g_struct_renderer_storage;
            local_renderer = g_struct_renderer;

            VkRendererConfig vk_cfg;
            vk_renderer_config_set_defaults(&vk_cfg);
            vk_cfg.enable_validation = SDL_FALSE;
            vk_cfg.clear_color[0] = 0.0f;
            vk_cfg.clear_color[1] = 0.0f;
            vk_cfg.clear_color[2] = 0.0f;
            vk_cfg.clear_color[3] = 1.0f;
#if defined(__APPLE__)
            vk_cfg.frames_in_flight = 1;
#endif
#if defined(__APPLE__)
            use_shared_device = true;
#else
            use_shared_device = true;
#endif
            g_struct_use_shared_device = use_shared_device;

            fprintf(stderr, "[struct-editor] init: use_shared_device=%d\n",
                    use_shared_device ? 1 : 0);
            if (use_shared_device) {
                if (!vk_shared_device_init(local_window, &vk_cfg)) {
                    fprintf(stderr, "[struct-editor] Failed to init shared Vulkan device.\n");
                    SDL_DestroyWindow(local_window);
                    g_struct_window = NULL;
                    g_struct_renderer = NULL;
                    return false;
                }

                VkRendererDevice* shared_device = vk_shared_device_get();
                if (!shared_device) {
                    fprintf(stderr, "[struct-editor] Failed to access shared Vulkan device.\n");
                    SDL_DestroyWindow(local_window);
                    g_struct_window = NULL;
                    g_struct_renderer = NULL;
                    return false;
                }

                if (vk_renderer_init_with_device((VkRenderer *)local_renderer, shared_device, local_window, &vk_cfg) != VK_SUCCESS) {
                    fprintf(stderr, "[struct-editor] vk_renderer_init_with_device failed\n");
                    SDL_DestroyWindow(local_window);
                    g_struct_window = NULL;
                    g_struct_renderer = NULL;
                    return false;
                }
                vk_shared_device_acquire();
            } else {
                if (vk_renderer_init((VkRenderer *)local_renderer, local_window, &vk_cfg) != VK_SUCCESS) {
                    fprintf(stderr, "[struct-editor] vk_renderer_init failed\n");
                    SDL_DestroyWindow(local_window);
                    g_struct_window = NULL;
                    g_struct_renderer = NULL;
                    return false;
                }
            }
            g_struct_initialized = true;
        } else {
            fprintf(stderr, "[struct-editor] reuse: showing cached window\n");
            if (base_w > 0 && base_h > 0) {
                SDL_SetWindowSize(local_window, base_w, base_h);
            }
            SDL_ShowWindow(local_window);
        }
        SDL_RaiseWindow(local_window);
        if (struct_wait_for_drawable(local_window, "drawable still 0 after show")) {
            struct_log_window_sizes(local_window, "after show");
            SDL_PumpEvents();
            SDL_Delay(32);
            break;
        }

        fprintf(stderr, "[struct-editor] window drawable size never became valid, resetting window.\n");
        vk_renderer_wait_idle((VkRenderer *)local_renderer);
        if (use_shared_device) {
            vk_renderer_shutdown_surface((VkRenderer *)local_renderer);
            vk_shared_device_release();
        } else {
            vk_renderer_shutdown((VkRenderer *)local_renderer);
        }
        SDL_DestroyWindow(local_window);
        g_struct_window = NULL;
        g_struct_renderer = NULL;
        g_struct_initialized = false;
        local_window = NULL;
        local_renderer = NULL;
        if (attempt == 1) {
            return false;
        }
    }
    int init_w = 0;
    int init_h = 0;
    SDL_GetWindowSize(local_window, &init_w, &init_h);
    if (init_w <= 0) init_w = base_w;
    if (init_h <= 0) init_h = base_h;
    vk_renderer_set_logical_size((VkRenderer *)local_renderer, (float)init_w, (float)init_h);
    {
        VkRenderer *vk_renderer = (VkRenderer *)local_renderer;
        fprintf(stderr,
                "[struct-editor] renderer: frames=%u swapchain images=%u format=%u extent=%ux%u\n",
                vk_renderer->frame_count,
                vk_renderer->context.swapchain.image_count,
                (unsigned)vk_renderer->context.swapchain.image_format,
                vk_renderer->context.swapchain.extent.width,
                vk_renderer->context.swapchain.extent.height);
    }

    StructuralPresetEditor editor = {0};
    editor.window = local_window;
    editor.renderer = local_renderer;
    editor.font_main = font_main;
    editor.font_small = font_small;
    editor.cfg = *cfg;
    editor.running = true;
    editor.applied = false;
    bool device_lost = false;
    if (preset_path && preset_path[0] != '\0') {
        snprintf(editor.preset_path, sizeof(editor.preset_path), "%s", preset_path);
    } else {
        snprintf(editor.preset_path, sizeof(editor.preset_path), "%s", "config/structural_scene.txt");
    }

    structural_scene_init(&editor.scene);
    structural_editor_init(&editor.editor, &editor.scene);
    editor.editor.snap_to_grid = true;
    editor.editor.show_stress = false;
    editor.scale_initialized = false;
    editor.scale_stress = 0.0f;
    editor.scale_moment = 0.0f;
    editor.scale_shear = 0.0f;
    editor.scale_combined = 0.0f;
    editor.ground_snap_enabled = true;
    editor.ground_snap_dist = 10.0f;
    editor.pointer_x = -1;
    editor.pointer_y = -1;
    if (!structural_scene_load(&editor.scene, editor.preset_path)) {
        structural_editor_set_status(&editor.editor, "Preset load failed (new scene).");
    }

    update_layout(&editor);

    InputContextManager local_mgr;
    input_context_manager_init(&local_mgr);
    ctx_mgr = &local_mgr;

    InputContext edit_ctx = {
        .on_pointer_down = on_pointer_down,
        .on_pointer_up = on_pointer_up,
        .on_pointer_move = on_pointer_move,
        .on_key_down = on_key_down,
        .user_data = &editor
    };
    input_context_manager_push(ctx_mgr, &edit_ctx);

    int frame_attempts = 0;
    while (editor.running) {
        InputCommands cmds;
        if (!input_poll_events(&cmds, NULL, ctx_mgr)) {
            editor.running = false;
            break;
        }
        if (cmds.quit) {
            editor.running = false;
            break;
        }

        if (editor.solve_requested) {
            editor.solve_requested = false;
            handle_solve(&editor);
        }

        update_layout(&editor);
        int win_w = 0;
        int win_h = 0;
        SDL_GetWindowSize(editor.window, &win_w, &win_h);
        if (win_w > 0 && win_h > 0) {
            int drawable_w = win_w;
            int drawable_h = win_h;
            SDL_Vulkan_GetDrawableSize(editor.window, &drawable_w, &drawable_h);
            if (drawable_w <= 0 || drawable_h <= 0) {
                struct_log_window_sizes(editor.window, "drawable 0, waiting");
                SDL_Delay(16);
                continue;
            }
            VkExtent2D swap_extent = ((VkRenderer *)editor.renderer)->context.swapchain.extent;
            if ((uint32_t)drawable_w != swap_extent.width ||
                (uint32_t)drawable_h != swap_extent.height) {
                fprintf(stderr, "[struct-editor] swapchain resize %ux%u -> %dx%d\n",
                        swap_extent.width, swap_extent.height, drawable_w, drawable_h);
                vk_renderer_recreate_swapchain((VkRenderer *)editor.renderer, editor.window);
                vk_renderer_set_logical_size((VkRenderer *)editor.renderer, (float)win_w, (float)win_h);
                SDL_Delay(8);
                continue;
            }
            VkCommandBuffer cmd = VK_NULL_HANDLE;
            VkFramebuffer fb = VK_NULL_HANDLE;
            VkExtent2D extent = {0};
            VkResult frame = vk_renderer_begin_frame((VkRenderer *)editor.renderer, &cmd, &fb, &extent);
            if (frame_attempts < 8) {
                fprintf(stderr, "[struct-editor] begin_frame attempt=%d result=%d extent=%ux%u\n",
                        frame_attempts, frame, extent.width, extent.height);
                frame_attempts++;
            }
            if (frame == VK_ERROR_OUT_OF_DATE_KHR || frame == VK_SUBOPTIMAL_KHR) {
                fprintf(stderr, "[struct-editor] begin_frame out-of-date/suboptimal\n");
                VkResult recreate = vk_renderer_recreate_swapchain((VkRenderer *)editor.renderer, editor.window);
                fprintf(stderr, "[struct-editor] recreate_swapchain result=%d\n", recreate);
                vk_renderer_set_logical_size((VkRenderer *)editor.renderer, (float)win_w, (float)win_h);
            } else if (frame == VK_ERROR_DEVICE_LOST) {
                static int logged_device_lost = 0;
                if (!logged_device_lost) {
                    fprintf(stderr, "[struct-editor] Vulkan device lost; closing editor.\n");
                    logged_device_lost = 1;
                }
                if (use_shared_device) {
                    vk_shared_device_mark_lost();
                }
                device_lost = true;
                editor.running = false;
                break;
            } else if (frame == VK_SUCCESS) {
                vk_renderer_set_logical_size((VkRenderer *)editor.renderer, (float)win_w, (float)win_h);
                render_scene(&editor);
                VkResult end = vk_renderer_end_frame((VkRenderer *)editor.renderer, cmd);
                if (frame_attempts < 8) {
                    fprintf(stderr, "[struct-editor] end_frame result=%d\n", end);
                }
                if (end == VK_ERROR_OUT_OF_DATE_KHR || end == VK_SUBOPTIMAL_KHR) {
                    fprintf(stderr, "[struct-editor] end_frame out-of-date/suboptimal\n");
                    VkResult recreate = vk_renderer_recreate_swapchain((VkRenderer *)editor.renderer, editor.window);
                    fprintf(stderr, "[struct-editor] recreate_swapchain result=%d\n", recreate);
                    vk_renderer_set_logical_size((VkRenderer *)editor.renderer, (float)win_w, (float)win_h);
                } else if (end == VK_ERROR_DEVICE_LOST) {
                    static int logged_device_lost_end = 0;
                    if (!logged_device_lost_end) {
                        fprintf(stderr, "[struct-editor] Vulkan device lost at end; closing editor.\n");
                        logged_device_lost_end = 1;
                    }
                    if (use_shared_device) {
                        vk_shared_device_mark_lost();
                    }
                    device_lost = true;
                    editor.running = false;
                    break;
                } else if (end != VK_SUCCESS) {
                    fprintf(stderr, "[struct-editor] vk_renderer_end_frame failed: %d\n", end);
                }
#if defined(__APPLE__)
                if (end == VK_SUCCESS) {
                    vk_renderer_wait_idle((VkRenderer *)editor.renderer);
                }
#endif
            } else {
                fprintf(stderr, "[struct-editor] vk_renderer_begin_frame failed: %d\n", frame);
            }
        }
        SDL_Delay(8);
    }

    input_context_manager_pop(ctx_mgr);

    SDL_FlushEvent(SDL_MOUSEBUTTONDOWN);
    SDL_FlushEvent(SDL_MOUSEBUTTONUP);
    SDL_FlushEvent(SDL_MOUSEMOTION);

    vk_renderer_wait_idle((VkRenderer *)local_renderer);
    if (device_lost) {
        if (use_shared_device) {
            vk_renderer_shutdown_surface((VkRenderer *)local_renderer);
            vk_shared_device_release();
        } else {
            vk_renderer_shutdown((VkRenderer *)local_renderer);
        }
        SDL_DestroyWindow(local_window);
        g_struct_window = NULL;
        g_struct_renderer = NULL;
        g_struct_initialized = false;
        if (!use_shared_device && restart_attempts == 0) {
            fprintf(stderr, "[struct-editor] device lost; retrying with fresh window.\n");
            restart_attempts++;
            goto retry;
        }
    } else {
        SDL_HideWindow(local_window);
    }
    return editor.applied;
}
