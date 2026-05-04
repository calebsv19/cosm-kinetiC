#include "app/menu/workspace_authoring/physics_sim_workspace_authoring_host.h"

#include <string.h>

#include "app/menu/shared_theme_font_adapter.h"
#include "core_font.h"
#include "core_theme.h"
#include "kit_workspace_authoring_ui.h"

static CoreResult physics_sim_workspace_authoring_invalid(const char *message) {
    CoreResult result = { CORE_ERR_INVALID_ARG, message };
    return result;
}

static uint32_t physics_sim_workspace_authoring_mod_bits(SDL_Keymod mods) {
    uint32_t bits = 0u;
    if ((mods & KMOD_SHIFT) != 0) bits |= KIT_WORKSPACE_AUTHORING_MOD_SHIFT;
    if ((mods & KMOD_ALT) != 0) bits |= KIT_WORKSPACE_AUTHORING_MOD_ALT;
    if ((mods & KMOD_CTRL) != 0) bits |= KIT_WORKSPACE_AUTHORING_MOD_CTRL;
    if ((mods & KMOD_GUI) != 0) bits |= KIT_WORKSPACE_AUTHORING_MOD_GUI;
    return bits;
}

static KitWorkspaceAuthoringKey physics_sim_workspace_authoring_key_from_sdl_keysym(
    const SDL_Keysym *keysym) {
    if (!keysym) return KIT_WORKSPACE_AUTHORING_KEY_UNKNOWN;
    switch (keysym->scancode) {
    case SDL_SCANCODE_C:
        return KIT_WORKSPACE_AUTHORING_KEY_C;
    case SDL_SCANCODE_V:
        return KIT_WORKSPACE_AUTHORING_KEY_V;
    default:
        break;
    }
    switch (keysym->sym) {
    case SDLK_TAB:
        return KIT_WORKSPACE_AUTHORING_KEY_TAB;
    case SDLK_RETURN:
    case SDLK_KP_ENTER:
        return KIT_WORKSPACE_AUTHORING_KEY_ENTER;
    case SDLK_ESCAPE:
        return KIT_WORKSPACE_AUTHORING_KEY_ESCAPE;
    case SDLK_h:
        return KIT_WORKSPACE_AUTHORING_KEY_H;
    case SDLK_v:
        return KIT_WORKSPACE_AUTHORING_KEY_V;
    case SDLK_x:
        return KIT_WORKSPACE_AUTHORING_KEY_X;
    case SDLK_BACKSPACE:
    case SDLK_DELETE:
        return KIT_WORKSPACE_AUTHORING_KEY_BACKSPACE;
    case SDLK_r:
        return KIT_WORKSPACE_AUTHORING_KEY_R;
    case SDLK_0:
    case SDLK_KP_0:
        return KIT_WORKSPACE_AUTHORING_KEY_DIGIT_0;
    case SDLK_1:
    case SDLK_KP_1:
        return KIT_WORKSPACE_AUTHORING_KEY_DIGIT_1;
    case SDLK_2:
    case SDLK_KP_2:
        return KIT_WORKSPACE_AUTHORING_KEY_DIGIT_2;
    case SDLK_3:
    case SDLK_KP_3:
        return KIT_WORKSPACE_AUTHORING_KEY_DIGIT_3;
    case SDLK_4:
    case SDLK_KP_4:
        return KIT_WORKSPACE_AUTHORING_KEY_DIGIT_4;
    case SDLK_5:
    case SDLK_KP_5:
        return KIT_WORKSPACE_AUTHORING_KEY_DIGIT_5;
    case SDLK_6:
    case SDLK_KP_6:
        return KIT_WORKSPACE_AUTHORING_KEY_DIGIT_6;
    case SDLK_c:
        return KIT_WORKSPACE_AUTHORING_KEY_C;
    case SDLK_z:
        return KIT_WORKSPACE_AUTHORING_KEY_Z;
    default:
        return KIT_WORKSPACE_AUTHORING_KEY_UNKNOWN;
    }
}

