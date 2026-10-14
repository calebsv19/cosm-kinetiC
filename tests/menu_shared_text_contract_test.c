#define _DARWIN_C_SOURCE
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "app/menu/menu_types.h"
#include "app/menu/menu_state.h"
#include "app/menu/menu_input.h"
#include "app/menu/menu_window_lifecycle.h"
#include "app/menu/menu_settings_draft.h"
#include "app/data_paths.h"
#include "config/config_loader.h"

static void key(SceneMenuInteraction *c, SDL_Keycode k, Uint16 mod, int repeat) {
    SDL_Event e; SDL_zero(e); e.type=SDL_KEYDOWN; e.key.keysym.sym=k;
    e.key.keysym.mod=mod; e.key.repeat=repeat;
    assert(menu_shared_text_event(c,&e));
}
static void input(SceneMenuInteraction *c, const char *s) {
    SDL_Event e;SDL_zero(e);e.type=SDL_TEXTINPUT;
    snprintf(e.text.text,sizeof(e.text.text),"%s",s);
    assert(menu_shared_text_event(c,&e));
}
static void compose(SceneMenuInteraction *c, const char *s) {
    SDL_Event e;SDL_zero(e);e.type=SDL_TEXTEDITING;
    snprintf(e.edit.text,sizeof(e.edit.text),"%s",s);e.edit.length=1;
    assert(menu_shared_text_event(c,&e));
}
static void replace(SceneMenuInteraction *c, const char *s) {
    key(c,SDLK_a,KMOD_GUI,0);input(c,s);
}
static CoreResult monospace(void *unused,const char *s,float *w) {
    (void)unused; *w=0;for(size_t i=0;s[i];++i) if(((unsigned char)s[i]&0xc0)!=0x80)*w+=8;
    return core_result_ok();
}
int main(void) {
    char root[]="/tmp/physics-menu-text-contract-XXXXXX";
    assert(mkdtemp(root) && chdir(root)==0);
    printf("fixture_root=%s\n",root);fflush(stdout);
    SDL_setenv("SDL_VIDEODRIVER","dummy",1);assert(SDL_Init(SDL_INIT_VIDEO)==0);
    assert(physics_sim_ensure_runtime_dirs());
    AppConfig cfg=app_config_default(); SceneMenuSelection selection={0};
    SceneMenuInteraction c={.cfg=&cfg,.selection=&selection};
    menu_settings_shell_init(&c.settings_shell,&cfg,&selection,NULL,c.active_mode,cfg.space_mode);
    int frames=c.settings_shell.draft.headless_frame_count;
    menu_begin_headless_frames_edit(&c);replace(&c,"123");
    key(&c,SDLK_RETURN,KMOD_NONE,1);assert(c.editing_headless_frames);
    compose(&c,"é");key(&c,SDLK_RETURN,KMOD_NONE,0);
    assert(c.editing_headless_frames && c.settings_shell.draft.headless_frame_count==frames);
    key(&c,SDLK_ESCAPE,KMOD_NONE,0);assert(c.editing_headless_frames && !c.shared_text.edit.composition[0]);
    key(&c,SDLK_RETURN,KMOD_NONE,0);assert(!c.editing_headless_frames);
    assert(c.settings_shell.draft.headless_frame_count==123 && cfg.headless_frame_count==frames);
    assert(access(physics_sim_runtime_config_path(),F_OK)!=0);
    menu_begin_headless_frames_edit(&c);replace(&c,"456");key(&c,SDLK_ESCAPE,KMOD_NONE,0);
    assert(c.settings_shell.draft.headless_frame_count==123);
    menu_begin_viscosity_edit(&c);replace(&c,"nan");key(&c,SDLK_RETURN,0,0);
    assert(c.settings_shell.draft.velocity_damping==cfg.velocity_damping);
    menu_begin_inflow_edit(&c);replace(&c,"12.5");key(&c,SDLK_RETURN,0,0);
    assert(c.settings_shell.draft.tunnel_inflow_speed==12.5f);
    char original[sizeof(cfg.input_root)];snprintf(original,sizeof(original),"%s",cfg.input_root);
    menu_begin_input_root_edit(&c);replace(&c,"Aé水Z");
    key(&c,SDLK_LEFT,0,0);key(&c,SDLK_BACKSPACE,0,0);
    assert(strcmp(c.input_root_input.buffer,"AéZ")==0);
    key(&c,SDLK_HOME,0,0);key(&c,SDLK_RIGHT,KMOD_SHIFT,0);input(&c,"B");
    assert(strcmp(c.input_root_input.buffer,"BéZ")==0);
    key(&c,SDLK_a,KMOD_GUI,0);key(&c,SDLK_c,KMOD_GUI,0);
    char *clip=SDL_GetClipboardText();assert(clip && strcmp(clip,"BéZ")==0);SDL_free(clip);
    key(&c,SDLK_x,KMOD_GUI,0);assert(!c.input_root_input.buffer[0]);
    key(&c,SDLK_v,KMOD_GUI,0);assert(strcmp(c.input_root_input.buffer,"BéZ")==0);
    compose(&c,"水");menu_window_lifecycle_invalidate(&c);
    assert(c.input_root_input.active && strcmp(c.input_root_input.buffer,"BéZ")==0);
    assert(!c.shared_text.edit.composition[0] && !c.shared_text.dragging);
    key(&c,SDLK_ESCAPE,0,0);assert(strcmp(cfg.input_root,original)==0);
    assert(access(physics_sim_runtime_config_path(),F_OK)!=0);
    menu_begin_output_root_edit(&c);replace(&c,"  text-output");key(&c,SDLK_RETURN,0,0);
    assert(strcmp(cfg.headless_output_dir,"text-output")==0);
    AppConfig loaded;ConfigLoadOptions opts={.path=physics_sim_runtime_config_path()};
    assert(config_loader_load(&loaded,&opts) && strcmp(loaded.headless_output_dir,"text-output")==0);
    c.active_mode=SIM_MODE_ATMOSPHERIC;cfg.space_mode=SPACE_MODE_3D;
    menu_begin_warm_start_edit(&c);replace(&c,"snapshot.bin");key(&c,SDLK_ESCAPE,0,0);
    assert(!cfg.atmospheric_warm_start_path[0]);
    menu_begin_warm_start_edit(&c);replace(&c,"snapshot.bin");key(&c,SDLK_RETURN,0,0);
    assert(config_loader_load(&loaded,&opts));
    assert(strcmp(loaded.atmospheric_warm_start_path,"snapshot.bin")==0);
    menu_begin_input_root_edit(&c);replace(&c,"  inputs");key(&c,SDLK_RETURN,0,0);
    assert(strcmp(cfg.input_root,"inputs")==0);
    assert(config_loader_load(&loaded,&opts) && strcmp(loaded.input_root,"inputs")==0);
    /* Capacity rejects an entire oversized edit and keeps valid scalar boundaries. */
    char long_text[100];memset(long_text,'x',sizeof(long_text)-1);long_text[99]=0;
    c.editing_input_root=true;
    menu_shared_text_begin(&c,&c.input_root_input,long_text,63);
    assert(strlen(c.input_root_input.buffer)==63);
    input(&c,"é");assert(strlen(c.input_root_input.buffer)==63);
    long_text[62]=(char)0xc3;long_text[63]=(char)0xa9;long_text[64]=0;
    menu_shared_text_begin(&c,&c.input_root_input,long_text,63);
    assert(strlen(c.input_root_input.buffer)==62);
    key(&c,SDLK_HOME,0,0);
    KitUiTextPresentationOptions options={.viewport={10,10,24,16},.line_height=16,.caret_width=1,.active=1,.reveal_caret=1};
    assert(kit_ui_text_presentation_build(&c.shared_text.view,&c.shared_text.edit,&options,monospace,NULL).code==CORE_OK);
    assert(c.shared_text.view.caret.x>=10 && c.shared_text.view.caret.x<=34);
    key(&c,SDLK_END,0,0);
    assert(kit_ui_text_presentation_build(&c.shared_text.view,&c.shared_text.edit,&options,monospace,NULL).code==CORE_OK);
    assert(c.shared_text.view.scroll_x>0 && c.shared_text.view.caret.x<=34);
    c.shared_text.view_ready=true;
    c.shared_text.hit_clip=(SDL_Rect){20,10,4,16};
    SDL_Event outside;SDL_zero(outside);outside.type=SDL_MOUSEBUTTONDOWN;
    outside.button.button=SDL_BUTTON_LEFT;outside.button.x=12;outside.button.y=12;
    assert(!menu_shared_text_event(&c,&outside) && !c.shared_text.dragging);
    menu_finish_input_root_edit(&c,false);menu_shared_text_sync(&c);
    assert(!c.shared_text.owner);
    CustomPresetLibrary library; preset_library_init(&library); c.library=&library;
    assert(preset_library_add_slot(&library,"initial",NULL)!=NULL);
    menu_begin_rename(&c,0);replace(&c,"é水 name");key(&c,SDLK_RETURN,0,0);
    assert(strcmp(preset_library_get_slot(&library,0)->name,"é水 name")==0);
    menu_begin_rename(&c,0);replace(&c,"other");key(&c,SDLK_ESCAPE,0,0);
    assert(strcmp(preset_library_get_slot(&library,0)->name,"é水 name")==0);
    menu_begin_rename(&c,0);c.shared_text.view_ready=true;
    SDL_Event wheel;SDL_zero(wheel);wheel.type=SDL_MOUSEWHEEL;wheel.wheel.y=1;
    assert(!menu_shared_text_event(&c,&wheel));
    assert(c.rename_input.active && !c.shared_text.view_ready && !c.shared_text.native_session);
    menu_finish_rename(&c,false);
    menu_shared_text_cancel_transient(&c);preset_library_shutdown(&library);SDL_Quit();
    puts("menu shared text owner contracts passed");return 0;
}
