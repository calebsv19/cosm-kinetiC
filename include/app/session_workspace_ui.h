#ifndef PHYSICS_SIM_SESSION_WORKSPACE_UI_H
#define PHYSICS_SIM_SESSION_WORKSPACE_UI_H
#include "app/ui/physics_sim_ui_host.h"
#include "app/session_workspace_text.h"
typedef struct SessionUi {
    PhysicsSimUiHost host;
    char capture_path[1024];
} SessionUi;
void session_ui_init(SessionUi *,SDL_Window *,SDL_Renderer *,void *,void (*)(void *));
void session_ui_button(SessionUi *,SessionText *,uint64_t,SDL_Rect,const char *,bool);
int session_ui_probe(SessionUi *);
void session_ui_capture(SessionUi *);
#endif
