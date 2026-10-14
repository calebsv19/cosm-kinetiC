#define _DARWIN_C_SOURCE
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "app/menu/menu_types.h"
#include "app/menu/menu_input.h"
#include "app/menu/menu_state.h"
#include "app/menu/menu_render.h"
#include "app/menu/menu_shared_controls.h"
#include "app/menu/menu_window_lifecycle.h"
#include "app/menu/menu_settings_draft.h"
#include "app/menu/menu_settings_layout.h"
#include "app/scene_menu_layout_helpers.h"
#include "app/data_paths.h"
#include "config/config_loader.h"

static void pointer(SceneMenuInteraction *c, Uint32 type, int x, int y) {
    SDL_Event e; SDL_zero(e); e.type=type; e.button.button=SDL_BUTTON_LEFT;
    e.button.x=x;e.button.y=y; (void)menu_shared_controls_event(c,&e);
}
static void click(SceneMenuInteraction *c, SDL_Rect r) {
    pointer(c,SDL_MOUSEBUTTONDOWN,r.x+r.w/2,r.y+r.h/2);
    pointer(c,SDL_MOUSEBUTTONUP,r.x+r.w/2,r.y+r.h/2);
}
static SDL_Rect solver_inc(SceneMenuInteraction *c) {
    MenuSettingsFieldLayout rows[MENU_SETTINGS_FIELD_COUNT];
    size_t n=menu_settings_layout_build_field_layouts(c,rows,MENU_SETTINGS_FIELD_COUNT);
    for(size_t i=0;i<n;i++)if(rows[i].field==MENU_SETTINGS_FIELD_SOLVER_ITERATIONS)return rows[i].inc_rect;
    assert(!"solver field absent");return (SDL_Rect){0};
}
int main(void) {
    char root[]="/tmp/physics-menu-controls-contract-XXXXXX";
    assert(mkdtemp(root) && chdir(root)==0);
    printf("fixture_root=%s\n",root);fflush(stdout);
    SDL_setenv("SDL_VIDEODRIVER","dummy",1); assert(SDL_Init(SDL_INIT_VIDEO)==0);
    assert(physics_sim_ensure_runtime_dirs());
    AppConfig cfg=app_config_default();SceneMenuSelection selection={0};
    selection.custom_slot_index=-1;selection.retained_scene_index=-1;
    bool running=true,start=false;
    SceneMenuInteraction c={.cfg=&cfg,.selection=&selection,.running=&running,.start_requested=&start};
    menu_settings_shell_init(&c.settings_shell,&cfg,&selection,NULL,c.active_mode,cfg.space_mode);
    scene_menu_update_dynamic_layout(&c,1360,860);
    menu_shared_controls_sync(&c);assert(c.shared_controls.count>20);
    SDL_Rect plus=solver_inc(&c);int original=cfg.fluid_solver_iterations;
    pointer(&c,SDL_MOUSEBUTTONDOWN,plus.x+5,plus.y+5);
    assert(c.settings_shell.draft.fluid_solver_iterations==original);
    assert(menu_shared_controls_state(&c,&plus,false).pressed);
    pointer(&c,SDL_MOUSEBUTTONUP,0,0);
    assert(c.settings_shell.draft.fluid_solver_iterations==original);
    pointer(&c,SDL_MOUSEBUTTONDOWN,0,0);
    pointer(&c,SDL_MOUSEBUTTONUP,plus.x+5,plus.y+5);
    assert(c.settings_shell.draft.fluid_solver_iterations==original);
    InputPointerState legacy={.x=plus.x+5,.y=plus.y+5,.button=SDL_BUTTON_LEFT};
    menu_pointer_up(&c,&legacy);assert(c.settings_shell.draft.fluid_solver_iterations==original);
    click(&c,plus);assert(c.settings_shell.draft.fluid_solver_iterations==original+1);
    assert(cfg.fluid_solver_iterations==original);
    click(&c,c.settings_reset_button.rect);assert(c.settings_shell.draft.fluid_solver_iterations==original);
    click(&c,plus);click(&c,c.settings_apply_button.rect);
    assert(cfg.fluid_solver_iterations==original+1);
    assert(access(physics_sim_runtime_config_path(),F_OK)!=0); /* Apply is not Save. */
    click(&c,plus);click(&c,c.settings_save_button.rect);
    assert(cfg.fluid_solver_iterations==original+2);
    AppConfig loaded;ConfigLoadOptions opts={.path=physics_sim_runtime_config_path()};
    assert(config_loader_load(&loaded,&opts));assert(loaded.fluid_solver_iterations==original+2);
    click(&c,plus);click(&c,c.settings_apply_button.rect);assert(cfg.fluid_solver_iterations==original+3);
    click(&c,c.settings_restore_saved_button.rect);assert(cfg.fluid_solver_iterations==original+2);
    click(&c,c.settings_defaults_button.rect);assert(cfg.fluid_solver_iterations==original+2);
    assert(c.settings_shell.draft.fluid_solver_iterations==original);
    pointer(&c,SDL_MOUSEBUTTONDOWN,plus.x+5,plus.y+5);
    menu_window_lifecycle_invalidate(&c);
    pointer(&c,SDL_MOUSEBUTTONUP,plus.x+5,plus.y+5);
    assert(c.settings_shell.draft.fluid_solver_iterations==original);
    c.headless_running=true;menu_shared_controls_sync(&c);click(&c,plus);
    assert(c.settings_shell.draft.fluid_solver_iterations==original);
    c.headless_running=false;menu_shared_controls_sync(&c);
    pointer(&c,SDL_MOUSEBUTTONDOWN,plus.x+5,plus.y+5);
    c.selection->custom_slot_index=2;
    pointer(&c,SDL_MOUSEBUTTONUP,plus.x+5,plus.y+5);
    assert(c.settings_shell.draft.fluid_solver_iterations==original);
    pointer(&c,SDL_MOUSEBUTTONDOWN,plus.x+5,plus.y+5);
    c.editing_input_root=true;c.input_root_input.active=true;strcpy(c.input_root_input.buffer,"unsaved path");
    pointer(&c,SDL_MOUSEBUTTONUP,plus.x+5,plus.y+5);
    assert(c.settings_shell.draft.fluid_solver_iterations==original);
    assert(!strcmp(c.input_root_input.buffer,"unsaved path"));
    c.editing_input_root=false;c.input_root_input.active=false;menu_shared_controls_sync(&c);
    SDL_Event key;SDL_zero(key);key.type=SDL_KEYDOWN;key.key.keysym.sym=SDLK_TAB;
    assert(menu_shared_controls_event(&c,&key));assert(c.shared_controls.interaction.focused_id);
    for (uint32_t i=0;i<c.shared_controls.count && !menu_shared_controls_state(&c,&plus,false).focused;i++)
        (void)menu_shared_controls_event(&c,&key);
    assert(menu_shared_controls_state(&c,&plus,false).focused);
    key.key.keysym.sym=SDLK_RETURN;key.type=SDL_KEYDOWN;
    (void)menu_shared_controls_event(&c,&key);
    assert(c.settings_shell.draft.fluid_solver_iterations==original);
    key.key.repeat=1;(void)menu_shared_controls_event(&c,&key);
    assert(c.settings_shell.draft.fluid_solver_iterations==original);
    key.key.repeat=0;key.type=SDL_KEYUP;(void)menu_shared_controls_event(&c,&key);
    assert(c.settings_shell.draft.fluid_solver_iterations==original+1);
    assert(cfg.fluid_solver_iterations==original+2);
    menu_shared_controls_cancel(&c);
    assert(config_loader_load(&loaded,&opts) && loaded.fluid_solver_iterations==original+2);
    CustomPresetLibrary library;
    preset_library_init(&library);c.library=&library;c.selection->custom_slot_index=-1;
    scene_menu_update_dynamic_layout(&c,1360,860);menu_update_scrollbar(&c);
    SDL_Rect add_rect;
    assert(menu_preset_row_rect(&c,0,true,&add_rect));
    click(&c,add_rect);assert(preset_library_count(&library)==1);
    scene_menu_update_dynamic_layout(&c,1360,860);menu_update_scrollbar(&c);
    SDL_Rect row_rect;assert(menu_preset_row_rect(&c,0,false,&row_rect));
    SDL_Rect delete_rect=menu_preset_delete_button_rect(&row_rect);
    pointer(&c,SDL_MOUSEBUTTONDOWN,delete_rect.x+5,delete_rect.y+5);
    pointer(&c,SDL_MOUSEBUTTONUP,0,0);assert(preset_library_count(&library)==1);
    click(&c,delete_rect);assert(preset_library_count(&library)==0);
    preset_library_shutdown(&library);
    SDL_Quit();
    puts("menu shared controls: matched release, cross release, legacy fallback refusal, disabled/takeover/window cancel, actual nudge/apply/save/revert/restore/defaults and disk save behavior passed");
    return 0;
}
