/* Native Vulkan hosts only: SDL drawing calls below use the Vulkan compatibility seam. */
#include "app/ui/physics_sim_ui_host.h"
#include "app/menu/menu_text_presentation.h"
#include "render/text_draw.h"
#include "vk_renderer.h"
#include <stdio.h>

void physics_sim_ui_host_button(PhysicsSimUiHost *h, uint64_t key,
    const EditorButton *button, TTF_Font *font) {
    SDL_Rect clip={0,0,h->observed.logical_width,h->observed.logical_height}, inherited;
    if (SDL_RenderIsClipEnabled(h->renderer)) {
        SDL_RenderGetClipRect(h->renderer,&inherited);SDL_IntersectRect(&clip,&inherited,&clip);
    }
    if (clip.w<=0 || clip.h<=0 || button->rect.w<=0 || button->rect.h<=0) return;
    KitUiInteractionControl control={0};
    (void)kit_ui_surface_register(&h->controls,(KitUiSurfaceKey){1,key},
        (KitRenderRect){button->rect.x,button->rect.y,button->rect.w,button->rect.h},
        &(KitRenderRect){clip.x,clip.y,clip.w,clip.h},button->enabled,&control);
    KitUiButtonState state=kit_ui_interaction_button_state(&h->controls.interaction,&control,false);
    scene_editor_draw_shared_button(h->renderer,button,font,&state);
}
static int capture(void *user,const char *path) {
    PhysicsSimUiHost *h=user;
    return vk_renderer_request_capture((VkRenderer *)h->renderer,path)==VK_SUCCESS;
}
int physics_sim_ui_host_probe(PhysicsSimUiHost *h) {
    int status=kit_ui_window_probe_tick_sdl(&h->probe,h->window,h->presented_frames,capture,h);
    if (status) {
        const VkRuntimeCapabilityReport *r=vk_runtime_get_capability_report(
            &((VkRenderer *)h->renderer)->context.device->runtime);
        fprintf(stdout,"UI_HOST_RUNTIME host=%s proof=%d validation=%u warnings=%u errors=%u\n",
            h->probe.program,status,(unsigned)r->validation_enabled,
            (unsigned)r->validation_warning_count,(unsigned)r->validation_error_count);
    }
    return status;
}
void physics_sim_ui_text_line(SDL_Renderer *renderer,TTF_Font *font,int x,int y,
    SDL_Color color,const char *text) {
    if (!renderer || !font) return;
    VkRenderer *vk=(VkRenderer *)renderer;
    int w=(int)vk->draw_state.logical_size[0],height=(int)vk->draw_state.logical_size[1],line=12;
    (void)physics_sim_text_measure_utf8(renderer,font,"Mg",NULL,&line);
    SDL_Rect rect={x,y,w-x-8,line};
    if (rect.w>0 && y>=0 && y+line<=height)
        menu_draw_contained_text(renderer,font,text,&rect,color,0);
}
void physics_sim_ui_tooltip(SDL_Renderer *renderer,TTF_Font *font,int x,int y,
    const char *lines[],int count,SDL_Color fill,SDL_Color border,SDL_Color ink) {
    if (!renderer || !font || count<=0) return;
    VkRenderer *vk=(VkRenderer *)renderer;
    int w=(int)vk->draw_state.logical_size[0],h=(int)vk->draw_state.logical_size[1],row=12,max_w=0;
    (void)physics_sim_text_measure_utf8(renderer,font,"Mg",NULL,&row);
    if (count>8) count=8;
    for (int i=0;i<count;i++) {
        int width=0;if (lines[i]) (void)physics_sim_text_measure_utf8(renderer,font,lines[i],&width,NULL);
        if (width>max_w) max_w=width;
    }
    SDL_Rect rect={x+16,y+16,max_w+12,count*(row+2)+10};
    if (rect.w>w-16) rect.w=w-16;
    if (rect.h>h-16) rect.h=h-16;
    if (rect.x+rect.w>w-8) rect.x=w-8-rect.w;
    if (rect.y+rect.h>h-8) rect.y=h-8-rect.h;
    if (rect.x<8) rect.x=8;if (rect.y<8) rect.y=8;
    SDL_SetRenderDrawColor(renderer,fill.r,fill.g,fill.b,fill.a);SDL_RenderFillRect(renderer,&rect);
    SDL_SetRenderDrawColor(renderer,border.r,border.g,border.b,border.a);SDL_RenderDrawRect(renderer,&rect);
    for (int i=0;i<count;i++) {
        SDL_Rect line={rect.x+6,rect.y+6+i*(row+2),rect.w-12,row};
        if (line.y+row>rect.y+rect.h-4) break;
        menu_draw_contained_text(renderer,font,lines[i] ? lines[i] : "",&line,ink,0);
    }
}
