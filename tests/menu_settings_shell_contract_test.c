#include "app/app_config.h"
#include "app/atmospheric/atmospheric_field.h"
#include "app/menu/menu_settings_draft.h"
#include "app/menu/menu_settings_schema.h"
#include "app/scene_menu.h"
#include "app/sim_runtime_3d_domain.h"
#include "app/sim_runtime_3d_solver.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

SpaceMode menu_normalize_space_mode(SpaceMode mode) {
    return mode == SPACE_MODE_3D ? SPACE_MODE_3D : SPACE_MODE_2D;
}

void menu_clamp_grid_size(AppConfig *cfg) {
    if (!cfg) return;
    if (cfg->grid_w < 32) cfg->grid_w = 32;
    if (cfg->grid_h < 32) cfg->grid_h = 32;
    if (cfg->grid_d < 0) cfg->grid_d = 0;
}

static bool field_list_contains(const MenuSettingsFieldId *fields,
                                size_t count,
                                MenuSettingsFieldId field) {
    for (size_t i = 0; i < count; ++i) {
        if (fields[i] == field) {
            return true;
        }
    }
    return false;
}

static bool test_provider_selection(void) {
    return menu_settings_schema_provider_for_modes(SIM_MODE_BOX, SPACE_MODE_2D) ==
               MENU_SETTINGS_PROVIDER_2D &&
           menu_settings_schema_provider_for_modes(SIM_MODE_BOX, SPACE_MODE_3D) ==
               MENU_SETTINGS_PROVIDER_3D &&
           menu_settings_schema_provider_for_modes(SIM_MODE_WIND_TUNNEL, SPACE_MODE_2D) ==
               MENU_SETTINGS_PROVIDER_WIND &&
           menu_settings_schema_provider_for_modes(SIM_MODE_WIND_TUNNEL, SPACE_MODE_3D) ==
               MENU_SETTINGS_PROVIDER_WIND &&
           menu_settings_schema_provider_for_modes(SIM_MODE_STRUCTURAL, SPACE_MODE_2D) ==
               MENU_SETTINGS_PROVIDER_STRUCTURAL &&
           menu_settings_schema_provider_for_modes(SIM_MODE_ATMOSPHERIC, SPACE_MODE_2D) ==
               MENU_SETTINGS_PROVIDER_ATMOSPHERIC_2D &&
           menu_settings_schema_provider_for_modes(SIM_MODE_ATMOSPHERIC, SPACE_MODE_3D) ==
               MENU_SETTINGS_PROVIDER_ATMOSPHERIC_3D;
}

static bool test_provider_field_sets(void) {
    const MenuSettingsFieldId *fields = NULL;
    size_t count = 0;

    count = menu_settings_schema_provider_fields(MENU_SETTINGS_PROVIDER_2D, &fields);
    if (!(field_list_contains(fields, count, MENU_SETTINGS_FIELD_GRID_X) &&
          !field_list_contains(fields, count, MENU_SETTINGS_FIELD_GRID_Y) &&
          !field_list_contains(fields, count, MENU_SETTINGS_FIELD_GRID_Z) &&
          field_list_contains(fields, count, MENU_SETTINGS_FIELD_BUOYANCY) &&
          !field_list_contains(fields, count, MENU_SETTINGS_FIELD_TUNNEL_INFLOW_SPEED))) {
        return false;
    }

    count = menu_settings_schema_provider_fields(MENU_SETTINGS_PROVIDER_3D, &fields);
    if (!(field_list_contains(fields, count, MENU_SETTINGS_FIELD_GRID_X) &&
          field_list_contains(fields, count, MENU_SETTINGS_FIELD_GRID_Y) &&
          field_list_contains(fields, count, MENU_SETTINGS_FIELD_GRID_Z) &&
          !field_list_contains(fields, count, MENU_SETTINGS_FIELD_3D_APPLIED_AXIS) &&
          !field_list_contains(fields, count, MENU_SETTINGS_FIELD_3D_APPLIED_DEPTH) &&
          !field_list_contains(fields, count, MENU_SETTINGS_FIELD_3D_DEPTH_POLICY))) {
        return false;
    }

    count = menu_settings_schema_provider_fields(MENU_SETTINGS_PROVIDER_WIND, &fields);
    if (!(field_list_contains(fields, count, MENU_SETTINGS_FIELD_TUNNEL_INFLOW_SPEED) &&
          field_list_contains(fields, count, MENU_SETTINGS_FIELD_TUNNEL_INFLOW_DENSITY) &&
          field_list_contains(fields, count, MENU_SETTINGS_FIELD_TUNNEL_VISCOSITY_SCALE) &&
          !field_list_contains(fields, count, MENU_SETTINGS_FIELD_GRID_Y) &&
          !field_list_contains(fields, count, MENU_SETTINGS_FIELD_BUOYANCY))) {
        return false;
    }

    count = menu_settings_schema_provider_fields(MENU_SETTINGS_PROVIDER_STRUCTURAL, &fields);
    if (!(count == 1 &&
          field_list_contains(fields, count, MENU_SETTINGS_FIELD_HEADLESS_FRAME_COUNT))) {
        return false;
    }

    count = menu_settings_schema_provider_fields(MENU_SETTINGS_PROVIDER_ATMOSPHERIC_2D, &fields);
    if (!(field_list_contains(fields, count, MENU_SETTINGS_FIELD_ATMOSPHERIC_SEED) &&
          field_list_contains(fields, count, MENU_SETTINGS_FIELD_ATMOSPHERIC_DENSITY_SCALE) &&
          field_list_contains(fields, count, MENU_SETTINGS_FIELD_ATMOSPHERIC_BASE_WIND_X) &&
          !field_list_contains(fields, count, MENU_SETTINGS_FIELD_ATMOSPHERIC_BASE_WIND_Z) &&
          !field_list_contains(fields, count, MENU_SETTINGS_FIELD_GRID_Z))) {
        return false;
    }

    count = menu_settings_schema_provider_fields(MENU_SETTINGS_PROVIDER_ATMOSPHERIC_3D, &fields);
    return field_list_contains(fields, count, MENU_SETTINGS_FIELD_ATMOSPHERIC_SEED) &&
           field_list_contains(fields, count, MENU_SETTINGS_FIELD_ATMOSPHERIC_BASE_WIND_Z) &&
           field_list_contains(fields, count, MENU_SETTINGS_FIELD_GRID_Z);
}

