#include <assert.h>
#include <stdio.h>
#include "app/ui/physics_sim_ui_host.h"
#include "app/editor/scene_editor_precision_ui.h"
#include "app/structural/structural_editor.h"
static int activated;static unsigned generation=1;
static void action(void *p,uint64_t key) {(void)p;assert(key==42);++activated;}
static uint32_t scope(void *p) {(void)p;return generation;}
static void controls(PhysicsSimUiHost *h,bool enabled) {
    physics_sim_ui_host_collect(h);
    assert(kit_ui_surface_register(&h->controls,(KitUiSurfaceKey){1,42},
        (KitRenderRect){10,10,80,30},NULL,enabled,NULL).code==CORE_OK);
    physics_sim_ui_host_commit(h);
}
static bool pointer(PhysicsSimUiHost *h,Uint32 type,int x) {
    SDL_Event e;SDL_zero(e);e.type=type;e.button.button=SDL_BUTTON_LEFT;e.button.x=x;e.button.y=20;
    return physics_sim_ui_host_filter(h,&e);
}
int main(void) {
    SDL_setenv("SDL_VIDEODRIVER","dummy",1);assert(!SDL_Init(SDL_INIT_VIDEO));
    PhysicsSimUiHost h={0};h.observed.presentable=1;h.observed.flags=SDL_WINDOW_INPUT_FOCUS;
    h.action=action;h.scope=scope;controls(&h,true);
    assert(pointer(&h,SDL_MOUSEBUTTONDOWN,40));assert(!activated);
    assert(pointer(&h,SDL_MOUSEBUTTONUP,40));assert(activated==1);
    controls(&h,true);pointer(&h,SDL_MOUSEBUTTONDOWN,200);pointer(&h,SDL_MOUSEBUTTONUP,40);assert(activated==1);
    controls(&h,false);pointer(&h,SDL_MOUSEBUTTONDOWN,40);pointer(&h,SDL_MOUSEBUTTONUP,40);assert(activated==1);
    controls(&h,true);pointer(&h,SDL_MOUSEBUTTONDOWN,40);physics_sim_ui_host_cancel(&h);
    assert(pointer(&h,SDL_MOUSEBUTTONUP,40));assert(activated==1);
    controls(&h,true);pointer(&h,SDL_MOUSEBUTTONDOWN,40);++generation;
    pointer(&h,SDL_MOUSEBUTTONUP,40);assert(activated==1);
    controls(&h,true);SDL_Event e;SDL_zero(e);e.type=SDL_KEYDOWN;e.key.keysym.sym=SDLK_TAB;
    assert(physics_sim_ui_host_filter(&h,&e));e.key.keysym.sym=SDLK_SPACE;
    assert(physics_sim_ui_host_filter(&h,&e));e.key.repeat=1;assert(physics_sim_ui_host_filter(&h,&e));assert(activated==1);
    e.type=SDL_KEYUP;e.key.repeat=0;assert(physics_sim_ui_host_filter(&h,&e));assert(activated==2);
    e.type=SDL_KEYDOWN;e.key.keysym.sym=SDLK_RETURN;e.key.repeat=1;assert(physics_sim_ui_host_filter(&h,&e));
    e.key.repeat=0;e.key.keysym.scancode=SDL_SCANCODE_RETURN;h.blocked_keys[SDL_SCANCODE_RETURN]=1;
    assert(physics_sim_ui_host_filter(&h,&e));e.type=SDL_KEYUP;assert(physics_sim_ui_host_filter(&h,&e));assert(!h.blocked_keys[SDL_SCANCODE_RETURN]);
    h.observed.presentable=0;assert(pointer(&h,SDL_MOUSEBUTTONDOWN,40));assert(activated==2);
    h.observed.presentable=1;h.geometry_dirty=true;assert(pointer(&h,SDL_MOUSEBUTTONDOWN,40));assert(activated==2);
    h.geometry_dirty=false;h.window=SDL_CreateWindow("owned",0,0,100,80,SDL_WINDOW_HIDDEN);assert(h.window);
    SDL_zero(e);e.type=SDL_WINDOWEVENT;e.window.windowID=SDL_GetWindowID(h.window)+1;e.window.event=SDL_WINDOWEVENT_CLOSE;
    assert(physics_sim_ui_host_filter(&h,&e));e.window.windowID=SDL_GetWindowID(h.window);assert(!physics_sim_ui_host_filter(&h,&e));
    SDL_DestroyWindow(h.window);
    bool pointer_down=true,drag_started=true,object=true,import=true,import_handle=true,handle=true,emitter=true,handle_started=true;
    int hits=3,base=2,hover=1;
    PrecisionUiTransient p={.pointer_down=&pointer_down,.drag_started=&drag_started,.object=&object,
        .import_body=&import,.import_handle=&import_handle,.handle=&handle,.emitter=&emitter,
        .handle_started=&handle_started,.hit_count=&hits,.hit_base=&base,.hover_object=&hover};
    scene_editor_precision_cancel_transient(&p);
    assert(!pointer_down&&!drag_started&&!object&&!import&&!import_handle&&!handle&&!emitter&&!handle_started&&!hits&&!base&&hover==-1);
    StructuralScene scene;structural_scene_init(&scene);StructuralEditor editor;structural_editor_init(&editor,&scene);scene.node_count=1;scene.nodes[0].x=12;scene.nodes[0].selected=true;
    editor.dragging=editor.box_selecting=editor.load_dragging=editor.moment_dragging=true;
    structural_editor_cancel_transient(&editor);
    assert(!editor.dragging&&!editor.box_selecting&&!editor.load_dragging&&!editor.moment_dragging);
    assert(scene.node_count==1&&scene.nodes[0].x==12&&scene.nodes[0].selected);
    SDL_Quit();puts("physics UI host contract passed");return 0;
}
