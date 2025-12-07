#include "app/editor/scene_editor_precision.h"

#include <math.h>
#include <stdio.h>

#include "app/editor/scene_editor_canvas.h"
#include "app/shape_lookup.h"
#include "geo/shape_asset.h"

static void draw_circle(SDL_Renderer *renderer, int cx, int cy, int radius, SDL_Color color) {
    if (!renderer || radius <= 0) return;
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    int x = radius - 1;
    int y = 0;
    int dx = 1;
    int dy = 1;
    int err = dx - (radius << 1);
    while (x >= y) {
        SDL_RenderDrawPoint(renderer, cx + x, cy + y);
        SDL_RenderDrawPoint(renderer, cx + y, cy + x);
        SDL_RenderDrawPoint(renderer, cx - y, cy + x);
        SDL_RenderDrawPoint(renderer, cx - x, cy + y);
        SDL_RenderDrawPoint(renderer, cx - x, cy - y);
        SDL_RenderDrawPoint(renderer, cx - y, cy - x);
        SDL_RenderDrawPoint(renderer, cx + y, cy - x);
        SDL_RenderDrawPoint(renderer, cx + x, cy - y);
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

static float clamp01(float v) {
    if (v < 0.0f) return 0.0f;
    if (v > 1.0f) return 1.0f;
    return v;
}

static void clamp_object(PresetObject *obj) {
    if (!obj) return;
    if (obj->position_x < 0.0f) obj->position_x = 0.0f;
    if (obj->position_x > 1.0f) obj->position_x = 1.0f;
    if (obj->position_y < 0.0f) obj->position_y = 0.0f;
    if (obj->position_y > 1.0f) obj->position_y = 1.0f;
    if (obj->size_x < 0.005f) obj->size_x = 0.005f;
    if (obj->size_y < 0.005f) obj->size_y = 0.005f;
}

static void screen_to_normalized(int w, int h, int sx, int sy, float *out_x, float *out_y) {
    if (!out_x || !out_y || w <= 0 || h <= 0) return;
    *out_x = (float)sx / (float)w;
    *out_y = (float)sy / (float)h;
}

static void draw_import_outline(SDL_Renderer *renderer,
                                const ImportedShape *imp,
                                const ShapeAssetLibrary *lib,
                                int win_w,
                                int win_h,
                                bool selected,
                                bool hovered) {
    if (!renderer || !imp || !lib) return;
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
    SDL_Color col = selected ? (SDL_Color){255, 255, 200, 255}
                             : (hovered ? (SDL_Color){180, 220, 255, 220}
                                        : (SDL_Color){180, 186, 195, 200});
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
    // draw a larger handle arrow scaled with canvas size for readability
    float handle_px = scene_editor_canvas_handle_size_px(win_w, win_h);
    float scale_px = (float)((win_w < win_h) ? win_w : win_h);
    float handle_norm = (scale_px > 0.0f) ? (handle_px / scale_px) : 0.0f;
    if (handle_norm > 0.0f) {
        float hx = imp->position_x;
        float hy = imp->position_y;
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

bool scene_editor_run_precision(const AppConfig *cfg,
                                FluidScenePreset *working,
                                int *selected_object,
                                int *selected_import,
                                const ShapeAssetLibrary *shape_library,
                                TTF_Font *font_small,
                                TTF_Font *font_main,
                                bool *dirty_out) {
    (void)font_main;
    if (!cfg || !working || !selected_object) return false;
    if (dirty_out) *dirty_out = false;

    float aspect = working->domain_height > 0.0f
                       ? working->domain_width / working->domain_height
                       : 1.0f;
    if (aspect < 0.2f) aspect = 0.2f;
    if (aspect > 10.0f) aspect = 10.0f;

    SDL_Rect display;
    int disp_w = 1920;
    int disp_h = 1080;
    if (SDL_GetDisplayBounds(0, &display) == 0) {
        disp_w = display.w;
        disp_h = display.h;
    }

    // Baseline height: use a tunnel-friendly 384 default for tunnel mode, otherwise config height.
    int target_h = (cfg->sim_mode == SIM_MODE_WIND_TUNNEL || working->domain == SCENE_DOMAIN_WIND_TUNNEL)
                       ? 384
                       : (cfg->window_h > 0 ? cfg->window_h : 600);
    if (target_h < 200) target_h = 200;
    int target_w = (int)lroundf((float)target_h * aspect);

    // Fit to display with margins; if too wide, shrink height so width fits.
    int margin_w = 80;
    int margin_h = 120;
    float scale_w = (float)(disp_w - margin_w) / (float)target_w;
    float scale_h = (float)(disp_h - margin_h) / (float)target_h;
    float scale = 1.0f;
    if (target_w + margin_w > disp_w || target_h + margin_h > disp_h) {
        scale = fminf(scale_w, scale_h);
        if (scale < 0.1f) scale = 0.1f;
    }
    target_w = (int)lroundf((float)target_w * scale);
    target_h = (int)lroundf((float)target_h * scale);
    if (target_w < 320) target_w = 320;
    if (target_h < 200) target_h = 200;

    int win_w = target_w;
    int win_h = target_h;

    SDL_Window *win = SDL_CreateWindow("Precision Editor",
                                       SDL_WINDOWPOS_CENTERED,
                                       SDL_WINDOWPOS_CENTERED,
                                       win_w,
                                       win_h,
                                       SDL_WINDOW_SHOWN);
    if (!win) return false;
    SDL_Renderer *renderer = SDL_CreateRenderer(win,
                                                -1,
                                                SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        SDL_DestroyWindow(win);
        return false;
    }

    FluidScenePreset local = *working;
    FluidScenePreset original = *working;
    int sel_obj = selected_object ? *selected_object : -1;
    int sel_import = selected_import ? *selected_import : -1;
    int hover_object = -1;
    int hover_import = -1;
    int pointer_x = -1;
    int pointer_y = -1;
    bool dragging_object = false;
    bool dragging_import = false;
    bool dragging_import_handle = false;
    bool dragging_handle = false;
    float drag_off_x = 0.0f, drag_off_y = 0.0f;
    float import_handle_start_dist = 0.0f;
    float import_handle_start_scale = 1.0f;
    float handle_ratio = 1.0f;
    float handle_initial = 0.0f;
    bool handle_started = false;
    bool running = true;
    bool apply = true;
    bool dirty = false;

    while (running) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            switch (ev.type) {
            case SDL_QUIT:
                running = false;
                apply = true;
                break;
            case SDL_WINDOWEVENT:
                if (ev.window.event == SDL_WINDOWEVENT_CLOSE) {
                    running = false;
                    apply = true;
                }
                break;
            case SDL_KEYDOWN: {
                SDL_Keycode key = ev.key.keysym.sym;
                switch (key) {
                case SDLK_ESCAPE:
                    running = false;
                    apply = false;
                    break;
                case SDLK_RETURN:
                case SDLK_KP_ENTER:
                    running = false;
                    apply = true;
                    break;
                case SDLK_EQUALS:
                case SDLK_PLUS:
                case SDLK_KP_PLUS:
                    if (sel_obj >= 0 && sel_obj < (int)local.object_count) {
                        PresetObject *obj = &local.objects[sel_obj];
                        obj->size_x *= 1.1f;
                        obj->size_y *= 1.1f;
                        clamp_object(obj);
                        dirty = true;
                    } else if (sel_import >= 0 && sel_import < (int)local.import_shape_count) {
                        ImportedShape *imp = &local.import_shapes[sel_import];
                        imp->scale *= 1.1f;
                        if (imp->scale < 0.01f) imp->scale = 0.01f;
                        dirty = true;
                    }
                    break;
                case SDLK_MINUS:
                case SDLK_UNDERSCORE:
                case SDLK_KP_MINUS:
                    if (sel_obj >= 0 && sel_obj < (int)local.object_count) {
                        PresetObject *obj = &local.objects[sel_obj];
                        obj->size_x *= 0.9f;
                        obj->size_y *= 0.9f;
                        clamp_object(obj);
                        dirty = true;
                    } else if (sel_import >= 0 && sel_import < (int)local.import_shape_count) {
                        ImportedShape *imp = &local.import_shapes[sel_import];
                        imp->scale *= 0.9f;
                        if (imp->scale < 0.01f) imp->scale = 0.01f;
                        dirty = true;
                    }
                    break;
                case SDLK_DELETE:
                case SDLK_BACKSPACE:
                    if (sel_import >= 0 && sel_import < (int)local.import_shape_count) {
                        for (int j = sel_import; j + 1 < (int)local.import_shape_count; ++j) {
                            local.import_shapes[j] = local.import_shapes[j + 1];
                        }
                        local.import_shape_count--;
                        sel_import = -1;
                        dirty = true;
                    }
                    break;
                case SDLK_UP:
                    if (sel_obj >= 0 && sel_obj < (int)local.object_count) {
                        PresetObject *obj = &local.objects[sel_obj];
                        obj->position_y = clamp01(obj->position_y - 0.01f);
                        clamp_object(obj);
                        dirty = true;
                    } else if (sel_import >= 0 && sel_import < (int)local.import_shape_count) {
                        ImportedShape *imp = &local.import_shapes[sel_import];
                        imp->position_y = clamp01(imp->position_y - 0.01f);
                        dirty = true;
                    }
                    break;
                case SDLK_DOWN:
                    if (sel_obj >= 0 && sel_obj < (int)local.object_count) {
                        PresetObject *obj = &local.objects[sel_obj];
                        obj->position_y = clamp01(obj->position_y + 0.01f);
                        clamp_object(obj);
                        dirty = true;
                    } else if (sel_import >= 0 && sel_import < (int)local.import_shape_count) {
                        ImportedShape *imp = &local.import_shapes[sel_import];
                        imp->position_y = clamp01(imp->position_y + 0.01f);
                        dirty = true;
                    }
                    break;
                case SDLK_LEFT:
                    if (sel_obj >= 0 && sel_obj < (int)local.object_count) {
                        PresetObject *obj = &local.objects[sel_obj];
                        obj->position_x = clamp01(obj->position_x - 0.01f);
                        clamp_object(obj);
                        dirty = true;
                    } else if (sel_import >= 0 && sel_import < (int)local.import_shape_count) {
                        ImportedShape *imp = &local.import_shapes[sel_import];
                        imp->position_x = clamp01(imp->position_x - 0.01f);
                        dirty = true;
                    }
                    break;
                case SDLK_RIGHT:
                    if (sel_obj >= 0 && sel_obj < (int)local.object_count) {
                        PresetObject *obj = &local.objects[sel_obj];
                        obj->position_x = clamp01(obj->position_x + 0.01f);
                        clamp_object(obj);
                        dirty = true;
                    } else if (sel_import >= 0 && sel_import < (int)local.import_shape_count) {
                        ImportedShape *imp = &local.import_shapes[sel_import];
                        imp->position_x = clamp01(imp->position_x + 0.01f);
                        dirty = true;
                    }
                    break;
                default:
                    break;
                }
                break;
            }
            case SDL_MOUSEBUTTONDOWN: {
                if (ev.button.button != SDL_BUTTON_LEFT) break;
                int mx = ev.button.x;
                int my = ev.button.y;
                EditorDragMode mode = DRAG_NONE;
                // Check imports first
                int imp_hit = scene_editor_canvas_hit_import(&local,
                                                             shape_library,
                                                             0,
                                                             0,
                                                             win_w,
                                                             win_h,
                                                             mx,
                                                             my);
                // Handle hit first for rotate/scale
                int handle_idx = -1;
                int hx = 0, hy = 0;
                float hit_radius_px = scene_editor_canvas_handle_size_px(win_w, win_h) * 0.6f;
                for (int i = (int)local.import_shape_count - 1; i >= 0; --i) {
                    const ImportedShape *imp = &local.import_shapes[i];
                    if (!imp->enabled) continue;
                    if (!scene_editor_canvas_import_handle_point(0, 0, win_w, win_h, imp, &hx, &hy)) continue;
                    float dx = (float)mx - (float)hx;
                    float dy = (float)my - (float)hy;
                    if ((dx * dx + dy * dy) <= hit_radius_px * hit_radius_px) {
                        handle_idx = i;
                        break;
                    }
                }
                if (handle_idx >= 0) {
                    sel_import = handle_idx;
                    sel_obj = -1;
                    dragging_import_handle = true;
                    const ImportedShape *imp = &local.import_shapes[handle_idx];
                    float nx, ny;
                    scene_editor_canvas_to_import_normalized(0, 0, win_w, win_h, mx, my, &nx, &ny);
                    float dx = nx - imp->position_x;
                    float dy = ny - imp->position_y;
                    float dist = sqrtf(dx * dx + dy * dy);
                    if (dist < 0.0001f) dist = 0.0001f;
                    import_handle_start_dist = dist;
                    import_handle_start_scale = imp->scale;
                    break;
                }
                if (imp_hit >= 0) {
                    sel_import = imp_hit;
                    sel_obj = -1;
                    dragging_import = true;
                    float nx, ny;
                    scene_editor_canvas_to_import_normalized(0, 0, win_w, win_h, mx, my, &nx, &ny);
                    drag_off_x = nx - local.import_shapes[imp_hit].position_x;
                    drag_off_y = ny - local.import_shapes[imp_hit].position_y;
                    break;
                }
                int handle_hit = scene_editor_canvas_hit_object_handle(&local,
                                                                       0,
                                                                       0,
                                                                       win_w,
                                                                       win_h,
                                                                       mx,
                                                                       my);
                if (handle_hit >= 0) {
                    sel_obj = handle_hit;
                    dragging_handle = true;
                    dragging_object = false;
                    handle_started = false;
                    PresetObject *obj = &local.objects[handle_hit];
                    int cx = 0, cy = 0;
                    scene_editor_canvas_project(0, 0, win_w, win_h,
                                                obj->position_x, obj->position_y,
                                                &cx, &cy);
                    float half_w_px = 0.0f, half_h_px = 0.0f;
                    scene_editor_canvas_object_visual_half_sizes_px(obj, win_w, win_h,
                                                                    &half_w_px, &half_h_px);
                    handle_ratio = (obj->type == PRESET_OBJECT_BOX && half_w_px > 0.0001f)
                                       ? (half_h_px / half_w_px)
                                       : 1.0f;
                    float dx_px = (float)mx - (float)cx;
                    float dy_px = (float)my - (float)cy;
                    float len_px = sqrtf(dx_px * dx_px + dy_px * dy_px);
                    float min_len_px = (obj->type == PRESET_OBJECT_BOX)
                                           ? (float)SCENE_EDITOR_OBJECT_MIN_HALF_PX
                                           : (float)SCENE_EDITOR_OBJECT_MIN_RADIUS_PX;
                    handle_initial = len_px - (float)SCENE_EDITOR_OBJECT_HANDLE_MARGIN_PX;
                    if (handle_initial < min_len_px) handle_initial = min_len_px;
                    break;
                }

                int obj_hit = scene_editor_canvas_hit_object(&local,
                                                             0,
                                                             0,
                                                             win_w,
                                                             win_h,
                                                             mx,
                                                             my);
                if (obj_hit >= 0) {
                    sel_obj = obj_hit;
                    sel_import = -1;
                    dragging_object = true;
                    dragging_import = false;
                    dragging_handle = false;
                    handle_started = false;
                    PresetObject *obj = &local.objects[obj_hit];
                    float nx, ny;
                    screen_to_normalized(win_w, win_h, mx, my, &nx, &ny);
                    drag_off_x = nx - obj->position_x;
                    drag_off_y = ny - obj->position_y;
                    break;
                }

                int hit = scene_editor_canvas_hit_test(&local,
                                                       0,
                                                       0,
                                                       win_w,
                                                       win_h,
                                                       mx,
                                                       my,
                                                       &mode,
                                                       NULL);
                if (hit >= 0) {
                    sel_obj = -1;
                    sel_import = -1;
                }
                break;
            }
            case SDL_MOUSEBUTTONUP:
                if (ev.button.button == SDL_BUTTON_LEFT) {
                    dragging_object = false;
                    dragging_import = false;
                    dragging_import_handle = false;
                    dragging_handle = false;
                    handle_started = false;
                }
                break;
            case SDL_MOUSEMOTION:
                pointer_x = ev.motion.x;
                pointer_y = ev.motion.y;
                hover_import = scene_editor_canvas_hit_import(&local,
                                                              shape_library,
                                                              0,
                                                              0,
                                                              win_w,
                                                              win_h,
                                                              pointer_x,
                                                              pointer_y);
                if (dragging_import_handle && sel_import >= 0 &&
                    sel_import < (int)local.import_shape_count) {
                    ImportedShape *imp = &local.import_shapes[sel_import];
                    float nx, ny;
                    scene_editor_canvas_to_import_normalized(0, 0, win_w, win_h, ev.motion.x, ev.motion.y, &nx, &ny);
                    float dx = nx - imp->position_x;
                    float dy = ny - imp->position_y;
                    float dist = sqrtf(dx * dx + dy * dy);
                    if (dist < 0.0001f) dist = 0.0001f;
                    float ratio = dist / import_handle_start_dist;
                    imp->scale = import_handle_start_scale * ratio;
                    if (imp->scale < 0.01f) imp->scale = 0.01f;
                    imp->rotation_deg = atan2f(dy, dx) * 180.0f / (float)M_PI;
                    dirty = true;
                } else if (dragging_import && sel_import >= 0 &&
                    sel_import < (int)local.import_shape_count) {
                    ImportedShape *imp = &local.import_shapes[sel_import];
                    float nx, ny;
                    scene_editor_canvas_to_import_normalized(0, 0, win_w, win_h, ev.motion.x, ev.motion.y, &nx, &ny);
                    imp->position_x = nx - drag_off_x;
                    imp->position_y = ny - drag_off_y;
                    float min_dim = (float)((win_w < win_h) ? win_w : win_h);
                    float span_x = 0.5f * ((float)win_w / min_dim);
                    float span_y = 0.5f * ((float)win_h / min_dim);
                    float min_x = 0.5f - span_x;
                    float max_x = 0.5f + span_x;
                    float min_y = 0.5f - span_y;
                    float max_y = 0.5f + span_y;
                    if (imp->position_x < min_x) imp->position_x = min_x;
                    if (imp->position_x > max_x) imp->position_x = max_x;
                    if (imp->position_y < min_y) imp->position_y = min_y;
                    if (imp->position_y > max_y) imp->position_y = max_y;
                    dirty = true;
                } else if (dragging_object && sel_obj >= 0 &&
                           sel_obj < (int)local.object_count) {
                    PresetObject *obj = &local.objects[sel_obj];
                    float nx, ny;
                    screen_to_normalized(win_w, win_h, ev.motion.x, ev.motion.y, &nx, &ny);
                    obj->position_x = clamp01(nx - drag_off_x);
                    obj->position_y = clamp01(ny - drag_off_y);
                    clamp_object(obj);
                    dirty = true;
                } else if (dragging_handle && sel_obj >= 0 &&
                    sel_obj < (int)local.object_count) {
                    PresetObject *obj = &local.objects[sel_obj];
                    int cx = 0, cy = 0;
                    scene_editor_canvas_project(0, 0, win_w, win_h,
                                                obj->position_x, obj->position_y,
                                                &cx, &cy);
                    float dx_px = (float)ev.motion.x - (float)cx;
                    float dy_px = (float)ev.motion.y - (float)cy;
                    float len_px = sqrtf(dx_px * dx_px + dy_px * dy_px);
                    float min_len_px = (obj->type == PRESET_OBJECT_BOX)
                                           ? (float)SCENE_EDITOR_OBJECT_MIN_HALF_PX
                                           : (float)SCENE_EDITOR_OBJECT_MIN_RADIUS_PX;
                    float adjusted_px = len_px - (float)SCENE_EDITOR_OBJECT_HANDLE_MARGIN_PX;
                    if (adjusted_px < min_len_px) adjusted_px = min_len_px;
                    if (!handle_started) {
                        if (fabsf(adjusted_px - handle_initial) <= 1.0f) {
                            break;
                        }
                        handle_started = true;
                    }
                    obj->angle = atan2f(dy_px, dx_px);
                    if (obj->type == PRESET_OBJECT_BOX) {
                        float ratio_px = handle_ratio;
                        if (ratio_px <= 0.01f) ratio_px = 1.0f;
                        float half_w_px = adjusted_px;
                        float half_h_px = adjusted_px * ratio_px;
                        if (half_w_px < (float)SCENE_EDITOR_OBJECT_MIN_HALF_PX) {
                            half_w_px = (float)SCENE_EDITOR_OBJECT_MIN_HALF_PX;
                        }
                        if (half_h_px < (float)SCENE_EDITOR_OBJECT_MIN_HALF_PX) {
                            half_h_px = (float)SCENE_EDITOR_OBJECT_MIN_HALF_PX;
                        }
                        obj->size_x = half_w_px / (float)win_w;
                        obj->size_y = half_h_px / (float)win_h;
                    } else {
                        obj->size_x = adjusted_px / (float)win_w;
                        obj->size_y = adjusted_px / (float)win_w;
                    }
                    clamp_object(obj);
                    dirty = true;
                    break;
                }
                if (dragging_object && sel_obj >= 0 &&
                    sel_obj < (int)local.object_count) {
                    PresetObject *obj = &local.objects[sel_obj];
                    float nx, ny;
                    screen_to_normalized(win_w, win_h, ev.motion.x, ev.motion.y, &nx, &ny);
                    nx -= drag_off_x;
                    ny -= drag_off_y;
                    obj->position_x = clamp01(nx);
                    obj->position_y = clamp01(ny);
                    clamp_object(obj);
                    dirty = true;
                    break;
                }
                hover_object = scene_editor_canvas_hit_object(&local,
                                                              0,
                                                              0,
                                                              win_w,
                                                              win_h,
                                                              ev.motion.x,
                                                              ev.motion.y);
                if (hover_object < 0) {
                    int handle_hover = scene_editor_canvas_hit_object_handle(&local,
                                                                             0,
                                                                             0,
                                                                             win_w,
                                                                             win_h,
                                                                             ev.motion.x,
                                                                             ev.motion.y);
                    if (handle_hover >= 0) hover_object = handle_hover;
                }
                break;
            default:
                break;
            }
        }

        SDL_SetRenderDrawColor(renderer, 20, 22, 26, 255);
        SDL_RenderClear(renderer);

        scene_editor_canvas_draw_background(renderer, 0, 0, win_w, win_h, false, 0.0f, 0.0f);
        scene_editor_canvas_draw_boundary_flows(renderer,
                                                0,
                                                0,
                                                win_w,
                                                win_h,
                                                local.boundary_flows,
                                                -1,
                                                -1,
                                                false);

        // Draw imports (asset outlines)
        for (int ii = 0; ii < (int)local.import_shape_count; ++ii) {
            const ImportedShape *imp = &local.import_shapes[ii];
            if (!imp->enabled) continue;
            bool sel = (ii == sel_import);
            bool hov = (ii == hover_import);
            draw_import_outline(renderer, imp, shape_library, win_w, win_h, sel, hov);
        }

        scene_editor_canvas_draw_emitters(renderer,
                                          0,
                                          0,
                                          win_w,
                                          win_h,
                                          &local,
                                          -1,
                                          -1,
                                          font_small,
                                          NULL);
        scene_editor_canvas_draw_objects(renderer,
                                         0,
                                         0,
                                         win_w,
                                         win_h,
                                         &local,
                                         sel_obj,
                                         hover_object);

        if (pointer_x >= 0 && pointer_y >= 0 && font_small) {
            if (hover_object >= 0 && hover_object < (int)local.object_count) {
                const PresetObject *obj = &local.objects[hover_object];
                const char *type = (obj->type == PRESET_OBJECT_BOX) ? "Box" : "Circle";
                char buf1[64];
                char buf2[64];
                char buf3[64];
                const char *lines[3] = {buf1, buf2, buf3};
                snprintf(buf1, sizeof(buf1), "Object: %s", type);
                if (obj->type == PRESET_OBJECT_BOX) {
                    snprintf(buf2, sizeof(buf2), "Size %.3f x %.3f", obj->size_x, obj->size_y);
                    snprintf(buf3, sizeof(buf3), "Pos %.3f, %.3f", obj->position_x, obj->position_y);
                } else {
                    snprintf(buf2, sizeof(buf2), "Radius %.3f", obj->size_x);
                    snprintf(buf3, sizeof(buf3), "Pos %.3f, %.3f", obj->position_x, obj->position_y);
                }
                scene_editor_canvas_draw_tooltip(renderer,
                                                 font_small,
                                                 pointer_x,
                                                 pointer_y,
                                                 lines,
                                                 3);
            } else if (hover_import >= 0 && hover_import < (int)local.import_shape_count) {
                const ImportedShape *imp = &local.import_shapes[hover_import];
                char buf1[96];
                char buf2[64];
                const char *lines[2] = {buf1, buf2};
                snprintf(buf1, sizeof(buf1), "Import: %s", imp->path[0] ? imp->path : "(unnamed)");
                snprintf(buf2, sizeof(buf2), "Pos %.3f, %.3f  Scale %.3f", imp->position_x, imp->position_y, imp->scale);
                scene_editor_canvas_draw_tooltip(renderer,
                                                 font_small,
                                                 pointer_x,
                                                 pointer_y,
                                                 lines,
                                                 2);
            }
        }

        const char *hint = "Precision editor: drag objects/handles, +/- resize, arrows nudge, Enter apply, Esc cancel";
        if (font_small) {
            SDL_Surface *surf = TTF_RenderUTF8_Blended(font_small, hint, (SDL_Color){190, 198, 209, 255});
            if (surf) {
                SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer, surf);
                if (tex) {
                    SDL_Rect dst = {12, win_h - surf->h - 12, surf->w, surf->h};
                    SDL_RenderCopy(renderer, tex, NULL, &dst);
                    SDL_DestroyTexture(tex);
                }
                SDL_FreeSurface(surf);
            }
        }

        SDL_RenderPresent(renderer);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(win);

    if (apply) {
        *working = local;
        *selected_object = sel_obj;
        if (selected_import) *selected_import = sel_import;
        if (dirty && dirty_out) *dirty_out = true;
        return true;
    }
    *working = original;
    return false;
}
