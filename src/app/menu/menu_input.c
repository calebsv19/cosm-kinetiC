#include "app/menu/menu_input.h"

#include <SDL2/SDL.h>
#include <stdio.h>

#include "app/editor/scene_editor.h"
#include "app/menu/menu_render.h"
#include "app/menu/menu_state.h"
#include "app/structural/structural_preset_editor.h"
#include "vk_renderer.h"

void menu_pointer_up(void *user, const InputPointerState *state) {
    SceneMenuInteraction *ctx = (SceneMenuInteraction *)user;
    if (!ctx || !state) return;
    if (state->button != SDL_BUTTON_LEFT) return;
    if (ctx->suppress_pointer_until_up) {
        ctx->suppress_pointer_until_up = false;
        return;
    }
    if (ctx->headless_running) return;

    if (ctx->scrollbar_dragging) {
        ctx->scrollbar_dragging = false;
        scrollbar_handle_pointer_up(&ctx->scrollbar);
        return;
    }

    int x = state->x;
    int y = state->y;
    Uint32 now = SDL_GetTicks();

    if (ctx->rename_input.active && ctx->renaming_slot >= 0) {
        SDL_Rect rename_rect;
        bool visible = menu_preset_row_rect(ctx,
                                            ctx->renaming_slot,
                                            false,
                                            &rename_rect);
        if (!visible || !menu_point_in_rect(x, y, &rename_rect)) {
            menu_finish_rename(ctx, false);
        }
    }

    if (menu_point_in_rect(x, y, &ctx->headless_toggle_button.rect)) {
        ctx->cfg->headless_enabled = !ctx->cfg->headless_enabled;
        return;
    }

    if (menu_point_in_rect(x, y, &ctx->inflow_rect)) {
        Uint32 now = SDL_GetTicks();
        bool double_click = (now - ctx->last_inflow_click_ticks) <= DOUBLE_CLICK_MS;
        ctx->last_inflow_click_ticks = now;
        if (double_click) {
            menu_begin_inflow_edit(ctx);
        }
        return;
    } else if (ctx->editing_inflow) {
        menu_finish_inflow_edit(ctx, true);
    }

    if (menu_point_in_rect(x, y, &ctx->viscosity_rect)) {
        Uint32 now = SDL_GetTicks();
        bool double_click = (now - ctx->last_viscosity_click_ticks) <= DOUBLE_CLICK_MS;
        ctx->last_viscosity_click_ticks = now;
        if (double_click) {
            menu_begin_viscosity_edit(ctx);
        }
        return;
    } else if (ctx->editing_viscosity) {
        menu_finish_viscosity_edit(ctx, true);
    }

    if (menu_point_in_rect(x, y, &ctx->headless_frames_rect)) {
        Uint32 now = SDL_GetTicks();
        bool double_click = (now - ctx->last_headless_click_ticks) <= DOUBLE_CLICK_MS;
        ctx->last_headless_click_ticks = now;
        if (double_click) {
            menu_begin_headless_frames_edit(ctx);
        }
        return;
    } else if (ctx->editing_headless_frames) {
        menu_finish_headless_frames_edit(ctx, true);
    }

    if (menu_point_in_rect(x, y, &ctx->start_button.rect)) {
        if (ctx->rename_input.active) {
            menu_finish_rename(ctx, true);
        }
        if (ctx->editing_headless_frames) {
            menu_finish_headless_frames_edit(ctx, true);
        }
        if (ctx->editing_inflow) {
            menu_finish_inflow_edit(ctx, true);
        }
        if (ctx->editing_viscosity) {
            menu_finish_viscosity_edit(ctx, true);
        }
        if (ctx->cfg->headless_enabled) {
            ctx->headless_pending = true;
            menu_set_status(ctx, "Headless run queued...", false);
            return;
        }
        if (ctx->preset_output && ctx->active_preset) {
            CustomPresetSlot *start_slot = preset_library_get_slot(
                ctx->library, ctx->selection->custom_slot_index);
            if (start_slot) start_slot->occupied = true;
            *ctx->preset_output = *ctx->active_preset;
            ctx->selection->headless_frame_count = ctx->cfg ? ctx->cfg->headless_frame_count : ctx->selection->headless_frame_count;
            *ctx->start_requested = true;
            *ctx->running = false;
        }
        return;
    }

    if (menu_point_in_rect(x, y, &ctx->edit_button.rect)) {
        if (ctx->rename_input.active) {
            menu_finish_rename(ctx, true);
        }
        CustomPresetSlot *slot = preset_library_get_slot(
            ctx->library, ctx->selection->custom_slot_index);
        if (!slot) return;
        if (ctx->active_mode == SIM_MODE_STRUCTURAL) {
            fprintf(stderr, "[menu] Edit preset pressed: structural mode.\n");
            if (slot->preset.structural_scene_path[0] == '\0') {
                menu_assign_structural_preset_path(slot, ctx->selection->custom_slot_index);
            }
#if defined(__APPLE__)
            fprintf(stderr, "[menu] Hiding menu window for structural editor.\n");
            if (ctx->renderer) {
                vk_renderer_wait_idle((VkRenderer *)ctx->renderer);
            }
            SDL_HideWindow(ctx->window);
            SDL_PumpEvents();
            SDL_Delay(80);
#endif
            fprintf(stderr, "[menu] Launching structural_preset_editor_run path=%s\n",
                    slot->preset.structural_scene_path);
            if (structural_preset_editor_run(ctx->window,
                                             NULL,
                                             ctx->font,
                                             ctx->font_small,
                                             ctx->cfg,
                                             slot->preset.structural_scene_path,
                                             ctx->context_mgr)) {
                slot->occupied = true;
            }
#if defined(__APPLE__)
            fprintf(stderr, "[menu] Structural editor returned, showing menu window.\n");
            SDL_ShowWindow(ctx->window);
            SDL_RaiseWindow(ctx->window);
#endif
        } else {
            FluidScenePreset *target = &slot->preset;
            char *name_buffer = slot->name;
            size_t name_capacity = sizeof(slot->name);
            if (scene_editor_run(ctx->window,
                                 ctx->renderer,
                                 ctx->font,
                                 ctx->font_small,
                                 ctx->cfg,
                                 target,
                                 ctx->context_mgr,
                                 ctx->shape_library,
                                 name_buffer,
                                 name_capacity)) {
                slot->preset = *target;
                slot->preset.name = slot->name;
                slot->preset.is_custom = true;
                slot->occupied = true;
            }
        }
        SDL_FlushEvent(SDL_MOUSEBUTTONDOWN);
        SDL_FlushEvent(SDL_MOUSEBUTTONUP);
        SDL_FlushEvent(SDL_MOUSEMOTION);
        ctx->suppress_pointer_until_up = true;
        return;
    }

    if (menu_point_in_rect(x, y, &ctx->quit_button.rect)) {
        *ctx->running = false;
        return;
    }

    if (menu_point_in_rect(x, y, &ctx->grid_dec_button.rect)) {
        ctx->cfg->grid_w = ctx->cfg->grid_h =
            (ctx->cfg->grid_w > 32) ? ctx->cfg->grid_w - 32 : 32;
        menu_set_custom_quality(ctx);
        return;
    }

    if (menu_point_in_rect(x, y, &ctx->grid_inc_button.rect)) {
        ctx->cfg->grid_w = ctx->cfg->grid_h =
            (ctx->cfg->grid_w < 512) ? ctx->cfg->grid_w + 32 : 512;
        menu_set_custom_quality(ctx);
        return;
    }

    if (menu_point_in_rect(x, y, &ctx->quality_prev_button.rect)) {
        menu_cycle_quality(ctx, -1);
        return;
    }
    if (menu_point_in_rect(x, y, &ctx->quality_next_button.rect)) {
        menu_cycle_quality(ctx, 1);
        return;
    }

    if (menu_point_in_rect(x, y, &ctx->volume_toggle_rect)) {
        ctx->cfg->save_volume_frames = !ctx->cfg->save_volume_frames;
        return;
    }

    if (menu_point_in_rect(x, y, &ctx->render_toggle_rect)) {
        ctx->cfg->save_render_frames = !ctx->cfg->save_render_frames;
        return;
    }

    if (menu_point_in_rect(x, y, &ctx->mode_toggle_button.rect)) {
        SimulationMode new_mode = (SimulationMode)((ctx->active_mode + 1) % SIMULATION_MODE_COUNT);
        menu_switch_mode(ctx, new_mode);
        menu_update_scrollbar(ctx);
        return;
    }

    SDL_Rect frames_rect = ctx->headless_frames_rect;
    bool in_frames_rect = menu_point_in_rect(x, y, &frames_rect);
    if (!in_frames_rect && ctx->editing_headless_frames) {
        menu_finish_headless_frames_edit(ctx, true);
    }
    SDL_Rect inflow_rect = ctx->inflow_rect;
    bool in_inflow_rect = menu_point_in_rect(x, y, &inflow_rect);
    if (!in_inflow_rect && ctx->editing_inflow) {
        menu_finish_inflow_edit(ctx, true);
    }
    SDL_Rect viscosity_rect = ctx->viscosity_rect;
    bool in_viscosity_rect = menu_point_in_rect(x, y, &viscosity_rect);
    if (!in_viscosity_rect && ctx->editing_viscosity) {
        menu_finish_viscosity_edit(ctx, true);
    }

    bool is_add = false;
    int row = menu_preset_index_from_point(ctx, x, y, &is_add);
    if (row < 0) return;

    if (is_add) {
        menu_add_new_preset(ctx);
        return;
    }

    int slot_index = menu_slot_index_from_visible_row(ctx, row);
    if (slot_index < 0) return;

    SDL_Rect row_rect;
    if (!menu_preset_row_rect(ctx, row, false, &row_rect)) {
        // Clicked outside visible rows due to rounding.
        return;
    }

    SDL_Rect delete_rect = menu_preset_delete_button_rect(&row_rect);
    if (menu_point_in_rect(x, y, &delete_rect)) {
        menu_delete_preset(ctx, slot_index);
        return;
    }

    bool double_click = (ctx->last_clicked_slot == slot_index) &&
                        (now - ctx->last_click_ticks <= DOUBLE_CLICK_MS);
    ctx->last_clicked_slot = slot_index;
    ctx->last_click_ticks = now;

    menu_select_custom(ctx, slot_index);
    menu_scroll_to_slot(ctx, slot_index);
    if (double_click) {
        menu_begin_rename(ctx, slot_index);
    }
}

