#include "app/editor/scene_editor_canvas.h"
#include "app/editor/scene_editor_internal.h"
#include "app/editor/scene_editor.h"
#include "app/menu/menu_render.h"
#include "geo/shape_asset.h"
#include "app/shape_lookup.h"
#include "render/import_project.h"
#include "physics/math/math2d.h"
#include "vk_renderer.h"

#include <math.h>
#include <string.h>

static SDL_Color COLOR_CANVAS    = {12, 14, 18, 255};
static SDL_Color COLOR_SOURCE    = {252, 163, 17, 255};
static SDL_Color COLOR_JET       = {64, 201, 255, 255};
static SDL_Color COLOR_SINK      = {200, 80, 255, 255};
static SDL_Color COLOR_TEXT      = {245, 247, 250, 255};
static SDL_Color COLOR_TEXT_DIM  = {180, 186, 195, 220};
static SDL_Color COLOR_SELECTED  = {255, 255, 255, 255};
static SDL_Color COLOR_GRID_LINE = {32, 36, 40, 255};
static SDL_Color COLOR_BOUNDARY_DISABLED = {45, 50, 58, 180};

static void refresh_canvas_theme(void) {
    COLOR_CANVAS = menu_color_bg();
    COLOR_TEXT = menu_color_text();
    COLOR_TEXT_DIM = menu_color_text_dim();
    COLOR_SELECTED = menu_color_accent();
    COLOR_GRID_LINE = menu_color_panel();
    COLOR_BOUNDARY_DISABLED = menu_color_panel();
    COLOR_BOUNDARY_DISABLED.a = 180;
}

static void draw_circle(SDL_Renderer *renderer, int cx, int cy, int radius, SDL_Color color);
static void draw_polyline(SDL_Renderer *renderer, const SDL_Point *pts, int count);

static inline float import_pos_to_unit_canvas(float pos, float span) {
    float min = 0.5f - span;
    float max = 0.5f + span;
    float t = (pos - min) / (max - min);
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return t;
}

static SDL_Color emitter_color(const FluidEmitter *em) {
    switch (em->type) {
    case EMITTER_DENSITY_SOURCE: return COLOR_SOURCE;
    case EMITTER_VELOCITY_JET:   return COLOR_JET;
    case EMITTER_SINK:           return COLOR_SINK;
    default:                     return COLOR_JET;
    }
}