static void physics_sim_workspace_authoring_note_consumed(
    PhysicsSimWorkspaceAuthoringHostState *host,
    int runtime_event) {
    if (!host) return;
    host->last_event_consumed = 1u;
    host->consumed_event_count += 1u;
    if (runtime_event) {
        host->captured_runtime_event_count += 1u;
    }
}

static void physics_sim_workspace_authoring_copy_text(char *dst,
                                                      size_t dst_size,
                                                      const char *src) {
    if (!dst || dst_size == 0) return;
    if (!src) {
        dst[0] = '\0';
        return;
    }
    strncpy(dst, src, dst_size - 1);
    dst[dst_size - 1] = '\0';
}

static void physics_sim_workspace_authoring_note_font_theme_status(
    PhysicsSimWorkspaceAuthoringHostState *host,
    const char *status) {
    if (!host || !status) return;
    physics_sim_workspace_authoring_copy_text(host->font_theme_status,
                                              sizeof(host->font_theme_status),
                                              status);
    host->font_theme_status_active = 1u;
    host->font_theme_status_count += 1u;
}

static void physics_sim_workspace_authoring_note_font_theme_changed(
    PhysicsSimWorkspaceAuthoringHostState *host,
    int needs_font_reload,
    int needs_theme_apply) {
    if (!host) return;
    host->font_theme_pending_changes = 1u;
    host->font_theme_change_count += 1u;
    if (needs_font_reload) {
        host->font_theme_needs_font_reload = 1u;
    }
    if (needs_theme_apply) {
        host->font_theme_needs_theme_apply = 1u;
    }
}

static void physics_sim_workspace_authoring_capture_font_theme_baseline(
    PhysicsSimWorkspaceAuthoringHostState *host,
    const AppConfig *cfg) {
    if (!host || host->font_theme_baseline_ready) return;
    host->baseline_text_zoom_step = cfg ? cfg->text_zoom_step : 0;
    if (!physics_sim_shared_theme_current_preset(host->baseline_theme_preset,
                                                 sizeof(host->baseline_theme_preset))) {
        physics_sim_workspace_authoring_copy_text(host->baseline_theme_preset,
                                                  sizeof(host->baseline_theme_preset),
                                                  "greyscale");
    }
    if (!physics_sim_shared_font_current_preset(host->baseline_font_preset,
                                                sizeof(host->baseline_font_preset))) {
        physics_sim_workspace_authoring_copy_text(host->baseline_font_preset,
                                                  sizeof(host->baseline_font_preset),
                                                  "ide");
    }
    host->font_theme_baseline_ready = 1u;
    host->font_theme_pending_changes = 0u;
    host->font_theme_status_active = 0u;
    host->font_theme_status[0] = '\0';
}

static void physics_sim_workspace_authoring_clear_font_theme_baseline(
    PhysicsSimWorkspaceAuthoringHostState *host) {
    if (!host) return;
    host->font_theme_baseline_ready = 0u;
    host->font_theme_pending_changes = 0u;
    host->font_theme_status_active = 0u;
    host->font_theme_status[0] = '\0';
}

static void physics_sim_workspace_authoring_restore_font_theme_baseline(
    PhysicsSimWorkspaceAuthoringHostState *host,
    AppConfig *cfg) {
    if (!host || !host->font_theme_baseline_ready) return;
    if (cfg) {
        cfg->text_zoom_step = app_config_text_zoom_step_clamp(host->baseline_text_zoom_step);
        host->font_theme_needs_font_reload = 1u;
    }
    if (host->baseline_theme_preset[0] &&
        physics_sim_shared_theme_set_preset(host->baseline_theme_preset)) {
        host->font_theme_needs_theme_apply = 1u;
    }
    if (host->baseline_font_preset[0] &&
        physics_sim_shared_font_set_preset(host->baseline_font_preset)) {
        host->font_theme_needs_font_reload = 1u;
    }
    physics_sim_workspace_authoring_clear_font_theme_baseline(host);
}