static bool test_space_specific_quality_context(void) {
    AppConfig cfg = app_config_default();
    SceneMenuSelection selection = {0};
    MenuSettingsShellState state = {0};

    selection.quality_index = -1;
    selection.quality_index_2d = 1;
    selection.quality_index_3d = 3;

    cfg.space_mode = SPACE_MODE_2D;
    menu_settings_shell_init(&state, &cfg, &selection, NULL, SIM_MODE_BOX, SPACE_MODE_2D);
    if (state.provider != MENU_SETTINGS_PROVIDER_2D ||
        menu_settings_shell_draft(&state)->quality_index != 1) {
        return false;
    }

    state.draft.quality_index = 2;
    cfg.space_mode = SPACE_MODE_2D;
    menu_settings_shell_apply_to_runtime(&state, &cfg, &selection, NULL);
    if (!(selection.quality_index == 2 &&
          selection.quality_index_2d == 2 &&
          selection.quality_index_3d == 3)) {
        return false;
    }

    cfg.space_mode = SPACE_MODE_3D;
    menu_settings_shell_reload_from_runtime(&state, &cfg, &selection, NULL, SIM_MODE_BOX, SPACE_MODE_3D);
    if (state.provider != MENU_SETTINGS_PROVIDER_3D ||
        menu_settings_shell_draft(&state)->quality_index != 3) {
        return false;
    }

    state.draft.quality_index = 0;
    cfg.space_mode = SPACE_MODE_3D;
    menu_settings_shell_apply_to_runtime(&state, &cfg, &selection, NULL);
    return selection.quality_index == 0 &&
           selection.quality_index_2d == 2 &&
           selection.quality_index_3d == 0;
}

static bool test_grid_nudge_behavior_by_provider(void) {
    AppConfig cfg = app_config_default();
    SceneMenuSelection selection = {0};
    MenuSettingsShellState state = {0};
    const MenuSettingsDraft *draft = NULL;

    cfg.grid_w = 128;
    cfg.grid_h = 128;
    cfg.grid_d = 0;
    menu_settings_shell_init(&state, &cfg, &selection, NULL, SIM_MODE_BOX, SPACE_MODE_2D);
    menu_settings_shell_nudge_field(&state, MENU_SETTINGS_FIELD_GRID_X, 1);
    draft = menu_settings_shell_draft(&state);
    if (!(draft->grid_x == 160 &&
          draft->grid_y == 160 &&
          draft->quality_index == -1)) {
        return false;
    }

    cfg.grid_w = 128;
    cfg.grid_h = 192;
    cfg.grid_d = 0;
    selection.quality_index_3d = 2;
    menu_settings_shell_reload_from_runtime(&state, &cfg, &selection, NULL, SIM_MODE_BOX, SPACE_MODE_3D);
    menu_settings_shell_nudge_field(&state, MENU_SETTINGS_FIELD_GRID_X, 1);
    draft = menu_settings_shell_draft(&state);
    if (!(draft->grid_x == sim_runtime_3d_applied_major_axis_cells_for_requested(160) &&
          draft->grid_y == 192 &&
          draft->quality_index == -1)) {
        return false;
    }

    menu_settings_shell_nudge_field(&state, MENU_SETTINGS_FIELD_GRID_Y, -1);
    draft = menu_settings_shell_draft(&state);
    if (!(draft->grid_x == sim_runtime_3d_applied_major_axis_cells_for_requested(160) &&
          draft->grid_y == sim_runtime_3d_applied_major_axis_cells_for_requested(160))) {
        return false;
    }

    return true;
}

