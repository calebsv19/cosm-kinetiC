#include "app/editor/scene_editor_internal.h"
#include <string.h>
#include "app/editor/scene_editor_viewport.h"
#include "app/editor/scene_editor_pane_host.h"

void scene_editor_shared_invalidate(SceneEditorState *ctx) {
    if (!ctx) return;
    KitUiInteractionEvent cancel = {.type = KIT_UI_INTERACTION_CANCEL};
    KitUiInteractionResult result;
    (void)kit_ui_surface_route(&ctx->shared_ui.controls, &cancel, &result);
    ctx->shared_ui.controls.activation_count = 0;
    ctx->shared_ui.controls.count = 0;
    scene_editor_shared_text_reset(ctx);
    ctx->shared_ui.window.pointer_down = false;
    ctx->shared_ui.window.release_blocked = true;
    ctx->dragging = ctx->dragging_object = ctx->dragging_object_handle = false;
    ctx->handle_resize_started = false;
    ctx->dragging_import_new = ctx->dragging_import_body = ctx->dragging_import_handle = false;
    ctx->pointer_down_in_canvas = ctx->pointer_drag_started = false;
    ctx->last_name_click = ctx->last_width_click = ctx->last_height_click = ctx->last_depth_click = 0;
    ctx->last_canvas_click = ctx->last_import_click = 0;
    ctx->viewport.alt_modifier_down = false;
    scene_editor_viewport_end_navigation(&ctx->viewport);
    scene_editor_pane_host_end_splitter_drag(&ctx->pane_host);
    editor_list_view_pointer_up(&ctx->list_view);
    editor_list_view_pointer_up(&ctx->import_view);
    int count = 0;
    const Uint8 *keys = SDL_GetKeyboardState(&count);
    memset(ctx->shared_ui.window.blocked_keys, 0,
           sizeof(ctx->shared_ui.window.blocked_keys));
    if (keys && count > 0) {
        if (count > SDL_NUM_SCANCODES) count = SDL_NUM_SCANCODES;
        memcpy(ctx->shared_ui.window.blocked_keys, keys, (size_t)count);
    }
}

uint32_t scene_editor_shared_refresh(SceneEditorState *ctx) {
    if (!ctx || !ctx->window) return 0;
    uint32_t changes = 0;
    if (kit_ui_window_refresh_sdl(&ctx->shared_ui.window.observed,
                                  ctx->window, &changes).code != CORE_OK) {
        ctx->shared_ui.window.observed.presentable = 0;
        scene_editor_shared_invalidate(ctx);
        return KIT_UI_WINDOW_VISIBILITY;
    }
    if (changes) scene_editor_shared_invalidate(ctx);
    if (changes & (KIT_UI_WINDOW_GEOMETRY | KIT_UI_WINDOW_MODE |
                   KIT_UI_WINDOW_DISPLAY | KIT_UI_WINDOW_VISIBILITY))
        ctx->shared_ui.window.geometry_dirty = true;
    return changes;
}

static Uint32 editor_event_window_id(const SDL_Event *e) {
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

static bool editor_window_filter(SceneEditorState *ctx, const SDL_Event *e) {
    if (!ctx || !e) return false;
    Uint32 id = editor_event_window_id(e);
    if (id && ctx->window && id != SDL_GetWindowID(ctx->window)) return true;
    (void)scene_editor_shared_refresh(ctx);
    if (kit_ui_window_event_invalidates_sdl(e))
        scene_editor_shared_invalidate(ctx);
    /* Close/quit must still reach the host, including during suspension. */
    if (e->type == SDL_QUIT || e->type == SDL_WINDOWEVENT) return false;
    SceneEditorWindowLifecycle *state = &ctx->shared_ui.window;
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
        scene_editor_shared_invalidate(ctx);
        (void)scene_editor_shared_refresh(ctx);
        return true;
    }
    return false;
}

extern bool scene_editor_shared_controls_event(SceneEditorState *, const SDL_Event *);
bool scene_editor_shared_filter(void *user, const SDL_Event *event) {
    SceneEditorState *s = user;
    if (editor_window_filter(s, event)) return true;
    if (scene_editor_shared_text_event(s, event)) return true;
    return scene_editor_shared_controls_event(s, event);
}
