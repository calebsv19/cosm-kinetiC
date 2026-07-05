#include "app/menu/menu_settings_draft.h"

#include "app/atmospheric/atmospheric_field.h"
#include "app/menu/menu_mode_utils.h"
#include "app/quality_profiles.h"
#include "app/menu/menu_settings_schema.h"
#include "app/sim_runtime_3d_domain.h"
#include "app/sim_runtime_3d_solver.h"
#include "app/water_mode.h"

static int clamp_int_value(int value, int min_value, int max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static float clamp_float_value(float value, float min_value, float max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static void mark_field_dirty(MenuSettingsShellState *state, MenuSettingsFieldId field) {
    if (!state || field < 0 || field >= MENU_SETTINGS_FIELD_COUNT) return;
    state->draft.dirty_bits |= 1ull << field;
}

static int schema_clamped_int(MenuSettingsFieldId field, int value) {
    const MenuSettingsFieldDef *def = menu_settings_schema_field(field);
    if (!def) return value;
    return clamp_int_value(value, (int)def->min_value, (int)def->max_value);
}

static float schema_clamped_float(MenuSettingsFieldId field, float value) {
    const MenuSettingsFieldDef *def = menu_settings_schema_field(field);
    if (!def) return value;
    return clamp_float_value(value, (float)def->min_value, (float)def->max_value);
}

static void sync_provider(MenuSettingsShellState *state,
                          SimulationMode sim_mode,
                          SpaceMode space_mode) {
    if (!state) return;
    state->provider = menu_settings_schema_provider_for_modes(sim_mode, space_mode);
}

static int selection_quality_index_for_space(const SceneMenuSelection *selection,
                                             SpaceMode space_mode,
                                             int fallback_quality) {
    if (!selection) return fallback_quality;
    if (menu_normalize_space_mode(space_mode) == SPACE_MODE_3D) {
        return selection->quality_index_3d;
    }
    return selection->quality_index_2d;
}

static void selection_set_quality_index_for_space(SceneMenuSelection *selection,
                                                  SpaceMode space_mode,
                                                  int quality_index) {
    if (!selection) return;
    if (menu_normalize_space_mode(space_mode) == SPACE_MODE_3D) {
        selection->quality_index_3d = quality_index;
    } else {
        selection->quality_index_2d = quality_index;
    }
    selection->quality_index = quality_index;
}

static QualityProfileCatalogId catalog_for_provider(MenuSettingsProviderId provider) {
    return (provider == MENU_SETTINGS_PROVIDER_3D ||
            provider == MENU_SETTINGS_PROVIDER_ATMOSPHERIC_3D ||
            provider == MENU_SETTINGS_PROVIDER_WATER)
               ? QUALITY_PROFILE_CATALOG_3D
               : QUALITY_PROFILE_CATALOG_2D;
}

static bool provider_is_3d_like(MenuSettingsProviderId provider) {
    return provider == MENU_SETTINGS_PROVIDER_3D ||
           provider == MENU_SETTINGS_PROVIDER_ATMOSPHERIC_3D ||
           provider == MENU_SETTINGS_PROVIDER_WATER;
}

static bool provider_is_atmospheric(MenuSettingsProviderId provider) {
    return provider == MENU_SETTINGS_PROVIDER_ATMOSPHERIC_2D ||
           provider == MENU_SETTINGS_PROVIDER_ATMOSPHERIC_3D;
}

static bool provider_supports_atmospheric_initial_state(MenuSettingsProviderId provider) {
    return provider == MENU_SETTINGS_PROVIDER_3D;
}

static void load_atmospheric_draft_values(MenuSettingsDraft *draft,
                                          const FluidScenePreset *preset) {
    if (!draft) return;
    draft->atmospheric_initial_state_enabled =
        preset && preset->atmospheric_initial_state_enabled;
    if (preset &&
        (preset->domain == SCENE_DOMAIN_ATMOSPHERIC ||
         preset->atmospheric_initial_state_enabled)) {
        draft->atmosphere = preset->atmosphere;
    } else {
        draft->atmosphere = atmospheric_preset_default_settings();
    }
    atmospheric_preset_sanitize(&draft->atmosphere);
    draft->atmosphere.enabled = true;
}

static bool atmosphere_matches_preset(const MenuSettingsDraft *draft,
                                      const FluidScenePreset *preset) {
    AtmosphericPresetSettings expected = {0};
    AtmosphericPresetSettings current = {0};
    if (!draft) return true;
    expected = draft->atmosphere;
    atmospheric_preset_sanitize(&expected);
    expected.enabled = true;
    if (preset && preset->domain == SCENE_DOMAIN_ATMOSPHERIC) {
        current = preset->atmosphere;
    } else {
        current = atmospheric_preset_default_settings();
    }
    atmospheric_preset_sanitize(&current);
    current.enabled = true;
    return expected.seed == current.seed &&
           expected.density_scale == current.density_scale &&
           expected.density_threshold == current.density_threshold &&
           expected.base_wind_x == current.base_wind_x &&
           expected.base_wind_y == current.base_wind_y &&
           expected.base_wind_z == current.base_wind_z &&
           expected.turbulence_strength == current.turbulence_strength &&
           expected.noise_scale == current.noise_scale &&
           expected.band_min_y == current.band_min_y &&
           expected.band_max_y == current.band_max_y;
}

static bool atmospheric_initial_state_matches_preset(const MenuSettingsDraft *draft,
                                                     const FluidScenePreset *preset) {
    bool current_enabled = preset && preset->atmospheric_initial_state_enabled;
    if (!draft) return true;
    return draft->atmospheric_initial_state_enabled == current_enabled;
}

static void load_draft_values(MenuSettingsDraft *draft,
                              const AppConfig *cfg,
                              const SceneMenuSelection *selection,
                              const FluidScenePreset *preset,
                              SpaceMode space_mode) {
    if (!draft || !cfg) return;
    draft->grid_x = cfg->grid_w;
    draft->grid_y = cfg->grid_h;
    draft->grid_z = cfg->grid_d;
    draft->quality_index = selection_quality_index_for_space(selection,
                                                             space_mode,
                                                             cfg->quality_index);
    draft->physics_substeps = cfg->physics_substeps;
    draft->fluid_solver_iterations = cfg->fluid_solver_iterations;
    draft->headless_frame_count = cfg->headless_frame_count;
    draft->density_diffusion = cfg->density_diffusion;
    draft->density_decay = cfg->density_decay;
    draft->fluid_buoyancy_force = cfg->fluid_buoyancy_force;
    draft->emitter_density_multiplier = cfg->emitter_density_multiplier;
    draft->emitter_velocity_multiplier = cfg->emitter_velocity_multiplier;
    draft->emitter_sink_multiplier = cfg->emitter_sink_multiplier;
    draft->tunnel_inflow_speed = cfg->tunnel_inflow_speed;
    draft->tunnel_inflow_density = cfg->tunnel_inflow_density;
    draft->tunnel_viscosity_scale = cfg->tunnel_viscosity_scale;
    draft->water_level = water_mode_level_clamp(cfg->water_level);
    draft->velocity_damping = cfg->velocity_damping;
    draft->save_volume_frames = cfg->save_volume_frames;
    draft->save_render_frames = cfg->save_render_frames;
    draft->enable_render_blur = cfg->enable_render_blur;
    load_atmospheric_draft_values(draft, preset);
    draft->dirty_bits = 0u;
}

static bool draft_matches_runtime(const MenuSettingsShellState *state,
                                  const AppConfig *cfg,
                                  const SceneMenuSelection *selection,
                                  const FluidScenePreset *preset,
                                  SpaceMode space_mode) {
    const MenuSettingsDraft *draft = state ? &state->draft : NULL;
    int selection_quality = -1;
    if (!draft || !cfg) return true;
    selection_quality = selection_quality_index_for_space(selection,
                                                         space_mode,
                                                         cfg->quality_index);
    return !(draft->grid_x != cfg->grid_w ||
             draft->grid_y != cfg->grid_h ||
             draft->grid_z != cfg->grid_d ||
             draft->quality_index != selection_quality ||
             draft->physics_substeps != cfg->physics_substeps ||
             draft->fluid_solver_iterations != cfg->fluid_solver_iterations ||
             draft->headless_frame_count != cfg->headless_frame_count ||
             draft->density_diffusion != cfg->density_diffusion ||
             draft->density_decay != cfg->density_decay ||
             draft->fluid_buoyancy_force != cfg->fluid_buoyancy_force ||
             draft->emitter_density_multiplier != cfg->emitter_density_multiplier ||
             draft->emitter_velocity_multiplier != cfg->emitter_velocity_multiplier ||
             draft->emitter_sink_multiplier != cfg->emitter_sink_multiplier ||
             draft->tunnel_inflow_speed != cfg->tunnel_inflow_speed ||
             draft->tunnel_inflow_density != cfg->tunnel_inflow_density ||
             draft->tunnel_viscosity_scale != cfg->tunnel_viscosity_scale ||
             draft->water_level != water_mode_level_clamp(cfg->water_level) ||
             draft->velocity_damping != cfg->velocity_damping ||
             draft->save_volume_frames != cfg->save_volume_frames ||
             draft->save_render_frames != cfg->save_render_frames ||
             draft->enable_render_blur != cfg->enable_render_blur ||
             (provider_supports_atmospheric_initial_state(state->provider) &&
              !atmospheric_initial_state_matches_preset(draft, preset)) ||
             (provider_is_atmospheric(state->provider) &&
              !atmosphere_matches_preset(draft, preset)));
}

static void load_draft_from_sources(MenuSettingsShellState *state,
                                    const AppConfig *cfg,
                                    const SceneMenuSelection *selection,
                                    const FluidScenePreset *preset,
                                    SimulationMode sim_mode,
                                    SpaceMode space_mode,
                                    bool update_saved_snapshot) {
    if (!state || !cfg) return;
    load_draft_values(&state->draft, cfg, selection, preset, space_mode);
    if (update_saved_snapshot) {
        load_draft_values(&state->saved_draft, cfg, selection, preset, space_mode);
        state->saved_snapshot_initialized = true;
    }
    sync_provider(state, sim_mode, space_mode);
}

const MenuSettingsDraft *menu_settings_shell_draft(const MenuSettingsShellState *state) {
    return state ? &state->draft : NULL;
}

void menu_settings_shell_init(MenuSettingsShellState *state,
                              const AppConfig *cfg,
                              const SceneMenuSelection *selection,
                              const FluidScenePreset *preset,
                              SimulationMode sim_mode,
                              SpaceMode space_mode) {
    if (!state || !cfg) return;
    load_draft_from_sources(state, cfg, selection, preset, sim_mode, space_mode, true);
    state->initialized = true;
}

void menu_settings_shell_reload_from_runtime(MenuSettingsShellState *state,
                                             const AppConfig *cfg,
                                             const SceneMenuSelection *selection,
                                             const FluidScenePreset *preset,
                                             SimulationMode sim_mode,
                                             SpaceMode space_mode) {
    if (!state || !cfg) return;
    load_draft_from_sources(state, cfg, selection, preset, sim_mode, space_mode, false);
    state->initialized = true;
}

void menu_settings_shell_capture_saved_from_runtime(MenuSettingsShellState *state,
                                                    const AppConfig *cfg,
                                                    const SceneMenuSelection *selection,
                                                    const FluidScenePreset *preset,
                                                    SimulationMode sim_mode,
                                                    SpaceMode space_mode) {
    if (!state || !cfg) return;
    load_draft_values(&state->saved_draft, cfg, selection, preset, space_mode);
    state->saved_snapshot_initialized = true;
    sync_provider(state, sim_mode, space_mode);
}

void menu_settings_shell_sync_provider(MenuSettingsShellState *state,
                                       SimulationMode sim_mode,
                                       SpaceMode space_mode) {
    sync_provider(state, sim_mode, space_mode);
}

void menu_settings_shell_sync_quality_context(MenuSettingsShellState *state,
                                              const SceneMenuSelection *selection,
                                              SpaceMode space_mode) {
    if (!state) return;
    state->draft.quality_index = selection_quality_index_for_space(selection,
                                                                   space_mode,
                                                                   state->draft.quality_index);
}

void menu_settings_shell_load_defaults(MenuSettingsShellState *state,
                                       SimulationMode sim_mode,
                                       SpaceMode space_mode) {
    AppConfig cfg = app_config_default();
    SceneMenuSelection selection = {0};
    if (!state) return;
    selection.quality_index = cfg.quality_index;
    selection.quality_index_2d = cfg.quality_index;
    selection.quality_index_3d = cfg.quality_index;
    selection.headless_frame_count = cfg.headless_frame_count;
    selection.tunnel_inflow_speed = cfg.tunnel_inflow_speed;
    {
        FluidScenePreset preset = {0};
        if (sim_mode == SIM_MODE_ATMOSPHERIC) {
            preset.domain = SCENE_DOMAIN_ATMOSPHERIC;
        } else if (sim_mode == SIM_MODE_WATER) {
            preset.domain = SCENE_DOMAIN_WATER;
            preset.dimension_mode = SCENE_DIMENSION_MODE_3D;
        } else {
            preset.domain = SCENE_DOMAIN_BOX;
        }
        preset.atmosphere = atmospheric_preset_default_settings();
        load_draft_from_sources(state, &cfg, &selection, &preset, sim_mode, space_mode, false);
    }
    state->initialized = true;
}

void menu_settings_shell_apply_to_runtime(const MenuSettingsShellState *state,
                                          AppConfig *cfg,
                                          SceneMenuSelection *selection,
                                          FluidScenePreset *preset) {
    if (!state || !cfg) return;
    cfg->grid_w = state->draft.grid_x;
    cfg->grid_h = state->draft.grid_y;
    cfg->grid_d = state->draft.grid_z;
    cfg->quality_index = state->draft.quality_index;
    cfg->physics_substeps = state->draft.physics_substeps;
    cfg->fluid_solver_iterations = state->draft.fluid_solver_iterations;
    cfg->headless_frame_count = state->draft.headless_frame_count;
    cfg->density_diffusion = state->draft.density_diffusion;
    cfg->density_decay = state->draft.density_decay;
    cfg->fluid_buoyancy_force = state->draft.fluid_buoyancy_force;
    cfg->emitter_density_multiplier = state->draft.emitter_density_multiplier;
    cfg->emitter_velocity_multiplier = state->draft.emitter_velocity_multiplier;
    cfg->emitter_sink_multiplier = state->draft.emitter_sink_multiplier;
    cfg->tunnel_inflow_speed = state->draft.tunnel_inflow_speed;
    cfg->tunnel_inflow_density = state->draft.tunnel_inflow_density;
    cfg->tunnel_viscosity_scale = state->draft.tunnel_viscosity_scale;
    cfg->water_level = water_mode_level_clamp(state->draft.water_level);
    cfg->velocity_damping = state->draft.velocity_damping;
    cfg->save_volume_frames = state->draft.save_volume_frames;
    cfg->save_render_frames = state->draft.save_render_frames;
    cfg->enable_render_blur = state->draft.enable_render_blur;
    menu_clamp_grid_size(cfg);
    if (selection) {
        selection_set_quality_index_for_space(selection, cfg->space_mode, state->draft.quality_index);
        selection->headless_frame_count = state->draft.headless_frame_count;
        selection->tunnel_inflow_speed = state->draft.tunnel_inflow_speed;
    }
    if (provider_is_atmospheric(state->provider) && preset) {
        preset->domain = SCENE_DOMAIN_ATMOSPHERIC;
        preset->dimension_mode = menu_normalize_space_mode(cfg->space_mode) == SPACE_MODE_3D
                                     ? SCENE_DIMENSION_MODE_3D
                                     : SCENE_DIMENSION_MODE_2D;
        preset->atmospheric_initial_state_enabled = false;
        preset->atmosphere = state->draft.atmosphere;
        preset->atmosphere.enabled = true;
        atmospheric_preset_sanitize(&preset->atmosphere);
    } else if (state->provider == MENU_SETTINGS_PROVIDER_WATER && preset) {
        WaterModeConfig water = water_mode_config_default(cfg);
        water_mode_apply_preset(&water, preset);
    } else if (provider_supports_atmospheric_initial_state(state->provider) && preset) {
        preset->atmospheric_initial_state_enabled =
            state->draft.atmospheric_initial_state_enabled;
        if (preset->atmospheric_initial_state_enabled) {
            preset->atmosphere = state->draft.atmosphere;
            preset->atmosphere.enabled = true;
            atmospheric_preset_sanitize(&preset->atmosphere);
        }
    }
}

bool menu_settings_shell_is_dirty(const MenuSettingsShellState *state,
                                  const AppConfig *cfg,
                                  const SceneMenuSelection *selection,
                                  const FluidScenePreset *preset) {
    if (!state || !cfg) return false;
    return !draft_matches_runtime(state,
                                  cfg,
                                  selection,
                                  preset,
                                  cfg->space_mode);
}

bool menu_settings_shell_saved_differs_from_runtime(const MenuSettingsShellState *state,
                                                    const AppConfig *cfg,
                                                    const SceneMenuSelection *selection,
                                                    const FluidScenePreset *preset) {
    MenuSettingsShellState saved_state = {0};
    if (!state || !cfg || !state->saved_snapshot_initialized) return false;
    saved_state = *state;
    saved_state.draft = state->saved_draft;
    return !draft_matches_runtime(&saved_state,
                                  cfg,
                                  selection,
                                  preset,
                                  cfg->space_mode);
}

void menu_settings_shell_restore_saved_to_draft(MenuSettingsShellState *state,
                                                SimulationMode sim_mode,
                                                SpaceMode space_mode) {
    if (!state || !state->saved_snapshot_initialized) return;
    state->draft = state->saved_draft;
    state->draft.dirty_bits = 0u;
    sync_provider(state, sim_mode, space_mode);
}

void menu_settings_shell_apply_saved_to_runtime(const MenuSettingsShellState *state,
                                                AppConfig *cfg,
                                                SceneMenuSelection *selection,
                                                FluidScenePreset *preset) {
    MenuSettingsShellState temp = {0};
    if (!state || !cfg || !state->saved_snapshot_initialized) return;
    temp.draft = state->saved_draft;
    temp.provider = state->provider;
    menu_settings_shell_apply_to_runtime(&temp, cfg, selection, preset);
}

void menu_settings_shell_set_custom_quality(MenuSettingsShellState *state) {
    if (!state) return;
    state->draft.quality_index = -1;
    mark_field_dirty(state, MENU_SETTINGS_FIELD_QUALITY_PRESET);
}

void menu_settings_shell_apply_quality(MenuSettingsShellState *state, int index) {
    AppConfig cfg = {0};
    QualityProfileCatalogId catalog = QUALITY_PROFILE_CATALOG_2D;
    if (!state) return;
    cfg.grid_w = state->draft.grid_x;
    cfg.grid_h = state->draft.grid_y;
    cfg.grid_d = state->draft.grid_z;
    cfg.physics_substeps = state->draft.physics_substeps;
    cfg.fluid_solver_iterations = state->draft.fluid_solver_iterations;
    cfg.enable_render_blur = state->draft.enable_render_blur;
    catalog = catalog_for_provider(state->provider);
    quality_profile_apply_for_catalog(&cfg, catalog, index);
    state->draft.grid_x = cfg.grid_w;
    state->draft.grid_y = cfg.grid_h;
    state->draft.physics_substeps = cfg.physics_substeps;
    state->draft.fluid_solver_iterations = cfg.fluid_solver_iterations;
    state->draft.enable_render_blur = cfg.enable_render_blur;
    state->draft.quality_index = cfg.quality_index;
    mark_field_dirty(state, MENU_SETTINGS_FIELD_GRID_X);
    mark_field_dirty(state, MENU_SETTINGS_FIELD_GRID_Y);
    mark_field_dirty(state, MENU_SETTINGS_FIELD_PHYSICS_SUBSTEPS);
    mark_field_dirty(state, MENU_SETTINGS_FIELD_SOLVER_ITERATIONS);
    mark_field_dirty(state, MENU_SETTINGS_FIELD_ENABLE_RENDER_BLUR);
    mark_field_dirty(state, MENU_SETTINGS_FIELD_QUALITY_PRESET);
}

void menu_settings_shell_nudge_field(MenuSettingsShellState *state,
                                     MenuSettingsFieldId field,
                                     int direction) {
    const MenuSettingsFieldDef *def = NULL;
    if (!state || direction == 0) return;
    def = menu_settings_schema_field(field);
    if (!def) return;

    switch (field) {
    case MENU_SETTINGS_FIELD_GRID_X:
        state->draft.grid_x = schema_clamped_int(field,
                                                 state->draft.grid_x +
                                                     (int)(def->step * direction));
        if (provider_is_3d_like(state->provider)) {
            state->draft.grid_x =
                sim_runtime_3d_applied_major_axis_cells_for_requested(state->draft.grid_x);
        }
        if (!provider_is_3d_like(state->provider)) {
            state->draft.grid_y = schema_clamped_int(MENU_SETTINGS_FIELD_GRID_Y,
                                                     state->draft.grid_y +
                                                         (int)(def->step * direction));
        }
        menu_settings_shell_set_custom_quality(state);
        mark_field_dirty(state, MENU_SETTINGS_FIELD_GRID_X);
        if (!provider_is_3d_like(state->provider)) {
            mark_field_dirty(state, MENU_SETTINGS_FIELD_GRID_Y);
        }
        return;
    case MENU_SETTINGS_FIELD_GRID_Y:
        state->draft.grid_y = schema_clamped_int(field,
                                                 state->draft.grid_y +
                                                     (int)(def->step * direction));
        if (provider_is_3d_like(state->provider)) {
            state->draft.grid_y =
                sim_runtime_3d_applied_major_axis_cells_for_requested(state->draft.grid_y);
        }
        menu_settings_shell_set_custom_quality(state);
        mark_field_dirty(state, MENU_SETTINGS_FIELD_GRID_Y);
        return;
    case MENU_SETTINGS_FIELD_GRID_Z:
        state->draft.grid_z = schema_clamped_int(field,
                                                 state->draft.grid_z +
                                                     (int)(def->step * direction));
        if (provider_is_3d_like(state->provider) && state->draft.grid_z > 0) {
            state->draft.grid_z =
                sim_runtime_3d_applied_depth_cells_for_requested(state->draft.grid_z);
        }
        menu_settings_shell_set_custom_quality(state);
        mark_field_dirty(state, MENU_SETTINGS_FIELD_GRID_Z);
        return;
    case MENU_SETTINGS_FIELD_PHYSICS_SUBSTEPS:
        state->draft.physics_substeps =
            schema_clamped_int(field,
                               state->draft.physics_substeps +
                                   (int)(def->step * direction));
        menu_settings_shell_set_custom_quality(state);
        mark_field_dirty(state, field);
        return;
    case MENU_SETTINGS_FIELD_SOLVER_ITERATIONS:
        state->draft.fluid_solver_iterations =
            schema_clamped_int(field,
                               state->draft.fluid_solver_iterations +
                                   (int)(def->step * direction));
        if (provider_is_3d_like(state->provider)) {
            state->draft.fluid_solver_iterations =
                sim_runtime_3d_solver_iterations_for_requested(
                    state->draft.fluid_solver_iterations);
        }
        menu_settings_shell_set_custom_quality(state);
        mark_field_dirty(state, field);
        return;
    case MENU_SETTINGS_FIELD_HEADLESS_FRAME_COUNT:
        state->draft.headless_frame_count =
            schema_clamped_int(field,
                               state->draft.headless_frame_count +
                                   (int)(def->step * direction));
        mark_field_dirty(state, field);
        return;
    case MENU_SETTINGS_FIELD_DENSITY_DIFFUSION:
        state->draft.density_diffusion =
            schema_clamped_float(field,
                                 state->draft.density_diffusion +
                                     (float)def->step * (float)direction);
        mark_field_dirty(state, field);
        return;
    case MENU_SETTINGS_FIELD_DENSITY_DECAY:
        state->draft.density_decay =
            schema_clamped_float(field,
                                 state->draft.density_decay +
                                     (float)def->step * (float)direction);
        mark_field_dirty(state, field);
        return;
    case MENU_SETTINGS_FIELD_BUOYANCY:
        state->draft.fluid_buoyancy_force =
            schema_clamped_float(field,
                                 state->draft.fluid_buoyancy_force +
                                     (float)def->step * (float)direction);
        mark_field_dirty(state, field);
        return;
    case MENU_SETTINGS_FIELD_EMITTER_DENSITY_MULTIPLIER:
        state->draft.emitter_density_multiplier =
            schema_clamped_float(field,
                                 state->draft.emitter_density_multiplier +
                                     (float)def->step * (float)direction);
        mark_field_dirty(state, field);
        return;
    case MENU_SETTINGS_FIELD_EMITTER_VELOCITY_MULTIPLIER:
        state->draft.emitter_velocity_multiplier =
            schema_clamped_float(field,
                                 state->draft.emitter_velocity_multiplier +
                                     (float)def->step * (float)direction);
        mark_field_dirty(state, field);
        return;
    case MENU_SETTINGS_FIELD_EMITTER_SINK_MULTIPLIER:
        state->draft.emitter_sink_multiplier =
            schema_clamped_float(field,
                                 state->draft.emitter_sink_multiplier +
                                     (float)def->step * (float)direction);
        mark_field_dirty(state, field);
        return;
    case MENU_SETTINGS_FIELD_TUNNEL_INFLOW_SPEED:
        state->draft.tunnel_inflow_speed =
            schema_clamped_float(field,
                                 state->draft.tunnel_inflow_speed +
                                     (float)def->step * (float)direction);
        mark_field_dirty(state, field);
        return;
    case MENU_SETTINGS_FIELD_TUNNEL_INFLOW_DENSITY:
        state->draft.tunnel_inflow_density =
            schema_clamped_float(field,
                                 state->draft.tunnel_inflow_density +
                                     (float)def->step * (float)direction);
        mark_field_dirty(state, field);
        return;
    case MENU_SETTINGS_FIELD_TUNNEL_VISCOSITY_SCALE:
        state->draft.tunnel_viscosity_scale =
            schema_clamped_float(field,
                                 state->draft.tunnel_viscosity_scale +
                                     (float)def->step * (float)direction);
        mark_field_dirty(state, field);
        return;
    case MENU_SETTINGS_FIELD_WATER_LEVEL:
        state->draft.water_level =
            schema_clamped_float(field,
                                 state->draft.water_level +
                                     (float)def->step * (float)direction);
        mark_field_dirty(state, field);
        return;
    case MENU_SETTINGS_FIELD_VELOCITY_DAMPING:
        state->draft.velocity_damping =
            schema_clamped_float(field,
                                 state->draft.velocity_damping +
                                     (float)def->step * (float)direction);
        mark_field_dirty(state, field);
        return;
    case MENU_SETTINGS_FIELD_ATMOSPHERIC_SEED:
        state->draft.atmosphere.seed =
            (uint32_t)schema_clamped_int(field,
                                         (int)state->draft.atmosphere.seed +
                                             (int)(def->step * direction));
        mark_field_dirty(state, field);
        return;
    case MENU_SETTINGS_FIELD_ATMOSPHERIC_DENSITY_SCALE:
        state->draft.atmosphere.density_scale =
            schema_clamped_float(field,
                                 state->draft.atmosphere.density_scale +
                                     (float)def->step * (float)direction);
        mark_field_dirty(state, field);
        return;
    case MENU_SETTINGS_FIELD_ATMOSPHERIC_DENSITY_THRESHOLD:
        state->draft.atmosphere.density_threshold =
            schema_clamped_float(field,
                                 state->draft.atmosphere.density_threshold +
                                     (float)def->step * (float)direction);
        mark_field_dirty(state, field);
        return;
    case MENU_SETTINGS_FIELD_ATMOSPHERIC_BASE_WIND_X:
        state->draft.atmosphere.base_wind_x =
            schema_clamped_float(field,
                                 state->draft.atmosphere.base_wind_x +
                                     (float)def->step * (float)direction);
        mark_field_dirty(state, field);
        return;
    case MENU_SETTINGS_FIELD_ATMOSPHERIC_BASE_WIND_Y:
        state->draft.atmosphere.base_wind_y =
            schema_clamped_float(field,
                                 state->draft.atmosphere.base_wind_y +
                                     (float)def->step * (float)direction);
        mark_field_dirty(state, field);
        return;
    case MENU_SETTINGS_FIELD_ATMOSPHERIC_BASE_WIND_Z:
        state->draft.atmosphere.base_wind_z =
            schema_clamped_float(field,
                                 state->draft.atmosphere.base_wind_z +
                                     (float)def->step * (float)direction);
        mark_field_dirty(state, field);
        return;
    case MENU_SETTINGS_FIELD_ATMOSPHERIC_TURBULENCE:
        state->draft.atmosphere.turbulence_strength =
            schema_clamped_float(field,
                                 state->draft.atmosphere.turbulence_strength +
                                     (float)def->step * (float)direction);
        mark_field_dirty(state, field);
        return;
    case MENU_SETTINGS_FIELD_ATMOSPHERIC_NOISE_SCALE:
        state->draft.atmosphere.noise_scale =
            schema_clamped_float(field,
                                 state->draft.atmosphere.noise_scale +
                                     (float)def->step * (float)direction);
        mark_field_dirty(state, field);
        return;
    case MENU_SETTINGS_FIELD_ATMOSPHERIC_BAND_MIN:
        state->draft.atmosphere.band_min_y =
            schema_clamped_float(field,
                                 state->draft.atmosphere.band_min_y +
                                     (float)def->step * (float)direction);
        if (state->draft.atmosphere.band_min_y > state->draft.atmosphere.band_max_y) {
            state->draft.atmosphere.band_max_y = state->draft.atmosphere.band_min_y;
            mark_field_dirty(state, MENU_SETTINGS_FIELD_ATMOSPHERIC_BAND_MAX);
        }
        mark_field_dirty(state, field);
        return;
    case MENU_SETTINGS_FIELD_ATMOSPHERIC_BAND_MAX:
        state->draft.atmosphere.band_max_y =
            schema_clamped_float(field,
                                 state->draft.atmosphere.band_max_y +
                                     (float)def->step * (float)direction);
        if (state->draft.atmosphere.band_max_y < state->draft.atmosphere.band_min_y) {
            state->draft.atmosphere.band_min_y = state->draft.atmosphere.band_max_y;
            mark_field_dirty(state, MENU_SETTINGS_FIELD_ATMOSPHERIC_BAND_MIN);
        }
        mark_field_dirty(state, field);
        return;
    default:
        return;
    }
}

void menu_settings_shell_toggle_field(MenuSettingsShellState *state,
                                      MenuSettingsFieldId field) {
    if (!state) return;
    switch (field) {
    case MENU_SETTINGS_FIELD_SAVE_VOLUME_FRAMES:
        state->draft.save_volume_frames = !state->draft.save_volume_frames;
        break;
    case MENU_SETTINGS_FIELD_SAVE_RENDER_FRAMES:
        state->draft.save_render_frames = !state->draft.save_render_frames;
        break;
    case MENU_SETTINGS_FIELD_ENABLE_RENDER_BLUR:
        state->draft.enable_render_blur = !state->draft.enable_render_blur;
        break;
    case MENU_SETTINGS_FIELD_ATMOSPHERIC_INITIAL_STATE:
        state->draft.atmospheric_initial_state_enabled =
            !state->draft.atmospheric_initial_state_enabled;
        state->draft.atmosphere.enabled = true;
        atmospheric_preset_sanitize(&state->draft.atmosphere);
        break;
    default:
        return;
    }
    mark_field_dirty(state, field);
}

void menu_settings_shell_adjust_grid(MenuSettingsShellState *state, int delta) {
    menu_settings_shell_nudge_field(state, MENU_SETTINGS_FIELD_GRID_X, delta < 0 ? -1 : 1);
}

void menu_settings_shell_adjust_substeps(MenuSettingsShellState *state, int delta) {
    menu_settings_shell_nudge_field(state, MENU_SETTINGS_FIELD_PHYSICS_SUBSTEPS, delta < 0 ? -1 : 1);
}

void menu_settings_shell_adjust_solver_iterations(MenuSettingsShellState *state, int delta) {
    menu_settings_shell_nudge_field(state, MENU_SETTINGS_FIELD_SOLVER_ITERATIONS, delta < 0 ? -1 : 1);
}

void menu_settings_shell_toggle_volume_frames(MenuSettingsShellState *state) {
    menu_settings_shell_toggle_field(state, MENU_SETTINGS_FIELD_SAVE_VOLUME_FRAMES);
}

void menu_settings_shell_toggle_render_frames(MenuSettingsShellState *state) {
    menu_settings_shell_toggle_field(state, MENU_SETTINGS_FIELD_SAVE_RENDER_FRAMES);
}

void menu_settings_shell_set_headless_frame_count(MenuSettingsShellState *state, int frames) {
    if (!state) return;
    state->draft.headless_frame_count =
        schema_clamped_int(MENU_SETTINGS_FIELD_HEADLESS_FRAME_COUNT, frames < 0 ? 0 : frames);
    mark_field_dirty(state, MENU_SETTINGS_FIELD_HEADLESS_FRAME_COUNT);
}

void menu_settings_shell_set_tunnel_inflow_speed(MenuSettingsShellState *state, float speed) {
    if (!state) return;
    state->draft.tunnel_inflow_speed =
        schema_clamped_float(MENU_SETTINGS_FIELD_TUNNEL_INFLOW_SPEED, speed);
    mark_field_dirty(state, MENU_SETTINGS_FIELD_TUNNEL_INFLOW_SPEED);
}

void menu_settings_shell_set_velocity_damping(MenuSettingsShellState *state, float damping) {
    if (!state) return;
    state->draft.velocity_damping =
        schema_clamped_float(MENU_SETTINGS_FIELD_VELOCITY_DAMPING, damping);
    mark_field_dirty(state, MENU_SETTINGS_FIELD_VELOCITY_DAMPING);
}
