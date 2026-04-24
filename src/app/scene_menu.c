#include "app/scene_menu.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "app/menu/menu_input.h"
#include "app/menu/menu_render.h"
#include "app/menu/menu_state.h"
#include "app/menu/menu_types.h"
#include "app/menu/menu_window.h"
#include "app/data_paths.h"
#include "app/scene_loop_diag.h"
#include "app/scene_loop_policy.h"
#include "app/scene_menu_layout_helpers.h"
#include "config/config_loader.h"
#include "input/input.h"
#include "render/text_upload_policy.h"
#include "render/vk_shared_device.h"
#include "vk_renderer.h"

enum {
    MENU_IDLE_RENDER_HEARTBEAT_MS = 250u
};

static void menu_persist_runtime_config(const AppConfig *cfg) {
    const char *runtime_config_path = physics_sim_runtime_config_path();
    if (!cfg) return;
    if (!config_loader_save(cfg, runtime_config_path)) {
        fprintf(stderr, "[menu] Failed to persist runtime config to %s\n",
                runtime_config_path);
    }
}

static bool menu_text_entry_active(const SceneMenuInteraction *ctx) {
    if (!ctx) return false;
    if (ctx->rename_input.active) return true;
    if (ctx->editing_headless_frames) return true;
    if (ctx->editing_inflow) return true;
    if (ctx->editing_viscosity) return true;
    if (ctx->editing_input_root) return true;
    if (ctx->editing_output_root) return true;
    return false;
}

static bool menu_apply_text_zoom_shortcut(SceneMenuInteraction *ctx,
                                          const InputCommands *cmds) {
    int next_step = 0;
    bool changed = false;
    if (!ctx || !ctx->cfg || !cmds) return false;
    if (menu_text_entry_active(ctx)) return false;
    if (!(cmds->text_zoom_in_requested ||
          cmds->text_zoom_out_requested ||
          cmds->text_zoom_reset_requested)) {
        return false;
    }

    next_step = ctx->cfg->text_zoom_step;
    if (cmds->text_zoom_reset_requested) {
        next_step = 0;
    } else {
        if (cmds->text_zoom_in_requested) {
            next_step += 1;
        }
        if (cmds->text_zoom_out_requested) {
            next_step -= 1;
        }
    }
    next_step = app_config_text_zoom_step_clamp(next_step);
    changed = (next_step != ctx->cfg->text_zoom_step);
    if (!changed) return false;

    ctx->cfg->text_zoom_step = next_step;
    if (!menu_reload_fonts(ctx)) {
        fprintf(stderr, "[menu] Failed to reload fonts after zoom update.\n");
    }
    menu_update_scrollbar(ctx);

    menu_persist_runtime_config(ctx->cfg);
    return true;
}

static void scene_menu_record_loop_diag(uint64_t frame_begin_counter,
                                        uint64_t perf_freq,
                                        uint32_t wait_blocked_ms,
                                        uint32_t wait_call_count) {
    uint64_t frame_end_counter = SDL_GetPerformanceCounter();
    double frame_elapsed_sec = 0.0;
    if (perf_freq > 0 && frame_end_counter >= frame_begin_counter) {
        frame_elapsed_sec = (double)(frame_end_counter - frame_begin_counter) /
                            (double)perf_freq;
    }
    scene_loop_diag_tick(frame_elapsed_sec, wait_blocked_ms, wait_call_count);
}

