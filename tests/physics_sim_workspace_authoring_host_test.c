#include "app/menu/workspace_authoring/physics_sim_workspace_authoring_host.h"
#include "app/menu/workspace_authoring/physics_sim_workspace_authoring_overlay_model.h"
#include "kit_workspace_authoring_ui.h"

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

static SDL_Event authoring_mouse_down(int x, int y) {
    SDL_Event event;
    memset(&event, 0, sizeof(event));
    event.type = SDL_MOUSEBUTTONDOWN;
    event.button.type = SDL_MOUSEBUTTONDOWN;
    event.button.button = SDL_BUTTON_LEFT;
    event.button.x = x;
    event.button.y = y;
    return event;
}

static void authoring_button_point(PhysicsSimWorkspaceAuthoringHostState *host,
                                   KitWorkspaceAuthoringOverlayButtonId button_id,
                                   int *out_x,
                                   int *out_y) {
    KitWorkspaceAuthoringOverlayButton buttons[4];
    uint32_t count;
    uint32_t i;
    *out_x = 0;
    *out_y = 0;
    count = kit_workspace_authoring_ui_build_overlay_buttons(
        (int)host->viewport_width,
        physics_sim_workspace_authoring_host_active(host),
        physics_sim_workspace_authoring_host_pane_overlay_active(host),
        buttons,
        (uint32_t)(sizeof(buttons) / sizeof(buttons[0])));
    for (i = 0u; i < count; ++i) {
        if (buttons[i].id == button_id) {
            *out_x = (int)(buttons[i].rect.x + buttons[i].rect.width * 0.5f);
            *out_y = (int)(buttons[i].rect.y + buttons[i].rect.height * 0.5f);
            return;
        }
    }
}

