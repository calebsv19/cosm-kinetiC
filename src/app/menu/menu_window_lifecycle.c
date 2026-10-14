#include "app/menu/menu_window_lifecycle.h"
#include <string.h>
#include "app/menu/menu_types.h"

void menu_window_lifecycle_invalidate(SceneMenuInteraction *ctx) {
    if (!ctx) return;
    menu_shared_controls_cancel(ctx);
    menu_shared_text_cancel_transient(ctx);
    ctx->window_lifecycle.pointer_down = false;
    ctx->window_lifecycle.release_blocked = true;
    ctx->suppress_pointer_until_up = false;
    ctx->scrollbar_dragging = false;
    scrollbar_handle_pointer_up(&ctx->scrollbar);
    ctx->last_click_ticks = 0;
    ctx->last_clicked_slot = -1;
    ctx->last_headless_click_ticks = 0;
    ctx->last_viscosity_click_ticks = 0;
    ctx->last_inflow_click_ticks = 0;
    ctx->hover_slot = -1;
    ctx->hover_retained_scene_index = -1;
    ctx->hover_add_entry = false;
    ctx->workspace_authoring.key_c_down = 0;
    ctx->workspace_authoring.key_v_down = 0;
    ctx->workspace_authoring.entry_chord_armed_key = 0;
    ctx->workspace_authoring.last_pointer_ready = 0;
    int count = 0;
    const Uint8 *keys = SDL_GetKeyboardState(&count);
    memset(ctx->window_lifecycle.blocked_keys, 0,
           sizeof(ctx->window_lifecycle.blocked_keys));
    if (keys && count > 0) {
        if (count > SDL_NUM_SCANCODES) count = SDL_NUM_SCANCODES;
        memcpy(ctx->window_lifecycle.blocked_keys, keys, (size_t)count);
    }
}

uint32_t menu_window_lifecycle_refresh(SceneMenuInteraction *ctx) {
    if (!ctx || !ctx->window) return 0;
    uint32_t changes = 0;
    if (kit_ui_window_refresh_sdl(&ctx->window_lifecycle.observed,
                                  ctx->window, &changes).code != CORE_OK) {
        ctx->window_lifecycle.observed.presentable = 0;
        menu_window_lifecycle_invalidate(ctx);
        return KIT_UI_WINDOW_VISIBILITY;
    }
    if (changes) menu_window_lifecycle_invalidate(ctx);
    if (changes & (KIT_UI_WINDOW_GEOMETRY | KIT_UI_WINDOW_MODE |
                   KIT_UI_WINDOW_DISPLAY | KIT_UI_WINDOW_VISIBILITY))
        ctx->window_lifecycle.geometry_dirty = true;
    return changes;
}

static Uint32 menu_event_window_id(const SDL_Event *e) {
    switch (e->type) {
    case SDL_WINDOWEVENT: return e->window.windowID;
    case SDL_MOUSEBUTTONDOWN: case SDL_MOUSEBUTTONUP: return e->button.windowID;
    case SDL_MOUSEMOTION: return e->motion.windowID;
    case SDL_MOUSEWHEEL: return e->wheel.windowID;
    case SDL_KEYDOWN: case SDL_KEYUP: return e->key.windowID;
    case SDL_TEXTINPUT: return e->text.windowID;
    case SDL_TEXTEDITING: return e->edit.windowID;
    default: return 0;
    }
}

bool menu_window_lifecycle_filter(SceneMenuInteraction *ctx, const SDL_Event *e) {
    if (!ctx || !e) return false;
    Uint32 id = menu_event_window_id(e);
    if (id && ctx->window && id != SDL_GetWindowID(ctx->window)) return true;
    (void)menu_window_lifecycle_refresh(ctx);
    if (kit_ui_window_event_invalidates_sdl(e))
        menu_window_lifecycle_invalidate(ctx);
    /* Close/quit must still reach the host, including during suspension. */
    if (e->type == SDL_QUIT || e->type == SDL_WINDOWEVENT) return false;
    PhysicsSimMenuWindowLifecycle *state = &ctx->window_lifecycle;
    bool pointer = e->type == SDL_MOUSEBUTTONDOWN || e->type == SDL_MOUSEBUTTONUP ||
                   e->type == SDL_MOUSEMOTION || e->type == SDL_MOUSEWHEEL;
    bool key_text = e->type == SDL_KEYDOWN || e->type == SDL_KEYUP ||
                    e->type == SDL_TEXTINPUT || e->type == SDL_TEXTEDITING;
    if ((pointer || key_text) && !state->observed.presentable) return true;
    /* Drain geometry-changing events before new hit rectangles are committed. */
    if (pointer && state->geometry_dirty) return true;
    if (key_text && !(state->observed.flags & SDL_WINDOW_INPUT_FOCUS)) return true;
    if (e->type == SDL_KEYUP && e->key.keysym.scancode < SDL_NUM_SCANCODES &&
        e->key.keysym.scancode >= 0) {
        bool blocked = state->blocked_keys[e->key.keysym.scancode] != 0;
        state->blocked_keys[e->key.keysym.scancode] = 0;
        if (blocked) return true;
    }
    if (e->type == SDL_KEYDOWN && e->key.keysym.scancode < SDL_NUM_SCANCODES &&
        e->key.keysym.scancode >= 0 && state->blocked_keys[e->key.keysym.scancode])
        return true;
    if (e->type == SDL_MOUSEBUTTONDOWN && e->button.button == SDL_BUTTON_LEFT) {
        state->release_blocked = false;
        state->pointer_down = true;
    } else if (e->type == SDL_MOUSEBUTTONUP && e->button.button == SDL_BUTTON_LEFT) {
        state->pointer_down = false;
        if (state->release_blocked) {
            state->release_blocked = false;
            return true;
        }
    } else if (e->type == SDL_MOUSEMOTION && state->release_blocked &&
               (e->motion.state & SDL_BUTTON_LMASK)) {
        return true;
    }
    int fullscreen = kit_ui_window_fullscreen_key_sdl(ctx->window, e);
    if (fullscreen) {
        menu_window_lifecycle_invalidate(ctx);
        (void)menu_window_lifecycle_refresh(ctx);
        return true;
    }
    return false;
}
