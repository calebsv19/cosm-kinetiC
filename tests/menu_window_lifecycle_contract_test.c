#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "app/menu/menu_types.h"
#include "app/menu/menu_window_lifecycle.h"

int main(void) {
    SDL_setenv("SDL_VIDEODRIVER", "dummy", 1);
    assert(SDL_Init(SDL_INIT_VIDEO) == 0);
    SDL_Window *window = SDL_CreateWindow("menu lifecycle contract", 0, 0, 128, 96, 0);
    assert(window);
    SceneMenuInteraction ctx = {0};
    AppConfig cfg = {0};
    ctx.cfg = &cfg; ctx.window = window;
    assert(menu_window_lifecycle_refresh(&ctx) != 0);
    assert(ctx.window_lifecycle.observed.presentable);
    assert(ctx.window_lifecycle.observed.logical_width == 128);
    ctx.window_lifecycle.geometry_dirty = false; /* host committed layout */
    ctx.editing_input_root = true; ctx.input_root_input.active = true;
    strcpy(ctx.input_root_input.buffer, "unsaved UTF-8 root \xc3\xa9");
    ctx.input_root_input.length = strlen(ctx.input_root_input.buffer);
    ctx.rename_input.active = true; strcpy(ctx.rename_input.buffer, "unsaved rename");
    ctx.workspace_authoring.active = 1;
    ctx.workspace_authoring.font_theme_pending_changes = 1;
    ctx.workspace_authoring.baseline_text_zoom_step = 3;
    ctx.workspace_authoring.key_c_down = 1;
    ctx.workspace_authoring.entry_chord_armed_key = 1;
    ctx.scrollbar_dragging = ctx.scrollbar.dragging = true;
    ctx.last_click_ticks = 100; ctx.last_clicked_slot = 4;
    TextInputField root = ctx.input_root_input, rename = ctx.rename_input;
    AppConfig saved_cfg = cfg;
    MenuSettingsShellState settings = ctx.settings_shell;
    SDL_Event e; SDL_zero(e);
    e.type = SDL_MOUSEBUTTONDOWN; e.button.windowID = SDL_GetWindowID(window);
    e.button.button = SDL_BUTTON_LEFT;
    assert(!menu_window_lifecycle_filter(&ctx, &e));
    assert(ctx.window_lifecycle.pointer_down);
    SDL_zero(e); e.type = SDL_WINDOWEVENT; e.window.windowID = SDL_GetWindowID(window);
    e.window.event = SDL_WINDOWEVENT_FOCUS_LOST;
    assert(!menu_window_lifecycle_filter(&ctx, &e));
    assert(!ctx.scrollbar_dragging && !ctx.scrollbar.dragging);
    assert(!ctx.window_lifecycle.pointer_down && ctx.window_lifecycle.release_blocked);
    assert(ctx.last_click_ticks == 0 && ctx.last_clicked_slot == -1);
    assert(!ctx.workspace_authoring.key_c_down && !ctx.workspace_authoring.entry_chord_armed_key);
    assert(ctx.workspace_authoring.active && ctx.workspace_authoring.font_theme_pending_changes);
    assert(ctx.workspace_authoring.baseline_text_zoom_step == 3);
    assert(ctx.editing_input_root && !memcmp(&ctx.input_root_input, &root, sizeof root));
    assert(!memcmp(&ctx.rename_input, &rename, sizeof rename));
    assert(!memcmp(&cfg, &saved_cfg, sizeof cfg));
    assert(!memcmp(&ctx.settings_shell, &settings, sizeof settings));
    SDL_zero(e); e.type = SDL_MOUSEMOTION; e.motion.windowID = SDL_GetWindowID(window);
    e.motion.state = SDL_BUTTON_LMASK; assert(menu_window_lifecycle_filter(&ctx, &e));
    SDL_zero(e); e.type = SDL_MOUSEBUTTONUP; e.button.windowID = SDL_GetWindowID(window);
    e.button.button = SDL_BUTTON_LEFT; assert(menu_window_lifecycle_filter(&ctx, &e));
    e.type = SDL_MOUSEBUTTONDOWN; assert(!menu_window_lifecycle_filter(&ctx, &e));
    e.type = SDL_MOUSEBUTTONUP; assert(!menu_window_lifecycle_filter(&ctx, &e));
    SDL_SetWindowSize(window, 192, 144);
    assert(menu_window_lifecycle_refresh(&ctx) & KIT_UI_WINDOW_GEOMETRY);
    assert(ctx.window_lifecycle.observed.logical_width == 192);
    assert(ctx.window_lifecycle.geometry_dirty);
    assert(ctx.window_lifecycle.release_blocked);
    e.type = SDL_MOUSEBUTTONDOWN; assert(menu_window_lifecycle_filter(&ctx, &e));
    ctx.window_lifecycle.geometry_dirty = false; /* new host rectangles committed */
    assert(!menu_window_lifecycle_filter(&ctx, &e));
    SDL_HideWindow(window);
    assert(menu_window_lifecycle_refresh(&ctx) & KIT_UI_WINDOW_VISIBILITY);
    assert(!ctx.window_lifecycle.observed.presentable);
    e.type = SDL_MOUSEBUTTONDOWN; assert(menu_window_lifecycle_filter(&ctx, &e));
    SDL_zero(e); e.type = SDL_QUIT; assert(!menu_window_lifecycle_filter(&ctx, &e));
    SDL_ShowWindow(window); assert(menu_window_lifecycle_refresh(&ctx) & KIT_UI_WINDOW_VISIBILITY);
    assert(ctx.window_lifecycle.observed.presentable);
    SDL_zero(e); e.type = SDL_MOUSEBUTTONDOWN; e.button.windowID = SDL_GetWindowID(window)+100;
    e.button.button = SDL_BUTTON_LEFT; assert(menu_window_lifecycle_filter(&ctx, &e));
    int x, y;
    assert(kit_ui_window_map_point_sdl(&ctx.window_lifecycle.observed, 384, 288, -5, 10, &x, &y));
    assert(x == -10 && y == 20);
    assert(!memcmp(&ctx.input_root_input, &root, sizeof root));
    SDL_DestroyWindow(window); SDL_Quit();
    puts("menu lifecycle: shared geometry/visibility, stale release/drag cancellation, fresh press, foreign window, draft preservation and mapping passed");
    return 0;
}