static void authoring_font_theme_button_point(int width,
                                              int height,
                                              KitWorkspaceAuthoringFontThemeButtonId button_id,
                                              int *out_x,
                                              int *out_y) {
    KitWorkspaceAuthoringFontThemeLayout layout;
    KitRenderRect rect = {0};
    *out_x = 0;
    *out_y = 0;
    assert(kit_workspace_authoring_ui_font_theme_build_layout(NULL, width, height, &layout));
    switch (button_id) {
    case KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_TEXT_SIZE_DEC:
        rect = layout.text_size_dec_button;
        break;
    case KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_TEXT_SIZE_INC:
        rect = layout.text_size_inc_button;
        break;
    case KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_TEXT_SIZE_RESET:
        rect = layout.text_size_reset_button;
        break;
    case KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_THEME_PRESET_MIDNIGHT_CONTRAST:
        rect = layout.theme_preset_buttons[2];
        break;
    default:
        assert(0 && "unsupported test button");
    }
    *out_x = (int)(rect.x + rect.width * 0.5f);
    *out_y = (int)(rect.y + rect.height * 0.5f);
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
    assert(!physics_sim_workspace_authoring_host_handle_sdl_event(&host, &plain_c, 0, NULL));
    assert(!physics_sim_workspace_authoring_host_active(&host));

    alt_c = authoring_key_event(SDL_KEYDOWN, SDL_SCANCODE_C, SDLK_c, KMOD_ALT);
    alt_v = authoring_key_event(SDL_KEYDOWN, SDL_SCANCODE_V, SDLK_v, KMOD_ALT);
    assert(physics_sim_workspace_authoring_host_handle_sdl_event(&host, &alt_c, 0, NULL));
    assert(!physics_sim_workspace_authoring_host_active(&host));
    assert(host.entry_chord_armed_key != 0u);
    assert(physics_sim_workspace_authoring_host_handle_sdl_event(&host, &alt_v, 0, NULL));
    assert(physics_sim_workspace_authoring_host_active(&host));
    assert(physics_sim_workspace_authoring_host_pane_overlay_active(&host));
    assert(host.enter_count == 1u);

    tab = authoring_key_event(SDL_KEYDOWN, SDL_SCANCODE_TAB, SDLK_TAB, KMOD_NONE);
    assert(physics_sim_workspace_authoring_host_handle_sdl_event(&host, &tab, 0, NULL));
    assert(physics_sim_workspace_authoring_host_font_theme_overlay_active(&host));
    assert(host.overlay_cycle_count == 1u);

    escape = authoring_key_event(SDL_KEYDOWN, SDL_SCANCODE_ESCAPE, SDLK_ESCAPE, KMOD_NONE);
    assert(physics_sim_workspace_authoring_host_handle_sdl_event(&host, &escape, 0, NULL));
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
    assert(!physics_sim_workspace_authoring_host_handle_sdl_event(&host, &alt_c, 1, NULL));
    assert(!physics_sim_workspace_authoring_host_handle_sdl_event(&host, &alt_v, 1, NULL));
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

    assert(physics_sim_workspace_authoring_host_handle_sdl_event(&host, &alt_c_down, 0, NULL));
    assert(!physics_sim_workspace_authoring_host_active(&host));
    assert(!physics_sim_workspace_authoring_host_handle_sdl_event(&host, &alt_c_up, 0, NULL));
    assert(physics_sim_workspace_authoring_host_handle_sdl_event(&host, &alt_v_down, 0, NULL));
    assert(physics_sim_workspace_authoring_host_active(&host));
    assert(host.enter_count == 1u);

    assert(physics_sim_workspace_authoring_host_handle_sdl_event(&host, &enter, 0, NULL));
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
    assert(physics_sim_workspace_authoring_host_handle_sdl_event(&host, &h_key, 0, NULL));
    assert(host.captured_runtime_event_count == 1u);

    memset(&mouse_down, 0, sizeof(mouse_down));
    mouse_down.type = SDL_MOUSEBUTTONDOWN;
    mouse_down.button.type = SDL_MOUSEBUTTONDOWN;
    mouse_down.button.button = SDL_BUTTON_LEFT;
    assert(physics_sim_workspace_authoring_host_handle_sdl_event(&host, &mouse_down, 0, NULL));
    assert(host.captured_runtime_event_count == 2u);
}

static void test_overlay_buttons_control_state(void) {
    PhysicsSimWorkspaceAuthoringHostState host;
    SDL_Event click;
    int x = 0;
    int y = 0;

    physics_sim_workspace_authoring_host_reset(&host);
    physics_sim_workspace_authoring_host_set_viewport(&host, 1280, 720);
    assert(physics_sim_workspace_authoring_host_enter(&host).code == CORE_OK);
    assert(physics_sim_workspace_authoring_host_pane_overlay_active(&host));

    authoring_button_point(&host, KIT_WORKSPACE_AUTHORING_OVERLAY_BUTTON_MODE, &x, &y);
    assert(x > 0 && y > 0);
    click = authoring_mouse_down(x, y);
    assert(physics_sim_workspace_authoring_host_handle_sdl_event(&host, &click, 0, NULL));
    assert(physics_sim_workspace_authoring_host_font_theme_overlay_active(&host));
    assert(host.overlay_button_click_count == 1u);

    assert(physics_sim_workspace_authoring_host_cycle_overlay(&host).code == CORE_OK);
    assert(physics_sim_workspace_authoring_host_pane_overlay_active(&host));
    authoring_button_point(&host, KIT_WORKSPACE_AUTHORING_OVERLAY_BUTTON_ADD, &x, &y);
    assert(x > 0 && y > 0);
    click = authoring_mouse_down(x, y);
    assert(physics_sim_workspace_authoring_host_handle_sdl_event(&host, &click, 0, NULL));
    assert(physics_sim_workspace_authoring_host_active(&host));
    assert(host.add_stub_count == 1u);

    authoring_button_point(&host, KIT_WORKSPACE_AUTHORING_OVERLAY_BUTTON_APPLY, &x, &y);
    assert(x > 0 && y > 0);
    click = authoring_mouse_down(x, y);
    assert(physics_sim_workspace_authoring_host_handle_sdl_event(&host, &click, 0, NULL));
    assert(!physics_sim_workspace_authoring_host_active(&host));
    assert(host.apply_count == 1u);
}

