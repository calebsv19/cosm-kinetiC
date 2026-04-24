#include "app/structural/structural_preset_editor_render_helpers.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "app/structural/structural_render.h"
#include "render/text_draw.h"

static void render_text(SDL_Renderer *renderer, TTF_Font *font,
                        int x, int y, SDL_Color color, const char *text) {
    (void)physics_sim_text_draw_utf8_at(renderer, font, text, x, y, color);
}

static bool render_hud_line_limited(SDL_Renderer *renderer,
                                    TTF_Font *font,
                                    int x,
                                    int *y,
                                    int step,
                                    int limit_y,
                                    SDL_Color color,
                                    const char *text) {
    if (!renderer || !font || !y || !text) return false;
    if (*y + step > limit_y) return false;
    render_text(renderer, font, x, *y, color, text);
    *y += step;
    return true;
}

static int structural_editor_font_height(SDL_Renderer *renderer, TTF_Font *font, int fallback) {
    int font_h = 0;
    if (!font) return fallback;
    font_h = TTF_FontHeight(font);
    if (font_h <= 0) return fallback;
    return physics_sim_text_logical_pixels(renderer, font_h);
}

void structural_preset_editor_apply_snap(const StructuralPresetEditor *editor, float *x, float *y) {
    if (!editor || !x || !y) return;
    if (!editor->editor.snap_to_grid) return;
    float grid = editor->editor.grid_size;
    if (grid <= 0.0f) return;
    *x = roundf(*x / grid) * grid;
    *y = roundf(*y / grid) * grid;
}

