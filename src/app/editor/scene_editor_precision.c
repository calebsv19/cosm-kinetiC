#include "app/editor/scene_editor_precision.h"
#include "app/editor/scene_editor_precision_helpers.h"

#include <math.h>
#include <stdio.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_vulkan.h>

#include "app/editor/scene_editor_canvas.h"
#include "app/editor/scene_editor_model.h"
#include "physics/math/math2d.h"
#include "vk_renderer.h"
#include "render/vk_shared_device.h"

static const int DRAG_THRESHOLD_PX = 4;

static SDL_Window *g_precision_window = NULL;
static VkRenderer g_precision_renderer_storage;
static SDL_Renderer *g_precision_renderer = NULL;
static bool g_precision_initialized = false;
static bool g_precision_use_shared_device = false;

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

    SDL_Window *win = g_precision_window;
    SDL_Renderer *renderer = g_precision_renderer;
    bool use_shared_device = g_precision_use_shared_device;
    if (!g_precision_initialized) {
        win = SDL_CreateWindow("Precision Editor",
                               SDL_WINDOWPOS_CENTERED,
                               SDL_WINDOWPOS_CENTERED,
                               win_w,
                               win_h,
                               SDL_WINDOW_SHOWN | SDL_WINDOW_VULKAN);
        if (!win) return false;

        g_precision_window = win;
        g_precision_renderer = (SDL_Renderer *)&g_precision_renderer_storage;
        renderer = g_precision_renderer;

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
        g_precision_use_shared_device = use_shared_device;

        if (use_shared_device) {
            if (!vk_shared_device_init(win, &vk_cfg)) {
                fprintf(stderr, "[precision] Failed to init shared Vulkan device.\n");
                SDL_DestroyWindow(win);
                g_precision_window = NULL;
                g_precision_renderer = NULL;
                return false;
            }

            VkRendererDevice* shared_device = vk_shared_device_get();
            if (!shared_device) {
                fprintf(stderr, "[precision] Failed to access shared Vulkan device.\n");
                SDL_DestroyWindow(win);
                g_precision_window = NULL;
                g_precision_renderer = NULL;
                return false;
            }

            if (vk_renderer_init_with_device((VkRenderer *)renderer, shared_device, win, &vk_cfg) != VK_SUCCESS) {
                SDL_DestroyWindow(win);
                g_precision_window = NULL;
                g_precision_renderer = NULL;
                return false;
            }
            vk_shared_device_acquire();
        } else {
            if (vk_renderer_init((VkRenderer *)renderer, win, &vk_cfg) != VK_SUCCESS) {
                SDL_DestroyWindow(win);
                g_precision_window = NULL;
                g_precision_renderer = NULL;
                return false;
            }
        }
        g_precision_initialized = true;
    } else {
        SDL_SetWindowSize(win, win_w, win_h);
        SDL_ShowWindow(win);
    }
    SDL_RaiseWindow(win);
    vk_renderer_set_logical_size((VkRenderer *)renderer, (float)win_w, (float)win_h);

    FluidScenePreset local = *working;
    FluidScenePreset original = *working;
    int sel_obj = selected_object ? *selected_object : -1;
    int sel_import = selected_import ? *selected_import : -1;
    int sel_emitter = -1, hover_object = -1, hover_import = -1;
    int hover_emitter = -1, hover_edge = -1, pointer_x = -1, pointer_y = -1;
    SceneEditorHit hit_stack[32];
    int hit_count = 0;
    int hit_base = 0;
    bool pointer_down = false, drag_started = false;
    int down_x = 0, down_y = 0;
    bool dragging_object = false, dragging_import = false, dragging_import_handle = false;
    bool dragging_handle = false, dragging_emitter = false;
    EditorDragMode emitter_drag_mode = DRAG_NONE;
    float emitter_drag_off_x = 0.0f, emitter_drag_off_y = 0.0f;
    float emitter_handle_offset_px = 0.0f;
    float drag_off_x = 0.0f, drag_off_y = 0.0f;
    float import_handle_start_dist = 0.0f, import_handle_start_scale = 1.0f;
    float handle_ratio = 1.0f, handle_initial = 0.0f;
    bool handle_started = false, running = true, apply = true, dirty = false, device_lost = false;

    while (running) {
        int local_obj_map[MAX_FLUID_EMITTERS];
        int local_imp_map[MAX_FLUID_EMITTERS];
        for (size_t ei = 0; ei < MAX_FLUID_EMITTERS; ++ei) {
            local_obj_map[ei] = -1;
            local_imp_map[ei] = -1;
        }
        for (size_t ei = 0; ei < local.emitter_count && ei < MAX_FLUID_EMITTERS; ++ei) {
            local_obj_map[ei] = local.emitters[ei].attached_object;
            local_imp_map[ei] = local.emitters[ei].attached_import;
        }
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
                case SDLK_g:
                    if (sel_obj >= 0 && sel_obj < (int)local.object_count) {
                        int em_idx = scene_editor_precision_local_emitter_index_for_object(&local, sel_obj);
                        if (em_idx < 0) { // skip objects tied to emitters
                            PresetObject *obj = &local.objects[sel_obj];
                            obj->gravity_enabled = !obj->gravity_enabled;
                            fprintf(stderr, "[precision] G pressed: toggled gravity on obj %d -> %d\n",
                                    sel_obj, obj->gravity_enabled ? 1 : 0);
                            dirty = true;
                        } else {
                            fprintf(stderr, "[precision] G on emitter-bound obj %d (ignored)\n", sel_obj);
                        }
                    } else if (sel_import >= 0 && sel_import < (int)local.import_shape_count) {
                        int em_idx = scene_editor_precision_local_emitter_index_for_import(&local, sel_import);
                        if (em_idx < 0) {
                            ImportedShape *imp = &local.import_shapes[sel_import];
                            imp->gravity_enabled = !imp->gravity_enabled;
                            fprintf(stderr, "[precision] G pressed: toggled gravity on import %d -> %d\n",
                                    sel_import, imp->gravity_enabled ? 1 : 0);
                            dirty = true;
                        } else {
                            fprintf(stderr, "[precision] G on emitter-bound import %d (ignored)\n", sel_import);
                        }
                    } else {
                        fprintf(stderr, "[precision] G pressed with no object selection (obj=%d)\n", sel_obj);
                    }
                    break;
                case SDLK_DELETE:
                case SDLK_BACKSPACE:
                    if (sel_import >= 0 && sel_import < (int)local.import_shape_count) {
                        // Remove any attached emitter first.
                        for (int ei = (int)local.emitter_count - 1; ei >= 0; --ei) {
                            if (local.emitters[ei].attached_import == sel_import) {
                                if (sel_emitter == ei) sel_emitter = -1;
                                for (int j = ei; j + 1 < (int)local.emitter_count; ++j) {
                                    local.emitters[j] = local.emitters[j + 1];
                                }
                                local.emitter_count--;
                            }
                        }
                        for (size_t ei = 0; ei < local.emitter_count; ++ei) {
                            if (local.emitters[ei].attached_import > sel_import) {
                                local.emitters[ei].attached_import--;
                            }
                        }
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
                        int em_idx = scene_editor_precision_local_emitter_index_for_object(&local, sel_obj);
                        if (em_idx >= 0 && em_idx < (int)local.emitter_count) {
                            local.emitters[em_idx].position_x = obj->position_x;
                            local.emitters[em_idx].position_y = obj->position_y;
                        }
                        dirty = true;
                    } else if (sel_import >= 0 && sel_import < (int)local.import_shape_count) {
                        ImportedShape *imp = &local.import_shapes[sel_import];
                        imp->position_y = clamp01(imp->position_y - 0.01f);
                        int em_idx = scene_editor_precision_local_emitter_index_for_import(&local, sel_import);
                        if (em_idx >= 0 && em_idx < (int)local.emitter_count) {
                            local.emitters[em_idx].position_x = imp->position_x;
                            local.emitters[em_idx].position_y = imp->position_y;
                        }
                        dirty = true;
                    }
                    break;
                case SDLK_DOWN:
                    if (sel_obj >= 0 && sel_obj < (int)local.object_count) {
                        PresetObject *obj = &local.objects[sel_obj];
                        obj->position_y = clamp01(obj->position_y + 0.01f);
                        clamp_object(obj);
                        int em_idx = scene_editor_precision_local_emitter_index_for_object(&local, sel_obj);
                        if (em_idx >= 0 && em_idx < (int)local.emitter_count) {
                            local.emitters[em_idx].position_x = obj->position_x;
                            local.emitters[em_idx].position_y = obj->position_y;
                        }
                        dirty = true;
                    } else if (sel_import >= 0 && sel_import < (int)local.import_shape_count) {
                        ImportedShape *imp = &local.import_shapes[sel_import];
                        imp->position_y = clamp01(imp->position_y + 0.01f);
                        int em_idx = scene_editor_precision_local_emitter_index_for_import(&local, sel_import);
                        if (em_idx >= 0 && em_idx < (int)local.emitter_count) {
                            local.emitters[em_idx].position_x = imp->position_x;
                            local.emitters[em_idx].position_y = imp->position_y;
                        }
                        dirty = true;
                    }
                    break;
                case SDLK_LEFT:
                    if (sel_obj >= 0 && sel_obj < (int)local.object_count) {
                        PresetObject *obj = &local.objects[sel_obj];
                        obj->position_x = clamp01(obj->position_x - 0.01f);
                        clamp_object(obj);
                        int em_idx = scene_editor_precision_local_emitter_index_for_object(&local, sel_obj);
                        if (em_idx >= 0 && em_idx < (int)local.emitter_count) {
                            local.emitters[em_idx].position_x = obj->position_x;
                            local.emitters[em_idx].position_y = obj->position_y;
                        }
                        dirty = true;
                    } else if (sel_import >= 0 && sel_import < (int)local.import_shape_count) {
                        ImportedShape *imp = &local.import_shapes[sel_import];
                        imp->position_x = clamp01(imp->position_x - 0.01f);
                        int em_idx = scene_editor_precision_local_emitter_index_for_import(&local, sel_import);
                        if (em_idx >= 0 && em_idx < (int)local.emitter_count) {
                            local.emitters[em_idx].position_x = imp->position_x;
                            local.emitters[em_idx].position_y = imp->position_y;
                        }
                        dirty = true;
                    }
                    break;
                case SDLK_RIGHT:
                    if (sel_obj >= 0 && sel_obj < (int)local.object_count) {
                        PresetObject *obj = &local.objects[sel_obj];
                        obj->position_x = clamp01(obj->position_x + 0.01f);
                        clamp_object(obj);
                        int em_idx = scene_editor_precision_local_emitter_index_for_object(&local, sel_obj);
                        if (em_idx >= 0 && em_idx < (int)local.emitter_count) {
                            local.emitters[em_idx].position_x = obj->position_x;
                            local.emitters[em_idx].position_y = obj->position_y;
                        }
                        dirty = true;
                    } else if (sel_import >= 0 && sel_import < (int)local.import_shape_count) {
                        ImportedShape *imp = &local.import_shapes[sel_import];
                        imp->position_x = clamp01(imp->position_x + 0.01f);
                        int em_idx = scene_editor_precision_local_emitter_index_for_import(&local, sel_import);
                        if (em_idx >= 0 && em_idx < (int)local.emitter_count) {
                            local.emitters[em_idx].position_x = imp->position_x;
                            local.emitters[em_idx].position_y = imp->position_y;
                        }
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
                pointer_down = true;
                drag_started = false;
                down_x = ev.button.x;
                down_y = ev.button.y;
                hit_count = scene_editor_canvas_collect_hits(&local,
                                                             shape_library,
                                                             0,
                                                             0,
                                                             win_w,
                                                             win_h,
                                                             down_x,
                                                             down_y,
                                                             local_obj_map,
                                                             local_imp_map,
                                                             hit_stack,
                                                             (int)(sizeof(hit_stack) / sizeof(hit_stack[0])));
                hit_base = 0;
                for (int i = 0; i < hit_count; ++i) {
                    SceneEditorHit *h = &hit_stack[i];
                    if ((h->kind == HIT_OBJECT || h->kind == HIT_OBJECT_HANDLE) && h->index == sel_obj) {
                        hit_base = i;
                        break;
                    }
                    if ((h->kind == HIT_IMPORT || h->kind == HIT_IMPORT_HANDLE) && h->index == sel_import) {
                        hit_base = i;
                        break;
                    }
                    if (h->kind == HIT_EMITTER && h->index == sel_emitter) {
                        hit_base = i;
                        break;
                    }
                }
                if (hit_count > 0) {
                    SceneEditorHit anchor = hit_stack[hit_base];
                    dragging_handle = false;
                    dragging_object = false;
                    dragging_import = false;
                    dragging_import_handle = false;
                    dragging_emitter = false;
                    handle_started = false;
                    switch (anchor.kind) {
                    case HIT_OBJECT_HANDLE: {
                        sel_obj = anchor.index;
                        sel_import = -1;
                        sel_emitter = -1;
                        dragging_handle = true;
                        PresetObject *obj = &local.objects[anchor.index];
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
                        float dx_px = (float)down_x - (float)cx;
                        float dy_px = (float)down_y - (float)cy;
                        float len_px = sqrtf(dx_px * dx_px + dy_px * dy_px);
                        float min_len_px = (obj->type == PRESET_OBJECT_BOX)
                                               ? (float)SCENE_EDITOR_OBJECT_MIN_HALF_PX
                                               : (float)SCENE_EDITOR_OBJECT_MIN_RADIUS_PX;
                        handle_initial = len_px - (float)SCENE_EDITOR_OBJECT_HANDLE_MARGIN_PX;
                        if (handle_initial < min_len_px) handle_initial = min_len_px;
                        break;
                    }
                    case HIT_OBJECT: {
                        sel_obj = anchor.index;
                        sel_import = -1;
                        sel_emitter = -1;
                        dragging_object = true;
                        PresetObject *obj = &local.objects[anchor.index];
                        float nx, ny;
                        scene_editor_precision_screen_to_normalized(win_w, win_h, down_x, down_y, &nx, &ny);
                        drag_off_x = nx - obj->position_x;
                        drag_off_y = ny - obj->position_y;
                        break;
                    }
                    case HIT_IMPORT_HANDLE: {
                        sel_import = anchor.index;
                        sel_obj = -1;
                        sel_emitter = -1;
                        dragging_import_handle = true;
                        const ImportedShape *imp = &local.import_shapes[anchor.index];
                        float nx, ny;
                        scene_editor_canvas_to_import_normalized(0, 0, win_w, win_h, down_x, down_y, &nx, &ny);
                        float dx = nx - imp->position_x;
                        float dy = ny - imp->position_y;
                        float dist = sqrtf(dx * dx + dy * dy);
                        if (dist < 0.0001f) dist = 0.0001f;
                        import_handle_start_dist = dist;
                        import_handle_start_scale = imp->scale;
                        break;
                    }
                    case HIT_IMPORT: {
                        sel_import = anchor.index;
                        sel_obj = -1;
                        sel_emitter = -1;
                        dragging_import = true;
                        float nx, ny;
                        scene_editor_canvas_to_import_normalized(0, 0, win_w, win_h, down_x, down_y, &nx, &ny);
                        drag_off_x = nx - local.import_shapes[anchor.index].position_x;
                        drag_off_y = ny - local.import_shapes[anchor.index].position_y;
                        break;
                    }
                    case HIT_EMITTER: {
                        int attached_obj = -1;
                        int attached_imp = -1;
                        if (anchor.index >= 0 && anchor.index < (int)local.emitter_count) {
                            attached_obj = local_obj_map[anchor.index];
                            attached_imp = local_imp_map[anchor.index];
                            if (attached_obj < 0) attached_obj = local.emitters[anchor.index].attached_object;
                            if (attached_imp < 0) attached_imp = local.emitters[anchor.index].attached_import;
                        }
                        sel_emitter = anchor.index;
                        if (attached_obj >= 0) {
                            sel_obj = attached_obj;
                            sel_import = -1;
                        } else if (attached_imp >= 0) {
                            sel_import = attached_imp;
                            sel_obj = -1;
                        } else {
                            sel_obj = -1;
                            sel_import = -1;
                        }
                        emitter_drag_mode = anchor.drag_mode;
                        emitter_handle_offset_px = 0.0f;
                        if (emitter_drag_mode == DRAG_POSITION && attached_obj >= 0 &&
                            attached_obj < (int)local.object_count) {
                            dragging_object = true;
                            PresetObject *obj = &local.objects[attached_obj];
                            float nx, ny;
                            scene_editor_precision_screen_to_normalized(win_w, win_h, down_x, down_y, &nx, &ny);
                            drag_off_x = nx - obj->position_x;
                            drag_off_y = ny - obj->position_y;
                        } else if (emitter_drag_mode == DRAG_POSITION && attached_imp >= 0 &&
                                   attached_imp < (int)local.import_shape_count) {
                            dragging_import = true;
                            float nx, ny;
                            scene_editor_canvas_to_import_normalized(0, 0, win_w, win_h, down_x, down_y, &nx, &ny);
                            drag_off_x = nx - local.import_shapes[attached_imp].position_x;
                            drag_off_y = ny - local.import_shapes[attached_imp].position_y;
                        } else {
                            dragging_emitter = true;
                            if (emitter_drag_mode == DRAG_POSITION) {
                                FluidEmitter *em = &local.emitters[anchor.index];
                                float nx, ny;
                                scene_editor_canvas_to_normalized(0, 0, win_w, win_h, down_x, down_y, &nx, &ny);
                                emitter_drag_off_x = nx - em->position_x;
                                emitter_drag_off_y = ny - em->position_y;
                            } else {
                                FluidEmitter *em = &local.emitters[anchor.index];
                                int cx = 0, cy = 0;
                                scene_editor_canvas_project(0, 0, win_w, win_h,
                                                            em->position_x, em->position_y,
                                                            &cx, &cy);
                                float dx = (float)down_x - (float)cx;
                                float dy = (float)down_y - (float)cy;
                                float len = sqrtf(dx * dx + dy * dy);
                                float min_dim = (float)((win_w < win_h) ? win_w : win_h);
                                float radius_px = em->radius * min_dim;
                                emitter_handle_offset_px = len - radius_px;
                                if (emitter_handle_offset_px < 0.0f) emitter_handle_offset_px = 0.0f;
                            }
                        }
                        drag_started = true;
                        break;
                    }
                    default:
                        break;
                    }
                }
                break;
            }
            case SDL_MOUSEBUTTONUP:
                if (ev.button.button == SDL_BUTTON_LEFT) {
                    if (pointer_down && !drag_started && hit_count > 1) {
                        int next = (hit_base + 1) % hit_count;
                        if (next != hit_base) {
                            SceneEditorHit next_hit = hit_stack[next];
                            switch (next_hit.kind) {
                            case HIT_OBJECT:
                            case HIT_OBJECT_HANDLE:
                                sel_obj = next_hit.index;
                                sel_import = -1;
                                sel_emitter = -1;
                                break;
                            case HIT_IMPORT:
                            case HIT_IMPORT_HANDLE:
                                sel_import = next_hit.index;
                                sel_obj = -1;
                                sel_emitter = -1;
                                break;
                            case HIT_EMITTER:
                                sel_emitter = next_hit.index;
                                sel_obj = -1;
                                sel_import = -1;
                                break;
                            default:
                                break;
                            }
                        }
                    }
                    dragging_object = false;
                    dragging_import = false;
                    dragging_import_handle = false;
                    dragging_handle = false;
                    dragging_emitter = false;
                    handle_started = false;
                    pointer_down = false;
                    drag_started = false;
                    hit_count = 0;
                    hit_base = 0;
                }
                break;
            case SDL_MOUSEMOTION:
                pointer_x = ev.motion.x;
                pointer_y = ev.motion.y;
                if (pointer_down && !drag_started) {
                    int dx = ev.motion.x - down_x;
                    int dy = ev.motion.y - down_y;
                    if ((dx * dx + dy * dy) > (DRAG_THRESHOLD_PX * DRAG_THRESHOLD_PX)) {
                        drag_started = true;
                    }
                }
                hover_import = scene_editor_canvas_hit_import(&local,
                                                              shape_library,
                                                              0,
                                                              0,
                                                              win_w,
                                                              win_h,
                                                              pointer_x,
                                                              pointer_y);
                EditorDragMode hover_em_mode = DRAG_NONE;
                hover_emitter = scene_editor_canvas_hit_test(&local,
                                                             0,
                                                             0,
                                                             win_w,
                                                             win_h,
                                                             pointer_x,
                                                             pointer_y,
                                                             &hover_em_mode,
                                                             local_obj_map,
                                                             local_imp_map);
                bool allow_drag = (!pointer_down) || drag_started;
                if (dragging_import_handle && allow_drag && sel_import >= 0 &&
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
                    int em_idx = scene_editor_precision_local_emitter_index_for_import(&local, sel_import);
                    if (em_idx >= 0 && em_idx < (int)local.emitter_count) {
                        local.emitters[em_idx].position_x = imp->position_x;
                        local.emitters[em_idx].position_y = imp->position_y;
                    }
                    dirty = true;
                } else if (dragging_import && allow_drag && sel_import >= 0 &&
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
                    // Keep attached emitter aligned in local copy.
                    int em_idx = scene_editor_precision_local_emitter_index_for_import(&local, sel_import);
                    if (em_idx >= 0 && em_idx < (int)local.emitter_count) {
                        local.emitters[em_idx].position_x = imp->position_x;
                        local.emitters[em_idx].position_y = imp->position_y;
                    }
                    dirty = true;
                } else if (dragging_object && allow_drag && sel_obj >= 0 &&
                           sel_obj < (int)local.object_count) {
                    PresetObject *obj = &local.objects[sel_obj];
                    float nx, ny;
                    scene_editor_precision_screen_to_normalized(win_w, win_h, ev.motion.x, ev.motion.y, &nx, &ny);
                    obj->position_x = clamp01(nx - drag_off_x);
                    obj->position_y = clamp01(ny - drag_off_y);
                    clamp_object(obj);
                    int em_idx = scene_editor_precision_local_emitter_index_for_object(&local, sel_obj);
                    if (em_idx >= 0 && em_idx < (int)local.emitter_count) {
                        local.emitters[em_idx].position_x = obj->position_x;
                        local.emitters[em_idx].position_y = obj->position_y;
                    }
                    dirty = true;
                } else if (dragging_handle && allow_drag && sel_obj >= 0 &&
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
                } else if (dragging_emitter && allow_drag && sel_emitter >= 0 &&
                           sel_emitter < (int)local.emitter_count) {
                    FluidEmitter *em = &local.emitters[sel_emitter];
                    int attached_obj = (sel_emitter < (int)local.emitter_count) ? local_obj_map[sel_emitter] : -1;
                    int attached_imp = (sel_emitter < (int)local.emitter_count) ? local_imp_map[sel_emitter] : -1;
                    if (attached_obj < 0) attached_obj = em->attached_object;
                    if (attached_imp < 0) attached_imp = em->attached_import;
                    if (emitter_drag_mode == DRAG_POSITION) {
                        float nx, ny;
                        scene_editor_canvas_to_normalized(0, 0, win_w, win_h, ev.motion.x, ev.motion.y, &nx, &ny);
                        em->position_x = clamp01(nx - emitter_drag_off_x);
                        em->position_y = clamp01(ny - emitter_drag_off_y);
                        if (attached_obj >= 0 && attached_obj < (int)local.object_count) {
                            PresetObject *obj = &local.objects[attached_obj];
                            obj->position_x = em->position_x;
                            obj->position_y = em->position_y;
                            clamp_object(obj);
                            em->position_x = obj->position_x;
                            em->position_y = obj->position_y;
                        } else if (attached_imp >= 0 && attached_imp < (int)local.import_shape_count) {
                            ImportedShape *imp = &local.import_shapes[attached_imp];
                            imp->position_x = em->position_x;
                            imp->position_y = em->position_y;
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
                            em->position_x = imp->position_x;
                            em->position_y = imp->position_y;
                        }
                        dirty = true;
                    } else if (emitter_drag_mode == DRAG_DIRECTION) {
                        int cx = 0, cy = 0;
                        scene_editor_canvas_project(0, 0, win_w, win_h,
                                                    em->position_x, em->position_y,
                                                    &cx,
                                                    &cy);
                        float dx = (float)(ev.motion.x - cx);
                        float dy = (float)(ev.motion.y - cy);
                        float len = sqrtf(dx * dx + dy * dy);
                        if (len > 0.0001f) {
                            em->dir_x = dx / len;
                            em->dir_y = dy / len;
                            float min_dim = (float)((win_w < win_h) ? win_w : win_h);
                            if (min_dim > 0.0f) {
                                float adj_len = len - emitter_handle_offset_px;
                                if (adj_len < 0.0f) adj_len = 0.0f;
                                float new_radius = adj_len / min_dim;
                                if (new_radius < 0.02f) new_radius = 0.02f;
                                if (new_radius > 0.6f) new_radius = 0.6f;
                                float ratio = (em->radius > 0.0001f) ? (new_radius / em->radius) : 1.0f;
                                em->radius = new_radius;
                                em->strength *= ratio;
                            }
                            dirty = true;
                        }
                    }
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
                hover_edge = scene_editor_canvas_hit_edge(0,
                                                          0,
                                                          win_w,
                                                          win_h,
                                                          pointer_x,
                                                          pointer_y);
                break;
            default:
                break;
            }
        }

        SDL_GetWindowSize(win, &win_w, &win_h);
        int drawable_w = win_w;
        int drawable_h = win_h;
        SDL_Vulkan_GetDrawableSize(win, &drawable_w, &drawable_h);
        if (drawable_w <= 0 || drawable_h <= 0) {
            SDL_Delay(16);
            continue;
        }
        VkExtent2D swap_extent = ((VkRenderer *)renderer)->context.swapchain.extent;
        if ((uint32_t)drawable_w != swap_extent.width ||
            (uint32_t)drawable_h != swap_extent.height) {
            vk_renderer_recreate_swapchain((VkRenderer *)renderer, win);
            vk_renderer_set_logical_size((VkRenderer *)renderer, (float)win_w, (float)win_h);
            continue;
        }

        VkCommandBuffer cmd = VK_NULL_HANDLE;
        VkFramebuffer fb = VK_NULL_HANDLE;
        VkExtent2D extent = {0};
        VkResult frame = vk_renderer_begin_frame((VkRenderer *)renderer, &cmd, &fb, &extent);
        if (frame == VK_ERROR_OUT_OF_DATE_KHR || frame == VK_SUBOPTIMAL_KHR) {
            vk_renderer_recreate_swapchain((VkRenderer *)renderer, win);
            vk_renderer_set_logical_size((VkRenderer *)renderer, (float)win_w, (float)win_h);
            continue;
        } else if (frame == VK_ERROR_DEVICE_LOST) {
            static int logged_device_lost = 0;
            if (!logged_device_lost) {
                fprintf(stderr, "[precision] Vulkan device lost; closing precision editor.\n");
                logged_device_lost = 1;
            }
            if (use_shared_device) {
                vk_shared_device_mark_lost();
            }
            device_lost = true;
            apply = false;
            break;
        } else if (frame != VK_SUCCESS) {
            fprintf(stderr, "[precision] vk_renderer_begin_frame failed: %d\n", frame);
            continue;
        }
        vk_renderer_set_logical_size((VkRenderer *)renderer, (float)win_w, (float)win_h);

        SDL_SetRenderDrawColor(renderer, 20, 22, 26, 255);
        SDL_Rect clear_rect = {0, 0, win_w, win_h};
        SDL_RenderFillRect(renderer, &clear_rect);

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

        scene_editor_precision_draw_import_overlays(renderer,
                                                    &local,
                                                    shape_library,
                                                    win_w,
                                                    win_h,
                                                    sel_import,
                                                    hover_import);

        scene_editor_canvas_draw_emitters(renderer,
                                          0,
                                          0,
                                          win_w,
                                          win_h,
                                          &local,
                                          sel_emitter,
                                          -1,
                                          font_small,
                                          local_obj_map,
                                          local_imp_map);
        scene_editor_canvas_draw_objects(renderer,
                                         0,
                                         0,
                                         win_w,
                                         win_h,
                                         &local,
                                         sel_obj,
                                         hover_object,
                                         local_obj_map);

        scene_editor_precision_draw_hover_tooltip(renderer,
                                                  font_small,
                                                  &local,
                                                  pointer_x,
                                                  pointer_y,
                                                  hover_object,
                                                  hover_import,
                                                  hover_emitter,
                                                  hover_edge);

        const char *hint = "Precision editor: drag objects/handles, +/- resize, arrows nudge, Enter apply, Esc cancel";
        if (font_small) {
            SDL_Surface *surf = TTF_RenderUTF8_Blended(font_small, hint, (SDL_Color){190, 198, 209, 255});
            if (surf) {
                VkRendererTexture tex = {0};
                if (vk_renderer_upload_sdl_surface_with_filter((VkRenderer *)renderer,
                                                               surf,
                                                               &tex,
                                                               VK_FILTER_LINEAR) == VK_SUCCESS) {
                    SDL_Rect dst = {12, win_h - surf->h - 12, surf->w, surf->h};
                    vk_renderer_draw_texture((VkRenderer *)renderer, &tex, NULL, &dst);
                    vk_renderer_queue_texture_destroy((VkRenderer *)renderer, &tex);
                }
                SDL_FreeSurface(surf);
            }
        }

        VkResult end = vk_renderer_end_frame((VkRenderer *)renderer, cmd);
        if (end == VK_ERROR_OUT_OF_DATE_KHR || end == VK_SUBOPTIMAL_KHR) {
            vk_renderer_recreate_swapchain((VkRenderer *)renderer, win);
            vk_renderer_set_logical_size((VkRenderer *)renderer, (float)win_w, (float)win_h);
        } else if (end == VK_ERROR_DEVICE_LOST) {
            static int logged_device_lost_end = 0;
            if (!logged_device_lost_end) {
                fprintf(stderr, "[precision] Vulkan device lost at end; closing precision editor.\n");
                logged_device_lost_end = 1;
            }
            if (use_shared_device) {
                vk_shared_device_mark_lost();
            }
            device_lost = true;
            apply = false;
            break;
        } else if (end != VK_SUCCESS) {
            fprintf(stderr, "[precision] vk_renderer_end_frame failed: %d\n", end);
        }
#if defined(__APPLE__)
        if (end == VK_SUCCESS) {
            vk_renderer_wait_idle((VkRenderer *)renderer);
        }
#endif
    }

    vk_renderer_wait_idle((VkRenderer *)renderer);
    if (device_lost) {
        if (use_shared_device) {
            vk_renderer_shutdown_surface((VkRenderer *)renderer);
            vk_shared_device_release();
        } else {
            vk_renderer_shutdown((VkRenderer *)renderer);
        }
        SDL_DestroyWindow(win);
        g_precision_window = NULL;
        g_precision_renderer = NULL;
        g_precision_initialized = false;
    } else {
        SDL_HideWindow(win);
    }

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
