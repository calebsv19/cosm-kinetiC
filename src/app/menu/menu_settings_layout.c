#include "app/menu/menu_settings_layout.h"

#include "app/menu/menu_settings_schema.h"

enum {
    MENU_SETTINGS_PANEL_PAD = 10,
    MENU_SETTINGS_PANEL_TOP_OFFSET = 26,
    MENU_SETTINGS_PANEL_COL_GAP = 8,
    MENU_SETTINGS_PANEL_ROW_GAP = 3,
    MENU_SETTINGS_PANEL_CELL_H = 26,
    MENU_SETTINGS_PANEL_TOGGLE_H = 24,
    MENU_SETTINGS_PANEL_ACTION_H = 22,
    MENU_SETTINGS_PANEL_SECTION_GAP = 5,
    MENU_SETTINGS_PANEL_STATUS_H = 12,
    MENU_SETTINGS_PANEL_BOTTOM_PAD = 6,
    MENU_SETTINGS_PANEL_ICON_W = 24
};

static bool is_primary_grid_field(MenuSettingsFieldId field) {
    switch (field) {
    case MENU_SETTINGS_FIELD_GRID_X:
    case MENU_SETTINGS_FIELD_GRID_Y:
    case MENU_SETTINGS_FIELD_GRID_Z:
    case MENU_SETTINGS_FIELD_PHYSICS_SUBSTEPS:
    case MENU_SETTINGS_FIELD_SOLVER_ITERATIONS:
    case MENU_SETTINGS_FIELD_QUALITY_PRESET:
    case MENU_SETTINGS_FIELD_DENSITY_DIFFUSION:
    case MENU_SETTINGS_FIELD_DENSITY_DECAY:
    case MENU_SETTINGS_FIELD_BUOYANCY:
    case MENU_SETTINGS_FIELD_VELOCITY_DAMPING:
    case MENU_SETTINGS_FIELD_EMITTER_DENSITY_MULTIPLIER:
    case MENU_SETTINGS_FIELD_EMITTER_VELOCITY_MULTIPLIER:
    case MENU_SETTINGS_FIELD_EMITTER_SINK_MULTIPLIER:
    case MENU_SETTINGS_FIELD_TUNNEL_INFLOW_SPEED:
    case MENU_SETTINGS_FIELD_TUNNEL_INFLOW_DENSITY:
    case MENU_SETTINGS_FIELD_TUNNEL_VISCOSITY_SCALE:
    case MENU_SETTINGS_FIELD_WATER_LEVEL:
    case MENU_SETTINGS_FIELD_ATMOSPHERIC_SEED:
    case MENU_SETTINGS_FIELD_ATMOSPHERIC_DENSITY_SCALE:
    case MENU_SETTINGS_FIELD_ATMOSPHERIC_DENSITY_THRESHOLD:
    case MENU_SETTINGS_FIELD_ATMOSPHERIC_BASE_WIND_X:
    case MENU_SETTINGS_FIELD_ATMOSPHERIC_BASE_WIND_Y:
    case MENU_SETTINGS_FIELD_ATMOSPHERIC_BASE_WIND_Z:
    case MENU_SETTINGS_FIELD_ATMOSPHERIC_TURBULENCE:
    case MENU_SETTINGS_FIELD_ATMOSPHERIC_NOISE_SCALE:
    case MENU_SETTINGS_FIELD_ATMOSPHERIC_BAND_MIN:
    case MENU_SETTINGS_FIELD_ATMOSPHERIC_BAND_MAX:
        return true;
    default:
        return false;
    }
}

static size_t collect_primary_grid_fields(const SceneMenuInteraction *ctx,
                                          MenuSettingsFieldId *out_fields,
                                          size_t max_fields) {
    const MenuSettingsFieldId *provider_fields = NULL;
    size_t provider_count = 0;
    size_t count = 0;
    size_t i = 0;
    if (!ctx) return 0u;
    provider_count = menu_settings_schema_provider_fields(ctx->settings_shell.provider,
                                                          &provider_fields);
    for (i = 0; i < provider_count; ++i) {
        if (!is_primary_grid_field(provider_fields[i])) continue;
        if (out_fields && count < max_fields) {
            out_fields[count] = provider_fields[i];
        }
        ++count;
    }
    return count;
}

static int primary_row_count(const SceneMenuInteraction *ctx) {
    size_t field_count = collect_primary_grid_fields(ctx, NULL, 0u);
    return (int)((field_count + 1u) / 2u);
}

static bool atmospheric_initial_state_toggle_visible(const SceneMenuInteraction *ctx) {
    return ctx && ctx->settings_shell.provider == MENU_SETTINGS_PROVIDER_3D;
}

