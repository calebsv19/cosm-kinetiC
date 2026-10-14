#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "app/editor/scene_editor_internal.h"
#include "app/editor/scene_editor_model.h"
#include "app/editor/scene_editor_input.h"

static void key(SceneEditorState *s, SDL_Keycode k, Uint16 mod, int repeat) {
    SDL_Event e; SDL_zero(e); e.type = SDL_KEYDOWN;
    e.key.keysym.sym = k; e.key.keysym.mod = mod; e.key.repeat = repeat;
    assert(scene_editor_shared_text_event(s, &e));
}
static void text(SceneEditorState *s, const char *value, bool preedit) {
    SDL_Event e; SDL_zero(e); e.type = preedit ? SDL_TEXTEDITING : SDL_TEXTINPUT;
    if (preedit) { snprintf(e.edit.text, sizeof(e.edit.text), "%s", value); e.edit.length = 1; }
    else snprintf(e.text.text, sizeof(e.text.text), "%s", value);
    assert(scene_editor_shared_text_event(s, &e));
}
static void replace(SceneEditorState *s, const char *value) {
    key(s, SDLK_a, KMOD_GUI, 0); text(s, value, false);
}
static void controls(SceneEditorState *s, EditorButton *button, bool enabled) {
    scene_editor_shared_controls_begin(s);
    SDL_Rect r = button->rect;
    assert(kit_ui_surface_register(&s->shared_ui.controls,
        (KitUiSurfaceKey){1, (uint64_t)((char *)button-(char *)s)},
        (KitRenderRect){r.x,r.y,r.w,r.h}, NULL, enabled, NULL).code == CORE_OK);
    scene_editor_shared_controls_end(s);
}
static bool pointer(SceneEditorState *s, Uint32 type, int x, int y) {
    SDL_Event e; SDL_zero(e); e.type = type;
    e.button.button = SDL_BUTTON_LEFT; e.button.x = x; e.button.y = y;
    return scene_editor_shared_filter(s, &e);
}
int main(void) {
    SDL_setenv("SDL_VIDEODRIVER", "dummy", 1); assert(SDL_Init(SDL_INIT_VIDEO)==0);
    SceneEditorState *s = calloc(1, sizeof(*s)); assert(s);
    s->cfg = app_config_default(); s->working.domain_width = 10; s->working.domain_height = 8;
    s->working.dimension_mode = SCENE_DIMENSION_MODE_2D;
    s->selected_object = s->selected_emitter = s->selected_row = -1;
    s->layout_win_w = 1000; s->layout_win_h = 800;
    physics_sim_editor_session_init(&s->session, &s->working, NULL);
    s->name_edit_ptr = s->local_name; s->name_edit_capacity = sizeof(s->local_name);
    strcpy(s->local_name, "Original");
    editor_begin_name_edit(s); replace(s, "Aé水Z");
    key(s, SDLK_LEFT, 0, 0); key(s, SDLK_BACKSPACE, 0, 0);
    assert(strcmp(s->name_input.buffer, "AéZ")==0 && strcmp(s->local_name, "Original")==0);
    key(s, SDLK_HOME, 0, 0); key(s, SDLK_RIGHT, KMOD_SHIFT, 0); text(s, "B", false);
    assert(strcmp(s->name_input.buffer, "BéZ")==0);
    key(s, SDLK_a, KMOD_GUI, 0); key(s, SDLK_c, KMOD_GUI, 0);
    key(s, SDLK_x, KMOD_GUI, 0); assert(!s->name_input.buffer[0]);
    key(s, SDLK_v, KMOD_GUI, 0); assert(strcmp(s->name_input.buffer, "BéZ")==0);
    text(s, "水", true); key(s, SDLK_RETURN, 0, 0);
    assert(s->renaming_name && s->shared_ui.edit.composition[0]);
    key(s, SDLK_ESCAPE, 0, 0); assert(s->renaming_name && !s->shared_ui.edit.composition[0]);
    key(s, SDLK_RETURN, 0, 1); assert(s->renaming_name);
    key(s, SDLK_RETURN, 0, 0); assert(!s->renaming_name && strcmp(s->local_name, "BéZ")==0);
    editor_begin_name_edit(s); replace(s, "discard"); key(s, SDLK_ESCAPE, 0, 0);
    assert(strcmp(s->local_name, "BéZ")==0);
    editor_begin_dimension_edit(s, SCENE_EDITOR_DIMENSION_WIDTH); replace(s, "12.5");
    text(s, "é", true); s->dragging_object = s->dragging_import_body = true;
    scene_editor_shared_invalidate(s);
    assert(s->editing_width && strcmp(s->width_input.buffer, "12.5")==0);
    assert(!s->shared_ui.edit.composition[0] && !s->dragging_object && !s->dragging_import_body);
    assert(s->working.domain_width==10);
    key(s, SDLK_RETURN, 0, 0); assert(s->working.domain_width==12.5f);
    editor_begin_dimension_edit(s, SCENE_EDITOR_DIMENSION_HEIGHT); replace(s, "32");
    key(s, SDLK_ESCAPE, 0, 0); assert(s->working.domain_height==8);
    /* Native owner entry must not flush text queued behind its first key event. */
    editor_begin_dimension_edit(s, SCENE_EDITOR_DIMENSION_WIDTH);
    SDL_Event queued; SDL_zero(queued); queued.type=SDL_KEYDOWN;
    queued.key.keysym.sym=SDLK_a; queued.key.keysym.mod=KMOD_GUI;
    assert(SDL_PushEvent(&queued)==1);
    SDL_zero(queued); queued.type=SDL_TEXTINPUT; strcpy(queued.text.text, "17.5");
    assert(SDL_PushEvent(&queued)==1);
    int received=0;
    while (SDL_PollEvent(&queued)) {
        if (queued.type!=SDL_KEYDOWN && queued.type!=SDL_TEXTINPUT) continue;
        assert(scene_editor_shared_text_event(s, &queued)); ++received;
    }
    assert(received==2 && strcmp(s->width_input.buffer, "17.5")==0);
    key(s, SDLK_ESCAPE, 0, 0); assert(s->working.domain_width==12.5f);
    s->selected_emitter = 0; s->working.emitter_count = 1;
    s->working.emitters[0].radius = .2f;
    s->radius_field.target = FIELD_RADIUS;
    begin_field_edit(s, &s->radius_field);
    text(s, "nan", false); assert(strcmp(s->radius_field.buffer, "0.200")==0);
    replace(s, "0.45");
    assert(fabsf(s->working.emitters[0].radius-.2f)<1e-6f);
    key(s, SDLK_RETURN, 0, 0); assert(fabsf(s->working.emitters[0].radius-.45f)<1e-6f);
    begin_field_edit(s, &s->radius_field); replace(s, "0.1"); key(s, SDLK_ESCAPE, 0, 0);
    assert(!s->active_field && s->selected_emitter==0 && fabsf(s->working.emitters[0].radius-.45f)<1e-6f);
    s->shared_ui.window.observed.presentable = 1;
    s->shared_ui.window.observed.flags = SDL_WINDOW_INPUT_FOCUS;
    s->shared_ui.window.release_blocked = false;
    s->selected_emitter = -1;
    s->btn_add_source = (EditorButton){{10,10,80,30}, "Source", true};
    controls(s, &s->btn_add_source, true);
    size_t n = s->working.emitter_count;
    assert(pointer(s, SDL_MOUSEBUTTONDOWN, 40, 20)); assert(s->working.emitter_count==n);
    assert(pointer(s, SDL_MOUSEBUTTONUP, 40, 20)); assert(s->working.emitter_count==n+1);
    s->selected_emitter = -1; controls(s, &s->btn_add_source, true); n=s->working.emitter_count;
    (void)pointer(s, SDL_MOUSEBUTTONDOWN, 400, 200); (void)pointer(s, SDL_MOUSEBUTTONUP, 40, 20);
    assert(s->working.emitter_count==n);
    controls(s, &s->btn_add_source, true);
    assert(pointer(s, SDL_MOUSEBUTTONDOWN, 40, 20)); scene_editor_shared_invalidate(s);
    assert(pointer(s, SDL_MOUSEBUTTONUP, 40, 20)); assert(s->working.emitter_count==n);
    controls(s, &s->btn_add_source, false);
    (void)pointer(s, SDL_MOUSEBUTTONDOWN, 40, 20); (void)pointer(s, SDL_MOUSEBUTTONUP, 40, 20);
    assert(s->working.emitter_count==n);
    controls(s, &s->btn_add_source, true); n=s->working.emitter_count;
    SDL_Event event; SDL_zero(event); event.type=SDL_KEYDOWN; event.key.keysym.sym=SDLK_TAB;
    assert(scene_editor_shared_filter(s, &event));
    event.key.keysym.sym=SDLK_SPACE; assert(scene_editor_shared_filter(s, &event));
    event.key.repeat=1; assert(scene_editor_shared_filter(s, &event));
    assert(s->working.emitter_count==n);
    event.type=SDL_KEYUP; event.key.repeat=0; assert(scene_editor_shared_filter(s, &event));
    assert(s->working.emitter_count==n+1);
    s->selected_emitter=-1; controls(s, &s->btn_add_source, true); n=s->working.emitter_count;
    assert(pointer(s, SDL_MOUSEBUTTONDOWN, 40, 20)); s->selected_row=3;
    (void)pointer(s, SDL_MOUSEBUTTONUP, 40, 20); assert(s->working.emitter_count==n);
    s->selected_row=-1;
    s->btn_boundary = (EditorButton){{10,10,80,30}, "Air Flow Mode", true};
    s->btn_import_delete = (EditorButton){{10,10,80,30}, "undrawn Delete", true};
    s->working.import_shape_count=1; s->selected_row=0;
    controls(s, &s->btn_boundary, true);
    (void)pointer(s, SDL_MOUSEBUTTONDOWN, 40, 20); (void)pointer(s, SDL_MOUSEBUTTONUP, 40, 20);
    assert(s->boundary_mode && s->working.import_shape_count==1);
    s->btn_save = (EditorButton){{10,60,80,30}, "Save", true}; s->running=true;
    controls(s, &s->btn_save, true);
    (void)pointer(s, SDL_MOUSEBUTTONDOWN, 40, 70); assert(!s->applied);
    (void)pointer(s, SDL_MOUSEBUTTONUP, 40, 70); assert(s->applied && !s->running);
    s->btn_menu = (EditorButton){{10,100,80,30}, "Menu", true}; s->running=true;
    controls(s, &s->btn_menu, true);
    (void)pointer(s, SDL_MOUSEBUTTONDOWN, 40, 110); (void)pointer(s, SDL_MOUSEBUTTONUP, 40, 110);
    assert(!s->applied && !s->running);
    s->shared_ui.window.observed.presentable=0;
    assert(pointer(s, SDL_MOUSEBUTTONDOWN, 40, 20));
    free(s); SDL_Quit(); puts("scene_editor_shared_ui_contract_test: ok"); return 0;
}
