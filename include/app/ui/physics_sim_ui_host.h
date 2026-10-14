#ifndef PHYSICS_SIM_UI_HOST_H
#define PHYSICS_SIM_UI_HOST_H
#include "kit_ui_window_sdl.h"
#include "kit_ui_window_probe_sdl.h"
#include "kit_ui_surface.h"
#include "app/editor/scene_editor_widgets.h"
typedef struct PhysicsSimUiHost {
    SDL_Window *window;
    SDL_Renderer *renderer;
    KitUiWindowState observed;
    KitUiSurface controls;
    KitUiWindowProbe probe;
    uint64_t presented_frames;
    bool geometry_dirty, release_blocked;
    Uint8 blocked_keys[SDL_NUM_SCANCODES];
    void *owner;
    void (*cancel)(void *);
    void (*action)(void *, uint64_t);
    uint32_t (*scope)(void *);
} PhysicsSimUiHost;
bool physics_sim_ui_proof_enabled(void);
void physics_sim_ui_host_init(PhysicsSimUiHost *, SDL_Window *, SDL_Renderer *,
    const char *, const char *, void *, void (*)(void *));
void physics_sim_ui_host_cancel(PhysicsSimUiHost *);
uint32_t physics_sim_ui_host_refresh(PhysicsSimUiHost *);
bool physics_sim_ui_host_filter(void *, const SDL_Event *);
void physics_sim_ui_host_collect(PhysicsSimUiHost *);
void physics_sim_ui_host_commit(PhysicsSimUiHost *);
void physics_sim_ui_host_button(PhysicsSimUiHost *, uint64_t, const EditorButton *, TTF_Font *);
int physics_sim_ui_host_probe(PhysicsSimUiHost *);
void physics_sim_ui_text_line(SDL_Renderer *, TTF_Font *, int, int, SDL_Color, const char *);
void physics_sim_ui_tooltip(SDL_Renderer *, TTF_Font *, int, int, const char *[], int,
    SDL_Color, SDL_Color, SDL_Color);
#endif