size_t menu_settings_layout_build_field_layouts(const SceneMenuInteraction *ctx,
                                                MenuSettingsFieldLayout *out_layouts,
                                                size_t max_layouts) {
    MenuSettingsFieldId fields[MENU_SETTINGS_FIELD_COUNT];
    size_t field_count = collect_primary_grid_fields(ctx,
                                                     fields,
                                                     MENU_SETTINGS_FIELD_COUNT);
    SDL_Rect panel = {0, 0, 0, 0};
    int cell_w = 0;
    size_t i = 0;
    if (!ctx) return 0u;
    panel = ctx->config_panel_rect;
    cell_w = (panel.w - MENU_SETTINGS_PANEL_PAD * 2 - MENU_SETTINGS_PANEL_COL_GAP) / 2;
    if (cell_w < 140) cell_w = 140;
    for (i = 0; i < field_count && i < max_layouts; ++i) {
        MenuSettingsFieldLayout layout = {0};
        int row = (int)(i / 2u);
        int col = (int)(i % 2u);
        int x = panel.x + MENU_SETTINGS_PANEL_PAD + col * (cell_w + MENU_SETTINGS_PANEL_COL_GAP);
        int y = panel.y + MENU_SETTINGS_PANEL_TOP_OFFSET +
                row * (MENU_SETTINGS_PANEL_CELL_H + MENU_SETTINGS_PANEL_ROW_GAP);
        layout.field = fields[i];
        layout.cell_rect = (SDL_Rect){x, y, cell_w, MENU_SETTINGS_PANEL_CELL_H};
        layout.dec_rect = (SDL_Rect){
            x + cell_w - MENU_SETTINGS_PANEL_ICON_W * 2 - 6,
            y + MENU_SETTINGS_PANEL_CELL_H - MENU_SETTINGS_PANEL_TOGGLE_H,
            MENU_SETTINGS_PANEL_ICON_W,
            MENU_SETTINGS_PANEL_TOGGLE_H
        };
        layout.inc_rect = (SDL_Rect){
            layout.dec_rect.x + MENU_SETTINGS_PANEL_ICON_W + 6,
            layout.dec_rect.y,
            MENU_SETTINGS_PANEL_ICON_W,
            MENU_SETTINGS_PANEL_TOGGLE_H
        };
        if (out_layouts) {
            out_layouts[i] = layout;
        }
    }
    return field_count < max_layouts ? field_count : max_layouts;
}

void menu_settings_layout_toggle_rects(const SceneMenuInteraction *ctx,
                                       SDL_Rect *volume_rect,
                                       SDL_Rect *render_rect,
                                       SDL_Rect *blur_rect,
                                       SDL_Rect *atmospheric_initial_state_rect) {
    SDL_Rect panel = {0, 0, 0, 0};
    int rows = 0;
    int cell_w = 0;
    int toggle_w = 0;
    int y = 0;
    if (!ctx) return;
    panel = ctx->config_panel_rect;
    rows = primary_row_count(ctx);
    cell_w = (panel.w - MENU_SETTINGS_PANEL_PAD * 2 - MENU_SETTINGS_PANEL_COL_GAP) / 2;
    if (cell_w < 140) cell_w = 140;
    toggle_w = (panel.w - MENU_SETTINGS_PANEL_PAD * 2 - MENU_SETTINGS_PANEL_COL_GAP * 2) / 3;
    if (toggle_w < 120) toggle_w = 120;
    y = panel.y + MENU_SETTINGS_PANEL_TOP_OFFSET +
        rows * (MENU_SETTINGS_PANEL_CELL_H + MENU_SETTINGS_PANEL_ROW_GAP) +
        MENU_SETTINGS_PANEL_SECTION_GAP;
    if (atmospheric_initial_state_rect) {
        *atmospheric_initial_state_rect = (SDL_Rect){0, 0, 0, 0};
    }
    if (atmospheric_initial_state_toggle_visible(ctx)) {
        if (atmospheric_initial_state_rect) {
            *atmospheric_initial_state_rect = (SDL_Rect){
                panel.x + MENU_SETTINGS_PANEL_PAD,
                y,
                panel.w - MENU_SETTINGS_PANEL_PAD * 2,
                MENU_SETTINGS_PANEL_TOGGLE_H
            };
        }
        y += MENU_SETTINGS_PANEL_TOGGLE_H + MENU_SETTINGS_PANEL_ROW_GAP;
    }
    if (volume_rect) {
        *volume_rect = (SDL_Rect){
            panel.x + MENU_SETTINGS_PANEL_PAD,
            y,
            toggle_w,
            MENU_SETTINGS_PANEL_TOGGLE_H
        };
    }
    if (render_rect) {
        *render_rect = (SDL_Rect){
            panel.x + MENU_SETTINGS_PANEL_PAD + toggle_w + MENU_SETTINGS_PANEL_COL_GAP,
            y,
            toggle_w,
            MENU_SETTINGS_PANEL_TOGGLE_H
        };
    }
    if (blur_rect) {
        *blur_rect = (SDL_Rect){
            panel.x + MENU_SETTINGS_PANEL_PAD + (toggle_w + MENU_SETTINGS_PANEL_COL_GAP) * 2,
            y,
            panel.x + panel.w - MENU_SETTINGS_PANEL_PAD -
                (panel.x + MENU_SETTINGS_PANEL_PAD + (toggle_w + MENU_SETTINGS_PANEL_COL_GAP) * 2),
            MENU_SETTINGS_PANEL_TOGGLE_H
        };
    }
}