static float emitter_visual_radius_norm(const FluidScenePreset *preset,
                                        int emitter_index,
                                        const int *emitter_object_map,
                                        const int *emitter_import_map) {
    if (!preset || emitter_index < 0 || emitter_index >= (int)preset->emitter_count) return 0.08f;
    const FluidEmitter *em = &preset->emitters[emitter_index];
    (void)emitter_object_map;
    (void)emitter_import_map;
    float radius_norm = em->radius;
    if (radius_norm < 0.02f) radius_norm = 0.02f;
    return radius_norm;
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

static void draw_asset_outline(SDL_Renderer *renderer,
                               const ShapeAsset *asset,
                               float pos_x, float pos_y,
                               float scale,
                               float rotation_rad,
                               int canvas_x, int canvas_y,
                               int canvas_w, int canvas_h,
                               SDL_Color col) {
    if (!renderer || !asset) return;
    ShapeAssetBounds b;
    if (!shape_asset_bounds(asset, &b) || !b.valid) return;
    float size_x = b.max_x - b.min_x;
    float size_y = b.max_y - b.min_y;
    float max_dim = fmaxf(size_x, size_y);
    if (max_dim <= 0.0001f) return;
    const float desired_fit = 0.25f; // default footprint for scale=1
    float norm = (scale * desired_fit) / max_dim;
    float cx = 0.5f * (b.min_x + b.max_x);
    float cy = 0.5f * (b.min_y + b.max_y);

    float cos_a = cosf(rotation_rad);
    float sin_a = sinf(rotation_rad);
    for (size_t pi = 0; pi < asset->path_count; ++pi) {
        const ShapeAssetPath *path = &asset->paths[pi];
        if (!path || path->point_count < 2) continue;
        for (size_t i = 1; i < path->point_count; ++i) {
            ShapeAssetPoint a = path->points[i - 1];
            ShapeAssetPoint bpt = path->points[i];
            // normalize about center so default scale ~1 fits in unit box
            float dax = (a.x - cx) * norm;
            float day = (a.y - cy) * norm;
            float dbx = (bpt.x - cx) * norm;
            float dby = (bpt.y - cy) * norm;
            float ra_x = dax * cos_a - day * sin_a + pos_x;
            float ra_y = dax * sin_a + day * cos_a + pos_y;
            float rb_x = dbx * cos_a - dby * sin_a + pos_x;
            float rb_y = dbx * sin_a + dby * cos_a + pos_y;
            float scale_px = (float)((canvas_w < canvas_h) ? canvas_w : canvas_h);
            float cx_px = (float)canvas_x + 0.5f * (float)canvas_w;
            float cy_px = (float)canvas_y + 0.5f * (float)canvas_h;
            int ax = (int)lroundf(cx_px + (ra_x - 0.5f) * scale_px);
            int ay = (int)lroundf(cy_px + (ra_y - 0.5f) * scale_px);
            int bx = (int)lroundf(cx_px + (rb_x - 0.5f) * scale_px);
            int by = (int)lroundf(cy_px + (rb_y - 0.5f) * scale_px);
            SDL_SetRenderDrawColor(renderer, col.r, col.g, col.b, col.a);
            SDL_RenderDrawLine(renderer, ax, ay, bx, by);
        }
    }

    // draw a visible handle arrow for imports
    float scale_px = (float)((canvas_w < canvas_h) ? canvas_w : canvas_h);
    float handle_norm = 0.0f;
    if (scale_px > 0.0f) {
        float extent_norm = fmaxf(0.5f * size_x * norm, 0.5f * size_y * norm);
        float margin = scene_editor_canvas_handle_size_norm(canvas_w, canvas_h) * 0.6f;
        handle_norm = extent_norm + margin;
    }
    if (handle_norm > 0.0f) {
        float hx = pos_x;
        float hy = pos_y;
        // Bias handle upward for default rotation to make it easier to spot.
        float angle = rotation_rad;
        float hx2 = hx + cosf(angle) * (handle_norm * 1.4f);
        float hy2 = hy + sinf(angle) * (handle_norm * 1.4f);
        float cx_px = (float)canvas_x + 0.5f * (float)canvas_w;
        float cy_px = (float)canvas_y + 0.5f * (float)canvas_h;
        int hx_px = (int)lroundf(cx_px + (hx - 0.5f) * scale_px);
        int hy_px = (int)lroundf(cy_px + (hy - 0.5f) * scale_px);
        int hx2_px = (int)lroundf(cx_px + (hx2 - 0.5f) * scale_px);
        int hy2_px = (int)lroundf(cy_px + (hy2 - 0.5f) * scale_px);
        SDL_SetRenderDrawColor(renderer, col.r, col.g, col.b, col.a);
        SDL_RenderDrawLine(renderer, hx_px, hy_px, hx2_px, hy2_px);
        float knob_px = scene_editor_canvas_handle_size_px(canvas_w, canvas_h) * 0.4f;
        if (knob_px < 4.0f) knob_px = 4.0f;
        draw_circle(renderer, hx2_px, hy2_px, (int)lroundf(knob_px), col);
    }
}

// Draw collider-derived outline (all parts) for a gravity-enabled import.
static void draw_import_collider_outline(SDL_Renderer *renderer,
                                         const SceneEditorState *state,
                                         const ImportedShape *imp,
                                         SDL_Color col) {
    if (!renderer || !state || !imp) return;
    if (!imp->gravity_enabled) return;
    if (imp->collider_part_count <= 0) return;

    float span_x = 1.0f, span_y = 1.0f;
    import_compute_span_from_window(state->cfg.window_w, state->cfg.window_h, &span_x, &span_y);
    float cx_unit = import_pos_to_unit_canvas(imp->position_x, span_x);
    float cy_unit = import_pos_to_unit_canvas(imp->position_y, span_y);
    float cx_win = cx_unit * (float)state->cfg.window_w;
    float cy_win = cy_unit * (float)state->cfg.window_h;

    SDL_SetRenderDrawColor(renderer, col.r, col.g, col.b, col.a);
    for (int pi = 0; pi < imp->collider_part_count && pi < 8; ++pi) {
        int offset = imp->collider_part_offsets[pi];
        int count = imp->collider_part_counts[pi];
        if (count < 3) continue;
        SDL_Point pts[40];
        int npts = 0;
        for (int vi = 0; vi < count && vi < 32; ++vi) {
            int idx = offset + vi;
            if (idx < 0 || idx >= 64) break;
            Vec2 v = imp->collider_parts_verts[idx];
            float wx = cx_win + v.x;
            float wy = cy_win + v.y;
            float norm_x = (state->cfg.window_w > 0) ? (wx / (float)state->cfg.window_w) : 0.0f;
            float norm_y = (state->cfg.window_h > 0) ? (wy / (float)state->cfg.window_h) : 0.0f;
            int sx, sy;
            scene_editor_canvas_project(state->canvas_x,
                                        state->canvas_y,
                                        state->canvas_width,
                                        state->canvas_height,
                                        norm_x,
                                        norm_y,
                                        &sx,
                                        &sy);
            pts[npts].x = sx;
            pts[npts].y = sy;
            npts++;
        }
        if (npts >= 3) {
            pts[npts] = pts[0];
            draw_polyline(renderer, pts, npts + 1);
        }
    }
}

void scene_editor_canvas_draw_imports(SDL_Renderer *renderer,
                                      const SceneEditorState *state) {
    if (!renderer || !state || !state->shape_library) return;
    refresh_canvas_theme();
    for (size_t i = 0; i < state->working.import_shape_count; ++i) {
        const ImportedShape *imp = &state->working.import_shapes[i];
        if (!imp->enabled) continue;
        SDL_Color col = (state->selected_row == (int)i && state->selection_kind == SELECTION_IMPORT)
                            ? COLOR_SELECTED
                            : COLOR_TEXT_DIM;
        // If an emitter is attached to this import, tint by emitter type.
        bool has_emitter = false;
        for (size_t ei = 0; ei < state->working.emitter_count; ++ei) {
            int attached_imp = state->emitter_import_map[ei];
            if (attached_imp < 0) attached_imp = state->working.emitters[ei].attached_import;
            if (attached_imp == (int)i) {
                col = emitter_color(&state->working.emitters[ei]);
                has_emitter = true;
                break;
            }
        }
        if (!has_emitter && imp->gravity_enabled) {
            col = (SDL_Color){90, 220, 120, 255};
        }
        // Always draw the authoring outline; if gravity is on, overlay the collider outline.
        const ShapeAsset *asset = shape_lookup_from_path(state->shape_library, imp->path);
        if (asset) {
            draw_asset_outline(renderer,
                               asset,
                               imp->position_x,
                               imp->position_y,
                               imp->scale,
                               imp->rotation_deg * (float)M_PI / 180.0f,
                               state->canvas_x,
                               state->canvas_y,
                               state->canvas_width,
                               state->canvas_height,
                               col);
        }
        if (imp->gravity_enabled) {
            draw_import_collider_outline(renderer, state, imp, col);
        }
    }
}

void scene_editor_canvas_draw_background(SDL_Renderer *renderer,
                                         int canvas_x,
                                         int canvas_y,
                                         int canvas_w,
                                         int canvas_h,
                                         bool preview_active,
                                         float preview_x_norm,
                                         float preview_y_norm) {
    if (!renderer || canvas_w <= 0 || canvas_h <= 0) return;
    refresh_canvas_theme();
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

    if (preview_active) {
        int gx, gy;
        scene_editor_canvas_project(canvas_x, canvas_y, canvas_w, canvas_h,
                                    preview_x_norm, preview_y_norm, &gx, &gy);
        SDL_SetRenderDrawColor(renderer, COLOR_SELECTED.r, COLOR_SELECTED.g, COLOR_SELECTED.b, 180);
        int preview_r = 16;
        SDL_Rect ghost = {gx - preview_r, gy - preview_r, preview_r * 2, preview_r * 2};
        SDL_RenderDrawRect(renderer, &ghost);
    }
}

static void draw_circle(SDL_Renderer *renderer, int cx, int cy, int radius, SDL_Color color) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    for (int dy = -radius; dy <= radius; ++dy) {
        for (int dx = -radius; dx <= radius; ++dx) {
            if (dx * dx + dy * dy <= radius * radius) {
                SDL_Rect dot = {cx + dx, cy + dy, 1, 1};
                SDL_RenderFillRect(renderer, &dot);
            }
        }
    }
}