void physics_sim_workspace_authoring_host_reset(
    PhysicsSimWorkspaceAuthoringHostState *host) {
    if (!host) return;
    memset(host, 0, sizeof(*host));
    host->overlay_mode = PHYSICS_SIM_WORKSPACE_AUTHORING_OVERLAY_PANE;
}

void physics_sim_workspace_authoring_host_set_viewport(
    PhysicsSimWorkspaceAuthoringHostState *host,
    int width,
    int height) {
    if (!host) return;
    host->viewport_width = width > 0 ? (uint32_t)width : 0u;
    host->viewport_height = height > 0 ? (uint32_t)height : 0u;
}

int physics_sim_workspace_authoring_host_active(
    const PhysicsSimWorkspaceAuthoringHostState *host) {
    return host && host->active ? 1 : 0;
}

int physics_sim_workspace_authoring_host_pane_overlay_active(
    const PhysicsSimWorkspaceAuthoringHostState *host) {
    if (!physics_sim_workspace_authoring_host_active(host)) return 0;
    return host->overlay_mode == PHYSICS_SIM_WORKSPACE_AUTHORING_OVERLAY_PANE ? 1 : 0;
}

int physics_sim_workspace_authoring_host_font_theme_overlay_active(
    const PhysicsSimWorkspaceAuthoringHostState *host) {
    if (!physics_sim_workspace_authoring_host_active(host)) return 0;
    return host->overlay_mode == PHYSICS_SIM_WORKSPACE_AUTHORING_OVERLAY_FONT_THEME ? 1 : 0;
}

CoreResult physics_sim_workspace_authoring_host_enter(
    PhysicsSimWorkspaceAuthoringHostState *host) {
    if (!host) return physics_sim_workspace_authoring_invalid("null authoring host");
    if (!physics_sim_workspace_authoring_host_active(host)) {
        host->active = 1u;
        host->overlay_mode = PHYSICS_SIM_WORKSPACE_AUTHORING_OVERLAY_PANE;
        host->enter_count += 1u;
    }
    host->last_event_entered = 1u;
    return core_result_ok();
}

CoreResult physics_sim_workspace_authoring_host_apply(
    PhysicsSimWorkspaceAuthoringHostState *host) {
    if (!host) return physics_sim_workspace_authoring_invalid("null authoring host");
    if (physics_sim_workspace_authoring_host_active(host)) {
        host->active = 0u;
        host->apply_count += 1u;
    }
    host->key_c_down = 0u;
    host->key_v_down = 0u;
    host->entry_chord_armed_key = KIT_WORKSPACE_AUTHORING_KEY_UNKNOWN;
    host->overlay_mode = PHYSICS_SIM_WORKSPACE_AUTHORING_OVERLAY_PANE;
    host->last_event_exited = 1u;
    return core_result_ok();
}

CoreResult physics_sim_workspace_authoring_host_cancel(
    PhysicsSimWorkspaceAuthoringHostState *host) {
    if (!host) return physics_sim_workspace_authoring_invalid("null authoring host");
    if (physics_sim_workspace_authoring_host_active(host)) {
        host->active = 0u;
        host->cancel_count += 1u;
    }
    host->key_c_down = 0u;
    host->key_v_down = 0u;
    host->entry_chord_armed_key = KIT_WORKSPACE_AUTHORING_KEY_UNKNOWN;
    host->overlay_mode = PHYSICS_SIM_WORKSPACE_AUTHORING_OVERLAY_PANE;
    host->last_event_exited = 1u;
    return core_result_ok();
}

CoreResult physics_sim_workspace_authoring_host_cancel_preview(
    PhysicsSimWorkspaceAuthoringHostState *host,
    AppConfig *cfg) {
    if (!host) return physics_sim_workspace_authoring_invalid("null authoring host");
    physics_sim_workspace_authoring_restore_font_theme_baseline(host, cfg);
    return physics_sim_workspace_authoring_host_cancel(host);
}

