#include "app/editor/scene_editor_precision_helpers.h"

#include <math.h>
#include <stdio.h>

#include "app/editor/scene_editor_canvas.h"
#include "app/editor/scene_editor_model.h"
#include "app/shape_lookup.h"
#include "geo/shape_asset.h"
#include "render/import_project.h"

static inline float import_pos_to_unit_local(float pos, float span) {
    float min = 0.5f - span;
    float max = 0.5f + span;
    float t = (pos - min) / (max - min);
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return t;
}

static void draw_circle(SDL_Renderer *renderer, int cx, int cy, int radius, SDL_Color color) {
    if (!renderer || radius <= 0) return;
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    int x = radius - 1;
    int y = 0;
    int dx = 1;
    int dy = 1;
    int err = dx - (radius << 1);
    while (x >= y) {
        SDL_Rect dot0 = {cx + x, cy + y, 1, 1};
        SDL_Rect dot1 = {cx + y, cy + x, 1, 1};
        SDL_Rect dot2 = {cx - y, cy + x, 1, 1};
        SDL_Rect dot3 = {cx - x, cy + y, 1, 1};
        SDL_Rect dot4 = {cx - x, cy - y, 1, 1};
        SDL_Rect dot5 = {cx - y, cy - x, 1, 1};
        SDL_Rect dot6 = {cx + y, cy - x, 1, 1};
        SDL_Rect dot7 = {cx + x, cy - y, 1, 1};
        SDL_RenderFillRect(renderer, &dot0);
        SDL_RenderFillRect(renderer, &dot1);
        SDL_RenderFillRect(renderer, &dot2);
        SDL_RenderFillRect(renderer, &dot3);
        SDL_RenderFillRect(renderer, &dot4);
        SDL_RenderFillRect(renderer, &dot5);
        SDL_RenderFillRect(renderer, &dot6);
        SDL_RenderFillRect(renderer, &dot7);
        if (err <= 0) {
            y++;
            err += dy;
            dy += 2;
        }
        if (err > 0) {
            x--;
            dx += 2;
            err += dx - (radius << 1);
        }
    }
}

static void draw_polyline(SDL_Renderer *renderer, const SDL_Point *pts, int count) {
    if (!renderer || !pts || count < 2) return;
    for (int i = 1; i < count; ++i) {
        SDL_RenderDrawLine(renderer,
                           pts[i - 1].x, pts[i - 1].y,
                           pts[i].x, pts[i].y);
    }
}

int scene_editor_precision_local_emitter_index_for_object(const FluidScenePreset *preset, int obj_index) {
    if (!preset || obj_index < 0 || obj_index >= (int)preset->object_count) return -1;
    for (size_t ei = 0; ei < preset->emitter_count; ++ei) {
        if (preset->emitters[ei].attached_object == obj_index) {
            return (int)ei;
        }
    }
    return -1;
}

int scene_editor_precision_local_emitter_index_for_import(const FluidScenePreset *preset, int import_index) {
    if (!preset || import_index < 0 || import_index >= (int)preset->import_shape_count) return -1;
    for (size_t ei = 0; ei < preset->emitter_count; ++ei) {
        if (preset->emitters[ei].attached_import == import_index) {
            return (int)ei;
        }
    }
    return -1;
}

void scene_editor_precision_screen_to_normalized(int w,
                                                 int h,
                                                 int sx,
                                                 int sy,
                                                 float *out_x,
                                                 float *out_y) {
    if (!out_x || !out_y || w <= 0 || h <= 0) return;
    *out_x = (float)sx / (float)w;
    *out_y = (float)sy / (float)h;
}

