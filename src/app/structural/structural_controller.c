#include "app/structural/structural_controller.h"
#include "app/structural/structural_controller_internal.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_vulkan.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "input/input.h"
#include "input/input_context.h"
#include "app/data_paths.h"
#include "config/config_loader.h"
#include "font_paths.h"
#include "render/text_upload_policy.h"
#include "vk_renderer.h"
#include "render/vk_shared_device.h"

static TTF_Font *load_font(const AppConfig *cfg, int size, SDL_Renderer *renderer) {
    const char *paths[] = {
        FONT_BODY_PATH_1,
        FONT_BODY_PATH_2,
        FONT_TITLE_PATH_1,
        FONT_TITLE_PATH_2
    };
    int scaled_size = app_config_scale_text_point_size(cfg, size, 6);
    scaled_size = physics_sim_text_raster_point_size(renderer, scaled_size, 6);
    for (size_t i = 0; i < sizeof(paths) / sizeof(paths[0]); ++i) {
        TTF_Font *font = TTF_OpenFont(paths[i], scaled_size);
        if (font) return font;
    }
    return NULL;
}

static bool structural_reload_fonts(StructuralController *ctrl,
                                    const AppConfig *cfg,
                                    SDL_Renderer *renderer) {
    TTF_Font *small = NULL;
    TTF_Font *hud = NULL;
    if (!ctrl || !cfg) return false;
    small = load_font(cfg, 12, renderer);
    hud = load_font(cfg, 14, renderer);
    if (!small || !hud) {
        if (small) TTF_CloseFont(small);
        if (hud) TTF_CloseFont(hud);
        return false;
    }
    if (ctrl->font_small) TTF_CloseFont(ctrl->font_small);
    if (ctrl->font_hud) TTF_CloseFont(ctrl->font_hud);
    ctrl->font_small = small;
    ctrl->font_hud = hud;
    return true;
}

static bool structural_apply_text_zoom_shortcut(const InputCommands *cmds,
                                                AppConfig *cfg,
                                                StructuralController *ctrl,
                                                SDL_Renderer *renderer) {
    int next_step = 0;
    const char *runtime_config_path = physics_sim_runtime_config_path();
    if (!cmds || !cfg || !ctrl) return false;
    if (!(cmds->text_zoom_in_requested ||
          cmds->text_zoom_out_requested ||
          cmds->text_zoom_reset_requested)) {
        return false;
    }
    next_step = cfg->text_zoom_step;
    if (cmds->text_zoom_reset_requested) {
        next_step = 0;
    } else {
        if (cmds->text_zoom_in_requested) next_step += 1;
        if (cmds->text_zoom_out_requested) next_step -= 1;
    }
    next_step = app_config_text_zoom_step_clamp(next_step);
    if (next_step == cfg->text_zoom_step) return false;
    cfg->text_zoom_step = next_step;

    if (!config_loader_save(cfg, runtime_config_path)) {
        fprintf(stderr, "[struct] Failed to persist runtime config to %s\n",
                runtime_config_path);
    }
    if (!structural_reload_fonts(ctrl, cfg, renderer)) {
        fprintf(stderr, "[struct] Failed to reload fonts after zoom update.\n");
    }
    return true;
}

static void on_pointer_down(void *user, const InputPointerState *state) {
    (void)user;
    (void)state;
}

static void on_pointer_up(void *user, const InputPointerState *state) {
    (void)user;
    (void)state;
}

static void on_pointer_move(void *user, const InputPointerState *state) {
    (void)user;
    (void)state;
}

static void runtime_reset_sim(StructuralController *ctrl) {
    if (!ctrl) return;
    structural_scene_clear_solution(&ctrl->scene);
    structural_controller_runtime_view_sync_from_scene(&ctrl->runtime, &ctrl->scene);
    ctrl->dynamic_playing = false;
    ctrl->dynamic_step = false;
    ctrl->gravity_ramp_time = 0.0f;
    ctrl->sim_time = 0.0f;
}