static void test_pane_overlay_rows_use_scene_editor_pane_host(void) {
    PhysicsSimWorkspaceAuthoringPaneRow rows[3];
    uint32_t count = physics_sim_workspace_authoring_overlay_build_pane_rows(
        1280,
        720,
        rows,
        (uint32_t)(sizeof(rows) / sizeof(rows[0])));
    assert(count == 3u);
    assert(rows[0].role == SCENE_EDITOR_PANE_LEFT);
    assert(rows[1].role == SCENE_EDITOR_PANE_CENTER);
    assert(rows[2].role == SCENE_EDITOR_PANE_RIGHT);
    assert(rows[0].pane_rect.width >= 220.0f);
    assert(rows[2].pane_rect.width >= 220.0f);
    assert(rows[1].pane_rect.width > rows[0].pane_rect.width);
    assert(rows[0].module_key && rows[0].module_key[0]);
    assert(rows[1].module_label && rows[1].module_label[0]);
}

static void test_font_theme_overlay_controls_preview_state(void) {
    PhysicsSimWorkspaceAuthoringHostState host;
    AppConfig cfg;
    SDL_Event click;
    int x = 0;
    int y = 0;

    memset(&cfg, 0, sizeof(cfg));
    cfg.text_zoom_step = 0;
    physics_sim_workspace_authoring_host_reset(&host);
    physics_sim_workspace_authoring_host_set_viewport(&host, 1280, 720);
    assert(physics_sim_workspace_authoring_host_enter(&host).code == CORE_OK);
    assert(physics_sim_workspace_authoring_host_cycle_overlay(&host).code == CORE_OK);
    assert(physics_sim_workspace_authoring_host_font_theme_overlay_active(&host));

    authoring_font_theme_button_point(1280,
                                      720,
                                      KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_TEXT_SIZE_INC,
                                      &x,
                                      &y);
    click = authoring_mouse_down(x, y);
    assert(physics_sim_workspace_authoring_host_handle_sdl_event(&host, &click, 0, &cfg));
    assert(cfg.text_zoom_step == 1);
    assert(host.font_theme_pending_changes == 1u);
    assert(host.font_theme_needs_font_reload == 1u);
    assert(host.font_theme_button_click_count == 1u);

    host.font_theme_needs_font_reload = 0u;
    authoring_font_theme_button_point(1280,
                                      720,
                                      KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_TEXT_SIZE_RESET,
                                      &x,
                                      &y);
    click = authoring_mouse_down(x, y);
    assert(physics_sim_workspace_authoring_host_handle_sdl_event(&host, &click, 0, &cfg));
    assert(cfg.text_zoom_step == 0);
    assert(host.font_theme_needs_font_reload == 1u);

    host.font_theme_needs_theme_apply = 0u;
    authoring_font_theme_button_point(1280,
                                      720,
                                      KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_THEME_PRESET_MIDNIGHT_CONTRAST,
                                      &x,
                                      &y);
    click = authoring_mouse_down(x, y);
    assert(physics_sim_workspace_authoring_host_handle_sdl_event(&host, &click, 0, &cfg));
    assert(host.font_theme_needs_theme_apply == 1u);
}

int main(void) {
    test_entry_chord_and_cancel();
    test_text_entry_blocks_inactive_entry_chord();
    test_sequential_physical_chord_and_apply();
    test_runtime_events_captured_while_active();
    test_overlay_buttons_control_state();
    test_pane_overlay_rows_use_scene_editor_pane_host();
    test_font_theme_overlay_controls_preview_state();
    return 0;
}
