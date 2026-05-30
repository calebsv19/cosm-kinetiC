#include "app/menu/menu_state.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "app/data_paths.h"
#include "app/menu/menu_settings_draft.h"

void menu_clamp_grid_size(AppConfig *cfg) {
    if (cfg->grid_w < 32) cfg->grid_w = 32;
    if (cfg->grid_h < 32) cfg->grid_h = 32;
    if (cfg->grid_w > 512) cfg->grid_w = 512;
    if (cfg->grid_h > 512) cfg->grid_h = 512;
    if (cfg->grid_d < 0) cfg->grid_d = 0;
    if (cfg->grid_d > 256) cfg->grid_d = 256;
}

void menu_begin_headless_frames_edit(SceneMenuInteraction *ctx) {
    if (!ctx) return;
    char buffer[32];
    const MenuSettingsDraft *draft = menu_settings_shell_draft(&ctx->settings_shell);
    int frames = draft ? draft->headless_frame_count : 0;
    if (frames < 0) frames = 0;
    snprintf(buffer, sizeof(buffer), "%d", frames);
    text_input_begin(&ctx->headless_frames_input, buffer, sizeof(buffer) - 1);
    ctx->editing_headless_frames = true;
}

void menu_finish_headless_frames_edit(SceneMenuInteraction *ctx, bool apply) {
    if (!ctx || !ctx->editing_headless_frames) return;
    if (apply) {
        const char *value = text_input_value(&ctx->headless_frames_input);
        if (value && value[0]) {
            char *end = NULL;
            long frames = strtol(value, &end, 10);
            if (end != value) {
                if (frames < 0) frames = 0;
                menu_settings_shell_set_headless_frame_count(&ctx->settings_shell, (int)frames);
                ctx->quality_index = ctx->settings_shell.draft.quality_index;
            }
        }
    }
    text_input_end(&ctx->headless_frames_input);
    ctx->editing_headless_frames = false;
}

void menu_begin_viscosity_edit(SceneMenuInteraction *ctx) {
    if (!ctx || !ctx->cfg) return;
    char buffer[32];
    const MenuSettingsDraft *draft = menu_settings_shell_draft(&ctx->settings_shell);
    float v = draft ? draft->velocity_damping : 0.0f;
    if (v < 0.0f) v = 0.0f;
    snprintf(buffer, sizeof(buffer), "%.8f", v);
    text_input_begin(&ctx->viscosity_input, buffer, sizeof(buffer) - 1);
    ctx->editing_viscosity = true;
}

void menu_finish_viscosity_edit(SceneMenuInteraction *ctx, bool apply) {
    if (!ctx || !ctx->editing_viscosity) return;
    if (apply && ctx->cfg) {
        const char *value = text_input_value(&ctx->viscosity_input);
        if (value && value[0]) {
            char *end = NULL;
            double v = strtod(value, &end);
            if (end != value && isfinite(v)) {
                menu_settings_shell_set_velocity_damping(&ctx->settings_shell, (float)v);
                ctx->quality_index = ctx->settings_shell.draft.quality_index;
            }
        }
    }
    text_input_end(&ctx->viscosity_input);
    ctx->editing_viscosity = false;
}

void menu_begin_inflow_edit(SceneMenuInteraction *ctx) {
    if (!ctx || !ctx->cfg) return;
    char buffer[32];
    const MenuSettingsDraft *draft = menu_settings_shell_draft(&ctx->settings_shell);
    float v = draft ? draft->tunnel_inflow_speed : 0.0f;
    snprintf(buffer, sizeof(buffer), "%.6f", v);
    text_input_begin(&ctx->inflow_input, buffer, sizeof(buffer) - 1);
    ctx->editing_inflow = true;
}