static void runtime_set_overlay(StructuralController *ctrl, SDL_Keycode key) {
    if (!ctrl) return;
    if (key == SDLK_t) {
        ctrl->show_stress = true;
        ctrl->show_bending = false;
        ctrl->show_shear = false;
        return;
    }
    if (key == SDLK_b) {
        ctrl->show_stress = false;
        ctrl->show_bending = true;
        ctrl->show_shear = false;
        return;
    }
    if (key == SDLK_v) {
        ctrl->show_stress = false;
        ctrl->show_bending = false;
        ctrl->show_shear = true;
        return;
    }
}

static void on_key_down(void *user, SDL_Keycode key, SDL_Keymod mod) {
    StructuralController *ctrl = (StructuralController *)user;
    if (!ctrl) return;
    if (key == SDLK_SPACE || key == SDLK_RETURN) {
        ctrl->solve_requested = true;
        return;
    }
    if (key == SDLK_e) {
        ctrl->dynamic_mode = !ctrl->dynamic_mode;
        ctrl->dynamic_playing = false;
        ctrl->dynamic_step = false;
        structural_controller_runtime_view_sync_from_scene(&ctrl->runtime, &ctrl->scene);
        ctrl->gravity_ramp_time = 0.0f;
        ctrl->sim_time = 0.0f;
        return;
    }
    if (key == SDLK_p) {
        if (ctrl->dynamic_mode) {
            ctrl->dynamic_playing = !ctrl->dynamic_playing;
            if (ctrl->dynamic_playing) {
                ctrl->gravity_ramp_time = 0.0f;
            }
        }
        return;
    }
    if (key == SDLK_z) {
        ctrl->integrator = (ctrl->integrator == STRUCT_INTEGRATOR_EXPLICIT)
                               ? STRUCT_INTEGRATOR_NEWMARK
                               : STRUCT_INTEGRATOR_EXPLICIT;
        return;
    }
    if (key == SDLK_s) {
        if (ctrl->dynamic_mode && !ctrl->dynamic_playing) {
            ctrl->dynamic_step = true;
        }
        return;
    }
    if (key == SDLK_6) {
        ctrl->time_scale = fmaxf(0.1f, ctrl->time_scale - 0.1f);
        return;
    }
    if (key == SDLK_7) {
        ctrl->time_scale = fminf(4.0f, ctrl->time_scale + 0.1f);
        return;
    }
    if (key == SDLK_a) {
        ctrl->damping_alpha = fmaxf(0.0f, ctrl->damping_alpha - 0.02f);
        return;
    }
    if (key == SDLK_f) {
        ctrl->damping_alpha = fminf(2.0f, ctrl->damping_alpha + 0.02f);
        return;
    }
    if (key == SDLK_h) {
        ctrl->damping_beta = fmaxf(0.0f, ctrl->damping_beta - 0.02f);
        return;
    }
    if (key == SDLK_j) {
        ctrl->damping_beta = fminf(2.0f, ctrl->damping_beta + 0.02f);
        return;
    }
    if (key == SDLK_u) {
        ctrl->gravity_ramp_enabled = !ctrl->gravity_ramp_enabled;
        ctrl->gravity_ramp_time = 0.0f;
        return;
    }
    if (key == SDLK_0 && ctrl->dynamic_mode) {
        if (ctrl->gravity_ramp_duration < 0.5f) {
            ctrl->gravity_ramp_duration = 0.5f;
        } else if (ctrl->gravity_ramp_duration < 1.0f) {
            ctrl->gravity_ramp_duration = 1.0f;
        } else if (ctrl->gravity_ramp_duration < 2.0f) {
            ctrl->gravity_ramp_duration = 2.0f;
        } else if (ctrl->gravity_ramp_duration < 4.0f) {
            ctrl->gravity_ramp_duration = 4.0f;
        } else {
            ctrl->gravity_ramp_duration = 0.5f;
        }
        return;
    }
    if (key == SDLK_r) {
        runtime_reset_sim(ctrl);
        return;
    }
    if (key == SDLK_i) {
        ctrl->show_ids = !ctrl->show_ids;
        return;
    }
    if (key == SDLK_c) {
        ctrl->show_constraints = !ctrl->show_constraints;
        return;
    }
    if (key == SDLK_l) {
        ctrl->show_loads = !ctrl->show_loads;
        return;
    }
    if (key == SDLK_o) {
        ctrl->show_deformed = !ctrl->show_deformed;
        return;
    }
    if (key == SDLK_MINUS) {
        ctrl->deform_scale = fmaxf(0.0f, ctrl->deform_scale - 1.0f);
        return;
    }
    if (key == SDLK_EQUALS) {
        ctrl->deform_scale += 1.0f;
        return;
    }
    if (key == SDLK_q) {
        ctrl->show_combined = !ctrl->show_combined;
        return;
    }
    if (key == SDLK_y) {
        ctrl->scale_use_percentile = !ctrl->scale_use_percentile;
        ctrl->scale_initialized = false;
        return;
    }
    if (key == SDLK_k) {
        ctrl->scale_freeze = !ctrl->scale_freeze;
        return;
    }
    if (key == SDLK_g) {
        if (ctrl->scale_gamma > 0.9f) ctrl->scale_gamma = 0.7f;
        else if (ctrl->scale_gamma > 0.6f) ctrl->scale_gamma = 0.5f;
        else if (ctrl->scale_gamma > 0.4f) ctrl->scale_gamma = 0.35f;
        else ctrl->scale_gamma = 1.0f;
        ctrl->scale_initialized = false;
        return;
    }
    if (key == SDLK_x) {
        ctrl->scale_thickness = !ctrl->scale_thickness;
        return;
    }
    if (key == SDLK_t || key == SDLK_b || key == SDLK_v) {
        runtime_set_overlay(ctrl, key);
        return;
    }
    (void)mod;
}

