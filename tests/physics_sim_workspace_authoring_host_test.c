#include "app/menu/workspace_authoring/physics_sim_workspace_authoring_host.h"

#include <assert.h>
#include <string.h>

static SDL_Event authoring_key_event(Uint32 type,
                                     SDL_Scancode scancode,
                                     SDL_Keycode key,
                                     SDL_Keymod mod) {
    SDL_Event event;
    memset(&event, 0, sizeof(event));
    event.type = type;
    event.key.type = type;
    event.key.keysym.scancode = scancode;
    event.key.keysym.sym = key;
    event.key.keysym.mod = mod;
    return event;
}

static void test_entry_chord_and_cancel(void) {
    PhysicsSimWorkspaceAuthoringHostState host;
    SDL_Event plain_c;
    SDL_Event alt_c;
    SDL_Event alt_v;
    SDL_Event tab;
    SDL_Event escape;

    physics_sim_workspace_authoring_host_reset(&host);
    plain_c = authoring_key_event(SDL_KEYDOWN, SDL_SCANCODE_C, SDLK_c, KMOD_NONE);
    assert(!physics_sim_workspace_authoring_host_handle_sdl_event(&host, &plain_c, 0));
    assert(!physics_sim_workspace_authoring_host_active(&host));

    alt_c = authoring_key_event(SDL_KEYDOWN, SDL_SCANCODE_C, SDLK_c, KMOD_ALT);
    alt_v = authoring_key_event(SDL_KEYDOWN, SDL_SCANCODE_V, SDLK_v, KMOD_ALT);
    assert(physics_sim_workspace_authoring_host_handle_sdl_event(&host, &alt_c, 0));
    assert(!physics_sim_workspace_authoring_host_active(&host));
    assert(host.entry_chord_armed_key != 0u);
    assert(physics_sim_workspace_authoring_host_handle_sdl_event(&host, &alt_v, 0));
    assert(physics_sim_workspace_authoring_host_active(&host));
    assert(physics_sim_workspace_authoring_host_pane_overlay_active(&host));
    assert(host.enter_count == 1u);

    tab = authoring_key_event(SDL_KEYDOWN, SDL_SCANCODE_TAB, SDLK_TAB, KMOD_NONE);
    assert(physics_sim_workspace_authoring_host_handle_sdl_event(&host, &tab, 0));
    assert(physics_sim_workspace_authoring_host_font_theme_overlay_active(&host));
    assert(host.overlay_cycle_count == 1u);

    escape = authoring_key_event(SDL_KEYDOWN, SDL_SCANCODE_ESCAPE, SDLK_ESCAPE, KMOD_NONE);
    assert(physics_sim_workspace_authoring_host_handle_sdl_event(&host, &escape, 0));
    assert(!physics_sim_workspace_authoring_host_active(&host));
    assert(host.cancel_count == 1u);
}

static void test_text_entry_blocks_inactive_entry_chord(void) {
    PhysicsSimWorkspaceAuthoringHostState host;
    SDL_Event alt_c;
    SDL_Event alt_v;

    physics_sim_workspace_authoring_host_reset(&host);
    alt_c = authoring_key_event(SDL_KEYDOWN, SDL_SCANCODE_C, SDLK_c, KMOD_ALT);
    alt_v = authoring_key_event(SDL_KEYDOWN, SDL_SCANCODE_V, SDLK_v, KMOD_ALT);
    assert(!physics_sim_workspace_authoring_host_handle_sdl_event(&host, &alt_c, 1));
    assert(!physics_sim_workspace_authoring_host_handle_sdl_event(&host, &alt_v, 1));
    assert(!physics_sim_workspace_authoring_host_active(&host));
    assert(host.consumed_event_count == 0u);
}

static void test_sequential_physical_chord_and_apply(void) {
    PhysicsSimWorkspaceAuthoringHostState host;
    SDL_Event alt_c_down;
    SDL_Event alt_c_up;
    SDL_Event alt_v_down;
    SDL_Event enter;

    physics_sim_workspace_authoring_host_reset(&host);
    alt_c_down = authoring_key_event(SDL_KEYDOWN, SDL_SCANCODE_C, SDLK_UNKNOWN, KMOD_ALT);
    alt_c_up = authoring_key_event(SDL_KEYUP, SDL_SCANCODE_C, SDLK_UNKNOWN, KMOD_ALT);
    alt_v_down = authoring_key_event(SDL_KEYDOWN, SDL_SCANCODE_V, SDLK_UNKNOWN, KMOD_ALT);
    enter = authoring_key_event(SDL_KEYDOWN, SDL_SCANCODE_RETURN, SDLK_RETURN, KMOD_NONE);

    assert(physics_sim_workspace_authoring_host_handle_sdl_event(&host, &alt_c_down, 0));
    assert(!physics_sim_workspace_authoring_host_active(&host));
    assert(!physics_sim_workspace_authoring_host_handle_sdl_event(&host, &alt_c_up, 0));
    assert(physics_sim_workspace_authoring_host_handle_sdl_event(&host, &alt_v_down, 0));
    assert(physics_sim_workspace_authoring_host_active(&host));
    assert(host.enter_count == 1u);

    assert(physics_sim_workspace_authoring_host_handle_sdl_event(&host, &enter, 0));
    assert(!physics_sim_workspace_authoring_host_active(&host));
    assert(host.apply_count == 1u);
}

static void test_runtime_events_captured_while_active(void) {
    PhysicsSimWorkspaceAuthoringHostState host;
    SDL_Event h_key;
    SDL_Event mouse_down;

    physics_sim_workspace_authoring_host_reset(&host);
    assert(physics_sim_workspace_authoring_host_enter(&host).code == CORE_OK);

    h_key = authoring_key_event(SDL_KEYDOWN, SDL_SCANCODE_H, SDLK_h, KMOD_NONE);
    assert(physics_sim_workspace_authoring_host_handle_sdl_event(&host, &h_key, 0));
    assert(host.captured_runtime_event_count == 1u);

    memset(&mouse_down, 0, sizeof(mouse_down));
    mouse_down.type = SDL_MOUSEBUTTONDOWN;
    mouse_down.button.type = SDL_MOUSEBUTTONDOWN;
    mouse_down.button.button = SDL_BUTTON_LEFT;
    assert(physics_sim_workspace_authoring_host_handle_sdl_event(&host, &mouse_down, 0));
    assert(host.captured_runtime_event_count == 2u);
}

int main(void) {
    test_entry_chord_and_cancel();
    test_text_entry_blocks_inactive_entry_chord();
    test_sequential_physical_chord_and_apply();
    test_runtime_events_captured_while_active();
    return 0;
}
