#include "app/editor/scene_editor_precision.h"

#include <math.h>
#include <stdio.h>

#include "app/editor/scene_editor_canvas.h"

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

bool scene_editor_run_precision(const AppConfig *cfg,
                                FluidScenePreset *working,
                                int *selected_object,
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
    int sel_obj = *selected_object;
    int hover_object = -1;
    int pointer_x = -1;
    int pointer_y = -1;
    bool dragging_object = false;
    bool dragging_handle = false;
    float drag_off_x = 0.0f, drag_off_y = 0.0f;
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
                    }
                    break;
                case SDLK_UP:
                    if (sel_obj >= 0 && sel_obj < (int)local.object_count) {
                        PresetObject *obj = &local.objects[sel_obj];
                        obj->position_y = clamp01(obj->position_y - 0.01f);
                        clamp_object(obj);
                        dirty = true;
                    }
                    break;
                case SDLK_DOWN:
                    if (sel_obj >= 0 && sel_obj < (int)local.object_count) {
                        PresetObject *obj = &local.objects[sel_obj];
                        obj->position_y = clamp01(obj->position_y + 0.01f);
                        clamp_object(obj);
                        dirty = true;
                    }
                    break;
                case SDLK_LEFT:
                    if (sel_obj >= 0 && sel_obj < (int)local.object_count) {
                        PresetObject *obj = &local.objects[sel_obj];
                        obj->position_x = clamp01(obj->position_x - 0.01f);
                        clamp_object(obj);
                        dirty = true;
                    }
                    break;
                case SDLK_RIGHT:
                    if (sel_obj >= 0 && sel_obj < (int)local.object_count) {
                        PresetObject *obj = &local.objects[sel_obj];
                        obj->position_x = clamp01(obj->position_x + 0.01f);
                        clamp_object(obj);
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
                    dragging_object = true;
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
                                                       &mode);
                if (hit >= 0) {
                    sel_obj = -1;
                }
                break;
            }
            case SDL_MOUSEBUTTONUP:
                if (ev.button.button == SDL_BUTTON_LEFT) {
                    dragging_object = false;
                    dragging_handle = false;
                    handle_started = false;
                }
                break;
            case SDL_MOUSEMOTION:
                pointer_x = ev.motion.x;
                pointer_y = ev.motion.y;
                if (dragging_handle && sel_obj >= 0 &&
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

        scene_editor_canvas_draw_background(renderer, 0, 0, win_w, win_h);
        scene_editor_canvas_draw_boundary_flows(renderer,
                                                0,
                                                0,
                                                win_w,
                                                win_h,
                                                local.boundary_flows,
                                                -1,
                                                -1,
                                                false);
        scene_editor_canvas_draw_emitters(renderer,
                                          0,
                                          0,
                                          win_w,
                                          win_h,
                                          &local,
                                          -1,
                                          -1,
                                          font_small);
        scene_editor_canvas_draw_objects(renderer,
                                         0,
                                         0,
                                         win_w,
                                         win_h,
                                         &local,
                                         sel_obj,
                                         hover_object);

        if (hover_object >= 0 && hover_object < (int)local.object_count &&
            pointer_x >= 0 && pointer_y >= 0 && font_small) {
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
        if (dirty && dirty_out) *dirty_out = true;
        return true;
    }
    *working = original;
    return false;
}