void menu_finish_inflow_edit(SceneMenuInteraction *ctx, bool apply) {
    if (!ctx || !ctx->editing_inflow) return;
    if (apply && ctx->cfg) {
        const char *value = text_input_value(&ctx->inflow_input);
        if (value && value[0]) {
            char *end = NULL;
            double v = strtod(value, &end);
            if (end != value && isfinite(v)) {
                menu_settings_shell_set_tunnel_inflow_speed(&ctx->settings_shell, (float)v);
                ctx->quality_index = ctx->settings_shell.draft.quality_index;
            }
        }
    }
    text_input_end(&ctx->inflow_input);
    ctx->editing_inflow = false;
}

void menu_begin_input_root_edit(SceneMenuInteraction *ctx) {
    if (!ctx || !ctx->cfg) return;
    text_input_begin(&ctx->input_root_input,
                     ctx->cfg->input_root,
                     sizeof(ctx->cfg->input_root) - 1);
    ctx->editing_input_root = true;
}

void menu_finish_input_root_edit(SceneMenuInteraction *ctx, bool apply) {
    if (!ctx || !ctx->editing_input_root) return;
    if (apply && ctx->cfg) {
        const char *value = text_input_value(&ctx->input_root_input);
        if (value) {
            size_t start = strspn(value, " \t\r\n");
            const char *trimmed = value + start;
            if (trimmed[0] == '\0') {
                snprintf(ctx->cfg->input_root,
                         sizeof(ctx->cfg->input_root),
                         "%s",
                         physics_sim_default_input_root());
            } else {
                snprintf(ctx->cfg->input_root,
                         sizeof(ctx->cfg->input_root),
                         "%s",
                         trimmed);
            }
        }
    }
    text_input_end(&ctx->input_root_input);
    ctx->editing_input_root = false;
}

void menu_begin_output_root_edit(SceneMenuInteraction *ctx) {
    if (!ctx || !ctx->cfg) return;
    text_input_begin(&ctx->output_root_input,
                     ctx->cfg->headless_output_dir,
                     sizeof(ctx->cfg->headless_output_dir) - 1);
    ctx->editing_output_root = true;
}

void menu_finish_output_root_edit(SceneMenuInteraction *ctx, bool apply) {
    if (!ctx || !ctx->editing_output_root) return;
    if (apply && ctx->cfg) {
        const char *value = text_input_value(&ctx->output_root_input);
        if (value) {
            size_t start = strspn(value, " \t\r\n");
            const char *trimmed = value + start;
            if (trimmed[0] == '\0') {
                snprintf(ctx->cfg->headless_output_dir,
                         sizeof(ctx->cfg->headless_output_dir),
                         "%s",
                         physics_sim_default_snapshot_dir());
            } else {
                snprintf(ctx->cfg->headless_output_dir,
                         sizeof(ctx->cfg->headless_output_dir),
                         "%s",
                         trimmed);
            }
        }
    }
    text_input_end(&ctx->output_root_input);
    ctx->editing_output_root = false;
}

void menu_begin_warm_start_edit(SceneMenuInteraction *ctx) {
    if (!ctx || !ctx->cfg || !menu_warm_start_controls_visible(ctx)) return;
    text_input_begin(&ctx->warm_start_input,
                     ctx->cfg->atmospheric_warm_start_path,
                     sizeof(ctx->cfg->atmospheric_warm_start_path) - 1);
    ctx->editing_warm_start = true;
}

void menu_finish_warm_start_edit(SceneMenuInteraction *ctx, bool apply) {
    if (!ctx || !ctx->editing_warm_start) return;
    if (apply && ctx->cfg) {
        const char *value = text_input_value(&ctx->warm_start_input);
        if (value) {
            size_t start = strspn(value, " \t\r\n");
            const char *trimmed = value + start;
            if (trimmed[0] == '\0') {
                ctx->cfg->atmospheric_warm_start_path[0] = '\0';
            } else {
                snprintf(ctx->cfg->atmospheric_warm_start_path,
                         sizeof(ctx->cfg->atmospheric_warm_start_path),
                         "%s",
                         trimmed);
            }
        }
    }
    text_input_end(&ctx->warm_start_input);
    ctx->editing_warm_start = false;
}
