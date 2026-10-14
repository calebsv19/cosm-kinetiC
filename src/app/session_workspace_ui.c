#include "app/session_workspace_ui.h"
#include "app/menu/shared_theme_font_adapter.h"
#include "app/menu/menu_render.h"
#include <stdio.h>
#include <string.h>
void session_ui_init(SessionUi *ui,SDL_Window *window,SDL_Renderer *renderer,
    void *owner,void (*cancel)(void *)) {
    memset(ui,0,sizeof(*ui));
    physics_sim_ui_host_init(&ui->host,window,renderer,"physics-inspection",
        "CODEWORK_SESSION_UI_PROOF",owner,cancel);
}
void session_ui_button(SessionUi *ui,SessionText *font,uint64_t key,SDL_Rect rect,
    const char *label,bool enabled) {
    PhysicsSimUiHost *h=&ui->host;
    SDL_Rect clip={0,0,h->observed.logical_width,h->observed.logical_height},old;
    if (SDL_RenderIsClipEnabled(h->renderer)) {
        SDL_RenderGetClipRect(h->renderer,&old);SDL_IntersectRect(&clip,&old,&clip);
    }
    if (clip.w<=0 || clip.h<=0 || rect.w<=0 || rect.h<=0) return;
    KitUiInteractionControl control={0};
    (void)kit_ui_surface_register(&h->controls,(KitUiSurfaceKey){1,key},
        (KitRenderRect){rect.x,rect.y,rect.w,rect.h},
        &(KitRenderRect){clip.x,clip.y,clip.w,clip.h},enabled,&control);
    KitUiButtonState state=kit_ui_interaction_button_state(&h->controls.interaction,&control,false);
    PhysicsSimMenuThemePalette palette;physics_sim_shared_theme_resolve_menu_palette(&palette);
    SDL_Color fill=enabled?palette.button_fill:palette.panel_fill;
    if (enabled && (state.hovered || state.pressed)) fill=palette.accent_primary;
    SDL_SetRenderDrawColor(h->renderer,fill.r,fill.g,fill.b,fill.a);SDL_RenderFillRect(h->renderer,&rect);
    if (state.focused) {
        SDL_Color accent=palette.accent_primary;
        SDL_SetRenderDrawColor(h->renderer,accent.r,accent.g,accent.b,255);SDL_RenderDrawRect(h->renderer,&rect);
    }
    SDL_Rect text={rect.x+7,rect.y+4,rect.w-14,rect.h-8};
    session_text_draw_rect(font,h->renderer,2,&text,menu_readable_text_color(fill,enabled?palette.text_primary:palette.text_muted),label,1);
}
static int request_capture(void *user,const char *path) {
    SessionUi *ui=user;
    int n=snprintf(ui->capture_path,sizeof(ui->capture_path),"%s",path);
    return n>0 && n<(int)sizeof(ui->capture_path);
}
int session_ui_probe(SessionUi *ui) {
    int status=kit_ui_window_probe_tick_sdl(&ui->host.probe,ui->host.window,
        ui->host.presented_frames,request_capture,ui);
    if (status) fprintf(stdout,"SESSION_UI_WINDOW proof=%d renderer=SDL frames=%llu\n",
        status,(unsigned long long)ui->host.presented_frames);
    return status;
}
void session_ui_capture(SessionUi *ui) {
    if (!ui->capture_path[0]) return;
    int w=0,h=0;SDL_GetRendererOutputSize(ui->host.renderer,&w,&h);
    SDL_Surface *surface=SDL_CreateRGBSurfaceWithFormat(0,w,h,32,SDL_PIXELFORMAT_ARGB8888);
    if (surface && SDL_RenderReadPixels(ui->host.renderer,NULL,surface->format->format,
        surface->pixels,surface->pitch)==0) {
        if (SDL_SaveBMP(surface,ui->capture_path)!=0) ui->host.probe.status=-1;
    } else ui->host.probe.status=-1;
    SDL_FreeSurface(surface);ui->capture_path[0]=0;
}