int structural_controller_run(AppConfig *cfg,
                              const ShapeAssetLibrary *shape_library,
                              const char *preset_path) {
    (void)shape_library;
    if (!cfg) return 1;

    bool sdl_initialized = false;
    bool ttf_initialized = false;
    if (SDL_WasInit(SDL_INIT_VIDEO) == 0) {
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
            fprintf(stderr, "[struct] SDL_Init failed: %s\n", SDL_GetError());
            return 1;
        }
        sdl_initialized = true;
    }
    if (TTF_WasInit() == 0) {
        if (TTF_Init() != 0) {
            fprintf(stderr, "[struct] SDL_ttf init failed: %s\n", TTF_GetError());
            if (sdl_initialized) {
                SDL_Quit();
            }
            return 1;
        }
        ttf_initialized = true;
    }

    SDL_Window *window = SDL_CreateWindow(
        "Physics Sim - Structural Mode",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        cfg->window_w, cfg->window_h,
        SDL_WINDOW_SHOWN | SDL_WINDOW_VULKAN);
    if (!window) {
        fprintf(stderr, "[struct] Failed to create window: %s\n", SDL_GetError());
        if (ttf_initialized) {
            TTF_Quit();
        }
        if (sdl_initialized) {
            SDL_Quit();
        }
        return 1;
    }

    VkRenderer renderer_storage;
    SDL_Renderer *renderer = (SDL_Renderer *)&renderer_storage;
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
    const bool use_shared_device = true;
#else
    const bool use_shared_device = true;
