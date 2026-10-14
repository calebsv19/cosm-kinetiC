#include "app/ui/physics_sim_ui_host.h"
#include "kit_ui_interaction_sdl.h"
#include <stdlib.h>
#include <string.h>

bool physics_sim_ui_proof_enabled(void) {
    const char *names[] = {"CODEWORK_SIMULATION_UI_PROOF", "CODEWORK_SESSION_UI_PROOF", "CODEWORK_PRECISION_UI_PROOF", "CODEWORK_STRUCTURAL_EDITOR_UI_PROOF",
        "CODEWORK_STRUCTURAL_RUNTIME_UI_PROOF", "CODEWORK_EDITOR_UI_PROOF", "CODEWORK_WINDOW_LIFECYCLE_PROOF"};
    for (size_t i=0; i<sizeof(names)/sizeof(names[0]); ++i) {
        const char *value=getenv(names[i]); if (value && *value) return true;
    }
    return false;
}
void physics_sim_ui_host_cancel(PhysicsSimUiHost *h) {
    KitUiInteractionEvent cancel={.type=KIT_UI_INTERACTION_CANCEL};
    KitUiInteractionResult result;
    (void)kit_ui_surface_route(&h->controls,&cancel,&result);
    h->controls.count=0; h->release_blocked=true;
    int count=0; const Uint8 *keys=SDL_GetKeyboardState(&count);
    memset(h->blocked_keys,0,sizeof(h->blocked_keys));
    if (count>SDL_NUM_SCANCODES) count=SDL_NUM_SCANCODES;
    if (keys && count>0) memcpy(h->blocked_keys,keys,(size_t)count);
    if (h->cancel) h->cancel(h->owner);
}
uint32_t physics_sim_ui_host_refresh(PhysicsSimUiHost *h) {
    if (!h || !h->window) return 0;
    uint32_t changes=0;
    if (kit_ui_window_refresh_sdl(&h->observed,h->window,&changes).code!=CORE_OK) {
        h->observed.presentable=0; changes=KIT_UI_WINDOW_VISIBILITY;
    }
    if (changes) physics_sim_ui_host_cancel(h);
    if (changes & (KIT_UI_WINDOW_GEOMETRY|KIT_UI_WINDOW_MODE|KIT_UI_WINDOW_DISPLAY|KIT_UI_WINDOW_VISIBILITY))
        h->geometry_dirty=true;
    return changes;
}
void physics_sim_ui_host_init(PhysicsSimUiHost *h, SDL_Window *window, SDL_Renderer *renderer,
    const char *name, const char *proof_env, void *owner, void (*cancel)(void *)) {
    memset(h,0,sizeof(*h)); h->window=window;h->renderer=renderer;h->owner=owner;h->cancel=cancel;
    kit_ui_window_probe_init_sdl(&h->probe,name);
    h->probe.directory=getenv(proof_env);
    if (h->probe.directory && !*h->probe.directory) h->probe.directory=NULL;
    (void)physics_sim_ui_host_refresh(h);
}
static Uint32 event_window(const SDL_Event *e) {
    switch(e->type) {
    case SDL_WINDOWEVENT:return e->window.windowID;
    case SDL_MOUSEMOTION:return e->motion.windowID;
    case SDL_MOUSEBUTTONDOWN:case SDL_MOUSEBUTTONUP:return e->button.windowID;
    case SDL_MOUSEWHEEL:return e->wheel.windowID;
    case SDL_KEYDOWN:case SDL_KEYUP:return e->key.windowID;
    case SDL_TEXTINPUT:return e->text.windowID;
    case SDL_TEXTEDITING:return e->edit.windowID;
    default:return 0;
    }
}
bool physics_sim_ui_host_filter(void *user, const SDL_Event *e) {
    PhysicsSimUiHost *h=user;
    Uint32 id=event_window(e);
    if (id && h->window && id!=SDL_GetWindowID(h->window)) return true;
    (void)physics_sim_ui_host_refresh(h);
    if (kit_ui_window_event_invalidates_sdl(e)) physics_sim_ui_host_cancel(h);
    if (e->type==SDL_QUIT || e->type==SDL_WINDOWEVENT) return false;
    bool pointer=e->type==SDL_MOUSEBUTTONDOWN || e->type==SDL_MOUSEBUTTONUP ||
        e->type==SDL_MOUSEMOTION || e->type==SDL_MOUSEWHEEL;
    bool key=e->type==SDL_KEYDOWN || e->type==SDL_KEYUP;
    if ((pointer || key) && !h->observed.presentable) return true;
    if (pointer && h->geometry_dirty) return true;
    if (key && !(h->observed.flags & SDL_WINDOW_INPUT_FOCUS)) return true;
    /* These hosts have no editable text owner. */
    if (e->type==SDL_TEXTINPUT || e->type==SDL_TEXTEDITING) return true;
    if (key && e->key.keysym.scancode>=0 && e->key.keysym.scancode<SDL_NUM_SCANCODES) {
        int code=e->key.keysym.scancode;
        if (h->blocked_keys[code]) {
            if (e->type==SDL_KEYUP) h->blocked_keys[code]=0;
            return true;
        }
    }
    if (e->type==SDL_MOUSEBUTTONDOWN && e->button.button==SDL_BUTTON_LEFT) h->release_blocked=false;
    if (e->type==SDL_MOUSEBUTTONUP && e->button.button==SDL_BUTTON_LEFT && h->release_blocked) {
        h->release_blocked=false;return true;
    }
    if (e->type==SDL_MOUSEMOTION && h->release_blocked && (e->motion.state & SDL_BUTTON_LMASK)) return true;
    if (kit_ui_window_fullscreen_key_sdl(h->window,e)) {
        physics_sim_ui_host_cancel(h);(void)physics_sim_ui_host_refresh(h);return true;
    }
    if (e->type==SDL_KEYDOWN && e->key.repeat &&
        (e->key.keysym.sym==SDLK_RETURN || e->key.keysym.sym==SDLK_KP_ENTER ||
         e->key.keysym.sym==SDLK_SPACE || e->key.keysym.sym==SDLK_ESCAPE)) return true;
    if (h->scope) kit_ui_surface_set_scope(&h->controls,h->scope(h->owner));
    KitUiInteractionEvent event;KitUiInteractionResult result;
    if (!kit_ui_interaction_event_from_sdl(e,&event)) return false;
    if (kit_ui_surface_route(&h->controls,&event,&result).code!=CORE_OK) return true;
    if (result.activated_id && kit_ui_surface_take_activation(&h->controls,result.activated_id)) {
        KitUiSurfaceKey key;
        if (kit_ui_surface_key(&h->controls,result.activated_id,&key) && h->action)
            h->action(h->owner,key.value);
    }
    return result.consumed!=0;
}
void physics_sim_ui_host_collect(PhysicsSimUiHost *h) {
    kit_ui_surface_begin(&h->controls,h->scope ? h->scope(h->owner) : 1);
}
void physics_sim_ui_host_commit(PhysicsSimUiHost *h) { (void)kit_ui_surface_end(&h->controls); }
