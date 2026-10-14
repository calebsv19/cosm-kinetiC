#include "../src/app/session_workspace.c"
#include "app/scene_controller_ui.h"
#include "app/scene_controller_internal.h"
#include "render/simulation_ui.h"
#include "render/renderer_sdl.h"
#include <assert.h>
static void collect(SessionUi *ui,SessionText *font,Workspace *w) {
    physics_sim_ui_host_collect(&ui->host);
    session_ui_button(ui,font,100,(SDL_Rect){10,10,90,36},"Field",true);
    session_ui_button(ui,font,202,(SDL_Rect){110,10,90,36},"Pause",w->run_id[0]!=0);
    session_ui_button(ui,font,200,(SDL_Rect){210,10,90,36},"Wind example",!w->child);
    physics_sim_ui_host_commit(&ui->host);
}
static bool pointer(SessionUi *ui,Uint32 type,int x) {
    SDL_Event e;SDL_zero(e);e.type=type;e.button.button=SDL_BUTTON_LEFT;e.button.x=x;e.button.y=24;
    return physics_sim_ui_host_filter(&ui->host,&e);
}
int main(void) {
    SDL_setenv("SDL_VIDEODRIVER","dummy",1);assert(!SDL_Init(SDL_INIT_VIDEO));assert(!TTF_Init());
    SDL_Window *window=SDL_CreateWindow("fixture",0,0,800,400,SDL_WINDOW_SHOWN);assert(window);
    SDL_Renderer *renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_SOFTWARE);assert(renderer);SDL_RenderSetLogicalSize(renderer,800,400);
    SessionText font={0};physics_sim_shared_font_load_persisted();assert(session_text_refresh(&font,renderer,window));
    Workspace w={.slice=.5,.probe=-1};bool dragging=true,fitted=true,running=true,setup=false;
    WorkspaceUiOwner owner={&w,&dragging,&fitted,&running,&setup};SessionUi ui;
    session_ui_init(&ui,window,renderer,&owner,cancel_gesture);ui.host.window=NULL;
    ui.host.observed.presentable=true;ui.host.observed.flags=SDL_WINDOW_INPUT_FOCUS;
    ui.host.observed.logical_width=800;ui.host.observed.logical_height=400;ui.host.geometry_dirty=false;ui.host.release_blocked=false;
    ui.host.action=workspace_action;ui.host.scope=workspace_scope;
    collect(&ui,&font,&w);assert(pointer(&ui,SDL_MOUSEBUTTONDOWN,40));assert(w.field==0);
    assert(pointer(&ui,SDL_MOUSEBUTTONUP,40));assert(w.field==1&&!w.locked_range);
    collect(&ui,&font,&w);pointer(&ui,SDL_MOUSEBUTTONDOWN,400);pointer(&ui,SDL_MOUSEBUTTONUP,40);assert(w.field==1);
    collect(&ui,&font,&w);pointer(&ui,SDL_MOUSEBUTTONDOWN,40);physics_sim_ui_host_cancel(&ui.host);
    pointer(&ui,SDL_MOUSEBUTTONUP,40);assert(w.field==1&&!dragging);
    collect(&ui,&font,&w);pointer(&ui,SDL_MOUSEBUTTONDOWN,40);strcpy(w.run_id,"different-run");pointer(&ui,SDL_MOUSEBUTTONUP,40);assert(w.field==1);
    w.child=12345;strcpy(w.last_command_id,"original-retry-id");w.retry_control=true;
    collect(&ui,&font,&w);pointer(&ui,SDL_MOUSEBUTTONDOWN,250);pointer(&ui,SDL_MOUSEBUTTONUP,250);assert(w.child==12345&&!w.scene_id[0]);
    collect(&ui,&font,&w);pointer(&ui,SDL_MOUSEBUTTONDOWN,150);assert(!w.queued_action[0]);pointer(&ui,SDL_MOUSEBUTTONUP,150);
    assert(!strcmp(w.queued_action,"pause")&&w.retry_control&&!strcmp(w.last_command_id,"original-retry-id")&&w.commands==0);
    w.child=0;w.queued_action[0]=0;kit_ui_surface_reset(&ui.host.controls);collect(&ui,&font,&w);
    SDL_Event e;SDL_zero(e);e.type=SDL_KEYDOWN;e.key.keysym.sym=SDLK_TAB;
    assert(physics_sim_ui_host_filter(&ui.host,&e));e.key.keysym.sym=SDLK_SPACE;assert(physics_sim_ui_host_filter(&ui.host,&e));
    e.key.repeat=1;assert(physics_sim_ui_host_filter(&ui.host,&e));assert(w.field==1);e.type=SDL_KEYUP;e.key.repeat=0;
    assert(physics_sim_ui_host_filter(&ui.host,&e));assert(w.field==2);
    SDL_Rect clip={12,12,200,100},observed;SDL_RenderSetClipRect(renderer,&clip);
    session_text_draw_rect(&font,renderer,0,&(SDL_Rect){24,24,400,40},(SDL_Color){255,255,255,255},"Aé水 long read-only label fitted into its enclosing clip",0);
    SDL_RenderGetClipRect(renderer,&observed);assert(SDL_RenderIsClipEnabled(renderer)&&!memcmp(&clip,&observed,sizeof(clip)));
    SceneState scene={0};scene.time=4.5;scene.runtime_view.viewport.navigation_active=true;scene.runtime_view.viewport.alt_modifier_down=true;
    StrokeSampler sampler={.pointer_down=true,.accumulator=3};SceneControllerUiOwner sim={&scene,&sampler};scene_controller_ui_cancel(&sim);
    assert(scene.time==4.5&&!scene.runtime_view.viewport.navigation_active&&!scene.runtime_view.viewport.alt_modifier_down&&!sampler.pointer_down&&!sampler.accumulator);
    simulation_ui_attach(window,renderer);SDL_SetWindowSize(window,400,200);
    (void)renderer_sdl_ui_presentable();int x=200,y=100;renderer_sdl_ui_map_pointer(&x,&y);
    assert(x==400&&y==200);simulation_ui_detach();
    session_text_destroy(&font);SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);TTF_Quit();SDL_Quit();
    puts("session shared UI: matched/disabled/stale/keyboard controls, queued retry identity, SDL text clip and transient-only cancellation passed");return 0;
}
