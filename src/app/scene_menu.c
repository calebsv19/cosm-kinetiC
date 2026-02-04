#include "app/scene_menu.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdbool.h>
#include <stdio.h>

#include "app/menu/menu_input.h"
#include "app/menu/menu_render.h"
#include "app/menu/menu_state.h"
#include "app/menu/menu_types.h"
#include "app/menu/menu_window.h"
#include "input/input.h"
#include "render/vk_shared_device.h"
#include "vk_renderer.h"

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
    SimulationMode selection_mode = menu_normalize_sim_mode(cfg->sim_mode);
    current_selection.sim_mode = selection_mode;
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
        .window = NULL,
        .renderer = NULL,
        .font = NULL,
        .font_small = NULL,
        .font_title = NULL,
        .vk_cfg = vk_cfg,
        .use_shared_device = use_shared_device,
        .start_button = {.rect = {MENU_WIDTH - 220, MENU_HEIGHT - 80, 180, 50}, .label = "Start"},
        .edit_button = {.rect = {MENU_WIDTH - 420, MENU_HEIGHT - 80, 180, 50}, .label = "Edit Preset"},
        .quit_button = {.rect = {20, MENU_HEIGHT - 70, 120, 40}, .label = "Quit"},
        .grid_dec_button = {.rect = {MENU_WIDTH - 260, 180, 40, 40}, .label = "-"},
        .grid_inc_button = {.rect = {MENU_WIDTH - 100, 180, 40, 40}, .label = "+"},
        .quality_prev_button = {.rect = {0, 0, 0, 0}, .label = "<"},
        .quality_next_button = {.rect = {0, 0, 0, 0}, .label = ">"},
        .headless_toggle_button = {.rect = {MENU_WIDTH - 220, MENU_HEIGHT - 130, 180, 40}, .label = "Headless"},
        .mode_toggle_button = {.rect = {MENU_WIDTH - 220, 60, 180, 36}, .label = "Mode"},
        .running = &run,
        .start_requested = &start_requested,
        .context_mgr = NULL,
        .renaming_slot = -1,
        .last_click_ticks = 0,
        .last_clicked_slot = -1,
        .scrollbar_dragging = false,
        .hover_slot = -1,
        .hover_add_entry = false,
        .headless_pending = false,
        .headless_running = false,
        .status_visible = false,
        .status_wait_ack = false,
        .headless_run_requested = false,
        .editing_headless_frames = false,
        .editing_inflow = false,
        .editing_viscosity = false,
        .last_headless_click_ticks = 0,
        .last_inflow_click_ticks = 0,
        .last_viscosity_click_ticks = 0,
        .headless_frames_rect = {0, 0, 0, 0},
        .inflow_rect = {0, 0, 0, 0},
        .viscosity_rect = {0, 0, 0, 0},
        .active_mode = selection_mode
    };
    if (!menu_create_window(&ctx)) {
        TTF_Quit();
        SDL_Quit();
        return false;
    }

    ctx.list_rect = menu_preset_list_rect();
    scrollbar_init(&ctx.scrollbar);
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
    menu_select_custom(&ctx, current_selection.custom_slot_index);
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
    bool device_lost = false;
    while (run) {
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

        InputCommands cmds;
        input_poll_events(&cmds, NULL, &context_mgr);
        if (cmds.quit) {
            run = false;
            break;
        }
        menu_clamp_grid_size(cfg);
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
        }

        int win_w = 0;
        int win_h = 0;
        SDL_GetWindowSize(ctx.window, &win_w, &win_h);
        if (win_w <= 0 || win_h <= 0) {
            win_w = MENU_WIDTH;
            win_h = MENU_HEIGHT;
        }
        int drawable_w = win_w;
        int drawable_h = win_h;
        SDL_Vulkan_GetDrawableSize(ctx.window, &drawable_w, &drawable_h);
        if (drawable_w <= 0 || drawable_h <= 0) {
            SDL_Delay(16);
            continue;
        }
        VkExtent2D swap_extent = ((VkRenderer *)ctx.renderer)->context.swapchain.extent;
        if ((uint32_t)drawable_w != swap_extent.width ||
            (uint32_t)drawable_h != swap_extent.height) {
            vk_renderer_recreate_swapchain((VkRenderer *)ctx.renderer, ctx.window);
            vk_renderer_set_logical_size((VkRenderer *)ctx.renderer, (float)win_w, (float)win_h);
            SDL_Delay(8);
            continue;
        }
        VkCommandBuffer cmd = VK_NULL_HANDLE;
        VkFramebuffer fb = VK_NULL_HANDLE;
        VkExtent2D extent = {0};
        VkResult frame = vk_renderer_begin_frame((VkRenderer *)ctx.renderer, &cmd, &fb, &extent);
        if (frame == VK_ERROR_OUT_OF_DATE_KHR || frame == VK_SUBOPTIMAL_KHR) {
            vk_renderer_recreate_swapchain((VkRenderer *)ctx.renderer, ctx.window);
            vk_renderer_set_logical_size((VkRenderer *)ctx.renderer, (float)win_w, (float)win_h);
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
            break;
        } else if (frame != VK_SUCCESS) {
            fprintf(stderr, "[menu] vk_renderer_begin_frame failed: %d\n", frame);
            continue;
        }
        vk_renderer_set_logical_size((VkRenderer *)ctx.renderer, (float)win_w, (float)win_h);

        SDL_Color bg_color = menu_color_bg();
        SDL_SetRenderDrawColor(ctx.renderer, bg_color.r, bg_color.g, bg_color.b, bg_color.a);
        int clear_w = win_w;
        int clear_h = win_h;
        SDL_Rect clear_rect = {0, 0, clear_w, clear_h};
        SDL_RenderFillRect(ctx.renderer, &clear_rect);

        if (ctx.font_title) {
            menu_draw_text(ctx.renderer, ctx.font_title, "Custom Presets", PRESET_LIST_MARGIN_X, 40, menu_color_text());
        }
        menu_draw_preset_list(&ctx);

        TTF_Font *toggle_font = ctx.font_small ? ctx.font_small : ctx.font;
        if (!toggle_font) toggle_font = ctx.font;
        ctx.mode_toggle_button.rect.y = 70;
        char mode_text[48];
        snprintf(mode_text, sizeof(mode_text), "Mode: %s", menu_mode_label(ctx.active_mode));
        menu_draw_toggle(ctx.renderer,
                    toggle_font,
                    &ctx.mode_toggle_button.rect,
                    mode_text,
                    ctx.active_mode == SIM_MODE_WIND_TUNNEL);

        SDL_Rect config_panel = {420, 120, 360, 320};
        menu_draw_panel(ctx.renderer, &config_panel);
        menu_draw_text(ctx.renderer, ctx.font, "Grid Resolution", config_panel.x + 12, config_panel.y + 12, menu_color_text_dim());
        char buffer[64];
        snprintf(buffer, sizeof(buffer), "%dx%d cells", cfg->grid_w, cfg->grid_h);
        menu_draw_text(ctx.renderer, ctx.font, buffer, config_panel.x + 12, config_panel.y + 42, menu_color_text());
        ctx.grid_dec_button.rect = (SDL_Rect){config_panel.x + 240, config_panel.y + 40, 40, 40};
        ctx.grid_inc_button.rect = (SDL_Rect){config_panel.x + 290, config_panel.y + 40, 40, 40};
        menu_draw_button(ctx.renderer, &ctx.grid_dec_button.rect, ctx.grid_dec_button.label, ctx.font, false);
        menu_draw_button(ctx.renderer, &ctx.grid_inc_button.rect, ctx.grid_inc_button.label, ctx.font, false);

        menu_draw_text(ctx.renderer, ctx.font, "Quality", config_panel.x + 12, config_panel.y + 86, menu_color_text_dim());
        ctx.quality_prev_button.rect = (SDL_Rect){config_panel.x + 12, config_panel.y + 110, 36, 36};
        ctx.quality_next_button.rect = (SDL_Rect){config_panel.x + config_panel.w - 48, config_panel.y + 110, 36, 36};
        menu_draw_button(ctx.renderer, &ctx.quality_prev_button.rect, ctx.quality_prev_button.label, ctx.font, false);
        menu_draw_button(ctx.renderer, &ctx.quality_next_button.rect, ctx.quality_next_button.label, ctx.font, false);
        const char *quality_name = menu_current_quality_name(&ctx);
        menu_draw_text(ctx.renderer,
                  ctx.font,
                  quality_name,
                  ctx.quality_prev_button.rect.x + ctx.quality_prev_button.rect.w + 8,
                  ctx.quality_prev_button.rect.y + 8,
                  menu_color_text());

        ctx.volume_toggle_rect = (SDL_Rect){config_panel.x + 12,
                                            config_panel.y + 160,
                                            config_panel.w - 24,
                                            36};
        ctx.render_toggle_rect = (SDL_Rect){config_panel.x + 12,
                                            config_panel.y + 210,
                                            config_panel.w - 24,
                                            36};

        menu_draw_toggle(ctx.renderer, toggle_font, &ctx.volume_toggle_rect,
                    "Save Volume Frames", ctx.cfg->save_volume_frames);
        menu_draw_toggle(ctx.renderer, toggle_font, &ctx.render_toggle_rect,
                    "Save Render Frames", ctx.cfg->save_render_frames);

        SDL_Rect headless_rect = ctx.headless_toggle_button.rect;
        headless_rect.y = ctx.start_button.rect.y - 50;
        headless_rect.x = ctx.start_button.rect.x;
        headless_rect.w = ctx.start_button.rect.w;
        ctx.headless_toggle_button.rect = headless_rect;

        menu_draw_button(ctx.renderer, &ctx.headless_toggle_button.rect, ctx.headless_toggle_button.label, ctx.font,
                    cfg->headless_enabled);
        SDL_Rect frames_rect = {ctx.headless_toggle_button.rect.x,
                                ctx.headless_toggle_button.rect.y - 35,
                                ctx.headless_toggle_button.rect.w,
                                28};
        ctx.headless_frames_rect = frames_rect;
        SDL_Color panel_color = menu_color_panel();
        SDL_SetRenderDrawColor(ctx.renderer, panel_color.r, panel_color.g, panel_color.b, 255);
        SDL_RenderFillRect(ctx.renderer, &frames_rect);
        SDL_Color accent_color = menu_color_accent();
        SDL_SetRenderDrawColor(ctx.renderer, accent_color.r, accent_color.g, accent_color.b, 180);
        SDL_RenderDrawRect(ctx.renderer, &frames_rect);

        if (ctx.editing_headless_frames) {
            menu_draw_text_input(ctx.renderer, ctx.font, &frames_rect, &ctx.headless_frames_input);
        } else {
            char frames_label[64];
            snprintf(frames_label, sizeof(frames_label), "Frames: %d", ctx.cfg->headless_frame_count);
            menu_draw_text(ctx.renderer, ctx.font, frames_label, frames_rect.x + 8, frames_rect.y + 6, menu_color_text());
        }

        SDL_Rect viscosity_rect = {frames_rect.x,
                                   frames_rect.y - 35,
                                   frames_rect.w,
                                   frames_rect.h};
        ctx.viscosity_rect = viscosity_rect;
        SDL_SetRenderDrawColor(ctx.renderer, panel_color.r, panel_color.g, panel_color.b, 255);
        SDL_RenderFillRect(ctx.renderer, &viscosity_rect);
        SDL_SetRenderDrawColor(ctx.renderer, accent_color.r, accent_color.g, accent_color.b, 140);
        SDL_RenderDrawRect(ctx.renderer, &viscosity_rect);
        if (ctx.editing_viscosity) {
            menu_draw_text_input(ctx.renderer, ctx.font_small, &viscosity_rect, &ctx.viscosity_input);
        } else {
            char viscosity_label[64];
            snprintf(viscosity_label, sizeof(viscosity_label), "Viscosity: %.6g", ctx.cfg->velocity_damping);
            menu_draw_text(ctx.renderer, ctx.font_small, viscosity_label, viscosity_rect.x + 8, viscosity_rect.y + 6, menu_color_text());
        }

        SDL_Rect inflow_rect = {viscosity_rect.x,
                                viscosity_rect.y - 35,
                                viscosity_rect.w,
                                viscosity_rect.h};
        ctx.inflow_rect = inflow_rect;
        SDL_SetRenderDrawColor(ctx.renderer, panel_color.r, panel_color.g, panel_color.b, 255);
        SDL_RenderFillRect(ctx.renderer, &inflow_rect);
        SDL_SetRenderDrawColor(ctx.renderer, accent_color.r, accent_color.g, accent_color.b, 120);
        SDL_RenderDrawRect(ctx.renderer, &inflow_rect);
        if (ctx.editing_inflow) {
            menu_draw_text_input(ctx.renderer, ctx.font_small, &inflow_rect, &ctx.inflow_input);
        } else {
            char inflow_label[64];
            snprintf(inflow_label, sizeof(inflow_label), "Inflow: %.3f", ctx.cfg->tunnel_inflow_speed);
            menu_draw_text(ctx.renderer, ctx.font_small, inflow_label, inflow_rect.x + 8, inflow_rect.y + 6, menu_color_text());
        }
        menu_draw_button(ctx.renderer, &ctx.start_button.rect, ctx.start_button.label, ctx.font, false);
        menu_draw_button(ctx.renderer, &ctx.edit_button.rect, ctx.edit_button.label, ctx.font, false);
        menu_draw_button(ctx.renderer, &ctx.quit_button.rect, ctx.quit_button.label, ctx.font, false);

        if (ctx.status_visible && ctx.font && ctx.status_text[0]) {
            int text_w = 0;
            int text_h = 0;
            TTF_SizeUTF8(ctx.font, ctx.status_text, &text_w, &text_h);
            int status_x = ctx.headless_frames_rect.x;
            int max_x = MENU_WIDTH - text_w - 20;
            if (status_x > max_x) status_x = max_x;
            if (status_x < 20) status_x = 20;
            int status_y = ctx.headless_frames_rect.y - text_h - 10;
            if (status_y < 20) status_y = 20;
            menu_draw_text(ctx.renderer,
                      ctx.font,
                      ctx.status_text,
                      status_x,
                      status_y,
                      ctx.status_wait_ack ? menu_color_accent() : menu_color_text_dim());
        }

        VkResult end = vk_renderer_end_frame((VkRenderer *)ctx.renderer, cmd);
        if (end == VK_ERROR_OUT_OF_DATE_KHR || end == VK_SUBOPTIMAL_KHR) {
            vk_renderer_recreate_swapchain((VkRenderer *)ctx.renderer, ctx.window);
            vk_renderer_set_logical_size((VkRenderer *)ctx.renderer, (float)win_w, (float)win_h);
        } else if (end != VK_SUCCESS) {
            fprintf(stderr, "[menu] vk_renderer_end_frame failed: %d\n", end);
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
        }
    }

    input_context_manager_pop(&context_mgr);

    if (ctx.rename_input.active) {
        menu_finish_rename(&ctx, false);
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