void menu_pointer_down(void *user, const InputPointerState *state) {
    SceneMenuInteraction *ctx = (SceneMenuInteraction *)user;
    if (!ctx || !state) return;
    if (state->button != SDL_BUTTON_LEFT) return;
    if (ctx->suppress_pointer_until_up) return;
    if (ctx->headless_running) return;
    if (ctx->status_wait_ack && ctx->status_visible) {
        menu_clear_status(ctx);
    }
    if (scrollbar_handle_pointer_down(&ctx->scrollbar, state->x, state->y)) {
        ctx->scrollbar_dragging = true;
    }
}

void menu_pointer_move(void *user, const InputPointerState *state) {
    SceneMenuInteraction *ctx = (SceneMenuInteraction *)user;
    if (!ctx || !state) return;
    if (ctx->headless_running) return;
    if (ctx->scrollbar_dragging) {
        scrollbar_handle_pointer_move(&ctx->scrollbar, state->x, state->y);
        return;
    }
    bool is_add = false;
    int row = menu_preset_index_from_point(ctx, state->x, state->y, &is_add);
    int visible_count = menu_visible_slot_count(ctx);
    if (!is_add && row >= 0 && row < visible_count) {
        ctx->hover_slot = menu_slot_index_from_visible_row(ctx, row);
        ctx->hover_add_entry = false;
    } else {
        ctx->hover_slot = -1;
        ctx->hover_add_entry = (is_add && row == visible_count);
    }
}