static void draw_line(SDL_Renderer *renderer, int x0, int y0, int x1, int y1, SDL_Color color) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_RenderDrawLine(renderer, x0, y0, x1, y1);
}

static void draw_polyline(SDL_Renderer *renderer, const SDL_Point *pts, int count) {
    if (!renderer || !pts || count < 2) return;
    for (int i = 1; i < count; ++i) {
        SDL_RenderDrawLine(renderer,
                           pts[i - 1].x, pts[i - 1].y,
                           pts[i].x, pts[i].y);
    }
}

void scene_editor_canvas_draw_emitters(SDL_Renderer *renderer,
                                       int canvas_x,
                                       int canvas_y,
                                       int canvas_w,
                                       int canvas_h,
                                       const FluidScenePreset *preset,
                                       int selected_emitter,
                                       int hover_emitter,
                                       TTF_Font *font_small,
                                       const int *emitter_object_map,
                                       const int *emitter_import_map) {
    (void)font_small;
    if (!renderer || !preset) return;
    refresh_canvas_theme();

    for (size_t i = 0; i < preset->emitter_count; ++i) {
        const FluidEmitter *em = &preset->emitters[i];
        int obj_index = emitter_object_map ? emitter_object_map[i] : -1;
        int imp_index = emitter_import_map ? emitter_import_map[i] : -1;
        SDL_Color color = emitter_color(em);
        SDL_Color overlay = color;
        if ((int)i == selected_emitter) {
            overlay = lighten_color(color, SCENE_EDITOR_SELECT_HIGHLIGHT_FACTOR);
        } else if ((int)i == hover_emitter) {
            overlay = lighten_color(color, 0.15f);
        }

        int cx, cy;
        scene_editor_canvas_project(canvas_x, canvas_y, canvas_w, canvas_h,
                                    em->position_x, em->position_y,
                                    &cx, &cy);

        float radius_norm = emitter_visual_radius_norm(preset, (int)i, emitter_object_map, emitter_import_map);
        if (obj_index >= 0 && (size_t)obj_index < preset->object_count) {
            const PresetObject *obj = &preset->objects[obj_index];
            if (obj->type == PRESET_OBJECT_CIRCLE) {
                int radius_px = (int)(fmaxf(obj->size_x, obj->size_y) * (float)canvas_w);
                if (radius_px < 4) radius_px = 4;
                draw_circle(renderer, cx, cy, radius_px + 2, (SDL_Color){overlay.r, overlay.g, overlay.b, 80});
            } else {
                float half_w_px, half_h_px;
                scene_editor_canvas_object_visual_half_sizes_px(obj, canvas_w, canvas_h, &half_w_px, &half_h_px);
                SDL_Point pts[5];
                float cos_a = cosf(obj->angle);
                float sin_a = sinf(obj->angle);
                float hx = obj->position_x * (float)canvas_w;
                float hy = obj->position_y * (float)canvas_h;
                const float corner_x[4] = {-half_w_px,  half_w_px,  half_w_px, -half_w_px};
                const float corner_y[4] = {-half_h_px, -half_h_px,  half_h_px,  half_h_px};
                for (int j = 0; j < 4; ++j) {
                    float lx = corner_x[j];
                    float ly = corner_y[j];
                    float rx = lx * cos_a - ly * sin_a;
                    float ry = lx * sin_a + ly * cos_a;
                    pts[j].x = (int)lroundf(hx + rx) + canvas_x;
                    pts[j].y = (int)lroundf(hy + ry) + canvas_y;
                }
                pts[4] = pts[0];
                SDL_SetRenderDrawColor(renderer, overlay.r, overlay.g, overlay.b, 80);
                draw_polyline(renderer, pts, 5);
            }
        } else if (imp_index >= 0 && (size_t)imp_index < preset->import_shape_count) {
            // For imports, rely on the import outline tint; skip drawing a circle.
            SDL_SetRenderDrawColor(renderer, overlay.r, overlay.g, overlay.b, 70);
            SDL_RenderDrawLine(renderer, cx - 6, cy - 6, cx + 6, cy + 6);
            SDL_RenderDrawLine(renderer, cx - 6, cy + 6, cx + 6, cy - 6);
        } else {
            int radius_px = (int)(radius_norm * (float)fmin(canvas_w, canvas_h));
            if (radius_px < 4) radius_px = 4;
            draw_circle(renderer, cx, cy, radius_px, overlay);
        }

        int hx = 0, hy = 0;
        float hit_r = 0.0f;
        if (scene_editor_canvas_emitter_handle_point(preset,
                                                     canvas_x,
                                                     canvas_y,
                                                     canvas_w,
                                                     canvas_h,
                                                     (int)i,
                                                     emitter_object_map,
                                                     emitter_import_map,
                                                     &hx,
                                                     &hy,
                                                     &hit_r)) {
            SDL_Color handle_col = color;
            if ((int)i == selected_emitter) {
                handle_col = lighten_color(color, 0.08f);
            } else if ((int)i == hover_emitter) {
                handle_col = lighten_color(color, 0.12f);
            }
            draw_line(renderer, cx, cy, hx, hy, handle_col);
            int knob_r = (int)lroundf(hit_r);
            if (knob_r < 4) knob_r = 4;
            draw_circle(renderer, hx, hy, knob_r, handle_col);
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
                SDL_Rect dot = {x, y, 1, 1};
                SDL_RenderFillRect(renderer, &dot);
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

void scene_editor_canvas_draw_objects(SDL_Renderer *renderer,
                                      int canvas_x,
                                      int canvas_y,
                                      int canvas_w,
                                      int canvas_h,
                                      const FluidScenePreset *preset,
                                      int selected_object,
                                      int hover_object,
                                      const int *emitter_object_map) {
    if (!renderer || !preset) return;
    refresh_canvas_theme();

    for (size_t i = 0; i < preset->object_count; ++i) {
        const PresetObject *obj = &preset->objects[i];
        int cx, cy;
        scene_editor_canvas_project(canvas_x, canvas_y, canvas_w, canvas_h,
                                    obj->position_x, obj->position_y,
                                    &cx, &cy);
        int em_idx = -1;
        if (emitter_object_map) {
            for (size_t ei = 0; ei < preset->emitter_count; ++ei) {
                if (emitter_object_map[ei] == (int)i) {
                    em_idx = (int)ei;
                    break;
                }
            }
        } else {
            for (size_t ei = 0; ei < preset->emitter_count; ++ei) {
                if (preset->emitters[ei].attached_object == (int)i) {
                    em_idx = (int)ei;
                    break;
                }
            }
        }
        SDL_Color base = (obj->type == PRESET_OBJECT_BOX)
                             ? (SDL_Color){170, 120, 80, 255}
                             : (SDL_Color){255, 80, 80, 255};
        bool has_emitter = em_idx >= 0;
        if (!has_emitter && obj->gravity_enabled) {
            base = (SDL_Color){90, 220, 120, 255};
        }
        if (em_idx >= 0) {
            base = emitter_color(&preset->emitters[em_idx]);
        }
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
                SDL_SetRenderDrawColor(renderer, COLOR_SELECTED.r, COLOR_SELECTED.g, COLOR_SELECTED.b, 255);
                SDL_RenderDrawLine(renderer, cx, cy, hx, hy);
                float knob_px = scene_editor_canvas_handle_size_px(canvas_w, canvas_h) * 0.45f;
                if (knob_px < 8.0f) knob_px = 8.0f;
                draw_circle(renderer, hx, hy, (int)lroundf(knob_px), COLOR_SELECTED);
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
    refresh_canvas_theme();
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
    refresh_canvas_theme();
    SDL_Color bg = COLOR_CANVAS;
    SDL_Color border = COLOR_SELECTED;
    bg.a = 230;
    border.a = 100;
    SDL_Color text = COLOR_TEXT;

    int padding = 6;
    int max_w = 0;
    int total_h = 0;
    SDL_Surface *surfaces[8] = {0};
    if (line_count > 8) line_count = 8;

    for (int i = 0; i < line_count; ++i) {
        if (!lines[i]) continue;
        SDL_Surface *surf = TTF_RenderUTF8_Blended(font, lines[i], text);
        if (!surf) continue;
        surfaces[i] = surf;
        if (surf->w > max_w) max_w = surf->w;
        total_h += surf->h;
    }
    if (max_w == 0 || total_h == 0) {
        for (int i = 0; i < line_count; ++i) {
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
        SDL_Surface *surf = surfaces[i];
        if (!surf) continue;
        SDL_Rect dst = {rect.x + padding, y_cursor, surf->w, surf->h};
        VkRendererTexture tex = {0};
        if (vk_renderer_upload_sdl_surface_with_filter((VkRenderer *)renderer,
                                                       surf,
                                                       &tex,
                                                       VK_FILTER_LINEAR) == VK_SUCCESS) {
            vk_renderer_draw_texture((VkRenderer *)renderer, &tex, NULL, &dst);
            vk_renderer_queue_texture_destroy((VkRenderer *)renderer, &tex);
        }
        y_cursor += surf->h + spacing;
    }

    for (int i = 0; i < line_count; ++i) {
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
    refresh_canvas_theme();
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
            text_w = surf->w;
            SDL_Rect dst = {rect.x + 8, rect.y + rect.h / 2 - surf->h / 2,
                            surf->w, surf->h};
            VkRendererTexture tex = {0};
            if (vk_renderer_upload_sdl_surface_with_filter((VkRenderer *)renderer,
                                                           surf,
                                                           &tex,
                                                           VK_FILTER_LINEAR) == VK_SUCCESS) {
                vk_renderer_draw_texture((VkRenderer *)renderer, &tex, NULL, &dst);
                vk_renderer_queue_texture_destroy((VkRenderer *)renderer, &tex);
            }
            SDL_FreeSurface(surf);
        }
        if (input->caret_visible) {
            int caret_x = rect.x + 8 + text_w + 2;
            SDL_SetRenderDrawColor(renderer, COLOR_SELECTED.r, COLOR_SELECTED.g, COLOR_SELECTED.b, 255);
            SDL_RenderDrawLine(renderer, caret_x, rect.y + 6,
                               caret_x, rect.y + rect.h - 6);
        }
    } else if (font_main) {
        const char *title = (name && name[0]) ? name : "Untitled Preset";
        SDL_Surface *surf = TTF_RenderUTF8_Blended(font_main, title, COLOR_TEXT);
        if (surf) {
            SDL_Rect dst = {rect.x, rect.y, surf->w, surf->h};
            VkRendererTexture tex = {0};
            if (vk_renderer_upload_sdl_surface_with_filter((VkRenderer *)renderer,
                                                           surf,
                                                           &tex,
                                                           VK_FILTER_LINEAR) == VK_SUCCESS) {
                vk_renderer_draw_texture((VkRenderer *)renderer, &tex, NULL, &dst);
                vk_renderer_queue_texture_destroy((VkRenderer *)renderer, &tex);
            }
            SDL_FreeSurface(surf);
        }
    }
    (void)font_small;
}
