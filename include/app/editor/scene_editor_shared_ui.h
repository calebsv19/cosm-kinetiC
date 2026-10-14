#ifndef SCENE_EDITOR_SHARED_UI_H
#define SCENE_EDITOR_SHARED_UI_H
#include "kit_ui_window_sdl.h"
#include "kit_ui_surface.h"
#include "kit_ui_text_edit.h"
#include "kit_ui_text_presentation.h"
#include <SDL2/SDL_ttf.h>
struct SceneEditorState;
struct EditorButton;
typedef struct SceneEditorWindowLifecycle {
    KitUiWindowState observed;
    bool release_blocked, pointer_down, geometry_dirty;
    Uint8 blocked_keys[SDL_NUM_SCANCODES];
} SceneEditorWindowLifecycle;
typedef struct SceneEditorSharedUi {
    SceneEditorWindowLifecycle window;
    KitUiSurface controls;
    KitUiTextEdit edit;
    KitUiTextPresentation view;
    char *buffer;
    const void *dispatch_owner;
    TTF_Font *font;
    SDL_Rect hit_clip;
    bool view_ready, dragging, native_session, dispatch;
} SceneEditorSharedUi;
void scene_editor_shared_invalidate(struct SceneEditorState *);
uint32_t scene_editor_shared_refresh(struct SceneEditorState *);
bool scene_editor_shared_filter(void *, const SDL_Event *);
void scene_editor_shared_controls_begin(struct SceneEditorState *);
void scene_editor_shared_controls_end(struct SceneEditorState *);
KitUiButtonState scene_editor_shared_button(struct SceneEditorState *, const void *, const SDL_Rect *, bool);
void scene_editor_shared_text_sync(struct SceneEditorState *);
void scene_editor_shared_text_reset(struct SceneEditorState *);
bool scene_editor_shared_text_event(struct SceneEditorState *, const SDL_Event *);
void scene_editor_shared_text_draw(struct SceneEditorState *, TTF_Font *, const SDL_Rect *, char *, SDL_Color, SDL_Color, SDL_Color);
#endif