void structural_preset_editor_apply_ground_snap(const StructuralPresetEditor *editor, float *x, float *y) {
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
                       float x0, float y0, float x1, float y1) {
    SDL_RenderDrawLine(renderer, (int)x0, (int)y0, (int)x1, (int)y1);
    float dx = x1 - x0;
    float dy = y1 - y0;
    float len = sqrtf(dx * dx + dy * dy);
    if (len < 1e-4f) return;
    dx /= len;
    dy /= len;
    float hx = x1 - dx * 8.0f;
    float hy = y1 - dy * 8.0f;
    float px = -dy;
    float py = dx;
    SDL_RenderDrawLine(renderer, (int)x1, (int)y1, (int)(hx + px * 4.0f), (int)(hy + py * 4.0f));
    SDL_RenderDrawLine(renderer, (int)x1, (int)y1, (int)(hx - px * 4.0f), (int)(hy - py * 4.0f));
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

void structural_preset_editor_world_to_screen(const StructuralPresetEditor *editor,
                                              float wx,
                                              float wy,
                                              float *sx,
                                              float *sy) {
    if (!editor || !sx || !sy) return;
    *sx = editor->preview_x + wx * editor->scale;
    *sy = editor->preview_y + wy * editor->scale;
}

void structural_preset_editor_screen_to_world(const StructuralPresetEditor *editor,
                                              int sx,
                                              int sy,
                                              float *wx,
                                              float *wy) {
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

bool structural_preset_editor_point_in_preview(const StructuralPresetEditor *editor, int x, int y) {
    if (!editor) return false;
    return x >= editor->preview_x && x <= (editor->preview_x + editor->preview_w) &&
           y >= editor->preview_y && y <= (editor->preview_y + editor->preview_h);
}

void structural_preset_editor_render_scene(StructuralPresetEditor *editor) {
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
    structural_preset_editor_world_to_screen(editor, 0.0f, editor->ground_y, &ground_sx, &ground_sy);
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
        structural_preset_editor_world_to_screen(editor, a->x, a->y, &ax, &ay);
        structural_preset_editor_world_to_screen(editor, b->x, b->y, &bx, &by);
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
        structural_preset_editor_point_in_preview(editor, editor->pointer_x, editor->pointer_y)) {
        const StructNode *start = structural_scene_get_node(scene, editor->editor.edge_start_node_id);
        if (start) {
            float wx = 0.0f;
            float wy = 0.0f;
            structural_preset_editor_screen_to_world(editor, editor->pointer_x, editor->pointer_y, &wx, &wy);
            structural_preset_editor_apply_snap(editor, &wx, &wy);
            structural_preset_editor_apply_ground_snap(editor, &wx, &wy);
            float sx0 = 0.0f;
            float sy0 = 0.0f;
            float sx1 = 0.0f;
            float sy1 = 0.0f;
            structural_preset_editor_world_to_screen(editor, start->x, start->y, &sx0, &sy0);
            structural_preset_editor_world_to_screen(editor, wx, wy, &sx1, &sy1);
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
            structural_preset_editor_world_to_screen(editor, prev_x, prev_y, &prev_sx, &prev_sy);

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
                structural_preset_editor_world_to_screen(editor, gx, gy, &sx, &sy);
                SDL_RenderDrawLine(renderer, (int)prev_sx, (int)prev_sy, (int)sx, (int)sy);
                prev_sx = sx;
                prev_sy = sy;
            }
        }
    }

    for (size_t i = 0; i < scene->node_count; ++i) {
        const StructNode *node = &scene->nodes[i];
        float sx = 0.0f, sy = 0.0f;
        structural_preset_editor_world_to_screen(editor, node->x, node->y, &sx, &sy);
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
            structural_preset_editor_world_to_screen(editor, node->x, node->y, &base_x, &base_y);
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
        structural_preset_editor_world_to_screen(editor, (float)ed->box_start_x, (float)ed->box_start_y, &sx0, &sy0);
        structural_preset_editor_world_to_screen(editor, (float)ed->box_end_x, (float)ed->box_end_y, &sx1, &sy1);
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
    int hud_limit_y = editor->btn_gravity_minus.rect.y - 10;
    int hud_step = structural_editor_font_height(renderer, editor->font_small, 16) + 2;
    int hud_block_step = hud_step + 4;
    if (hud_step < 18) hud_step = 18;
    if (hud_block_step < 22) hud_block_step = 22;
    if (hud_limit_y < hud_y) hud_limit_y = hud_y;
    if (editor->font_main) {
        int title_step = structural_editor_font_height(renderer, editor->font_main, 20) + 8;
        if (title_step < 22) title_step = 22;
        (void)render_hud_line_limited(renderer,
                                      editor->font_main,
                                      hud_x,
                                      &hud_y,
                                      title_step,
                                      hud_limit_y,
                                      text,
                                      "Structural Preset");
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
    (void)render_hud_line_limited(renderer, editor->font_small, hud_x, &hud_y, hud_step, hud_limit_y, dim, line);
    snprintf(line, sizeof(line), "Nodes: %zu  Edges: %zu", scene->node_count, scene->edge_count);
    (void)render_hud_line_limited(renderer, editor->font_small, hud_x, &hud_y, hud_step, hud_limit_y, dim, line);
    const char *mat_name = (scene->material_count > 0 && ed->active_material >= 0 &&
                            ed->active_material < (int)scene->material_count)
                               ? scene->materials[ed->active_material].name
                               : "None";
    snprintf(line, sizeof(line), "Material: %s (M)", mat_name);
    (void)render_hud_line_limited(renderer, editor->font_small, hud_x, &hud_y, hud_step, hud_limit_y, dim, line);
    const char *case_name = (scene->load_case_count > 0 && scene->active_load_case >= 0 &&
                             scene->active_load_case < (int)scene->load_case_count)
                                ? scene->load_cases[scene->active_load_case].name
                                : "None";
    snprintf(line, sizeof(line), "Load case: %s ([ / ])", case_name);
    (void)render_hud_line_limited(renderer, editor->font_small, hud_x, &hud_y, hud_step, hud_limit_y, dim, line);
    snprintf(line, sizeof(line), "Overlay: T axial | B bend | V shear | Q combined");
    (void)render_hud_line_limited(renderer, editor->font_small, hud_x, &hud_y, hud_step, hud_limit_y, dim, line);
    snprintf(line, sizeof(line), "Scale: %s P%.0f gamma %.2f %s %s",
             ed->scale_use_percentile ? "Pct" : "Max",
             ed->scale_percentile * 100.0f,
             ed->scale_gamma,
             ed->scale_freeze ? "freeze" : "live",
             ed->scale_thickness ? "thick" : "flat");
    (void)render_hud_line_limited(renderer, editor->font_small, hud_x, &hud_y, hud_step, hud_limit_y, dim, line);
    snprintf(line, sizeof(line), "Viz: Ctrl+Q combined | Ctrl+Y pct | Ctrl+G gamma");
    (void)render_hud_line_limited(renderer, editor->font_small, hud_x, &hud_y, hud_step, hud_limit_y, dim, line);
    snprintf(line, sizeof(line), "Viz: Ctrl+K freeze | Ctrl+X thick");
    (void)render_hud_line_limited(renderer, editor->font_small, hud_x, &hud_y, hud_step, hud_limit_y, dim, line);
    snprintf(line, sizeof(line), "Constraints: X/Y/Q");
    (void)render_hud_line_limited(renderer, editor->font_small, hud_x, &hud_y, hud_step, hud_limit_y, dim, line);
    snprintf(line, sizeof(line), "Solve: Space | Reset: R | New case: N");
    (void)render_hud_line_limited(renderer, editor->font_small, hud_x, &hud_y, hud_step, hud_limit_y, dim, line);
    snprintf(line, sizeof(line), "Snap: G | Deform scale: -/=");
    (void)render_hud_line_limited(renderer, editor->font_small, hud_x, &hud_y, hud_block_step, hud_limit_y, dim, line);
    snprintf(line, sizeof(line), "Gravity: %s (g=%.2f)",
             editor->scene.gravity_enabled ? "On" : "Off",
             editor->scene.gravity_strength);
    (void)render_hud_line_limited(renderer, editor->font_small, hud_x, &hud_y, hud_block_step, hud_limit_y, dim, line);

    if (editor->last_result.warning[0]) {
        SDL_Color warn = {255, 180, 100, 255};
        (void)render_hud_line_limited(renderer,
                                      editor->font_small,
                                      hud_x,
                                      &hud_y,
                                      hud_step,
                                      hud_limit_y,
                                      warn,
                                      editor->last_result.warning);
    }
    if (ed->status_message[0]) {
        SDL_Color status = {160, 200, 160, 255};
        (void)render_hud_line_limited(renderer,
                                      editor->font_small,
                                      hud_x,
                                      &hud_y,
                                      hud_step,
                                      hud_limit_y,
                                      status,
                                      ed->status_message);
    }

    if (editor->pointer_x >= 0 && editor->pointer_y >= 0 &&
        structural_preset_editor_point_in_preview(editor, editor->pointer_x, editor->pointer_y) &&
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
            structural_preset_editor_world_to_screen(editor, a->x, a->y, &ax, &ay);
            structural_preset_editor_world_to_screen(editor, b->x, b->y, &bx, &by);
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
            int tip_step = structural_editor_font_height(renderer, editor->font_small, 14) + 2;
            int tip_w1 = 0;
            int tip_w2 = 0;
            int tip_w3 = 0;
            int tip_max_w = 0;
            int tip_h = 0;
            int win_w = 0;
            int win_h = 0;
            if (tip_step < 16) tip_step = 16;
            SDL_Color tip = {240, 240, 240, 255};
            SDL_Color dim2 = {180, 190, 200, 255};
            snprintf(line, sizeof(line), "Edge %d (A %d, B %d)",
                     best_edge->id, best_a->id, best_b->id);
            if (TTF_SizeUTF8(editor->font_small, line, &tip_w1, NULL) != 0) {
                tip_w1 = 0;
            } else {
                tip_w1 = physics_sim_text_logical_pixels(renderer, tip_w1);
            }
            snprintf(line, sizeof(line), "Axial: %.3f  Shear: %.3f",
                     best_edge->axial_stress,
                     0.5f * (best_edge->shear_force_a + best_edge->shear_force_b));
            if (TTF_SizeUTF8(editor->font_small, line, &tip_w2, NULL) != 0) {
                tip_w2 = 0;
            } else {
                tip_w2 = physics_sim_text_logical_pixels(renderer, tip_w2);
            }
            snprintf(line, sizeof(line), "Moment A: %.3f  B: %.3f",
                     best_edge->bending_moment_a,
                     best_edge->bending_moment_b);
            if (TTF_SizeUTF8(editor->font_small, line, &tip_w3, NULL) != 0) {
                tip_w3 = 0;
            } else {
                tip_w3 = physics_sim_text_logical_pixels(renderer, tip_w3);
            }
            tip_max_w = tip_w1;
            if (tip_w2 > tip_max_w) tip_max_w = tip_w2;
            if (tip_w3 > tip_max_w) tip_max_w = tip_w3;
            tip_h = tip_step * 3;
            if (editor->window) {
                SDL_GetWindowSize(editor->window, &win_w, &win_h);
            }
            if (win_w > 0) {
                int max_tx = win_w - tip_max_w - 12;
                if (tx > max_tx) tx = max_tx;
                if (tx < 8) tx = 8;
            }
            if (win_h > 0) {
                int max_ty = win_h - tip_h - 12;
                if (ty > max_ty) ty = max_ty;
                if (ty < 8) ty = 8;
            }

            snprintf(line, sizeof(line), "Edge %d (A %d, B %d)",
                     best_edge->id, best_a->id, best_b->id);
            render_text(renderer, editor->font_small, tx, ty, tip, line);
            ty += tip_step;
            snprintf(line, sizeof(line), "Axial: %.3f  Shear: %.3f",
                     best_edge->axial_stress,
                     0.5f * (best_edge->shear_force_a + best_edge->shear_force_b));
            render_text(renderer, editor->font_small, tx, ty, dim2, line);
            ty += tip_step;
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
