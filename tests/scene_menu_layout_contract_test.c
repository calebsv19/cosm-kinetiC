#include "app/app_config.h"
#include "app/menu/menu_settings_draft.h"
#include "app/menu/menu_settings_schema.h"
#include "app/scene_menu_layout_helpers.h"

#include <stdio.h>
#include <string.h>

bool menu_showing_retained_catalog(const SceneMenuInteraction *ctx) {
    return ctx && ctx->cfg &&
           ctx->cfg->space_mode == SPACE_MODE_3D &&
           ctx->active_mode != SIM_MODE_WATER;
}

bool menu_warm_start_controls_visible(const SceneMenuInteraction *ctx) {
    (void)ctx;
    return false;
}

SpaceMode menu_normalize_space_mode(SpaceMode mode) {
    return mode == SPACE_MODE_3D ? SPACE_MODE_3D : SPACE_MODE_2D;
}

void menu_clamp_grid_size(AppConfig *cfg) {
    if (!cfg) return;
    if (cfg->grid_w < 32) cfg->grid_w = 32;
    if (cfg->grid_h < 32) cfg->grid_h = 32;
    if (cfg->grid_d < 0) cfg->grid_d = 0;
}

static bool rect_has_area(SDL_Rect rect) {
    return rect.w > 0 && rect.h > 0;
}

static bool same_row(SDL_Rect a, SDL_Rect b) {
    return a.y == b.y && a.h == b.h;
}

static bool rects_overlap(SDL_Rect a, SDL_Rect b) {
    if (!rect_has_area(a) || !rect_has_area(b)) return false;
    return a.x < b.x + b.w &&
           a.x + a.w > b.x &&
           a.y < b.y + b.h &&
           a.y + a.h > b.y;
}

static SceneMenuInteraction make_context(AppConfig *cfg,
                                         SceneMenuSelection *selection,
                                         SimulationMode mode,
                                         SpaceMode space_mode,
                                         bool scene_project_mode) {
    SceneMenuInteraction ctx;
    memset(&ctx, 0, sizeof(ctx));
    cfg->space_mode = space_mode;
    cfg->sim_mode = mode;
    ctx.cfg = cfg;
    ctx.selection = selection;
    ctx.active_mode = mode;
    if (scene_project_mode) {
        ctx.scene_project_cache_status.is_scene_project = true;
        snprintf(ctx.scene_project_cache_status.project_root,
                 sizeof(ctx.scene_project_cache_status.project_root),
                 "/tmp/codework_scene_project");
        snprintf(ctx.scene_project_cache_status.summary,
                 sizeof(ctx.scene_project_cache_status.summary),
                 "Scene project cache: none yet.");
    }
    menu_settings_shell_init(&ctx.settings_shell,
                             cfg,
                             selection,
                             NULL,
                             mode,
                             space_mode);
    return ctx;
}