void menu_settings_layout_action_rects(const SceneMenuInteraction *ctx,
                                       SDL_Rect *apply_rect,
                                       SDL_Rect *save_rect,
                                       SDL_Rect *revert_rect,
                                       SDL_Rect *restore_saved_rect,
                                       SDL_Rect *defaults_rect) {
    SDL_Rect panel = {0, 0, 0, 0};
    SDL_Rect volume_rect = {0, 0, 0, 0};
    SDL_Rect render_rect = {0, 0, 0, 0};
    SDL_Rect blur_rect = {0, 0, 0, 0};
    int button_w = 0;
    int half_button_w = 0;
    int y = 0;
    if (!ctx) return;
    panel = ctx->config_panel_rect;
    menu_settings_layout_toggle_rects(ctx, &volume_rect, &render_rect, &blur_rect, NULL);
    y = blur_rect.y + blur_rect.h + MENU_SETTINGS_PANEL_SECTION_GAP;
    button_w = (panel.w - MENU_SETTINGS_PANEL_PAD * 2 - 16) / 3;
    if (button_w < 72) button_w = 72;
    half_button_w = (panel.w - MENU_SETTINGS_PANEL_PAD * 2 - 8) / 2;
    if (half_button_w < 72) half_button_w = 72;
    if (apply_rect) {
        *apply_rect = (SDL_Rect){
            panel.x + MENU_SETTINGS_PANEL_PAD,
            y,
            button_w,
            MENU_SETTINGS_PANEL_ACTION_H
        };
    }
    if (save_rect) {
        *save_rect = (SDL_Rect){
            panel.x + MENU_SETTINGS_PANEL_PAD + button_w + 8,
            y,
            button_w,
            MENU_SETTINGS_PANEL_ACTION_H
        };
    }
    if (revert_rect) {
        *revert_rect = (SDL_Rect){
            panel.x + MENU_SETTINGS_PANEL_PAD,
            y + MENU_SETTINGS_PANEL_ACTION_H + MENU_SETTINGS_PANEL_ROW_GAP,
            half_button_w,
            MENU_SETTINGS_PANEL_ACTION_H
        };
    }
    if (restore_saved_rect) {
        *restore_saved_rect = (SDL_Rect){
            panel.x + MENU_SETTINGS_PANEL_PAD + (button_w + 8) * 2,
            y,
            panel.x + panel.w - MENU_SETTINGS_PANEL_PAD -
                (panel.x + MENU_SETTINGS_PANEL_PAD + (button_w + 8) * 2),
            MENU_SETTINGS_PANEL_ACTION_H
        };
    }
    if (defaults_rect) {
        *defaults_rect = (SDL_Rect){
            panel.x + MENU_SETTINGS_PANEL_PAD + half_button_w + 8,
            y + MENU_SETTINGS_PANEL_ACTION_H + MENU_SETTINGS_PANEL_ROW_GAP,
            panel.x + panel.w - MENU_SETTINGS_PANEL_PAD -
                (panel.x + MENU_SETTINGS_PANEL_PAD + half_button_w + 8),
            MENU_SETTINGS_PANEL_ACTION_H
        };
    }
}

int menu_settings_layout_status_y(const SceneMenuInteraction *ctx) {
    SDL_Rect revert_rect = {0, 0, 0, 0};
    if (!ctx) return 0;
    menu_settings_layout_action_rects(ctx, NULL, NULL, &revert_rect, NULL, NULL);
    return revert_rect.y + revert_rect.h + 6;
}

int menu_settings_layout_panel_height(const SceneMenuInteraction *ctx) {
    int rows = primary_row_count(ctx);
    int primary_h = 0;
    int total_h = 0;
    int optional_toggle_h = 0;
    if (rows > 0) {
        primary_h = rows * MENU_SETTINGS_PANEL_CELL_H +
                    (rows - 1) * MENU_SETTINGS_PANEL_ROW_GAP;
    }
    if (atmospheric_initial_state_toggle_visible(ctx)) {
        optional_toggle_h = MENU_SETTINGS_PANEL_TOGGLE_H + MENU_SETTINGS_PANEL_ROW_GAP;
    }
    total_h = MENU_SETTINGS_PANEL_TOP_OFFSET +
              primary_h +
              MENU_SETTINGS_PANEL_SECTION_GAP +
              optional_toggle_h +
              MENU_SETTINGS_PANEL_TOGGLE_H +
              MENU_SETTINGS_PANEL_SECTION_GAP +
              MENU_SETTINGS_PANEL_ACTION_H +
              MENU_SETTINGS_PANEL_ROW_GAP +
              MENU_SETTINGS_PANEL_ACTION_H +
              MENU_SETTINGS_PANEL_STATUS_H +
              MENU_SETTINGS_PANEL_BOTTOM_PAD;
    if (total_h < 180) total_h = 180;
    return total_h;
}