int physics_sim_workspace_authoring_host_apply_font_theme_button(
    PhysicsSimWorkspaceAuthoringHostState *host,
    AppConfig *cfg,
    KitWorkspaceAuthoringFontThemeButtonId button_id) {
    KitWorkspaceAuthoringFontThemeAction action;
    const char *preset_name = NULL;
    if (!host ||
        !physics_sim_workspace_authoring_host_font_theme_overlay_active(host) ||
        !kit_workspace_authoring_ui_font_theme_button_enabled(button_id)) {
        return 0;
    }

    host->last_font_theme_button_id = (uint32_t)button_id;
    action = kit_workspace_authoring_ui_font_theme_action_for_button(button_id);
    switch (action.type) {
    case KIT_WORKSPACE_AUTHORING_FONT_THEME_ACTION_TEXT_SIZE_DEC:
        if (cfg) {
            int next_step = app_config_text_zoom_step_clamp(cfg->text_zoom_step - 1);
            if (next_step != cfg->text_zoom_step) {
                cfg->text_zoom_step = next_step;
                physics_sim_workspace_authoring_note_font_theme_changed(host, 1, 0);
            }
        }
        physics_sim_workspace_authoring_note_font_theme_status(host, "Text size decreased.");
        host->font_theme_button_click_count += 1u;
        return 1;
    case KIT_WORKSPACE_AUTHORING_FONT_THEME_ACTION_TEXT_SIZE_INC:
        if (cfg) {
            int next_step = app_config_text_zoom_step_clamp(cfg->text_zoom_step + 1);
            if (next_step != cfg->text_zoom_step) {
                cfg->text_zoom_step = next_step;
                physics_sim_workspace_authoring_note_font_theme_changed(host, 1, 0);
            }
        }
        physics_sim_workspace_authoring_note_font_theme_status(host, "Text size increased.");
        host->font_theme_button_click_count += 1u;
        return 1;
    case KIT_WORKSPACE_AUTHORING_FONT_THEME_ACTION_TEXT_SIZE_RESET:
        if (cfg && cfg->text_zoom_step != 0) {
            cfg->text_zoom_step = 0;
            physics_sim_workspace_authoring_note_font_theme_changed(host, 1, 0);
        }
        physics_sim_workspace_authoring_note_font_theme_status(host, "Text size reset.");
        host->font_theme_button_click_count += 1u;
        return 1;
    case KIT_WORKSPACE_AUTHORING_FONT_THEME_ACTION_SET_FONT_PRESET:
        preset_name = core_font_preset_name(action.font_preset_id);
        if (preset_name && physics_sim_shared_font_set_preset(preset_name)) {
            physics_sim_workspace_authoring_note_font_theme_changed(host, 1, 0);
            physics_sim_workspace_authoring_note_font_theme_status(host, "Font preset changed.");
            host->font_theme_button_click_count += 1u;
            return 1;
        }
        physics_sim_workspace_authoring_note_font_theme_status(host, "Font preset change failed.");
        return 1;
    case KIT_WORKSPACE_AUTHORING_FONT_THEME_ACTION_SET_THEME_PRESET:
        preset_name = core_theme_preset_name(action.theme_preset_id);
        if (preset_name && physics_sim_shared_theme_set_preset(preset_name)) {
            physics_sim_workspace_authoring_note_font_theme_changed(host, 0, 1);
            physics_sim_workspace_authoring_note_font_theme_status(host, "Theme preset changed.");
            host->font_theme_button_click_count += 1u;
            return 1;
        }
        physics_sim_workspace_authoring_note_font_theme_status(host, "Theme preset change failed.");
        return 1;
    case KIT_WORKSPACE_AUTHORING_FONT_THEME_ACTION_CUSTOM_THEME_STATUS:
        physics_sim_workspace_authoring_note_font_theme_status(
            host,
            action.custom_status_text ? action.custom_status_text : "Custom theme action requested.");
        host->font_theme_button_click_count += 1u;
        return 1;
    case KIT_WORKSPACE_AUTHORING_FONT_THEME_ACTION_NONE:
    default:
        break;
    }
    return 0;
}