bool scene_menu_run(AppConfig *cfg,
                    FluidScenePreset *preset_state,
                    SceneMenuSelection *selection,
                    CustomPresetLibrary *library,
                    const ShapeAssetLibrary *shape_library) {
    if (!cfg || !preset_state || !selection || !library) return false;

    int restart_attempts = 0;
    bool sdl_initialized = false;
restart_menu:
    sdl_initialized = false;
    if (SDL_WasInit(SDL_INIT_VIDEO) == 0) {
        if (SDL_Init(SDL_INIT_VIDEO) != 0) {
            fprintf(stderr, "SDL menu init failed: %s\n", SDL_GetError());
            return false;
        }
        sdl_initialized = true;
    }
    if (TTF_WasInit() == 0) {
        if (TTF_Init() != 0) {
            fprintf(stderr, "SDL_ttf init failed: %s\n", TTF_GetError());
            if (sdl_initialized) {
                SDL_Quit();
            }
            return false;
        }
    }

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
    size_t preset_count = 0;
    const FluidScenePreset *presets = scene_presets_get_all(&preset_count);
    menu_clamp_grid_size(cfg);

    SceneMenuSelection current_selection = *selection;
    int slot_count = preset_library_count(library);
    if (slot_count <= 0) {
        current_selection.custom_slot_index = -1;
        library->active_slot = -1;
    } else {
        if (current_selection.custom_slot_index < 0 ||
            current_selection.custom_slot_index >= slot_count) {
            int active = library->active_slot;
            if (active < 0 || active >= slot_count) active = 0;
            current_selection.custom_slot_index = active;
        }
    }
    for (int mode = 0; mode < SIMULATION_MODE_COUNT; ++mode) {
        int stored = current_selection.last_mode_slot[mode];
        if (stored < 0 || stored >= slot_count) {
            current_selection.last_mode_slot[mode] = -1;
        }
    }
    if (current_selection.retained_scene_index < 0) {
        current_selection.retained_scene_index = 0;
    }
    SimulationMode selection_mode = menu_normalize_sim_mode(cfg->sim_mode);
    SpaceMode selection_space_mode = menu_normalize_space_mode(cfg->space_mode);
    current_selection.sim_mode = selection_mode;
    cfg->space_mode = selection_space_mode;
    if (selection_mode >= 0 && selection_mode < SIMULATION_MODE_COUNT &&
        current_selection.last_mode_slot[selection_mode] < 0) {
        current_selection.last_mode_slot[selection_mode] = current_selection.custom_slot_index;
    }

    if (current_selection.headless_frame_count > 0) {
        cfg->headless_frame_count = current_selection.headless_frame_count;
    } else {
        current_selection.headless_frame_count = cfg->headless_frame_count;
    }
    if (current_selection.tunnel_inflow_speed > 0.0f) {
        cfg->tunnel_inflow_speed = current_selection.tunnel_inflow_speed;
    } else {
        current_selection.tunnel_inflow_speed = cfg->tunnel_inflow_speed;
    }

    bool run = true;
    bool start_requested = false;

    SceneMenuInteraction ctx = {
        .cfg = cfg,
        .presets = presets,
        .preset_count = preset_count,
        .selection = &current_selection,
        .library = library,
        .shape_library = shape_library,
        .preset_output = preset_state,
        .preview_preset = *preset_state,
        .active_preset = preset_state,
        .scene_library = {0},
        .editor_bootstrap = {0},
        .window = NULL,
        .renderer = NULL,
        .font = NULL,
        .font_small = NULL,
        .font_title = NULL,
        .vk_cfg = vk_cfg,
        .use_shared_device = use_shared_device,
        .start_button = {.rect = {MENU_WIDTH - 220, MENU_HEIGHT - 80, 180, 50}, .label = "Start"},
        .duplicate_button = {.rect = {MENU_WIDTH - 620, MENU_HEIGHT - 80, 180, 50}, .label = "Duplicate"},
        .edit_button = {.rect = {MENU_WIDTH - 420, MENU_HEIGHT - 80, 180, 50}, .label = "Edit Preset"},
        .quit_button = {.rect = {20, MENU_HEIGHT - 70, 120, 40}, .label = "Quit"},
        .grid_dec_button = {.rect = {MENU_WIDTH - 260, 180, 40, 40}, .label = "-"},
        .grid_inc_button = {.rect = {MENU_WIDTH - 100, 180, 40, 40}, .label = "+"},
        .quality_prev_button = {.rect = {0, 0, 0, 0}, .label = "<"},
        .quality_next_button = {.rect = {0, 0, 0, 0}, .label = ">"},
        .headless_toggle_button = {.rect = {MENU_WIDTH - 220, MENU_HEIGHT - 130, 180, 40}, .label = "Headless"},
        .mode_toggle_button = {.rect = {MENU_WIDTH - 220, 60, 180, 36}, .label = "Mode"},
        .space_toggle_button = {.rect = {MENU_WIDTH - 220, 104, 180, 36}, .label = "Space"},
        .input_root_edit_button = {.rect = {0, 0, 0, 0}, .label = "Edit"},
        .input_root_folder_button = {.rect = {0, 0, 0, 0}, .label = "Folder"},
        .output_root_edit_button = {.rect = {0, 0, 0, 0}, .label = "Edit"},
        .output_root_folder_button = {.rect = {0, 0, 0, 0}, .label = "Folder"},
        .running = &run,
        .start_requested = &start_requested,
        .context_mgr = NULL,
        .renaming_slot = -1,
        .last_click_ticks = 0,
        .last_clicked_slot = -1,
        .scrollbar_dragging = false,
        .hover_slot = -1,
        .hover_retained_scene_index = -1,
        .hover_add_entry = false,
        .headless_pending = false,
        .headless_running = false,
        .status_visible = false,
        .status_wait_ack = false,
        .headless_run_requested = false,
        .editing_headless_frames = false,
        .editing_inflow = false,
        .editing_viscosity = false,
        .editing_input_root = false,
        .editing_output_root = false,
        .last_headless_click_ticks = 0,
        .last_inflow_click_ticks = 0,
        .last_viscosity_click_ticks = 0,
        .headless_frames_rect = {0, 0, 0, 0},
        .inflow_rect = {0, 0, 0, 0},
        .viscosity_rect = {0, 0, 0, 0},
        .input_root_rect = {0, 0, 0, 0},
        .output_root_rect = {0, 0, 0, 0},
        .active_mode = selection_mode
    };
    if (!menu_create_window(&ctx)) {
        TTF_Quit();
        SDL_Quit();
        return false;
    }

    ctx.list_rect = menu_preset_list_rect();
    scrollbar_init(&ctx.scrollbar);
    menu_refresh_scene_library(&ctx);
    menu_update_scrollbar(&ctx);

    InputContextManager context_mgr;
    input_context_manager_init(&context_mgr);
    ctx.context_mgr = &context_mgr;

    ctx.quality_index = current_selection.quality_index;
    if (ctx.quality_index >= 0) {
        menu_apply_quality_profile_index(&ctx, ctx.quality_index);
    } else {
        ctx.quality_index = -1;
        if (ctx.cfg) ctx.cfg->quality_index = -1;
    }

    menu_ensure_slot_for_mode(&ctx);
    if (menu_showing_retained_catalog(&ctx)) {
        (void)menu_select_retained_scene(&ctx, current_selection.retained_scene_index);
    } else {
        menu_select_custom(&ctx, current_selection.custom_slot_index);
    }
    *selection = current_selection;

    InputContext menu_ctx = {
        .on_pointer_down = menu_pointer_down,
        .on_pointer_up = menu_pointer_up,
        .on_pointer_move = menu_pointer_move,
        .on_wheel = menu_wheel,
        .on_key_down = menu_key_down,
        .on_text_input = menu_text_input,
        .user_data = &ctx
    };
    input_context_manager_push(&context_mgr, &menu_ctx);

    Uint32 prev_ticks = SDL_GetTicks();
    uint64_t perf_freq = SDL_GetPerformanceFrequency();
    bool resize_pending = false;
    bool frame_dirty = true;
    Uint64 last_present_ticks = SDL_GetTicks64();
    bool device_lost = false;
    while (run) {
        uint64_t frame_begin_counter = SDL_GetPerformanceCounter();
        SceneLoopWaitPolicyInput wait_policy = {
            .headless_mode = false,
            .simulation_active = false,
            .interaction_active = menu_text_entry_active(&ctx) || ctx.scrollbar_dragging,
            .background_busy = ctx.headless_pending || ctx.headless_running || ctx.headless_run_requested,
            .resize_pending = resize_pending
        };
        uint32_t wait_blocked_ms = 0u;
        uint32_t wait_call_count = 0u;
        uint32_t event_count = 0u;
        int wait_timeout_ms = scene_loop_compute_wait_timeout_ms(&wait_policy);

        Uint32 now_ticks = SDL_GetTicks();
        double dt = (double)(now_ticks - prev_ticks) / 1000.0;
        prev_ticks = now_ticks;
        text_input_update(&ctx.rename_input, dt);
        if (ctx.editing_headless_frames) {
            text_input_update(&ctx.headless_frames_input, dt);
        }
        if (ctx.editing_inflow) {
            text_input_update(&ctx.inflow_input, dt);
        }
        if (ctx.editing_viscosity) {
            text_input_update(&ctx.viscosity_input, dt);
        }
        if (ctx.editing_input_root) {
            text_input_update(&ctx.input_root_input, dt);
        }
        if (ctx.editing_output_root) {
            text_input_update(&ctx.output_root_input, dt);
        }

        int win_w = 0;
        int win_h = 0;
        SDL_GetWindowSize(ctx.window, &win_w, &win_h);
        if (win_w <= 0 || win_h <= 0) {
            win_w = MENU_WIDTH;
            win_h = MENU_HEIGHT;
        }
        scene_menu_update_dynamic_layout(&ctx, win_w, win_h);
        menu_update_scrollbar(&ctx);

        InputCommands cmds;
        input_poll_events_with_wait(&cmds,
                                    NULL,
                                    &context_mgr,
                                    wait_timeout_ms,
                                    &wait_blocked_ms,
                                    &wait_call_count,
                                    &event_count);
        if (event_count > 0u) {
            frame_dirty = true;
        }
        if (cmds.quit) {
            run = false;
            scene_menu_record_loop_diag(frame_begin_counter,
                                        perf_freq,
                                        wait_blocked_ms,
                                        wait_call_count);
            break;
        }
        if (menu_apply_text_zoom_shortcut(&ctx, &cmds)) {
            frame_dirty = true;
        }
        menu_clamp_grid_size(cfg);
        scene_menu_update_dynamic_layout(&ctx, win_w, win_h);
        menu_update_scrollbar(&ctx);

        if (ctx.headless_pending && !ctx.headless_running) {
            ctx.headless_pending = false;
            ctx.headless_run_requested = true;
            ctx.headless_running = true;
            char msg[128];
            if (ctx.cfg->headless_frame_count <= 0) {
                snprintf(msg, sizeof(msg), "Headless running...");
            } else {
                snprintf(msg, sizeof(msg), "Headless running %d frames...",
                         ctx.cfg->headless_frame_count);
            }
            menu_set_status(&ctx, msg, false);
            frame_dirty = true;
        }

        {
            Uint64 now_present_ticks = SDL_GetTicks64();
            bool heartbeat_due =
                (now_present_ticks - last_present_ticks) >= MENU_IDLE_RENDER_HEARTBEAT_MS;
            bool force_render = resize_pending || ctx.headless_run_requested;
            bool should_render = frame_dirty || heartbeat_due || force_render;
            if (!should_render) {
                scene_menu_record_loop_diag(frame_begin_counter,
                                            perf_freq,
                                            wait_blocked_ms,
                                            wait_call_count);
                continue;
            }
        }

        int drawable_w = win_w;
        int drawable_h = win_h;
        SDL_Vulkan_GetDrawableSize(ctx.window, &drawable_w, &drawable_h);
        if (drawable_w <= 0 || drawable_h <= 0) {
            resize_pending = true;
            scene_menu_record_loop_diag(frame_begin_counter,
                                        perf_freq,
                                        wait_blocked_ms,
                                        wait_call_count);
            continue;
        }
        VkExtent2D swap_extent = ((VkRenderer *)ctx.renderer)->context.swapchain.extent;
        if ((uint32_t)drawable_w != swap_extent.width ||
            (uint32_t)drawable_h != swap_extent.height) {
            vk_renderer_recreate_swapchain((VkRenderer *)ctx.renderer, ctx.window);
            vk_renderer_set_logical_size((VkRenderer *)ctx.renderer, (float)win_w, (float)win_h);
            resize_pending = true;
            scene_menu_record_loop_diag(frame_begin_counter,
                                        perf_freq,
                                        wait_blocked_ms,
                                        wait_call_count);
            continue;
        }
        VkCommandBuffer cmd = VK_NULL_HANDLE;
        VkFramebuffer fb = VK_NULL_HANDLE;
        VkExtent2D extent = {0};
        VkResult frame = vk_renderer_begin_frame((VkRenderer *)ctx.renderer, &cmd, &fb, &extent);
        if (frame == VK_ERROR_OUT_OF_DATE_KHR || frame == VK_SUBOPTIMAL_KHR) {
            vk_renderer_recreate_swapchain((VkRenderer *)ctx.renderer, ctx.window);
            vk_renderer_set_logical_size((VkRenderer *)ctx.renderer, (float)win_w, (float)win_h);
            resize_pending = true;
            scene_menu_record_loop_diag(frame_begin_counter,
                                        perf_freq,
                                        wait_blocked_ms,
                                        wait_call_count);
            continue;
        } else if (frame == VK_ERROR_DEVICE_LOST) {
            static int logged_device_lost = 0;
            if (!logged_device_lost) {
                fprintf(stderr, "[menu] Vulkan device lost; exiting menu loop.\n");
                logged_device_lost = 1;
            }
            if (ctx.use_shared_device) {
                vk_shared_device_mark_lost();
            }
            device_lost = true;
            run = false;
            scene_menu_record_loop_diag(frame_begin_counter,
                                        perf_freq,
                                        wait_blocked_ms,
                                        wait_call_count);
            break;
        } else if (frame != VK_SUCCESS) {
            fprintf(stderr, "[menu] vk_renderer_begin_frame failed: %d\n", frame);
            resize_pending = true;
            scene_menu_record_loop_diag(frame_begin_counter,
                                        perf_freq,
                                        wait_blocked_ms,
                                        wait_call_count);
            continue;
        }
        resize_pending = false;
        vk_renderer_set_logical_size((VkRenderer *)ctx.renderer, (float)win_w, (float)win_h);

        SDL_Color bg_color = menu_color_bg();
        SDL_SetRenderDrawColor(ctx.renderer, bg_color.r, bg_color.g, bg_color.b, bg_color.a);
        int clear_w = win_w;
        int clear_h = win_h;
        SDL_Rect clear_rect = {0, 0, clear_w, clear_h};
        SDL_RenderFillRect(ctx.renderer, &clear_rect);

        {
            int body_h = scene_menu_font_height(ctx.renderer, ctx.font, 22);
            int small_h = scene_menu_font_height(ctx.renderer, ctx.font_small ? ctx.font_small : ctx.font, 18);
            int title_y = 40;
            if (ctx.font_title) {
                menu_draw_text(ctx.renderer,
                               ctx.font_title,
                               "Custom Presets",
                               ctx.list_rect.x,
                               title_y,
                               menu_color_text());
            }
            menu_draw_preset_list(&ctx);

            TTF_Font *toggle_font = ctx.font_small ? ctx.font_small : ctx.font;
            if (!toggle_font) toggle_font = ctx.font;
            char mode_text[48];
            snprintf(mode_text, sizeof(mode_text), "Mode: %s", menu_mode_label(ctx.active_mode));
            menu_draw_toggle(ctx.renderer,
                             toggle_font,
                             &ctx.mode_toggle_button.rect,
                             mode_text,
                             ctx.active_mode == SIM_MODE_WIND_TUNNEL);

            {
                char space_text[64];
                snprintf(space_text,
                         sizeof(space_text),
                         "Space: %s",
                         menu_space_mode_label(ctx.cfg ? ctx.cfg->space_mode : SPACE_MODE_2D));
                menu_draw_toggle(ctx.renderer,
                                 toggle_font,
                                 &ctx.space_toggle_button.rect,
                                 space_text,
                                 (ctx.cfg && menu_normalize_space_mode(ctx.cfg->space_mode) == SPACE_MODE_3D));
                if (ctx.cfg && menu_normalize_space_mode(ctx.cfg->space_mode) == SPACE_MODE_3D) {
                    const char *scaffold_hint =
                        "3D scaffold backend: XYZ domain + derived XY compatibility slice";
                    char scaffold_hint_fit[160];
                    int hint_x = ctx.config_panel_rect.x;
                    int hint_y = ctx.mode_toggle_button.rect.y + ctx.mode_toggle_button.rect.h + 6;
                    int hint_w = ctx.config_panel_rect.w;
                    scene_menu_fit_text_to_width(ctx.renderer,
                                           toggle_font,
                                           scaffold_hint,
                                           hint_w,
                                           scaffold_hint_fit,
                                           sizeof(scaffold_hint_fit));
                    menu_draw_text(ctx.renderer,
                                   toggle_font,
                                   scaffold_hint_fit,
                                   hint_x,
                                   hint_y,
                                   menu_color_text_dim());
                }
            }

            SDL_Color panel_color = menu_color_panel();
            SDL_Color accent_color = menu_color_accent();
            SDL_Rect config_panel = ctx.config_panel_rect;
            menu_draw_panel(ctx.renderer, &config_panel);
            SDL_SetRenderDrawColor(ctx.renderer, accent_color.r, accent_color.g, accent_color.b, 120);
            SDL_RenderDrawRect(ctx.renderer, &config_panel);
            menu_draw_text(ctx.renderer,
                           ctx.font_small ? ctx.font_small : ctx.font,
                           "Simulation Settings",
                           config_panel.x + 10,
                           config_panel.y + 8,
                           menu_color_text_dim());

            int grid_label_y = config_panel.y + small_h + 14;
            int grid_value_y = ctx.grid_dec_button.rect.y + (ctx.grid_dec_button.rect.h - body_h) / 2;
            menu_draw_text(ctx.renderer,
                           ctx.font,
                           "Grid Resolution",
                           config_panel.x + 12,
                           grid_label_y,
                           menu_color_text_dim());
            char buffer[64];
            snprintf(buffer, sizeof(buffer), "%dx%d cells", cfg->grid_w, cfg->grid_h);
            menu_draw_text(ctx.renderer,
                           ctx.font,
                           buffer,
                           config_panel.x + 12,
                           grid_value_y,
                           menu_color_text());
            menu_draw_button(ctx.renderer, &ctx.grid_dec_button.rect, ctx.grid_dec_button.label, ctx.font, false);
            menu_draw_button(ctx.renderer, &ctx.grid_inc_button.rect, ctx.grid_inc_button.label, ctx.font, false);

            menu_draw_text(ctx.renderer,
                           ctx.font,
                           "Quality",
                           config_panel.x + 12,
                           ctx.quality_prev_button.rect.y - small_h - 8,
                           menu_color_text_dim());
            menu_draw_button(ctx.renderer, &ctx.quality_prev_button.rect, ctx.quality_prev_button.label, ctx.font, false);
            menu_draw_button(ctx.renderer, &ctx.quality_next_button.rect, ctx.quality_next_button.label, ctx.font, false);
            const char *quality_name = menu_current_quality_name(&ctx);
            char quality_buf[128];
            int quality_x = ctx.quality_prev_button.rect.x + ctx.quality_prev_button.rect.w + 8;
            int quality_w = ctx.quality_next_button.rect.x - 8 - quality_x;
            if (quality_w < 10) quality_w = 10;
            scene_menu_fit_text_to_width(ctx.renderer, ctx.font, quality_name, quality_w, quality_buf, sizeof(quality_buf));
            menu_draw_text(ctx.renderer,
                           ctx.font,
                           quality_buf,
                           quality_x,
                           ctx.quality_prev_button.rect.y + (ctx.quality_prev_button.rect.h - body_h) / 2,
                           menu_color_text());

            menu_draw_toggle(ctx.renderer, toggle_font, &ctx.volume_toggle_rect,
                             "Save Volume Frames", ctx.cfg->save_volume_frames);
            menu_draw_toggle(ctx.renderer, toggle_font, &ctx.render_toggle_rect,
                             "Save Render Frames", ctx.cfg->save_render_frames);

            SDL_Rect io_panel = {
                .x = ctx.output_root_rect.x - 12,
                .y = ctx.output_root_rect.y - small_h - 16,
                .w = ctx.output_root_rect.w + 24,
                .h = (ctx.headless_toggle_button.rect.y + ctx.headless_toggle_button.rect.h) -
                     (ctx.output_root_rect.y - small_h - 16) + 12
            };
            if (io_panel.h < 80) io_panel.h = 80;
            menu_draw_panel(ctx.renderer, &io_panel);
            SDL_SetRenderDrawColor(ctx.renderer, accent_color.r, accent_color.g, accent_color.b, 120);
            SDL_RenderDrawRect(ctx.renderer, &io_panel);
            menu_draw_text(ctx.renderer,
                           ctx.font_small ? ctx.font_small : ctx.font,
                           "Data I/O + Batch",
                           io_panel.x + 10,
                           io_panel.y + 8,
                           menu_color_text_dim());

            SDL_SetRenderDrawColor(ctx.renderer, panel_color.r, panel_color.g, panel_color.b, 255);
            SDL_RenderFillRect(ctx.renderer, &ctx.headless_frames_rect);
            SDL_SetRenderDrawColor(ctx.renderer, accent_color.r, accent_color.g, accent_color.b, 180);
            SDL_RenderDrawRect(ctx.renderer, &ctx.headless_frames_rect);
            if (ctx.editing_headless_frames) {
                menu_draw_text_input(ctx.renderer, ctx.font, &ctx.headless_frames_rect, &ctx.headless_frames_input);
            } else {
                char frames_label[64];
                char frames_fit[64];
                snprintf(frames_label, sizeof(frames_label), "Frames: %d", ctx.cfg->headless_frame_count);
                scene_menu_fit_text_to_width(ctx.renderer,
                                       ctx.font,
                                       frames_label,
                                       ctx.headless_frames_rect.w - 16,
                                       frames_fit,
                                       sizeof(frames_fit));
                menu_draw_text(ctx.renderer,
                               ctx.font,
                               frames_fit,
                               ctx.headless_frames_rect.x + 8,
                               ctx.headless_frames_rect.y + (ctx.headless_frames_rect.h - body_h) / 2,
                               menu_color_text());
            }

            SDL_SetRenderDrawColor(ctx.renderer, panel_color.r, panel_color.g, panel_color.b, 255);
            SDL_RenderFillRect(ctx.renderer, &ctx.viscosity_rect);
            SDL_SetRenderDrawColor(ctx.renderer, accent_color.r, accent_color.g, accent_color.b, 140);
            SDL_RenderDrawRect(ctx.renderer, &ctx.viscosity_rect);
            if (ctx.editing_viscosity) {
                menu_draw_text_input(ctx.renderer, ctx.font_small, &ctx.viscosity_rect, &ctx.viscosity_input);
            } else {
                char viscosity_label[64];
                char viscosity_fit[64];
                snprintf(viscosity_label, sizeof(viscosity_label), "Viscosity: %.6g", ctx.cfg->velocity_damping);
                scene_menu_fit_text_to_width(ctx.renderer,
                                       ctx.font_small,
                                       viscosity_label,
                                       ctx.viscosity_rect.w - 16,
                                       viscosity_fit,
                                       sizeof(viscosity_fit));
                menu_draw_text(ctx.renderer,
                               ctx.font_small,
                               viscosity_fit,
                               ctx.viscosity_rect.x + 8,
                               ctx.viscosity_rect.y + (ctx.viscosity_rect.h - small_h) / 2,
                               menu_color_text());
            }

            SDL_SetRenderDrawColor(ctx.renderer, panel_color.r, panel_color.g, panel_color.b, 255);
            SDL_RenderFillRect(ctx.renderer, &ctx.inflow_rect);
            SDL_SetRenderDrawColor(ctx.renderer, accent_color.r, accent_color.g, accent_color.b, 120);
            SDL_RenderDrawRect(ctx.renderer, &ctx.inflow_rect);
            if (ctx.editing_inflow) {
                menu_draw_text_input(ctx.renderer, ctx.font_small, &ctx.inflow_rect, &ctx.inflow_input);
            } else {
                char inflow_label[64];
                char inflow_fit[64];
                snprintf(inflow_label, sizeof(inflow_label), "Inflow: %.3f", ctx.cfg->tunnel_inflow_speed);
                scene_menu_fit_text_to_width(ctx.renderer,
                                       ctx.font_small,
                                       inflow_label,
                                       ctx.inflow_rect.w - 16,
                                       inflow_fit,
                                       sizeof(inflow_fit));
                menu_draw_text(ctx.renderer,
                               ctx.font_small,
                               inflow_fit,
                               ctx.inflow_rect.x + 8,
                               ctx.inflow_rect.y + (ctx.inflow_rect.h - small_h) / 2,
                               menu_color_text());
            }

            SDL_SetRenderDrawColor(ctx.renderer, panel_color.r, panel_color.g, panel_color.b, 255);
            SDL_RenderFillRect(ctx.renderer, &ctx.input_root_rect);
            SDL_SetRenderDrawColor(ctx.renderer, accent_color.r, accent_color.g, accent_color.b, 160);
            SDL_RenderDrawRect(ctx.renderer, &ctx.input_root_rect);
            menu_draw_button(ctx.renderer,
                             &ctx.input_root_edit_button.rect,
                             ctx.input_root_edit_button.label,
                             ctx.font_small ? ctx.font_small : ctx.font,
                             false);
            menu_draw_button(ctx.renderer,
                             &ctx.input_root_folder_button.rect,
                             ctx.input_root_folder_button.label,
                             ctx.font_small ? ctx.font_small : ctx.font,
                             false);
            {
                SDL_Rect path_rect = ctx.input_root_rect;
                int button_gap = 8;
                path_rect.w = ctx.input_root_edit_button.rect.x - ctx.input_root_rect.x - button_gap;
                if (path_rect.w < 64) path_rect.w = 64;
                if (ctx.editing_input_root) {
                    menu_draw_text_input(ctx.renderer,
                                         ctx.font_small ? ctx.font_small : ctx.font,
                                         &path_rect,
                                         &ctx.input_root_input);
                } else {
                    char input_label[320];
                    char input_fit[320];
                    const char *input_root = (ctx.cfg && ctx.cfg->input_root[0])
                                                 ? ctx.cfg->input_root
                                                 : physics_sim_default_input_root();
                    snprintf(input_label, sizeof(input_label), "Input Root: %s", input_root);
                    scene_menu_fit_text_to_width(ctx.renderer,
                                           ctx.font_small ? ctx.font_small : ctx.font,
                                           input_label,
                                           path_rect.w - 16,
                                           input_fit,
                                           sizeof(input_fit));
                    menu_draw_text(ctx.renderer,
                                   ctx.font_small ? ctx.font_small : ctx.font,
                                   input_fit,
                                   path_rect.x + 8,
                                   path_rect.y + (path_rect.h - small_h) / 2,
                                   menu_color_text());
                }
            }

            SDL_SetRenderDrawColor(ctx.renderer, panel_color.r, panel_color.g, panel_color.b, 255);
            SDL_RenderFillRect(ctx.renderer, &ctx.output_root_rect);
            SDL_SetRenderDrawColor(ctx.renderer, accent_color.r, accent_color.g, accent_color.b, 160);
            SDL_RenderDrawRect(ctx.renderer, &ctx.output_root_rect);
            menu_draw_button(ctx.renderer,
                             &ctx.output_root_edit_button.rect,
                             ctx.output_root_edit_button.label,
                             ctx.font_small ? ctx.font_small : ctx.font,
                             false);
            menu_draw_button(ctx.renderer,
                             &ctx.output_root_folder_button.rect,
                             ctx.output_root_folder_button.label,
                             ctx.font_small ? ctx.font_small : ctx.font,
                             false);
            {
                SDL_Rect path_rect = ctx.output_root_rect;
                int button_gap = 8;
                path_rect.w = ctx.output_root_edit_button.rect.x - ctx.output_root_rect.x - button_gap;
                if (path_rect.w < 64) path_rect.w = 64;
                if (ctx.editing_output_root) {
                    menu_draw_text_input(ctx.renderer,
                                         ctx.font_small ? ctx.font_small : ctx.font,
                                         &path_rect,
                                         &ctx.output_root_input);
                } else {
                    char output_label[320];
                    char output_fit[320];
                    const char *output_root = (ctx.cfg && ctx.cfg->headless_output_dir[0])
                                                  ? ctx.cfg->headless_output_dir
                                                  : physics_sim_default_snapshot_dir();
                    snprintf(output_label, sizeof(output_label), "Output Root: %s", output_root);
                    scene_menu_fit_text_to_width(ctx.renderer,
                                           ctx.font_small ? ctx.font_small : ctx.font,
                                           output_label,
                                           path_rect.w - 16,
                                           output_fit,
                                           sizeof(output_fit));
                    menu_draw_text(ctx.renderer,
                                   ctx.font_small ? ctx.font_small : ctx.font,
                                   output_fit,
                                   path_rect.x + 8,
                                   path_rect.y + (path_rect.h - small_h) / 2,
                                   menu_color_text());
                }
            }

            menu_draw_button(ctx.renderer,
                             &ctx.headless_toggle_button.rect,
                             ctx.headless_toggle_button.label,
                             ctx.font,
                             cfg->headless_enabled);
        }
        menu_draw_button(ctx.renderer, &ctx.start_button.rect, ctx.start_button.label, ctx.font, false);
        if (menu_showing_retained_catalog(&ctx)) {
            menu_draw_button(ctx.renderer, &ctx.duplicate_button.rect, ctx.duplicate_button.label, ctx.font, false);
        }
        menu_draw_button(ctx.renderer, &ctx.edit_button.rect, ctx.edit_button.label, ctx.font, false);
        menu_draw_button(ctx.renderer, &ctx.quit_button.rect, ctx.quit_button.label, ctx.font, false);

        if (ctx.status_visible && ctx.font && ctx.status_text[0]) {
            int text_w = 0;
            int text_h = 0;
            int status_x = ctx.headless_frames_rect.x;
            int max_text_w = win_w - status_x - 20;
            if (max_text_w < 10) max_text_w = 10;
            char status_buf[128];
            scene_menu_fit_text_to_width(ctx.renderer,
                                         ctx.font,
                                         ctx.status_text,
                                         max_text_w,
                                         status_buf,
                                         sizeof(status_buf));
            TTF_SizeUTF8(ctx.font, status_buf, &text_w, &text_h);
            text_w = physics_sim_text_logical_pixels(ctx.renderer, text_w);
            text_h = physics_sim_text_logical_pixels(ctx.renderer, text_h);
            int max_x = win_w - text_w - 20;
            if (status_x > max_x) status_x = max_x;
            if (status_x < 20) status_x = 20;
            int status_y = ctx.headless_frames_rect.y - text_h - 10;
            if (status_y > ctx.inflow_rect.y - text_h - 10) {
                status_y = ctx.inflow_rect.y - text_h - 10;
            }
            if (status_y < 20) status_y = 20;
            menu_draw_text(ctx.renderer,
                      ctx.font,
                      status_buf,
                      status_x,
                      status_y,
                      ctx.status_wait_ack ? menu_color_accent() : menu_color_text_dim());
        }

        VkResult end = vk_renderer_end_frame((VkRenderer *)ctx.renderer, cmd);
        if (end == VK_ERROR_OUT_OF_DATE_KHR || end == VK_SUBOPTIMAL_KHR) {
            vk_renderer_recreate_swapchain((VkRenderer *)ctx.renderer, ctx.window);
            vk_renderer_set_logical_size((VkRenderer *)ctx.renderer, (float)win_w, (float)win_h);
            resize_pending = true;
            frame_dirty = true;
        } else if (end != VK_SUCCESS) {
            fprintf(stderr, "[menu] vk_renderer_end_frame failed: %d\n", end);
            frame_dirty = true;
        } else {
            resize_pending = false;
            frame_dirty = false;
            last_present_ticks = SDL_GetTicks64();
        }
#if defined(__APPLE__)
        if (end == VK_SUCCESS) {
            vk_renderer_wait_idle((VkRenderer *)ctx.renderer);
        }
#endif

        if (ctx.headless_run_requested) {
            ctx.headless_run_requested = false;
            menu_run_headless_batch(&ctx);
            ctx.headless_running = false;
            frame_dirty = true;
        }

        scene_menu_record_loop_diag(frame_begin_counter,
                                    perf_freq,
                                    wait_blocked_ms,
                                    wait_call_count);
    }

    input_context_manager_pop(&context_mgr);

    if (ctx.rename_input.active) {
        menu_finish_rename(&ctx, false);
    }
    if (ctx.editing_input_root) {
        menu_finish_input_root_edit(&ctx, false);
    }
    if (ctx.editing_output_root) {
        menu_finish_output_root_edit(&ctx, false);
    }

    menu_destroy_window(&ctx);
    if (device_lost && ctx.use_shared_device) {
        vk_shared_device_shutdown();
    }

    if (!start_requested && ctx.active_preset) {
        *preset_state = *ctx.active_preset;
    }
    current_selection.headless_frame_count = ctx.cfg ? ctx.cfg->headless_frame_count : current_selection.headless_frame_count;
    current_selection.tunnel_inflow_speed = ctx.cfg ? ctx.cfg->tunnel_inflow_speed : current_selection.tunnel_inflow_speed;
    *selection = current_selection;

    if (device_lost && restart_attempts == 0) {
        fprintf(stderr, "[menu] Restarting menu after device lost.\n");
        restart_attempts++;
        goto restart_menu;
    }
    return start_requested;
}
