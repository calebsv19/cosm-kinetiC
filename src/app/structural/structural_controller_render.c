#include "app/structural/structural_controller_internal.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "app/structural/structural_render.h"
#include "render/text_upload_policy.h"
#include "vk_renderer.h"

static void render_text(SDL_Renderer *renderer, TTF_Font *font,
                        int x, int y, SDL_Color color, const char *text) {
    if (!renderer || !font || !text) return;
    {
        SDL_Surface *surface = TTF_RenderUTF8_Blended(font, text, color);
        if (!surface) return;
        {
            SDL_Rect dst = {
                x,
                y,
                physics_sim_text_logical_pixels(renderer, surface->w),
                physics_sim_text_logical_pixels(renderer, surface->h)
            };
            VkRendererTexture texture = {0};
            if (vk_renderer_upload_sdl_surface_with_filter((VkRenderer *)renderer,
                                                           surface,
                                                           &texture,
                                                           physics_sim_text_upload_filter(renderer)) == VK_SUCCESS) {
                vk_renderer_draw_texture((VkRenderer *)renderer, &texture, NULL, &dst);
                vk_renderer_queue_texture_destroy((VkRenderer *)renderer, &texture);
            }
        }
        SDL_FreeSurface(surface);
    }
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

static int structural_font_height(SDL_Renderer *renderer, TTF_Font *font, int fallback) {
    int font_h = 0;
    if (!font) return fallback;
    font_h = TTF_FontHeight(font);
    if (font_h <= 0) return fallback;
    return physics_sim_text_logical_pixels(renderer, font_h);
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
    {
        float dx = x1 - x0;
        float dy = y1 - y0;
        float len = sqrtf(dx * dx + dy * dy);
        if (len < 1e-3f) return;
        {
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
    }
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
    {
        float t = ((px - ax) * dx + (py - ay) * dy) / len2;
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;
        {
            float cx = ax + t * dx;
            float cy = ay + t * dy;
            float ex = px - cx;
            float ey = py - cy;
            return sqrtf(ex * ex + ey * ey);
        }
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
    {
        float p = clamp01(percentile);
        if (p < 0.5f) p = 0.5f;
        {
            size_t idx = (size_t)lroundf((float)(count - 1) * p);
            if (idx >= count) idx = count - 1;
            {
                float scale = use_percentile ? values[idx] : values[count - 1];
                if (scale < 1e-6f) scale = 1.0f;
                return scale;
            }
        }
    }
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

    {
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
}

static SDL_Color stress_color_with_yield(const StructuralScene *scene,
                                         const StructEdge *edge,
                                         float stress_max,
                                         float gamma) {
    if (!scene || !edge) return (SDL_Color){110, 110, 120, 255};
    {
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
                {
                    SDL_Color warn = {170, 90, 210, 255};
                    SDL_Color out = base;
                    out.r = (Uint8)((float)base.r + ((float)warn.r - (float)base.r) * t);
                    out.g = (Uint8)((float)base.g + ((float)warn.g - (float)base.g) * t);
                    out.b = (Uint8)((float)base.b + ((float)warn.b - (float)base.b) * t);
                    return out;
                }
            }
        }
        return base;
    }
}

static void draw_constraints(SDL_Renderer *renderer, const StructNode *node) {
    if (!node) return;
    {
        int x = (int)node->x;
        int y = (int)node->y;
        if (node->fixed_x) {
            SDL_RenderDrawLine(renderer, x - 6, y - 6, x - 6, y + 6);
        }
        if (node->fixed_y) {
            SDL_RenderDrawLine(renderer, x - 6, y + 6, x + 6, y + 6);
        }
        if (node->fixed_theta) {
            draw_circle(renderer, (float)x, (float)y, 6.0f);
        }
    }
}

static void draw_moment_icon(SDL_Renderer *renderer, float cx, float cy, float radius, float moment) {
    if (fabsf(moment) < 1e-4f) return;
    draw_circle(renderer, cx, cy, radius);
    {
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
}

void structural_controller_render_scene(SDL_Renderer *renderer, StructuralController *ctrl) {
    if (!renderer || !ctrl) return;
    {
        StructuralScene *scene = &ctrl->scene;
        int w = ctrl->window_w;
        int h = ctrl->window_h;
        if (w <= 0 || h <= 0) return;

        SDL_SetRenderDrawColor(renderer, 16, 18, 20, 255);
        {
            SDL_Rect clear_rect = {0, 0, w, h};
            SDL_RenderFillRect(renderer, &clear_rect);
        }

        {
            float ground_y = (float)h - scene->ground_offset;
            SDL_SetRenderDrawColor(renderer, 90, 80, 70, 255);
            SDL_RenderDrawLine(renderer, 0, (int)ground_y, w, (int)ground_y);
        }

        if (!ctrl->scale_freeze || !ctrl->scale_initialized) {
            compute_edge_scales(scene,
                                ctrl->scale_use_percentile,
                                ctrl->scale_percentile,
                                &ctrl->scale_stress,
                                &ctrl->scale_moment,
                                &ctrl->scale_shear,
                                &ctrl->scale_combined);
            ctrl->scale_initialized = true;
        }
        {
            float stress_scale = ctrl->scale_stress;
            float moment_scale = ctrl->scale_moment;
            float shear_scale = ctrl->scale_shear;
            float combined_scale = ctrl->scale_combined;

            for (size_t i = 0; i < scene->edge_count; ++i) {
                const StructEdge *edge = &scene->edges[i];
                const StructNode *a = structural_scene_get_node(scene, edge->a_id);
                const StructNode *b = structural_scene_get_node(scene, edge->b_id);
                if (!a || !b) continue;
                SDL_Color c0 = {110, 110, 120, 255};
                SDL_Color c1 = c0;
                float value_mag = 0.0f;
                if (scene->has_solution) {
                    if (ctrl->show_bending) {
                        c0 = structural_render_color_diverging(-edge->bending_moment_a, moment_scale,
                                                               ctrl->scale_gamma);
                        c1 = structural_render_color_diverging(-edge->bending_moment_b, moment_scale,
                                                               ctrl->scale_gamma);
                        value_mag = fmaxf(fabsf(edge->bending_moment_a), fabsf(edge->bending_moment_b));
                    } else if (ctrl->show_shear) {
                        float v = 0.5f * (edge->shear_force_a + edge->shear_force_b);
                        SDL_Color base = (SDL_Color){90, 95, 110, 255};
                        float mag = fabsf(v);
                        float t = mag / fmaxf(1e-6f, shear_scale);
                        if (t > 1.0f) t = 1.0f;
                        t = powf(t, ctrl->scale_gamma);
                        {
                            SDL_Color heat = structural_render_color_heat(mag, shear_scale, ctrl->scale_gamma);
                            c0.r = (Uint8)((float)base.r + ((float)heat.r - (float)base.r) * t);
                            c0.g = (Uint8)((float)base.g + ((float)heat.g - (float)base.g) * t);
                            c0.b = (Uint8)((float)base.b + ((float)heat.b - (float)base.b) * t);
                            c1 = c0;
                            value_mag = fabsf(v);
                        }
                    } else if (ctrl->show_stress) {
                        if (ctrl->show_combined) {
                            float v = 0.5f * (edge->shear_force_a + edge->shear_force_b);
                            float combined = sqrtf(edge->axial_stress * edge->axial_stress + v * v);
                            c0 = structural_render_color_heat(combined, combined_scale, ctrl->scale_gamma);
                            c1 = c0;
                            value_mag = combined;
                        } else {
                            c0 = stress_color_with_yield(scene, edge, stress_scale, ctrl->scale_gamma);
                            c1 = c0;
                            value_mag = fabsf(edge->axial_stress);
                        }
                    }
                }
                if (!scene->has_solution) {
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
                {
                    float width = 6.0f;
                    if (ctrl->scale_thickness && scene->has_solution) {
                        float ref = ctrl->show_bending ? moment_scale
                                  : ctrl->show_shear ? shear_scale
                                  : ctrl->show_combined ? combined_scale
                                  : stress_scale;
                        if (ref < 1e-6f) ref = 1.0f;
                        {
                            float t = value_mag / ref;
                            if (t < 0.0f) t = 0.0f;
                            if (t > 1.0f) t = 1.0f;
                            t = powf(t, ctrl->scale_gamma);
                            width = width * (1.0f + ctrl->thickness_gain * t);
                        }
                    }
                    structural_render_draw_beam(renderer, a->x, a->y, b->x, b->y, width, c0, c1);
                }
                if (edge->release_a || edge->release_b) {
                    SDL_SetRenderDrawColor(renderer, 200, 200, 220, 255);
                    if (edge->release_a) draw_circle(renderer, a->x, a->y, 5.0f);
                    if (edge->release_b) draw_circle(renderer, b->x, b->y, 5.0f);
                }
            }
        }

        if (ctrl->show_deformed && scene->has_solution && !ctrl->dynamic_mode) {
            SDL_SetRenderDrawColor(renderer, 120, 200, 120, 180);
            for (size_t i = 0; i < scene->edge_count; ++i) {
                const StructEdge *edge = &scene->edges[i];
                const StructNode *a = structural_scene_get_node(scene, edge->a_id);
                const StructNode *b = structural_scene_get_node(scene, edge->b_id);
                if (!a || !b) continue;
                {
                    int idx_a = -1;
                    int idx_b = -1;
                    for (size_t n = 0; n < scene->node_count; ++n) {
                        if (scene->nodes[n].id == edge->a_id) idx_a = (int)n;
                        if (scene->nodes[n].id == edge->b_id) idx_b = (int)n;
                    }
                    if (idx_a < 0 || idx_b < 0) continue;
                    {
                        float dx = b->x - a->x;
                        float dy = b->y - a->y;
                        float L = sqrtf(dx * dx + dy * dy);
                        if (L < 1e-4f) continue;
                        float c = dx / L;
                        float s = dy / L;
                        float scale = ctrl->deform_scale;
                        float uax = scene->disp_x[idx_a];
                        float uay = scene->disp_y[idx_a];
                        float ubx = scene->disp_x[idx_b];
                        float uby = scene->disp_y[idx_b];
                        float tax = scene->disp_theta[idx_a];
                        float tbx = scene->disp_theta[idx_b];
                        if (ctrl->runtime.u && ctrl->runtime.dof_count >= scene->node_count * 3) {
                            uax = ctrl->runtime.u[idx_a * 3 + 0];
                            uay = ctrl->runtime.u[idx_a * 3 + 1];
                            tax = ctrl->runtime.u[idx_a * 3 + 2];
                            ubx = ctrl->runtime.u[idx_b * 3 + 0];
                            uby = ctrl->runtime.u[idx_b * 3 + 1];
                            tbx = ctrl->runtime.u[idx_b * 3 + 2];
                        }
                        {
                            float u1 = (c * uax + s * uay) * scale;
                            float v1 = (-s * uax + c * uay) * scale;
                            float u2 = (c * ubx + s * uby) * scale;
                            float v2 = (-s * ubx + c * uby) * scale;
                            float t1 = tax * scale;
                            float t2 = tbx * scale;
                            float prev_x = a->x + (0.0f + u1) * c - v1 * s;
                            float prev_y = a->y + (0.0f + u1) * s + v1 * c;
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
                                SDL_RenderDrawLine(renderer, (int)prev_x, (int)prev_y, (int)gx, (int)gy);
                                prev_x = gx;
                                prev_y = gy;
                            }
                        }
                    }
                }
            }
        }

        for (size_t i = 0; i < scene->node_count; ++i) {
            const StructNode *node = &scene->nodes[i];
            SDL_SetRenderDrawColor(renderer, 230, 230, 240, 255);
            structural_render_draw_endcap(renderer, node->x, node->y, 4.0f);

            if (ctrl->show_constraints) {
                SDL_SetRenderDrawColor(renderer, 200, 140, 120, 255);
                draw_constraints(renderer, node);
            }
            if (ctrl->show_ids && ctrl->font_small) {
                char label[16];
                SDL_Color color = {200, 200, 200, 255};
                snprintf(label, sizeof(label), "%d", node->id);
                render_text(renderer, ctrl->font_small, (int)node->x + 6, (int)node->y + 6, color, label);
            }
        }

        if (ctrl->show_loads) {
            for (size_t i = 0; i < scene->load_count; ++i) {
                const StructLoad *load = &scene->loads[i];
                if (load->case_id != scene->active_load_case) continue;
                {
                    const StructNode *node = structural_scene_get_node(scene, load->node_id);
                    if (!node) continue;
                    {
                        float scale = 10.0f;
                        if (fabsf(load->fx) > 1e-4f || fabsf(load->fy) > 1e-4f) {
                            SDL_SetRenderDrawColor(renderer, 140, 200, 255, 255);
                            draw_arrow(renderer, node->x, node->y, node->x + load->fx * scale, node->y + load->fy * scale);
                        }
                        if (fabsf(load->mz) > 1e-4f) {
                            SDL_SetRenderDrawColor(renderer, 255, 180, 120, 255);
                            {
                                float radius = 6.0f + fminf(6.0f, fabsf(load->mz) * 4.0f);
                                draw_moment_icon(renderer, node->x, node->y, radius, load->mz);
                            }
                        }
                    }
                }
            }
        }

        {
            SDL_Color hud_color = {220, 220, 220, 255};
            int hud_x = 16;
            int hud_y = 16;
            if (ctrl->font_hud) {
                int hud_limit_y = (ctrl->window_h > 0) ? (ctrl->window_h - 20) : 1000000;
                int hud_line_step = structural_font_height(renderer, ctrl->font_hud, 14) + 4;
                char line[128];
                if (hud_line_step < 18) hud_line_step = 18;
                snprintf(line, sizeof(line), "Mode: Structural");
                (void)render_hud_line_limited(renderer, ctrl->font_hud, hud_x, &hud_y, hud_line_step, hud_limit_y, hud_color, line);
                snprintf(line, sizeof(line), "Nodes: %zu  Edges: %zu", scene->node_count, scene->edge_count);
                (void)render_hud_line_limited(renderer, ctrl->font_hud, hud_x, &hud_y, hud_line_step, hud_limit_y, hud_color, line);
                snprintf(line, sizeof(line), "Solve: Space | Reset sim: R");
                (void)render_hud_line_limited(renderer, ctrl->font_hud, hud_x, &hud_y, hud_line_step, hud_limit_y, hud_color, line);
                snprintf(line, sizeof(line), "Dynamic: E %s | Play: P %s | Step: S",
                         ctrl->dynamic_mode ? "On" : "Off",
                         ctrl->dynamic_playing ? "On" : "Off");
                (void)render_hud_line_limited(renderer, ctrl->font_hud, hud_x, &hud_y, hud_line_step, hud_limit_y, hud_color, line);
                snprintf(line, sizeof(line), "Integrator: %s (Z)",
                         ctrl->integrator == STRUCT_INTEGRATOR_NEWMARK ? "Newmark" : "Explicit");
                (void)render_hud_line_limited(renderer, ctrl->font_hud, hud_x, &hud_y, hud_line_step, hud_limit_y, hud_color, line);
                snprintf(line, sizeof(line), "Time scale: %.2f (6/7)", ctrl->time_scale);
                (void)render_hud_line_limited(renderer, ctrl->font_hud, hud_x, &hud_y, hud_line_step, hud_limit_y, hud_color, line);
                snprintf(line, sizeof(line), "Damping a: %.2f (A/F)", ctrl->damping_alpha);
                (void)render_hud_line_limited(renderer, ctrl->font_hud, hud_x, &hud_y, hud_line_step, hud_limit_y, hud_color, line);
                snprintf(line, sizeof(line), "Damping b: %.2f (H/J)", ctrl->damping_beta);
                (void)render_hud_line_limited(renderer, ctrl->font_hud, hud_x, &hud_y, hud_line_step, hud_limit_y, hud_color, line);
                snprintf(line, sizeof(line), "Gravity ramp: %s %.2fs (U/0)",
                         ctrl->gravity_ramp_enabled ? "On" : "Off",
                         ctrl->gravity_ramp_duration);
                (void)render_hud_line_limited(renderer, ctrl->font_hud, hud_x, &hud_y, hud_line_step, hud_limit_y, hud_color, line);
                snprintf(line, sizeof(line), "Sim time: %.2fs", ctrl->sim_time);
                (void)render_hud_line_limited(renderer, ctrl->font_hud, hud_x, &hud_y, hud_line_step, hud_limit_y, hud_color, line);
                snprintf(line, sizeof(line), "Overlay: T axial | B bend | V shear | Q combined");
                (void)render_hud_line_limited(renderer, ctrl->font_hud, hud_x, &hud_y, hud_line_step, hud_limit_y, hud_color, line);
                snprintf(line, sizeof(line), "Scale: %s P%.0f gamma %.2f %s %s",
                         ctrl->scale_use_percentile ? "Pct" : "Max",
                         ctrl->scale_percentile * 100.0f,
                         ctrl->scale_gamma,
                         ctrl->scale_freeze ? "freeze" : "live",
                         ctrl->scale_thickness ? "thick" : "flat");
                (void)render_hud_line_limited(renderer, ctrl->font_hud, hud_x, &hud_y, hud_line_step, hud_limit_y, hud_color, line);
                snprintf(line, sizeof(line), "Viz: Q combined | Y pct | G gamma | K freeze | X thick");
                (void)render_hud_line_limited(renderer, ctrl->font_hud, hud_x, &hud_y, hud_line_step, hud_limit_y, hud_color, line);
                if (ctrl->last_result.warning[0]) {
                    SDL_Color warn = {255, 180, 100, 255};
                    (void)render_hud_line_limited(renderer, ctrl->font_hud, hud_x, &hud_y, hud_line_step, hud_limit_y, warn, ctrl->last_result.warning);
                }
                if (ctrl->last_result.warning[0]) {
                    SDL_Color status = {160, 200, 160, 255};
                    (void)render_hud_line_limited(renderer, ctrl->font_hud, hud_x, &hud_y, hud_line_step, hud_limit_y, status, ctrl->last_result.warning);
                }
            }
        }

        if (ctrl->pointer_x >= 0 && ctrl->pointer_y >= 0) {
            float best = 1e9f;
            const StructEdge *best_edge = NULL;
            const StructNode *best_a = NULL;
            const StructNode *best_b = NULL;
            for (size_t i = 0; i < scene->edge_count; ++i) {
                const StructEdge *edge = &scene->edges[i];
                const StructNode *a = structural_scene_get_node(scene, edge->a_id);
                const StructNode *b = structural_scene_get_node(scene, edge->b_id);
                if (!a || !b) continue;
                {
                    float dist = distance_to_segment((float)ctrl->pointer_x,
                                                     (float)ctrl->pointer_y,
                                                     a->x, a->y,
                                                     b->x, b->y);
                    if (dist < best) {
                        best = dist;
                        best_edge = edge;
                        best_a = a;
                        best_b = b;
                    }
                }
            }
            if (best_edge && best <= 12.0f && ctrl->font_small) {
                char line[160];
                int tx = ctrl->pointer_x + 12;
                int ty = ctrl->pointer_y + 12;
                int tip_line_step = structural_font_height(renderer, ctrl->font_small, 14) + 2;
                int line_w1 = 0;
                int line_w2 = 0;
                int line_w3 = 0;
                int max_w = 0;
                int tip_h = 0;
                if (tip_line_step < 16) tip_line_step = 16;
                {
                    SDL_Color tip = {240, 240, 240, 255};
                    SDL_Color dim = {180, 190, 200, 255};
                    snprintf(line, sizeof(line), "Edge %d (A %d, B %d)",
                             best_edge->id, best_a->id, best_b->id);
                    if (TTF_SizeUTF8(ctrl->font_small, line, &line_w1, NULL) != 0) {
                        line_w1 = 0;
                    } else {
                        line_w1 = physics_sim_text_logical_pixels(renderer, line_w1);
                    }
                    snprintf(line, sizeof(line), "Axial: %.3f  Shear: %.3f",
                             best_edge->axial_stress,
                             0.5f * (best_edge->shear_force_a + best_edge->shear_force_b));
                    if (TTF_SizeUTF8(ctrl->font_small, line, &line_w2, NULL) != 0) {
                        line_w2 = 0;
                    } else {
                        line_w2 = physics_sim_text_logical_pixels(renderer, line_w2);
                    }
                    snprintf(line, sizeof(line), "Moment A: %.3f  B: %.3f",
                             best_edge->bending_moment_a,
                             best_edge->bending_moment_b);
                    if (TTF_SizeUTF8(ctrl->font_small, line, &line_w3, NULL) != 0) {
                        line_w3 = 0;
                    } else {
                        line_w3 = physics_sim_text_logical_pixels(renderer, line_w3);
                    }
                    max_w = line_w1;
                    if (line_w2 > max_w) max_w = line_w2;
                    if (line_w3 > max_w) max_w = line_w3;
                    tip_h = tip_line_step * 3;
                    if (ctrl->window_w > 0) {
                        int max_tx = ctrl->window_w - max_w - 12;
                        if (tx > max_tx) tx = max_tx;
                        if (tx < 8) tx = 8;
                    }
                    if (ctrl->window_h > 0) {
                        int max_ty = ctrl->window_h - tip_h - 12;
                        if (ty > max_ty) ty = max_ty;
                        if (ty < 8) ty = 8;
                    }

                    snprintf(line, sizeof(line), "Edge %d (A %d, B %d)",
                             best_edge->id, best_a->id, best_b->id);
                    render_text(renderer, ctrl->font_small, tx, ty, tip, line);
                    ty += tip_line_step;
                    snprintf(line, sizeof(line), "Axial: %.3f  Shear: %.3f",
                             best_edge->axial_stress,
                             0.5f * (best_edge->shear_force_a + best_edge->shear_force_b));
                    render_text(renderer, ctrl->font_small, tx, ty, dim, line);
                    ty += tip_line_step;
                    snprintf(line, sizeof(line), "Moment A: %.3f  B: %.3f",
                             best_edge->bending_moment_a,
                             best_edge->bending_moment_b);
                    render_text(renderer, ctrl->font_small, tx, ty, dim, line);
                }
            }
        }
    }
}