CoreResult physics_sim_workspace_authoring_host_cycle_overlay(
    PhysicsSimWorkspaceAuthoringHostState *host) {
    if (!host) return physics_sim_workspace_authoring_invalid("null authoring host");
    if (!physics_sim_workspace_authoring_host_active(host)) return core_result_ok();
    host->overlay_mode =
        host->overlay_mode == PHYSICS_SIM_WORKSPACE_AUTHORING_OVERLAY_PANE
            ? PHYSICS_SIM_WORKSPACE_AUTHORING_OVERLAY_FONT_THEME
            : PHYSICS_SIM_WORKSPACE_AUTHORING_OVERLAY_PANE;
    host->overlay_cycle_count += 1u;
    return core_result_ok();
}

static int physics_sim_workspace_authoring_host_handle_overlay_click(
    PhysicsSimWorkspaceAuthoringHostState *host,
    AppConfig *cfg,
    int x,
    int y) {
    KitWorkspaceAuthoringOverlayButton buttons[4];
    KitWorkspaceAuthoringOverlayButtonId hit = KIT_WORKSPACE_AUTHORING_OVERLAY_BUTTON_NONE;
    uint32_t count = 0u;
    if (!host || !physics_sim_workspace_authoring_host_active(host)) return 0;
    if (host->viewport_width == 0u) return 0;

    count = kit_workspace_authoring_ui_build_overlay_buttons(
        (int)host->viewport_width,
        1,
        physics_sim_workspace_authoring_host_pane_overlay_active(host),
        buttons,
        (uint32_t)(sizeof(buttons) / sizeof(buttons[0])));
    hit = kit_workspace_authoring_ui_overlay_hit_test(buttons, count, (float)x, (float)y);
    host->last_overlay_button_id = (uint32_t)hit;
    switch (hit) {
    case KIT_WORKSPACE_AUTHORING_OVERLAY_BUTTON_MODE:
        (void)physics_sim_workspace_authoring_host_cycle_overlay(host);
        host->overlay_button_click_count += 1u;
        return 1;
    case KIT_WORKSPACE_AUTHORING_OVERLAY_BUTTON_APPLY:
        physics_sim_workspace_authoring_clear_font_theme_baseline(host);
        (void)physics_sim_workspace_authoring_host_apply(host);
        host->overlay_button_click_count += 1u;
        return 1;
    case KIT_WORKSPACE_AUTHORING_OVERLAY_BUTTON_CANCEL:
        (void)physics_sim_workspace_authoring_host_cancel_preview(host, cfg);
        host->overlay_button_click_count += 1u;
        return 1;
    case KIT_WORKSPACE_AUTHORING_OVERLAY_BUTTON_ADD:
        host->add_stub_count += 1u;
        host->overlay_button_click_count += 1u;
        return 1;
    case KIT_WORKSPACE_AUTHORING_OVERLAY_BUTTON_NONE:
    default:
        break;
    }
    return 0;
}

static int physics_sim_workspace_authoring_host_handle_font_theme_click(
    PhysicsSimWorkspaceAuthoringHostState *host,
    AppConfig *cfg,
    int x,
    int y) {
    KitWorkspaceAuthoringFontThemeLayout layout;
    KitWorkspaceAuthoringFontThemeButtonId hit;
    if (!host || !physics_sim_workspace_authoring_host_font_theme_overlay_active(host)) return 0;
    if (host->viewport_width == 0u || host->viewport_height == 0u) return 0;
    if (!kit_workspace_authoring_ui_font_theme_build_layout(NULL,
                                                            (int)host->viewport_width,
                                                            (int)host->viewport_height,
                                                            &layout)) {
        return 0;
    }
    hit = kit_workspace_authoring_ui_font_theme_hit_button(&layout, (float)x, (float)y);
    if (hit == KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_NONE) {
        return 0;
    }
    return physics_sim_workspace_authoring_host_apply_font_theme_button(host, cfg, hit);
}