void scene_editor_precision_draw_import_outline(SDL_Renderer *renderer,
                                                const ImportedShape *imp,
                                                const ShapeAssetLibrary *lib,
                                                int win_w,
                                                int win_h,
                                                bool selected,
                                                bool hovered,
                                                const SDL_Color *tint_override) {
    if (!renderer || !imp || !lib) return;
    float span_x = 1.0f, span_y = 1.0f;
    import_compute_span_from_window(win_w, win_h, &span_x, &span_y);
    float cx_unit = import_pos_to_unit_local(imp->position_x, span_x);
    float cy_unit = import_pos_to_unit_local(imp->position_y, span_y);
    float cx_win = cx_unit * (float)win_w;
    float cy_win = cy_unit * (float)win_h;

    SDL_Color col = selected ? (SDL_Color){255, 255, 200, 255}
                             : (hovered ? (SDL_Color){180, 220, 255, 220}
                                        : (SDL_Color){180, 186, 195, 200});
    if (tint_override) {
        col = *tint_override;
    }

    if (imp->gravity_enabled && imp->collider_part_count > 0) {
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
                int sx = (int)lroundf(wx);
                int sy = (int)lroundf(wy);
                pts[npts++] = (SDL_Point){sx, sy};
            }
            if (npts >= 3) {
                pts[npts] = pts[0];
                draw_polyline(renderer, pts, npts + 1);
            }
        }
    } else {
        const ShapeAsset *asset = shape_lookup_from_path(lib, imp->path);
        if (!asset) return;
        ShapeAssetBounds b;
        if (!shape_asset_bounds(asset, &b) || !b.valid) return;
        float size_x = b.max_x - b.min_x;
        float size_y = b.max_y - b.min_y;
        float max_dim = fmaxf(size_x, size_y);
        if (max_dim <= 0.0001f) return;
        const float desired_fit = 0.25f;
        float norm = (imp->scale * desired_fit) / max_dim;
        float cx = 0.5f * (b.min_x + b.max_x);
        float cy = 0.5f * (b.min_y + b.max_y);
        float cos_a = cosf(imp->rotation_deg * (float)M_PI / 180.0f);
        float sin_a = sinf(imp->rotation_deg * (float)M_PI / 180.0f);
        for (size_t pi = 0; pi < asset->path_count; ++pi) {
            const ShapeAssetPath *path = &asset->paths[pi];
            if (!path || path->point_count < 2) continue;
            for (size_t i = 1; i < path->point_count; ++i) {
                ShapeAssetPoint a = path->points[i - 1];
                ShapeAssetPoint bpt = path->points[i];
                float axn = (a.x - cx) * norm;
                float ayn = (a.y - cy) * norm;
                float bxn = (bpt.x - cx) * norm;
                float byn = (bpt.y - cy) * norm;
                float ra_x = axn * cos_a - ayn * sin_a + imp->position_x;
                float ra_y = axn * sin_a + ayn * cos_a + imp->position_y;
                float rb_x = bxn * cos_a - byn * sin_a + imp->position_x;
                float rb_y = bxn * sin_a + byn * cos_a + imp->position_y;
                float scale_px = (float)((win_w < win_h) ? win_w : win_h);
                float cx_px = 0.5f * (float)win_w;
                float cy_px = 0.5f * (float)win_h;
                int ax = (int)lroundf(cx_px + (ra_x - 0.5f) * scale_px);
                int ay = (int)lroundf(cy_px + (ra_y - 0.5f) * scale_px);
                int bx = (int)lroundf(cx_px + (rb_x - 0.5f) * scale_px);
                int by = (int)lroundf(cy_px + (rb_y - 0.5f) * scale_px);
                SDL_SetRenderDrawColor(renderer, col.r, col.g, col.b, col.a);
                SDL_RenderDrawLine(renderer, ax, ay, bx, by);
            }
        }
    }

    float handle_px = scene_editor_canvas_handle_size_px(win_w, win_h);
    float scale_px = (float)((win_w < win_h) ? win_w : win_h);
    float handle_norm = (scale_px > 0.0f) ? (handle_px / scale_px) : 0.0f;
    if (handle_norm > 0.0f) {
        float hx = imp->position_x;
        float hy = imp->position_y;
        float cos_a = cosf(imp->rotation_deg * (float)M_PI / 180.0f);
        float sin_a = sinf(imp->rotation_deg * (float)M_PI / 180.0f);
        float hx2 = hx + cos_a * (handle_norm * 1.4f);
        float hy2 = hy + sin_a * (handle_norm * 1.4f);
        float cx_px = 0.5f * (float)win_w;
        float cy_px = 0.5f * (float)win_h;
        int hx_px = (int)lroundf(cx_px + (hx - 0.5f) * scale_px);
        int hy_px = (int)lroundf(cy_px + (hy - 0.5f) * scale_px);
        int hx2_px = (int)lroundf(cx_px + (hx2 - 0.5f) * scale_px);
        int hy2_px = (int)lroundf(cy_px + (hy2 - 0.5f) * scale_px);
        SDL_SetRenderDrawColor(renderer, col.r, col.g, col.b, col.a);
        SDL_RenderDrawLine(renderer, hx_px, hy_px, hx2_px, hy2_px);
        draw_circle(renderer, hx2_px, hy2_px, (int)lroundf(handle_px * 0.4f), col);
    }
}

SDL_Color scene_editor_precision_emitter_color(const FluidEmitter *em) {
    switch (em->type) {
    case EMITTER_DENSITY_SOURCE: return (SDL_Color){246, 233, 90, 255};
    case EMITTER_VELOCITY_JET:   return (SDL_Color){74, 232, 124, 255};
    case EMITTER_SINK:           return (SDL_Color){232, 96, 136, 255};
    default:                     return (SDL_Color){255, 255, 255, 255};
    }
}

void scene_editor_precision_draw_import_overlays(SDL_Renderer *renderer,
                                                 const FluidScenePreset *scene,
                                                 const ShapeAssetLibrary *lib,
                                                 int win_w,
                                                 int win_h,
                                                 int selected_import,
                                                 int hover_import) {
    if (!renderer || !scene || !lib) return;
    for (int ii = 0; ii < (int)scene->import_shape_count; ++ii) {
        const ImportedShape *imp = &scene->import_shapes[ii];
        if (!imp->enabled) continue;
        bool sel = (ii == selected_import);
        bool hov = (ii == hover_import);
        SDL_Color tint = {180, 186, 195, 200};
        bool has_tint = false;
        for (size_t ei = 0; ei < scene->emitter_count; ++ei) {
            int attached_imp = scene->emitters[ei].attached_import;
            if (attached_imp == ii) {
                tint = scene_editor_precision_emitter_color(&scene->emitters[ei]);
                has_tint = true;
                break;
            }
        }
        if (!has_tint && imp->gravity_enabled) {
            tint = (SDL_Color){90, 220, 120, 255};
            has_tint = true;
        }
        scene_editor_precision_draw_import_outline(renderer,
                                                   imp,
                                                   lib,
                                                   win_w,
                                                   win_h,
                                                   sel,
                                                   hov,
                                                   has_tint ? &tint : NULL);
    }
}

