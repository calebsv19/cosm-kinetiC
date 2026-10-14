#include "app/session_workspace_text.h"
#include "app/menu/shared_theme_font_adapter.h"
#include "font_paths.h"
#include "app/menu/menu_text_presentation.h"
#include <stdio.h>
#include <math.h>
#include <string.h>

void session_text_destroy(SessionText *text) {
    for (int i = 0; i < 96; i++) {
        if (text->entries[i].texture)
            SDL_DestroyTexture(text->entries[i].texture);
        text->entries[i].texture = NULL;
    }
    for (int i = 0; i < 3; i++) {
        if (text->fonts[i])
            TTF_CloseFont(text->fonts[i]);
        text->fonts[i] = NULL;
    }
    text->scale = 0;
    text->next = 0;
}
bool session_text_refresh(SessionText *text, SDL_Renderer *renderer, SDL_Window *window) {
    int w, h, rw, rh;
    SDL_GetWindowSize(window, &w, &h);
    SDL_GetRendererOutputSize(renderer, &rw, &rh);
    float scale = fmaxf(w > 0 ? (float)rw / w : 1, h > 0 ? (float)rh / h : 1);
    scale = fmaxf(1, fminf(4, scale));
    if (fabsf(scale - text->scale) < .01f && fabsf(text->zoom - text->loaded_zoom) < .01f &&
        text->fonts[0])
        return true;
    session_text_destroy(text);
    text->scale = scale;
    if (text->zoom <= 0)
        text->zoom = 1.25f;
    text->loaded_zoom = text->zoom;
    for (int role = 0; role < 3; role++) {
        char path[512] = {0};
        int size = 18;
        if (role == 0)
            physics_sim_shared_font_resolve_menu_body(path, sizeof(path), &size);
        else if (role == 1)
            physics_sim_shared_font_resolve_menu_title(path, sizeof(path), &size);
        else
            physics_sim_shared_font_resolve_menu_small(path, sizeof(path), &size);
        size = (int)lroundf(size * text->zoom);
        text->logical_sizes[role] = size;
        text->raster_sizes[role] = (int)lroundf(size * scale);
        text->fonts[role] = TTF_OpenFont(*path ? path : FONT_BODY_PATH_1, text->raster_sizes[role]);
        if (!text->fonts[role]) {
            session_text_destroy(text);
            return false;
        }
        TTF_SetFontKerning(text->fonts[role], 1);
        TTF_SetFontHinting(text->fonts[role], TTF_HINTING_LIGHT);
    }
    return true;
}
static void draw_native(SessionText *text, SDL_Renderer *renderer, int role, int x, int y,
                       SDL_Color color, const char *label) {
    if (!label || !*label || role < 0 || role > 2 || !text->fonts[role])
        return;
    SessionTextEntry *entry = NULL;
    for (int i = 0; i < 96; i++) {
        SessionTextEntry *e = &text->entries[i];
        if (e->texture && e->role == role && !memcmp(&e->color, &color, sizeof(color)) &&
            !strcmp(e->text, label)) {
            entry = e;
            break;
        }
    }
    if (!entry) {
        entry = &text->entries[text->next++ % 96];
        if (entry->texture)
            SDL_DestroyTexture(entry->texture);
        entry->texture = NULL;
        SDL_Surface *surface = TTF_RenderUTF8_Blended(text->fonts[role], label, color);
        if (!surface)
            return;
        entry->texture = SDL_CreateTextureFromSurface(renderer, surface);
        entry->w = surface->w;
        entry->h = surface->h;
        SDL_FreeSurface(surface);
        if (!entry->texture)
            return;
        SDL_SetTextureScaleMode(entry->texture, SDL_ScaleModeLinear);
        SDL_SetTextureBlendMode(entry->texture, SDL_BLENDMODE_BLEND);
        snprintf(entry->text, sizeof(entry->text), "%s", label);
        entry->color = color;
        entry->role = role;
    }
    // Logical placement, native-resolution glyph pixels. Avoid doubling an 18px bitmap.
    SDL_FRect dst = {(float)x, (float)y, entry->w / text->scale, entry->h / text->scale};
    SDL_RenderCopyF(renderer, entry->texture, NULL, &dst);
}

typedef struct SessionTextPaint { SessionText *font; SDL_Renderer *renderer; int role; SDL_Color color; } SessionTextPaint;
static CoreResult measure(void *user,const char *run,float *width) {
    SessionTextPaint *paint=user;int w=0;
    if (TTF_SizeUTF8(paint->font->fonts[paint->role],run,&w,NULL)!=0)
        return (CoreResult){CORE_ERR_INVALID_ARG,"session text measure"};
    *width=w/paint->font->scale;return core_result_ok();
}
static CoreResult paint_text(void *user,KitRenderVec2 origin,const char *run) {
    SessionTextPaint *p=user;
    draw_native(p->font,p->renderer,p->role,(int)origin.x,(int)origin.y,p->color,run);
    return core_result_ok();
}
static CoreResult no_rect(void *user,KitRenderRect rect,KitRenderColor color) {
    (void)user;(void)rect;(void)color;return core_result_ok();
}
void session_text_draw_rect(SessionText *font,SDL_Renderer *renderer,int role,
    const SDL_Rect *rect,SDL_Color color,const char *label,int align) {
    if (!font || !renderer || !label || !rect || rect->w<=0 || rect->h<=0 || role<0 || role>2 || !font->fonts[role]) return;
    char fitted[512];SessionTextPaint paint={font,renderer,role,color};
    if (menu_text_fit_measured(label,rect->w,fitted,sizeof(fitted),measure,&paint).code!=CORE_OK) return;
    int row=(int)ceilf(TTF_FontHeight(font->fonts[role])/font->scale);
    KitUiTextPresentation view;
    if (menu_text_label_build(&view,fitted,sizeof(fitted),rect,row,align,measure,&paint).code!=CORE_OK) return;
    SDL_Rect old,clip=*rect;bool had_clip=SDL_RenderIsClipEnabled(renderer);SDL_RenderGetClipRect(renderer,&old);
    if (had_clip) SDL_IntersectRect(&clip,&old,&clip);
    SDL_RenderSetClipRect(renderer,&clip);
    KitUiTextPainter painter={&paint,no_rect,paint_text};KitUiTextPresentationColors colors={0};
    (void)kit_ui_text_presentation_paint(&view,&colors,&painter);
    SDL_RenderSetClipRect(renderer,had_clip?&old:NULL);
}
void session_text_draw(SessionText *font,SDL_Renderer *renderer,int role,int x,int y,
    SDL_Color color,const char *label) {
    if (!font || role<0 || role>2 || !font->fonts[role]) return;
    int w=0,h=0;SDL_RenderGetLogicalSize(renderer,&w,&h);
    int row=(int)ceilf(TTF_FontHeight(font->fonts[role])/font->scale);
    SDL_Rect rect={x,y,w-x-24,row};
    if (y>=0 && y+row<=h-8) session_text_draw_rect(font,renderer,role,&rect,color,label,0);
}