static bool validate_layout_for_size(int width, int height, bool scene_project_mode) {
    AppConfig cfg = app_config_default();
    SceneMenuSelection selection = {0};
    SceneMenuInteraction ctx = make_context(&cfg,
                                            &selection,
                                            SIM_MODE_WIND_TUNNEL,
                                            SPACE_MODE_3D,
                                            scene_project_mode);
    char error[160];
    scene_menu_update_dynamic_layout(&ctx, width, height);
    if (!scene_menu_layout_validate_no_overlap(&ctx, error, sizeof(error))) {
        fprintf(stderr, "scene_menu_layout_contract_test: %s at %dx%d\n", error, width, height);
        fprintf(stderr,
                "  settings=(%d,%d %dx%d) io=(%d,%d %dx%d) dup=(%d,%d %dx%d) edit=(%d,%d %dx%d) start=(%d,%d %dx%d)\n",
                ctx.config_panel_rect.x,
                ctx.config_panel_rect.y,
                ctx.config_panel_rect.w,
                ctx.config_panel_rect.h,
                ctx.data_io_panel_rect.x,
                ctx.data_io_panel_rect.y,
                ctx.data_io_panel_rect.w,
                ctx.data_io_panel_rect.h,
                ctx.duplicate_button.rect.x,
                ctx.duplicate_button.rect.y,
                ctx.duplicate_button.rect.w,
                ctx.duplicate_button.rect.h,
                ctx.edit_button.rect.x,
                ctx.edit_button.rect.y,
                ctx.edit_button.rect.w,
                ctx.edit_button.rect.h,
                ctx.start_button.rect.x,
                ctx.start_button.rect.y,
                ctx.start_button.rect.w,
                ctx.start_button.rect.h);
        return false;
    }
    if (!same_row(ctx.volume_toggle_rect, ctx.render_toggle_rect) ||
        !same_row(ctx.volume_toggle_rect, ctx.blur_toggle_rect)) {
        fprintf(stderr, "scene_menu_layout_contract_test: render toggles are not one row\n");
        return false;
    }
    if (rects_overlap(ctx.cache_command_button.rect, ctx.duplicate_button.rect) ||
        rects_overlap(ctx.cache_command_button.rect, ctx.edit_button.rect) ||
        rects_overlap(ctx.cache_command_button.rect, ctx.start_button.rect) ||
        rects_overlap(ctx.cache_show_button.rect, ctx.duplicate_button.rect) ||
        rects_overlap(ctx.cache_show_button.rect, ctx.edit_button.rect) ||
        rects_overlap(ctx.cache_show_button.rect, ctx.start_button.rect)) {
        fprintf(stderr, "scene_menu_layout_contract_test: cache button overlaps bottom actions\n");
        return false;
    }
    if (ctx.data_io_panel_rect.y <= ctx.config_panel_rect.y + ctx.config_panel_rect.h) {
        fprintf(stderr, "scene_menu_layout_contract_test: data io panel lacks vertical separation\n");
        return false;
    }
    if (scene_project_mode) {
        if (rect_has_area(ctx.input_root_rect) || rect_has_area(ctx.output_root_rect) ||
            rect_has_area(ctx.input_root_edit_button.rect) ||
            rect_has_area(ctx.output_root_edit_button.rect) ||
            rect_has_area(ctx.input_root_folder_button.rect) ||
            rect_has_area(ctx.output_root_folder_button.rect) ||
            rect_has_area(ctx.output_root_show_button.rect)) {
            fprintf(stderr, "scene_menu_layout_contract_test: legacy root controls visible in scene project mode\n");
            return false;
        }
        if (!rect_has_area(ctx.scene_project_root_rect) ||
            !rect_has_area(ctx.scene_project_cache_target_rect) ||
            !rect_has_area(ctx.scene_project_cache_status_rect) ||
            !rect_has_area(ctx.cache_command_button.rect) ||
            !rect_has_area(ctx.cache_show_button.rect) ||
            rects_overlap(ctx.cache_command_button.rect, ctx.cache_show_button.rect)) {
            fprintf(stderr, "scene_menu_layout_contract_test: scene project controls missing\n");
            return false;
        }
    } else {
        if (!rect_has_area(ctx.input_root_rect) || !rect_has_area(ctx.output_root_rect) ||
            !rect_has_area(ctx.input_root_edit_button.rect) ||
            !rect_has_area(ctx.output_root_edit_button.rect) ||
            !rect_has_area(ctx.input_root_folder_button.rect) ||
            !rect_has_area(ctx.output_root_folder_button.rect) ||
            !rect_has_area(ctx.output_root_show_button.rect) ||
            rects_overlap(ctx.output_root_edit_button.rect, ctx.output_root_show_button.rect) ||
            rects_overlap(ctx.output_root_folder_button.rect, ctx.output_root_show_button.rect)) {
            fprintf(stderr, "scene_menu_layout_contract_test: legacy root controls missing\n");
            return false;
        }
        if (rect_has_area(ctx.scene_project_root_rect) ||
            rect_has_area(ctx.scene_project_cache_target_rect) ||
            rect_has_area(ctx.scene_project_cache_status_rect) ||
            rect_has_area(ctx.cache_command_button.rect) ||
            rect_has_area(ctx.cache_show_button.rect)) {
            fprintf(stderr, "scene_menu_layout_contract_test: scene project controls visible in legacy mode\n");
            return false;
        }
    }
    return true;
}

int main(void) {
    if (!validate_layout_for_size(MENU_WIDTH, MENU_HEIGHT, false)) return 1;
    if (!validate_layout_for_size(MENU_MIN_WIDTH, MENU_MIN_HEIGHT, false)) return 1;
    if (!validate_layout_for_size(MENU_WIDTH, MENU_HEIGHT, true)) return 1;
    if (!validate_layout_for_size(MENU_MIN_WIDTH, MENU_MIN_HEIGHT, true)) return 1;
    fprintf(stdout, "scene_menu_layout_contract_test: success\n");
    return 0;
}