void scene_editor_precision_draw_hover_tooltip(SDL_Renderer *renderer,
                                               TTF_Font *font_small,
                                               const FluidScenePreset *scene,
                                               int pointer_x,
                                               int pointer_y,
                                               int hover_object,
                                               int hover_import,
                                               int hover_emitter,
                                               int hover_edge) {
    if (!renderer || !font_small || !scene || pointer_x < 0 || pointer_y < 0) return;

    const char *lines[3] = {0};
    char buf1[96];
    char buf2[64];
    char buf3[64];
    int line_count = 0;
    if (hover_emitter >= 0 && hover_emitter < (int)scene->emitter_count) {
        const FluidEmitter *em = &scene->emitters[hover_emitter];
        snprintf(buf1, sizeof(buf1), "%s emitter", emitter_type_name(em->type));
        snprintf(buf2, sizeof(buf2), "Radius %.3f  Strength %.2f", em->radius, em->strength);
        snprintf(buf3, sizeof(buf3), "Pos %.3f, %.3f", em->position_x, em->position_y);
        lines[0] = buf1;
        lines[1] = buf2;
        lines[2] = buf3;
        line_count = 3;
    } else if (hover_object >= 0 && hover_object < (int)scene->object_count) {
        const PresetObject *obj = &scene->objects[hover_object];
        const char *type = (obj->type == PRESET_OBJECT_BOX) ? "Box" : "Circle";
        snprintf(buf1, sizeof(buf1), "Object: %s", type);
        if (obj->type == PRESET_OBJECT_BOX) {
            snprintf(buf2, sizeof(buf2), "Size %.3f x %.3f", obj->size_x, obj->size_y);
            snprintf(buf3, sizeof(buf3), "Pos %.3f, %.3f", obj->position_x, obj->position_y);
        } else {
            snprintf(buf2, sizeof(buf2), "Radius %.3f", obj->size_x);
            snprintf(buf3, sizeof(buf3), "Pos %.3f, %.3f", obj->position_x, obj->position_y);
        }
        lines[0] = buf1;
        lines[1] = buf2;
        lines[2] = buf3;
        line_count = 3;
        int em_idx = scene_editor_precision_local_emitter_index_for_object(scene, hover_object);
        if (em_idx >= 0 && em_idx < (int)scene->emitter_count) {
            const FluidEmitter *em = &scene->emitters[em_idx];
            snprintf(buf3, sizeof(buf3), "Emitter: %s (r=%.3f, s=%.2f)",
                     emitter_type_name(em->type), em->radius, em->strength);
            lines[2] = buf3;
        }
    } else if (hover_import >= 0 && hover_import < (int)scene->import_shape_count) {
        const ImportedShape *imp = &scene->import_shapes[hover_import];
        snprintf(buf1, sizeof(buf1), "Import: %s", imp->path[0] ? imp->path : "(unnamed)");
        snprintf(buf2, sizeof(buf2), "Pos %.3f, %.3f  Scale %.3f", imp->position_x, imp->position_y, imp->scale);
        lines[0] = buf1;
        lines[1] = buf2;
        line_count = 2;
        int em_idx = scene_editor_precision_local_emitter_index_for_import(scene, hover_import);
        if (em_idx >= 0 && em_idx < (int)scene->emitter_count) {
            const FluidEmitter *em = &scene->emitters[em_idx];
            snprintf(buf3, sizeof(buf3), "Emitter: %s (r=%.3f, s=%.2f)",
                     emitter_type_name(em->type), em->radius, em->strength);
            lines[2] = buf3;
            line_count = 3;
        }
    } else if (hover_edge >= 0 && hover_edge < BOUNDARY_EDGE_COUNT) {
        const BoundaryFlow *flow = &scene->boundary_flows[hover_edge];
        snprintf(buf1, sizeof(buf1), "Edge: %s", boundary_edge_name(hover_edge));
        snprintf(buf2, sizeof(buf2), "Mode: %s", boundary_mode_label(flow->mode));
        lines[0] = buf1;
        lines[1] = buf2;
        line_count = 2;
        if (flow->mode != BOUNDARY_FLOW_DISABLED) {
            snprintf(buf3, sizeof(buf3), "Strength %.2f", flow->strength);
            lines[2] = buf3;
            line_count = 3;
        }
    }

    if (line_count > 0) {
        scene_editor_canvas_draw_tooltip(renderer,
                                         font_small,
                                         pointer_x,
                                         pointer_y,
                                         lines,
                                         line_count);
    }
}