#endif

    if (use_shared_device) {
        if (!vk_shared_device_init(window, &vk_cfg)) {
            fprintf(stderr, "[struct] Failed to init shared Vulkan device.\n");
            SDL_DestroyWindow(window);
            if (ttf_initialized) {
                TTF_Quit();
            }
            if (sdl_initialized) {
                SDL_Quit();
            }
            return 1;
        }

        VkRendererDevice* shared_device = vk_shared_device_get();
        if (!shared_device) {
            fprintf(stderr, "[struct] Failed to access shared Vulkan device.\n");
            SDL_DestroyWindow(window);
            if (ttf_initialized) {
                TTF_Quit();
            }
            if (sdl_initialized) {
                SDL_Quit();
            }
            return 1;
        }

        if (vk_renderer_init_with_device((VkRenderer *)renderer, shared_device, window, &vk_cfg) != VK_SUCCESS) {
            fprintf(stderr, "[struct] Failed to init Vulkan renderer.\n");
            SDL_DestroyWindow(window);
            if (ttf_initialized) {
                TTF_Quit();
            }
            if (sdl_initialized) {
                SDL_Quit();
            }
            return 1;
        }
        vk_shared_device_acquire();
    } else {
        if (vk_renderer_init((VkRenderer *)renderer, window, &vk_cfg) != VK_SUCCESS) {
            fprintf(stderr, "[struct] Failed to init Vulkan renderer.\n");
            SDL_DestroyWindow(window);
            if (ttf_initialized) {
                TTF_Quit();
            }
            if (sdl_initialized) {
                SDL_Quit();
            }
            return 1;
        }
    }

    StructuralController ctrl = {0};
    structural_scene_init(&ctrl.scene);
    structural_controller_runtime_view_resize(&ctrl.runtime, ctrl.scene.node_count);
    ctrl.show_constraints = true;
    ctrl.show_loads = true;
    ctrl.show_ids = false;
    ctrl.show_deformed = true;
    ctrl.show_stress = true;
    ctrl.show_bending = false;
    ctrl.show_shear = false;
    ctrl.show_combined = false;
    ctrl.scale_use_percentile = true;
    ctrl.scale_freeze = false;
    ctrl.scale_initialized = false;
    ctrl.scale_thickness = true;
    ctrl.scale_gamma = 0.6f;
    ctrl.scale_percentile = 0.95f;
    ctrl.scale_stress = 0.0f;
    ctrl.scale_moment = 0.0f;
    ctrl.scale_shear = 0.0f;
    ctrl.scale_combined = 0.0f;
    ctrl.thickness_gain = 0.6f;
    ctrl.deform_scale = 10.0f;
    ctrl.time_scale = 1.0f;
    ctrl.damping_alpha = 0.1f;
    ctrl.damping_beta = 0.1f;
    ctrl.newmark_beta = 0.25f;
    ctrl.newmark_gamma = 0.5f;
    ctrl.gravity_ramp_duration = 1.0f;
    ctrl.font_small = load_font(cfg, 12, renderer);
    ctrl.font_hud = load_font(cfg, 14, renderer);
    ctrl.running = true;
    SDL_GetWindowSize(window, &ctrl.window_w, &ctrl.window_h);
    vk_renderer_set_logical_size((VkRenderer *)renderer,
                                 (float)ctrl.window_w,
                                 (float)ctrl.window_h);
    if (preset_path && preset_path[0] != '\0') {
        snprintf(ctrl.preset_path, sizeof(ctrl.preset_path), "%s", preset_path);
        if (!structural_scene_load(&ctrl.scene, ctrl.preset_path)) {
            snprintf(ctrl.last_result.warning, sizeof(ctrl.last_result.warning),
                     "Preset load failed.");
        }
        structural_controller_runtime_view_resize(&ctrl.runtime, ctrl.scene.node_count);
    }

    InputContextManager ctx_mgr;
    input_context_manager_init(&ctx_mgr);
    InputContext ctx = {
        .on_pointer_down = on_pointer_down,
        .on_pointer_up = on_pointer_up,
        .on_pointer_move = on_pointer_move,
        .on_key_down = on_key_down,
        .user_data = &ctrl
    };
    input_context_manager_push(&ctx_mgr, &ctx);

    Uint32 last_ticks = SDL_GetTicks();
    while (ctrl.running) {
        InputCommands cmds;
        bool running = input_poll_events(&cmds, NULL, &ctx_mgr);
        if (!running || cmds.quit) {
            ctrl.running = false;
        }
        (void)structural_apply_text_zoom_shortcut(&cmds, cfg, &ctrl, renderer);

        Uint32 now = SDL_GetTicks();
        float dt = (float)(now - last_ticks) / 1000.0f;
        last_ticks = now;
        if (dt > 0.05f) dt = 0.05f;
        dt *= ctrl.time_scale;

        if (ctrl.solve_requested) {
            ctrl.solve_requested = false;
            StructuralSolveResult result = {0};
            bool ok = structural_solve_frame(&ctrl.scene, &result);
            ctrl.last_result = result;
            if (ok) {
                snprintf(ctrl.last_result.warning, sizeof(ctrl.last_result.warning),
                         "Solve ok (%d iters, r=%.3f).", result.iterations, result.residual);
                structural_controller_runtime_view_sync_from_scene(&ctrl.runtime, &ctrl.scene);
            }
        }

        if (ctrl.dynamic_mode && (ctrl.dynamic_playing || ctrl.dynamic_step)) {
            structural_controller_runtime_step_dynamic(&ctrl, dt);
            ctrl.sim_time += dt;
            ctrl.dynamic_step = false;
        }

        SDL_GetMouseState(&ctrl.pointer_x, &ctrl.pointer_y);
        SDL_GetWindowSize(window, &ctrl.window_w, &ctrl.window_h);
        if (ctrl.window_w > 0 && ctrl.window_h > 0) {
            int drawable_w = ctrl.window_w;
            int drawable_h = ctrl.window_h;
            SDL_Vulkan_GetDrawableSize(window, &drawable_w, &drawable_h);
            if (drawable_w <= 0 || drawable_h <= 0) {
                SDL_Delay(16);
                continue;
            }
            VkExtent2D swap_extent = ((VkRenderer *)renderer)->context.swapchain.extent;
            if ((uint32_t)drawable_w != swap_extent.width ||
                (uint32_t)drawable_h != swap_extent.height) {
                vk_renderer_recreate_swapchain((VkRenderer *)renderer, window);
                vk_renderer_set_logical_size((VkRenderer *)renderer,
                                             (float)ctrl.window_w,
                                             (float)ctrl.window_h);
                SDL_Delay(8);
                continue;
            }
            VkCommandBuffer cmd = VK_NULL_HANDLE;
            VkFramebuffer fb = VK_NULL_HANDLE;
            VkExtent2D extent = {0};
            VkResult frame = vk_renderer_begin_frame((VkRenderer *)renderer, &cmd, &fb, &extent);
            if (frame == VK_ERROR_OUT_OF_DATE_KHR || frame == VK_SUBOPTIMAL_KHR) {
                vk_renderer_recreate_swapchain((VkRenderer *)renderer, window);
                vk_renderer_set_logical_size((VkRenderer *)renderer,
                                             (float)ctrl.window_w,
                                             (float)ctrl.window_h);
            } else if (frame == VK_ERROR_DEVICE_LOST) {
                static int logged_device_lost = 0;
                if (!logged_device_lost) {
                    fprintf(stderr, "[struct] Vulkan device lost; exiting structural mode.\n");
                    logged_device_lost = 1;
                }
                if (use_shared_device) {
                    vk_shared_device_mark_lost();
                }
                ctrl.running = false;
                break;
            } else if (frame == VK_SUCCESS) {
                vk_renderer_set_logical_size((VkRenderer *)renderer,
                                             (float)ctrl.window_w,
                                             (float)ctrl.window_h);
                structural_controller_render_scene(renderer, &ctrl);
                VkResult end = vk_renderer_end_frame((VkRenderer *)renderer, cmd);
                if (end == VK_ERROR_OUT_OF_DATE_KHR || end == VK_SUBOPTIMAL_KHR) {
                    vk_renderer_recreate_swapchain((VkRenderer *)renderer, window);
                    vk_renderer_set_logical_size((VkRenderer *)renderer,
                                                 (float)ctrl.window_w,
                                                 (float)ctrl.window_h);
                } else if (end == VK_ERROR_DEVICE_LOST) {
                    static int logged_device_lost_end = 0;
                    if (!logged_device_lost_end) {
                        fprintf(stderr, "[struct] Vulkan device lost at end; exiting structural mode.\n");
                        logged_device_lost_end = 1;
                    }
                    if (use_shared_device) {
                        vk_shared_device_mark_lost();
                    }
                    ctrl.running = false;
                    break;
                } else if (end != VK_SUCCESS) {
                    fprintf(stderr, "[struct] vk_renderer_end_frame failed: %d\n", end);
                }
            } else {
                fprintf(stderr, "[struct] vk_renderer_begin_frame failed: %d\n", frame);
            }
        }
        SDL_Delay(8);
    }

    structural_controller_runtime_view_clear(&ctrl.runtime);
    if (ctrl.font_small) TTF_CloseFont(ctrl.font_small);
    if (ctrl.font_hud) TTF_CloseFont(ctrl.font_hud);
    vk_renderer_wait_idle((VkRenderer *)renderer);
    if (use_shared_device) {
        vk_renderer_shutdown_surface((VkRenderer *)renderer);
        vk_shared_device_release();
    } else {
        vk_renderer_shutdown((VkRenderer *)renderer);
    }
    SDL_DestroyWindow(window);
    return 0;
}
