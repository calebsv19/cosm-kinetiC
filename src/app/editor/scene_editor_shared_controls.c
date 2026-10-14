#include "app/editor/scene_editor_internal.h"
#include "app/editor/scene_editor_input.h"
#include "kit_ui_interaction_sdl.h"

static uint32_t scope(const SceneEditorState *s) {
    uint32_t h = 2166136261u;
    const int values[] = {s->selected_object, s->selected_emitter, s->selected_row,
        s->selected_import_row, s->showing_import_picker, s->inspector_view,
        s->dirty, s->boundary_mode, s->working.emitter_count, s->working.import_shape_count,
        s->session.selection.retained_object_index, s->session.legacy_selection.import_index,
        s->renaming_name, s->editing_width, s->editing_height, s->editing_depth,
        s->active_field != NULL, s->layout_win_w, s->layout_win_h};
    for (size_t i = 0; i < sizeof(values)/sizeof(values[0]); ++i)
        h = (h ^ (uint32_t)values[i]) * 16777619u;
    return h ? h : 1;
}
void scene_editor_shared_controls_begin(SceneEditorState *s) {
    kit_ui_surface_begin(&s->shared_ui.controls, scope(s));
}
void scene_editor_shared_controls_end(SceneEditorState *s) {
    (void)kit_ui_surface_end(&s->shared_ui.controls);
}
KitUiButtonState scene_editor_shared_button(SceneEditorState *s, const void *owner,
    const SDL_Rect *rect, bool enabled) {
    KitRenderRect bounds = {rect->x, rect->y, rect->w, rect->h};
    SDL_Rect clip = {0, 0, s->layout_win_w, s->layout_win_h};
    SDL_Rect inherited;
    if (SDL_RenderIsClipEnabled(s->renderer)) {
        SDL_RenderGetClipRect(s->renderer, &inherited);
        SDL_IntersectRect(&clip, &inherited, &clip);
    }
    if (clip.w <= 0 || clip.h <= 0) return (KitUiButtonState){0};
    KitRenderRect visible = {clip.x, clip.y, clip.w, clip.h};
    KitUiInteractionControl control = {0};
    (void)kit_ui_surface_register(&s->shared_ui.controls,
        (KitUiSurfaceKey){1, (uint64_t)((const char *)owner - (const char *)s)},
        bounds, &visible, enabled, &control);
    return kit_ui_interaction_button_state(&s->shared_ui.controls.interaction, &control, false);
}
bool scene_editor_shared_controls_event(SceneEditorState *s, const SDL_Event *native) {
    KitUiSurface *surface = &s->shared_ui.controls;
    if (surface->scope != scope(s)) {
        kit_ui_surface_set_scope(surface, scope(s));
        /* A new owner needs a painted snapshot before it can receive input. */
        surface->count = 0;
    }
    KitUiInteractionEvent event;
    KitUiInteractionResult result;
    if (!kit_ui_interaction_event_from_sdl(native, &event)) return false;
    if (kit_ui_surface_route(surface, &event, &result).code != CORE_OK) return true;
    if (result.activated_id && kit_ui_surface_take_activation(surface, result.activated_id)) {
        for (uint32_t i = 0; i < surface->count; ++i) {
            if (surface->controls[i].id != result.activated_id) continue;
            KitRenderRect r = surface->controls[i].bounds;
            InputPointerState pointer = {.x = (int)(r.x+r.width/2),
                .y = (int)(r.y+r.height/2), .button = SDL_BUTTON_LEFT};
            s->shared_ui.dispatch_owner = (const char *)s + surface->keys[i].value;
            s->shared_ui.dispatch = true;
            editor_pointer_down(s, &pointer);
            s->shared_ui.dispatch = false;
            s->shared_ui.dispatch_owner = NULL;
            break;
        }
    }
    return result.consumed != 0;
}