int physics_sim_workspace_authoring_host_handle_sdl_event(
    PhysicsSimWorkspaceAuthoringHostState *host,
    const SDL_Event *event,
    int text_entry_active,
    AppConfig *cfg) {
    KitWorkspaceAuthoringKey key = KIT_WORKSPACE_AUTHORING_KEY_UNKNOWN;
    uint32_t mod_bits = 0u;
    int authoring_alt_only = 0;
    int chord_pair_pressed = 0;
    const char *trigger = NULL;

    if (!host || !event) return 0;
    host->last_event_consumed = 0u;
    host->last_event_entered = 0u;
    host->last_event_exited = 0u;

    if (event->type == SDL_KEYUP) {
        key = physics_sim_workspace_authoring_key_from_sdl_keysym(&event->key.keysym);
        if (key == KIT_WORKSPACE_AUTHORING_KEY_C) {
            host->key_c_down = 0u;
        } else if (key == KIT_WORKSPACE_AUTHORING_KEY_V) {
            host->key_v_down = 0u;
        }
        return 0;
    }

    if (event->type == SDL_MOUSEMOTION &&
        physics_sim_workspace_authoring_host_active(host)) {
        host->last_pointer_x = event->motion.x > 0 ? (uint32_t)event->motion.x : 0u;
        host->last_pointer_y = event->motion.y > 0 ? (uint32_t)event->motion.y : 0u;
        host->last_pointer_ready = 1u;
        physics_sim_workspace_authoring_note_consumed(host, 1);
        return 1;
    }

    if (event->type == SDL_MOUSEBUTTONDOWN &&
        event->button.button == SDL_BUTTON_LEFT &&
        physics_sim_workspace_authoring_host_active(host)) {
        int overlay_hit = 0;
        host->last_pointer_x = event->button.x > 0 ? (uint32_t)event->button.x : 0u;
        host->last_pointer_y = event->button.y > 0 ? (uint32_t)event->button.y : 0u;
        host->last_pointer_ready = 1u;
        overlay_hit = physics_sim_workspace_authoring_host_handle_overlay_click(
            host,
            cfg,
            event->button.x,
            event->button.y);
        if (!overlay_hit &&
            physics_sim_workspace_authoring_host_handle_font_theme_click(host,
                                                                         cfg,
                                                                         event->button.x,
                                                                         event->button.y)) {
            physics_sim_workspace_authoring_note_consumed(host, 0);
            return 1;
        }
        physics_sim_workspace_authoring_note_consumed(host, overlay_hit ? 0 : 1);
        return 1;
    }

    if (physics_sim_workspace_authoring_host_active(host) &&
        (event->type == SDL_MOUSEBUTTONDOWN ||
         event->type == SDL_MOUSEBUTTONUP ||
         event->type == SDL_MOUSEWHEEL ||
         event->type == SDL_TEXTINPUT)) {
        physics_sim_workspace_authoring_note_consumed(host, 1);
        return 1;
    }

    if (event->type != SDL_KEYDOWN || event->key.repeat != 0) {
        return 0;
    }

    key = physics_sim_workspace_authoring_key_from_sdl_keysym(&event->key.keysym);
    mod_bits = physics_sim_workspace_authoring_mod_bits((SDL_Keymod)event->key.keysym.mod);
    authoring_alt_only = ((mod_bits & KIT_WORKSPACE_AUTHORING_MOD_ALT) != 0u) &&
                         ((mod_bits & (KIT_WORKSPACE_AUTHORING_MOD_SHIFT |
                                       KIT_WORKSPACE_AUTHORING_MOD_CTRL |
                                       KIT_WORKSPACE_AUTHORING_MOD_GUI)) == 0u);

    if (text_entry_active && !physics_sim_workspace_authoring_host_active(host)) {
        return 0;
    }

    if (authoring_alt_only) {
        if (key == KIT_WORKSPACE_AUTHORING_KEY_C) {
            host->key_c_down = 1u;
        } else if (key == KIT_WORKSPACE_AUTHORING_KEY_V) {
            host->key_v_down = 1u;
        }
    }

    chord_pair_pressed = kit_workspace_authoring_entry_chord_pressed(
        key,
        mod_bits,
        host->key_c_down ? 1 : 0,
        host->key_v_down ? 1 : 0);
    if (authoring_alt_only &&
        (key == KIT_WORKSPACE_AUTHORING_KEY_C || key == KIT_WORKSPACE_AUTHORING_KEY_V) &&
        host->entry_chord_armed_key != KIT_WORKSPACE_AUTHORING_KEY_UNKNOWN &&
        host->entry_chord_armed_key != (uint8_t)key) {
        chord_pair_pressed = 1;
    }
    if (chord_pair_pressed) {
        if (physics_sim_workspace_authoring_host_active(host)) {
            (void)physics_sim_workspace_authoring_host_cancel_preview(host, cfg);
        } else {
            (void)physics_sim_workspace_authoring_host_enter(host);
            physics_sim_workspace_authoring_capture_font_theme_baseline(host, cfg);
        }
        host->entry_chord_armed_key = KIT_WORKSPACE_AUTHORING_KEY_UNKNOWN;
        physics_sim_workspace_authoring_note_consumed(host, 0);
        return 1;
    }

    if (authoring_alt_only &&
        (key == KIT_WORKSPACE_AUTHORING_KEY_C || key == KIT_WORKSPACE_AUTHORING_KEY_V)) {
        host->entry_chord_armed_key = (uint8_t)key;
        physics_sim_workspace_authoring_note_consumed(host, 0);
        return 1;
    }

    if (!physics_sim_workspace_authoring_host_active(host)) {
        return 0;
    }

    if (physics_sim_workspace_authoring_host_font_theme_overlay_active(host) &&
        ((mod_bits & (KIT_WORKSPACE_AUTHORING_MOD_CTRL | KIT_WORKSPACE_AUTHORING_MOD_GUI)) != 0u)) {
        if (event->key.keysym.sym == SDLK_EQUALS || event->key.keysym.sym == SDLK_PLUS ||
            event->key.keysym.sym == SDLK_KP_PLUS) {
            (void)physics_sim_workspace_authoring_host_apply_font_theme_button(
                host,
                cfg,
                KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_TEXT_SIZE_INC);
            physics_sim_workspace_authoring_note_consumed(host, 0);
            return 1;
        }
        if (event->key.keysym.sym == SDLK_MINUS || event->key.keysym.sym == SDLK_KP_MINUS) {
            (void)physics_sim_workspace_authoring_host_apply_font_theme_button(
                host,
                cfg,
                KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_TEXT_SIZE_DEC);
            physics_sim_workspace_authoring_note_consumed(host, 0);
            return 1;
        }
        if (event->key.keysym.sym == SDLK_0 || event->key.keysym.sym == SDLK_KP_0) {
            (void)physics_sim_workspace_authoring_host_apply_font_theme_button(
                host,
                cfg,
                KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_TEXT_SIZE_RESET);
            physics_sim_workspace_authoring_note_consumed(host, 0);
            return 1;
        }
    }

    if (key == KIT_WORKSPACE_AUTHORING_KEY_ESCAPE) {
        (void)physics_sim_workspace_authoring_host_cancel_preview(host, cfg);
        physics_sim_workspace_authoring_note_consumed(host, 0);
        return 1;
    }
    if (key == KIT_WORKSPACE_AUTHORING_KEY_ENTER) {
        physics_sim_workspace_authoring_clear_font_theme_baseline(host);
        (void)physics_sim_workspace_authoring_host_apply(host);
        physics_sim_workspace_authoring_note_consumed(host, 0);
        return 1;
    }

    trigger = kit_workspace_authoring_trigger_from_key(key, mod_bits);
    if (trigger && strcmp(trigger, "tab") == 0) {
        (void)physics_sim_workspace_authoring_host_cycle_overlay(host);
        physics_sim_workspace_authoring_note_consumed(host, 0);
        return 1;
    }
    if (trigger) {
        physics_sim_workspace_authoring_note_consumed(host, 1);
        return 1;
    }
    return 0;
}
