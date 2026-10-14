#include "app/menu/menu_shared_controls.h"
#include <string.h>
#include "app/menu/menu_types.h"
#include "app/menu/menu_input.h"
#include "app/menu/menu_state.h"
#include "app/menu/menu_settings_layout.h"
#include "app/menu/menu_settings_schema.h"
#include "kit_ui_interaction_sdl.h"

static bool text_active(const SceneMenuInteraction *c) {
    return c->rename_input.active || c->editing_input_root || c->editing_output_root ||
        c->editing_warm_start || c->editing_inflow || c->editing_viscosity ||
        c->editing_headless_frames;
}
static void add(SceneMenuInteraction *c, uint64_t key, const SDL_Rect *r, bool enabled) {
    if (!r || r->w <= 0 || r->h <= 0) return;
    KitRenderRect bounds = {(float)r->x,(float)r->y,(float)r->w,(float)r->h};
    KitRenderRect clip={0,0,(float)c->window_lifecycle.observed.logical_width,
                            (float)c->window_lifecycle.observed.logical_height};
    const KitRenderRect *visible=clip.width>0 && clip.height>0 ? &clip : NULL;
    (void)kit_ui_surface_register(&c->shared_controls,(KitUiSurfaceKey){1,key},
                                 bounds,visible,enabled,NULL);
}
void menu_shared_controls_cancel(SceneMenuInteraction *c) {
    if (!c) return;
    KitUiInteractionEvent e = {.type=KIT_UI_INTERACTION_CANCEL};
    KitUiInteractionResult out;
    (void)kit_ui_surface_route(&c->shared_controls,&e,&out);
    c->shared_controls.activation_count = 0;
}
void menu_shared_controls_sync(SceneMenuInteraction *c) {
    if (!c) return;
    uint32_t scope=2166136261u;
    const int owners[]={c->active_mode,c->cfg ? c->cfg->space_mode : 0,
        c->settings_shell.provider,c->selection ? c->selection->custom_slot_index : -1,
        c->selection ? c->selection->retained_scene_index : -1,
        (c->rename_input.active?1:0) | (c->editing_input_root?2:0) |
        (c->editing_output_root?4:0) | (c->editing_warm_start?8:0) |
        (c->editing_inflow?16:0) | (c->editing_viscosity?32:0) |
        (c->editing_headless_frames?64:0),
        c->workspace_authoring.active,c->headless_running,
        c->library ? preset_library_count(c->library) : 0};
    for (size_t i=0;i<sizeof owners/sizeof owners[0];i++) scope=(scope^(uint32_t)owners[i])*16777619u;
    kit_ui_surface_begin(&c->shared_controls,scope ? scope : 1);
    bool enabled=!c->headless_running && !c->workspace_authoring.active;
    add(c,1,&c->start_button.rect,enabled);add(c,2,&c->edit_button.rect,enabled);
    add(c,3,&c->quit_button.rect,enabled);add(c,4,&c->mode_toggle_button.rect,enabled);
    add(c,5,&c->space_toggle_button.rect,enabled);add(c,6,&c->headless_toggle_button.rect,enabled);
    add(c,7,&c->input_root_edit_button.rect,enabled);add(c,8,&c->input_root_folder_button.rect,enabled);
    add(c,9,&c->output_root_edit_button.rect,enabled);add(c,10,&c->output_root_folder_button.rect,enabled);
    add(c,11,&c->output_root_show_button.rect,enabled);
    int width=c->window_lifecycle.observed.logical_width;
    if (width>0) {
        SDL_Rect inspect={width-240,0,240,38};
        add(c,17,&inspect,enabled);
    }
    if (menu_warm_start_controls_visible(c)) {
        add(c,12,&c->warm_start_edit_button.rect,enabled);add(c,13,&c->warm_start_file_button.rect,enabled);
    }
    if (menu_showing_retained_catalog(c)) {
        add(c,14,&c->duplicate_button.rect,enabled);
        if (menu_has_scene_project_cache_status(c)) {
            add(c,15,&c->cache_command_button.rect,enabled);add(c,16,&c->cache_show_button.rect,enabled);
        }
    }
    if (c->settings_shell.provider != MENU_SETTINGS_PROVIDER_STRUCTURAL) {
        MenuSettingsFieldLayout fields[MENU_SETTINGS_FIELD_COUNT];
        size_t n=menu_settings_layout_build_field_layouts(c,fields,MENU_SETTINGS_FIELD_COUNT);
        for (size_t i=0;i<n;i++) {
            const MenuSettingsFieldDef *def=menu_settings_schema_field(fields[i].field);
            if (!def || def->runtime_display_only) continue;
            add(c,100+2*(uint64_t)fields[i].field,&fields[i].dec_rect,enabled);
            add(c,101+2*(uint64_t)fields[i].field,&fields[i].inc_rect,enabled);
        }
        add(c,300,&c->volume_toggle_rect,enabled);add(c,301,&c->render_toggle_rect,enabled);
        add(c,302,&c->blur_toggle_rect,enabled);add(c,303,&c->atmospheric_initial_state_toggle_rect,enabled);
        add(c,400,&c->settings_apply_button.rect,enabled);add(c,401,&c->settings_save_button.rect,enabled);
        add(c,402,&c->settings_reset_button.rect,enabled);add(c,403,&c->settings_restore_saved_button.rect,enabled);
        add(c,404,&c->settings_defaults_button.rect,enabled);
    }
    if (c->library && !menu_showing_retained_catalog(c)) {
        int count=menu_visible_entry_count(c);
        KitRenderRect list_clip={(float)c->list_rect.x,(float)c->list_rect.y,
            (float)c->list_rect.w,(float)c->list_rect.h};
        for (int row=0;row<=count;row++) {
            SDL_Rect rect;
            bool creating=row==count;
            if (!menu_preset_row_rect(c,row,creating,&rect)) continue;
            int slot=creating ? -1 : menu_slot_index_from_visible_row(c,row);
            if (!creating && slot<0) continue;
            if (!creating) rect=menu_preset_delete_button_rect(&rect);
            KitRenderRect bounds={(float)rect.x,(float)rect.y,(float)rect.w,(float)rect.h};
            (void)kit_ui_surface_register(&c->shared_controls,
                (KitUiSurfaceKey){2,creating ? 2000u : 1000u+(uint64_t)slot},
                bounds,&list_clip,enabled,NULL);
        }
    }
    if (kit_ui_surface_end(&c->shared_controls).code != CORE_OK) menu_shared_controls_cancel(c);
}
bool menu_shared_controls_contains(SceneMenuInteraction *c,int x,int y) {
    if (!c) return false;
    menu_shared_controls_sync(c);
    for (uint32_t i=0;i<c->shared_controls.count;i++) {
        KitRenderRect r=c->shared_controls.controls[i].bounds;
        if (x>=r.x && y>=r.y && x<r.x+r.width && y<r.y+r.height) return true;
    }
    return false;
}
KitUiButtonState menu_shared_controls_state(const SceneMenuInteraction *c,const SDL_Rect *r,bool selected) {
    KitUiButtonState state={0};state.selected=selected;
    if (!c || !r) return state;
    for (uint32_t i=0;i<c->shared_controls.count;i++) {
        const KitUiInteractionControl *control=&c->shared_controls.controls[i];
        KitRenderRect b=control->bounds;
        if (b.x==r->x && b.y==r->y && b.width==r->w && b.height==r->h)
            return kit_ui_interaction_button_state(&c->shared_controls.interaction,control,selected);
    }
    return state;
}
bool menu_shared_controls_event(SceneMenuInteraction *c,const SDL_Event *native) {
    if (!c || !native) return false;
    menu_shared_controls_sync(c);
    if (native->type==SDL_MOUSEWHEEL || c->scrollbar_dragging || c->workspace_authoring.active) {
        menu_shared_controls_cancel(c);return false;
    }
    if ((native->type==SDL_KEYDOWN || native->type==SDL_KEYUP) && text_active(c)) {
        menu_shared_controls_cancel(c);return false;
    }
    KitUiInteractionEvent event;KitUiInteractionResult result;
    if (!kit_ui_interaction_event_from_sdl(native,&event)) {
        if (native->type==SDL_KEYDOWN || native->type==SDL_TEXTINPUT) menu_shared_controls_cancel(c);
        return false;
    }
    if (kit_ui_surface_route(&c->shared_controls,&event,&result).code!=CORE_OK) {
        menu_shared_controls_cancel(c);return true;
    }
    if (result.activated_id) {
        for (uint32_t i=0;i<c->shared_controls.count;i++) {
            const KitUiInteractionControl *control=&c->shared_controls.controls[i];
            if (control->id!=result.activated_id) continue;
            if (!kit_ui_surface_take_activation(&c->shared_controls,control->id)) break;
            InputPointerState pointer={.x=(int)(control->bounds.x+control->bounds.width/2),
                .y=(int)(control->bounds.y+control->bounds.height/2),.button=SDL_BUTTON_LEFT};
            if (c->shared_controls.keys[i].domain==1 && c->shared_controls.keys[i].value==17) {
                menu_key_down(c,SDLK_F8,KMOD_NONE);
            } else {
                c->shared_control_dispatch=true;
                menu_pointer_up(c,&pointer);
                c->shared_control_dispatch=false;
            }
            break;
        }
    }
    return result.consumed != 0;
}