static bool test_3d_depth_and_solver_clamps(void) {
    AppConfig cfg = app_config_default();
    SceneMenuSelection selection = {0};
    MenuSettingsShellState state = {0};
    const MenuSettingsDraft *draft = NULL;

    cfg.grid_w = 96;
    cfg.grid_h = 96;
    cfg.grid_d = 0;
    cfg.fluid_solver_iterations = 47;
    menu_settings_shell_init(&state, &cfg, &selection, NULL, SIM_MODE_BOX, SPACE_MODE_3D);

    menu_settings_shell_nudge_field(&state, MENU_SETTINGS_FIELD_GRID_Z, 1);
    draft = menu_settings_shell_draft(&state);
    if (draft->grid_z != sim_runtime_3d_applied_depth_cells_for_requested(16)) {
        return false;
    }

    menu_settings_shell_nudge_field(&state, MENU_SETTINGS_FIELD_SOLVER_ITERATIONS, 1);
    draft = menu_settings_shell_draft(&state);
    return draft->fluid_solver_iterations ==
           sim_runtime_3d_solver_iterations_for_requested(48);
}

static bool test_atmospheric_draft_roundtrip(void) {
    AppConfig cfg = app_config_default();
    SceneMenuSelection selection = {0};
    FluidScenePreset preset = {0};
    MenuSettingsShellState state = {0};
    const MenuSettingsDraft *draft = NULL;

    cfg.space_mode = SPACE_MODE_3D;
    cfg.grid_w = 96;
    cfg.grid_h = 128;
    cfg.grid_d = 32;
    preset.domain = SCENE_DOMAIN_ATMOSPHERIC;
    preset.dimension_mode = SCENE_DIMENSION_MODE_3D;
    preset.atmosphere = atmospheric_preset_default_settings();
    preset.atmosphere.seed = 77u;
    preset.atmosphere.density_scale = 3.5f;

    menu_settings_shell_init(&state,
                             &cfg,
                             &selection,
                             &preset,
                             SIM_MODE_ATMOSPHERIC,
                             SPACE_MODE_3D);
    if (state.provider != MENU_SETTINGS_PROVIDER_ATMOSPHERIC_3D) {
        return false;
    }
    draft = menu_settings_shell_draft(&state);
    if (!draft ||
        draft->atmosphere.seed != 77u ||
        draft->atmosphere.density_scale != 3.5f) {
        return false;
    }

    menu_settings_shell_nudge_field(&state, MENU_SETTINGS_FIELD_ATMOSPHERIC_SEED, 1);
    menu_settings_shell_nudge_field(&state, MENU_SETTINGS_FIELD_ATMOSPHERIC_DENSITY_SCALE, 1);
    if (!menu_settings_shell_is_dirty(&state, &cfg, &selection, &preset)) {
        return false;
    }
    menu_settings_shell_apply_to_runtime(&state, &cfg, &selection, &preset);
    if (preset.domain != SCENE_DOMAIN_ATMOSPHERIC ||
        preset.dimension_mode != SCENE_DIMENSION_MODE_3D ||
        preset.atmosphere.seed != 78u ||
        preset.atmosphere.density_scale <= 3.5f) {
        return false;
    }
    return !menu_settings_shell_is_dirty(&state, &cfg, &selection, &preset);
}

int main(void) {
    if (!test_provider_selection()) {
        fprintf(stderr, "menu_settings_shell_contract_test: provider selection failed\n");
        return 1;
    }
    if (!test_provider_field_sets()) {
        fprintf(stderr, "menu_settings_shell_contract_test: provider field sets failed\n");
        return 1;
    }
    if (!test_space_specific_quality_context()) {
        fprintf(stderr, "menu_settings_shell_contract_test: space-specific quality context failed\n");
        return 1;
    }
    if (!test_grid_nudge_behavior_by_provider()) {
        fprintf(stderr, "menu_settings_shell_contract_test: grid nudge behavior failed\n");
        return 1;
    }
    if (!test_3d_depth_and_solver_clamps()) {
        fprintf(stderr, "menu_settings_shell_contract_test: 3d depth/solver clamps failed\n");
        return 1;
    }
    if (!test_atmospheric_draft_roundtrip()) {
        fprintf(stderr, "menu_settings_shell_contract_test: atmospheric draft roundtrip failed\n");
        return 1;
    }
    fprintf(stdout, "menu_settings_shell_contract_test: success\n");
    return 0;
}
