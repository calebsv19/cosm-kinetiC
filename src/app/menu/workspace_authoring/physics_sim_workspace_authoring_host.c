#include "app/menu/workspace_authoring/physics_sim_workspace_authoring_host.h"

#include <string.h>

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

void physics_sim_workspace_authoring_host_reset(
    PhysicsSimWorkspaceAuthoringHostState *host) {
    if (!host) return;
    memset(host, 0, sizeof(*host));
    host->overlay_mode = PHYSICS_SIM_WORKSPACE_AUTHORING_OVERLAY_PANE;
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

int physics_sim_workspace_authoring_host_handle_sdl_event(
    PhysicsSimWorkspaceAuthoringHostState *host,
    const SDL_Event *event,
    int text_entry_active) {
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

    if (physics_sim_workspace_authoring_host_active(host) &&
        (event->type == SDL_MOUSEBUTTONDOWN ||
         event->type == SDL_MOUSEBUTTONUP ||
         event->type == SDL_MOUSEMOTION ||
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
            (void)physics_sim_workspace_authoring_host_cancel(host);
        } else {
            (void)physics_sim_workspace_authoring_host_enter(host);
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

    if (key == KIT_WORKSPACE_AUTHORING_KEY_ESCAPE) {
        (void)physics_sim_workspace_authoring_host_cancel(host);
        physics_sim_workspace_authoring_note_consumed(host, 0);
        return 1;
    }
    if (key == KIT_WORKSPACE_AUTHORING_KEY_ENTER) {
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