void menu_wheel(void *user, const InputWheelState *wheel) {
    SceneMenuInteraction *ctx = (SceneMenuInteraction *)user;
    if (!ctx || !wheel) return;
    scrollbar_handle_wheel(&ctx->scrollbar, wheel->y);
}

void menu_key_down(void *user, SDL_Keycode key, SDL_Keymod mod) {
    SceneMenuInteraction *ctx = (SceneMenuInteraction *)user;
    (void)mod;
    if (!ctx) return;
    if (ctx->headless_running) {
        if (key == SDLK_ESCAPE) {
            ctx->headless_run_requested = false;
            ctx->headless_running = false;
            menu_set_status(ctx, "Headless run canceled.", true);
        }
        return;
    }

    if (ctx->rename_input.active) {
        if (key == SDLK_RETURN || key == SDLK_KP_ENTER) {
            menu_finish_rename(ctx, true);
        } else if (key == SDLK_ESCAPE) {
            menu_finish_rename(ctx, false);
        } else {
            text_input_handle_key(&ctx->rename_input, key);
        }
        return;
    }

    if (ctx->editing_headless_frames) {
        if (key == SDLK_RETURN || key == SDLK_KP_ENTER) {
            menu_finish_headless_frames_edit(ctx, true);
        } else if (key == SDLK_ESCAPE) {
            menu_finish_headless_frames_edit(ctx, false);
        } else {
            text_input_handle_key(&ctx->headless_frames_input, key);
        }
        return;
    }

    if (ctx->editing_inflow) {
        if (key == SDLK_RETURN || key == SDLK_KP_ENTER) {
            menu_finish_inflow_edit(ctx, true);
        } else if (key == SDLK_ESCAPE) {
            menu_finish_inflow_edit(ctx, false);
        } else {
            text_input_handle_key(&ctx->inflow_input, key);
        }
        return;
    }

    if (ctx->editing_viscosity) {
        if (key == SDLK_RETURN || key == SDLK_KP_ENTER) {
            menu_finish_viscosity_edit(ctx, true);
        } else if (key == SDLK_ESCAPE) {
            menu_finish_viscosity_edit(ctx, false);
        } else {
            text_input_handle_key(&ctx->viscosity_input, key);
        }
        return;
    }
}

void menu_text_input(void *user, const char *text) {
    SceneMenuInteraction *ctx = (SceneMenuInteraction *)user;
    if (!ctx || !text) return;
    if (ctx->headless_running) return;
    if (ctx->rename_input.active) {
        text_input_handle_text(&ctx->rename_input, text);
        return;
    }
    if (ctx->editing_headless_frames) {
        text_input_handle_text(&ctx->headless_frames_input, text);
        return;
    }
    if (ctx->editing_inflow) {
        text_input_handle_text(&ctx->inflow_input, text);
        return;
    }
    if (ctx->editing_viscosity) {
        text_input_handle_text(&ctx->viscosity_input, text);
        return;
    }
}
