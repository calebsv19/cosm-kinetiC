#include "app/editor/scene_editor_precision_ui.h"
void scene_editor_precision_cancel_transient(void *user) {
    PrecisionUiTransient *t=user;
    *t->pointer_down=*t->drag_started=*t->object=*t->import_body=*t->import_handle=false;
    *t->handle=*t->emitter=*t->handle_started=false;
    *t->hit_count=*t->hit_base=0;
    int *hover[]={t->hover_object,t->hover_import,t->hover_emitter,t->hover_edge,t->pointer_x,t->pointer_y};
    for (size_t i=0;i<sizeof(hover)/sizeof(hover[0]);++i) if (hover[i]) *hover[i]=-1;
}

#include "app/ui/physics_sim_ui_host.h"
#include "app/menu/menu_render.h"
#include "render/text_draw.h"
void scene_editor_precision_draw_hint(SDL_Renderer *renderer,TTF_Font *font,int width,int height) {
    const char *hint="Precision editor: drag objects/handles, +/- resize, arrows nudge, Enter apply, Esc cancel";
    int measured=0,row=12;
    (void)physics_sim_text_measure_utf8(renderer,font,hint,&measured,&row);
    if (measured>width-24) {
        physics_sim_ui_text_line(renderer,font,12,height-2*row-16,menu_color_text_dim(),
            "Drag objects/handles | +/- resize | Arrows nudge");
        physics_sim_ui_text_line(renderer,font,12,height-row-12,menu_color_text_dim(),"Enter apply | Esc cancel");
    } else physics_sim_ui_text_line(renderer,font,12,height-row-12,menu_color_text_dim(),hint);
}
