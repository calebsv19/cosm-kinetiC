#define _DARWIN_C_SOURCE
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "app/menu/menu_types.h"
#include "app/menu/menu_state.h"
#include "app/menu/menu_input.h"
#include "app/menu/menu_settings_draft.h"
#include "app/menu/menu_window_lifecycle.h"
#include "app/menu/menu_text_presentation.h"
#include "app/data_paths.h"
#include "config/config_loader.h"

static CoreResult mono(void *user, const char *text, float *width) {
    (void)user; *width=0;
    for(size_t i=0;text[i];++i)if(((unsigned char)text[i]&0xc0)!=0x80)*width+=8;
    return core_result_ok();
}
static void check_utf8(char *text, size_t cap) {
    KitUiTextEdit edit={0};assert(kit_ui_text_bind(&edit,text,cap,0).code==CORE_OK);
}
int main(void) {
    char root[]="/tmp/physics-menu-containment-contract-XXXXXX";
    assert(mkdtemp(root) && chdir(root)==0);printf("fixture_root=%s\n",root);fflush(stdout);
    SDL_setenv("SDL_VIDEODRIVER","dummy",1);assert(SDL_Init(SDL_INIT_VIDEO)==0);
    assert(physics_sim_ensure_runtime_dirs());
    AppConfig cfg=app_config_default();SceneMenuSelection selection={0};
    SceneMenuInteraction c={.cfg=&cfg,.selection=&selection};
    menu_settings_shell_init(&c.settings_shell,&cfg,&selection,NULL,c.active_mode,cfg.space_mode);
    char long_root[256];memset(long_root,'a',200);memcpy(long_root+200,"/é水🧪",11);long_root[211]=0;
    snprintf(cfg.input_root,sizeof(cfg.input_root),"%s",long_root);
    menu_begin_input_root_edit(&c);
    assert(strcmp(menu_shared_text_value(&c,&c.input_root_input),long_root)==0);
    assert(strlen(c.input_root_input.buffer)<=63 && c.input_root_input.length<=63);
    menu_shared_text_key(&c,SDLK_END,KMOD_NONE);
    menu_text_input(&c,"Z");assert(strlen(menu_shared_text_value(&c,&c.input_root_input))==strlen(long_root)+1);
    menu_window_lifecycle_invalidate(&c);
    assert(strlen(menu_shared_text_value(&c,&c.input_root_input))==strlen(long_root)+1);
    menu_key_down(&c,SDLK_ESCAPE,0);assert(strcmp(cfg.input_root,long_root)==0);
    assert(access(physics_sim_runtime_config_path(),F_OK)!=0);
    menu_begin_input_root_edit(&c);menu_key_down(&c,SDLK_RETURN,0);
    assert(strcmp(cfg.input_root,long_root)==0); /* unchanged long value never truncates on Enter */
    AppConfig loaded;ConfigLoadOptions options={.path=physics_sim_runtime_config_path()};
    assert(config_loader_load(&loaded,&options) && strcmp(loaded.input_root,long_root)==0);
    char max_path[512];memset(max_path,'b',511);max_path[511]=0;
    menu_begin_output_root_edit(&c);menu_shared_text_key(&c,SDLK_a,KMOD_GUI);
    menu_shared_text_insert(&c,max_path); /* 511 exceeds output's 255-byte schema */
    assert(strcmp(menu_shared_text_value(&c,&c.output_root_input),cfg.headless_output_dir)==0);
    menu_shared_text_key(&c,SDLK_a,KMOD_GUI);max_path[255]=0;menu_shared_text_insert(&c,max_path);
    assert(strlen(menu_shared_text_value(&c,&c.output_root_input))==255);
    menu_shared_text_insert(&c,"é");assert(strlen(menu_shared_text_value(&c,&c.output_root_input))==255);
    menu_key_down(&c,SDLK_RETURN,0);
    assert(config_loader_load(&loaded,&options) && strlen(loaded.headless_output_dir)==255);
    c.active_mode=SIM_MODE_ATMOSPHERIC;cfg.space_mode=SPACE_MODE_3D;
    memset(max_path,'c',511);max_path[511]=0;
    menu_begin_warm_start_edit(&c);menu_shared_text_key(&c,SDLK_a,KMOD_GUI);menu_shared_text_insert(&c,max_path);
    assert(strlen(menu_shared_text_value(&c,&c.warm_start_input))==511);
    menu_shared_text_insert(&c,"x");assert(strlen(menu_shared_text_value(&c,&c.warm_start_input))==511);
    menu_key_down(&c,SDLK_RETURN,0);
    assert(config_loader_load(&loaded,&options) && strlen(loaded.atmospheric_warm_start_path)==511);
    menu_begin_warm_start_edit(&c);menu_shared_text_key(&c,SDLK_a,KMOD_GUI);menu_shared_text_insert(&c,"cancel");
    menu_key_down(&c,SDLK_ESCAPE,0);assert(strlen(cfg.atmospheric_warm_start_path)==511);
    AppConfig malformed=cfg;malformed.input_root[0]=(char)0xff;malformed.input_root[1]=0;
    assert(!config_loader_save(&malformed,physics_sim_runtime_config_path()));
    assert(config_loader_load(&loaded,&options) && strcmp(loaded.input_root,long_root)==0);
    char fitted[32];
    assert(menu_text_fit_measured("é水🧪abcdef",40,fitted,sizeof fitted,mono,NULL).code==CORE_OK);
    check_utf8(fitted,sizeof fitted);float width;mono(NULL,fitted,&width);assert(width<=40 && strstr(fitted,"..."));
    SDL_Rect label_bounds={10,20,40,20};KitUiTextPresentation label_view;
    assert(menu_text_label_build(&label_view,fitted,sizeof fitted,&label_bounds,16,1,mono,NULL).code==CORE_OK);
    assert(label_view.count==1 && label_view.row_text[label_view.rows[0].text_offset]);
    assert(label_view.rows[0].origin.x>=10 && label_view.rows[0].origin.x+label_view.rows[0].width<=50);
    assert(label_view.rows[0].origin.y>=20 && !label_view.options.active);
    char tiny[7];assert(menu_text_fit_measured("é水🧪abcdef",1000,tiny,sizeof tiny,mono,NULL).code==CORE_OK);
    check_utf8(tiny,sizeof tiny);assert(strlen(tiny)<sizeof tiny);
    assert(menu_text_fit_measured("é水",1,fitted,sizeof fitted,mono,NULL).code==CORE_OK && !fitted[0]);
    char truncated[4];menu_text_copy_utf8(truncated,sizeof truncated,"é水");assert(strcmp(truncated,"é")==0);
    char status[140];memset(status,'s',126);memcpy(status+126,"é",3);menu_set_status(&c,status,true);
    check_utf8(c.status_text,sizeof(c.status_text));assert(strlen(c.status_text)==126 && c.status_wait_ack);
    c.config_panel_rect=(SDL_Rect){400,100,600,200};c.data_io_panel_rect=(SDL_Rect){400,500,600,150};
    SDL_Rect r=menu_status_text_rect(&c,1120,720,20);
    assert(r.x>=400 && r.x+r.w<=1000 && r.y>=304 && r.y+r.h<=496);
    c.data_io_panel_rect.y=302;r=menu_status_text_rect(&c,1120,720,20);assert(r.h==0);
    menu_shared_text_cancel_transient(&c);SDL_Quit();puts("menu path capacity and text containment contracts passed");return 0;
}
