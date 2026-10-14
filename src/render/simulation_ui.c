#include "render/simulation_ui.h"
#include "render/renderer_sdl.h"
#include "app/ui/physics_sim_ui_host.h"
#include <string.h>
#include <stdio.h>
#include "app/data_paths.h"
#include "config/config_loader.h"
/* The renderer owns the surface; its caller owns computation and gesture state. */
static PhysicsSimUiHost ui;
static int canvas_width,canvas_height;
void simulation_ui_attach(SDL_Window *window, SDL_Renderer *renderer) {
    physics_sim_ui_host_init(&ui, window, renderer, "physics-simulation",
        "CODEWORK_SIMULATION_UI_PROOF", NULL, NULL);
    canvas_width=ui.observed.logical_width;canvas_height=ui.observed.logical_height;
}
void simulation_ui_detach(void) { memset(&ui, 0, sizeof(ui)); }
void renderer_sdl_ui_bind(void *owner, void (*cancel)(void *)) {
    ui.owner=owner; ui.cancel=cancel;
}
bool renderer_sdl_ui_filter(void *unused, const SDL_Event *event) {
    (void)unused;
    return ui.window && physics_sim_ui_host_filter(&ui, event);
}
int renderer_sdl_ui_poll(void) {
    if (!ui.window) return 0;
    (void)physics_sim_ui_host_refresh(&ui);
    return physics_sim_ui_host_probe(&ui);
}
bool renderer_sdl_ui_presentable(void) {
    if (!ui.window) return true;
    (void)physics_sim_ui_host_refresh(&ui);
    return ui.observed.presentable;
}
bool renderer_sdl_ui_pointer_allowed(void) {
    return !ui.window || (ui.observed.presentable && !ui.geometry_dirty && !ui.release_blocked);
}
void simulation_ui_layout_ready(void) { ui.geometry_dirty=false; }
void simulation_ui_presented(void) { ++ui.presented_frames; }

void renderer_sdl_ui_map_pointer(int *x,int *y) {
    if (!x || !y || !ui.window) return;
    int mapped_x,mapped_y;
    if (kit_ui_window_map_point_sdl(&ui.observed,canvas_width,canvas_height,*x,*y,&mapped_x,&mapped_y)) {
        *x=mapped_x;*y=mapped_y;
    }
}

void renderer_sdl_ui_apply_zoom(AppConfig *cfg,const InputCommands *commands) {
    if (!cfg || !commands || !(commands->text_zoom_in_requested || commands->text_zoom_out_requested || commands->text_zoom_reset_requested)) return;
    int next=commands->text_zoom_reset_requested?0:cfg->text_zoom_step+commands->text_zoom_in_requested-commands->text_zoom_out_requested;
    next=app_config_text_zoom_step_clamp(next);
    if (next==cfg->text_zoom_step) return;
    cfg->text_zoom_step=next;
    if (!config_loader_save(cfg,physics_sim_runtime_config_path())) fprintf(stderr,"[ui] Failed to save text size.\n");
    if (!renderer_sdl_reload_text(cfg)) fprintf(stderr,"[ui] Failed to refresh runtime fonts.\n");
}
