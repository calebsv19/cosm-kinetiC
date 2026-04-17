#include "app/scene_controller.h"

#include <SDL2/SDL.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "command/command_bus.h"
#include "app/scene_state.h"
#include "app/scene_presets.h"
#include "app/sim_mode.h"
#include "app/sim_runtime_backend.h"
#include "app/quality_profiles.h"
#include "app/data_paths.h"
#include "input/input.h"
#include "input/stroke_buffer.h"
#include "render/renderer_sdl.h"
#include "render/retained_runtime_scene_overlay.h"
#include "render/vk_shared_device.h"
#include "export/volume_frames.h"
#include "export/render_frames.h"
#include "timing.h"
#include "timer_hud/time_scope.h"

typedef struct CommandDispatchContext {
    SceneState *scene;
    bool        snapshot_requested;
} CommandDispatchContext;

typedef struct StrokeSampler {
    StrokeBuffer buffer;
    BrushMode    brush_mode;
    bool         pointer_down;
    float        last_emit_x;
    float        last_emit_y;
    float        current_x;
    float        current_y;
    double       accumulator;
    double       sample_interval;
    float        sample_spacing;
} StrokeSampler;

typedef struct SceneControllerRs1DiagTotals {
    uint64_t frame_count;
    uint64_t derive_ns_total;
    uint64_t submit_ns_total;
    uint64_t invalidation_reason_bits_total;
    uint64_t dispatched_commands_total;
} SceneControllerRs1DiagTotals;

typedef struct SceneControllerIr1DiagTotals {
    uint64_t frame_count;
    uint64_t routed_global_total;
    uint64_t routed_scene_total;
    uint64_t routed_fallback_total;
    uint64_t invalidation_reason_bits_total;
} SceneControllerIr1DiagTotals;

static bool scene_controller_retained_runtime_view_active(const SceneState *scene) {
    return retained_runtime_scene_overlay_active(scene);
}

static void scene_controller_runtime_pointer_down(void *user,
                                                  const InputPointerState *state) {
    SceneState *scene = (SceneState *)user;
    if (!scene || !state || !scene_controller_retained_runtime_view_active(scene)) return;

    if (state->button == SDL_BUTTON_MIDDLE) {
        (void)scene_editor_viewport_begin_navigation(&scene->runtime_viewport,
                                                     SCENE_EDITOR_VIEWPORT_NAV_PAN,
                                                     state->x,
                                                     state->y);
    } else if (state->button == SDL_BUTTON_LEFT && scene->runtime_viewport.alt_modifier_down) {
        (void)scene_editor_viewport_begin_navigation(&scene->runtime_viewport,
                                                     SCENE_EDITOR_VIEWPORT_NAV_ORBIT,
                                                     state->x,
                                                     state->y);
    }
}

static void scene_controller_runtime_pointer_up(void *user,
                                                const InputPointerState *state) {
    SceneState *scene = (SceneState *)user;
    if (!scene || !state || !scene_controller_retained_runtime_view_active(scene)) return;
    if (!scene->runtime_viewport.navigation_active) return;
    if (state->button == SDL_BUTTON_LEFT || state->button == SDL_BUTTON_MIDDLE) {
        scene_editor_viewport_end_navigation(&scene->runtime_viewport);
    }
}

static void scene_controller_runtime_pointer_move(void *user,
                                                  const InputPointerState *state) {
    SceneState *scene = (SceneState *)user;
    if (!scene || !state || !scene_controller_retained_runtime_view_active(scene)) return;
    if (!scene->runtime_viewport.navigation_active) return;
    (void)scene_editor_viewport_update_navigation(&scene->runtime_viewport,
                                                  state->x,
                                                  state->y,
                                                  scene->config ? scene->config->window_w : 1,
                                                  scene->config ? scene->config->window_h : 1);
}

static void scene_controller_runtime_wheel(void *user,
                                           const InputWheelState *wheel) {
    SceneState *scene = (SceneState *)user;
    if (!scene || !wheel || !scene_controller_retained_runtime_view_active(scene)) return;
    (void)scene_editor_viewport_apply_wheel(&scene->runtime_viewport, wheel->y);
}

static void scene_controller_runtime_key_down(void *user,
                                              SDL_Keycode key,
                                              SDL_Keymod mod) {
    SceneState *scene = (SceneState *)user;
    if (!scene || !scene_controller_retained_runtime_view_active(scene)) return;
    scene->runtime_viewport.alt_modifier_down = (mod & KMOD_ALT) != 0;
    if (key == SDLK_f) {
        (void)retained_runtime_scene_overlay_frame_view(scene,
                                                        scene->config ? scene->config->window_w : 1,
                                                        scene->config ? scene->config->window_h : 1);
    } else if (key == SDLK_LEFTBRACKET) {
        (void)scene_backend_step_compatibility_slice(scene, -1);
    } else if (key == SDLK_RIGHTBRACKET) {
        (void)scene_backend_step_compatibility_slice(scene, 1);
    }
}

static void scene_controller_runtime_key_up(void *user,
                                            SDL_Keycode key,
                                            SDL_Keymod mod) {
    SceneState *scene = (SceneState *)user;
    (void)key;
    if (!scene || !scene_controller_retained_runtime_view_active(scene)) return;
    scene->runtime_viewport.alt_modifier_down = (mod & KMOD_ALT) != 0;
    if (!scene->runtime_viewport.alt_modifier_down &&
        scene->runtime_viewport.navigation_active &&
        scene->runtime_viewport.navigation_mode == SCENE_EDITOR_VIEWPORT_NAV_ORBIT) {
        scene_editor_viewport_end_navigation(&scene->runtime_viewport);
    }
}

static const double DEFAULT_SAMPLE_RATE = 240.0;
static const float  DEFAULT_SAMPLE_SPACING = 3.0f;
static const size_t MAX_SAMPLES_PER_FRAME = 512;

static bool scene_controller_env_flag_enabled(const char *name) {
    if (!name) {
        return false;
    }
    const char *value = getenv(name);
    if (!value || !value[0]) {
        return false;
    }
    return strcmp(value, "1") == 0 ||
           strcmp(value, "true") == 0 ||
           strcmp(value, "TRUE") == 0 ||
           strcmp(value, "yes") == 0 ||
           strcmp(value, "on") == 0;
}

static bool scene_controller_rs1_diag_enabled(void) {
    return scene_controller_env_flag_enabled("PHYSICS_SIM_RS1_DIAG");
}

static bool scene_controller_ir1_diag_enabled(void) {
    return scene_controller_env_flag_enabled("PHYSICS_SIM_IR1_DIAG");
}

static bool handle_scene_command(const Command *cmd, void *user_data) {
    CommandDispatchContext *ctx = (CommandDispatchContext *)user_data;
    if (!ctx || !cmd) return false;

    switch (cmd->type) {
    case COMMAND_EXPORT_SNAPSHOT:
        ctx->snapshot_requested = true;
        return true;
    case COMMAND_TOGGLE_VORTICITY:
        renderer_sdl_toggle_vorticity();
        return true;
    case COMMAND_TOGGLE_PRESSURE:
        renderer_sdl_toggle_pressure();
        return true;
    case COMMAND_TOGGLE_VELOCITY_VECTORS:
        renderer_sdl_toggle_velocity_vectors();
        return true;
    case COMMAND_TOGGLE_VELOCITY_MODE:
        renderer_sdl_toggle_velocity_mode();
        return true;
    case COMMAND_TOGGLE_PARTICLE_FLOW:
        renderer_sdl_toggle_flow_particles();
        return true;
    case COMMAND_TOGGLE_KIT_VIZ_DENSITY:
        renderer_sdl_toggle_kit_viz_density();
        return true;
    case COMMAND_TOGGLE_KIT_VIZ_VELOCITY:
        renderer_sdl_toggle_kit_viz_velocity();
        return true;
    case COMMAND_TOGGLE_KIT_VIZ_PRESSURE:
        renderer_sdl_toggle_kit_viz_pressure();
        return true;
    case COMMAND_TOGGLE_KIT_VIZ_VORTICITY:
        renderer_sdl_toggle_kit_viz_vorticity();
        return true;
    case COMMAND_TOGGLE_KIT_VIZ_PARTICLES:
        renderer_sdl_toggle_kit_viz_particles();
        return true;
    default:
        return scene_handle_command(ctx->scene, cmd);
    }
}

static void build_snapshot_path(char *buffer, size_t buffer_size,
                                const char *dir, int index) {
    if (!buffer || buffer_size == 0) return;
    const char *base_dir = (dir && dir[0]) ? dir : physics_sim_default_snapshot_dir();
    snprintf(buffer, buffer_size, "%s/frame_%04d.ps2d", base_dir, index);
}

static void stroke_sampler_init(StrokeSampler *sampler,
                                size_t capacity,
                                const AppConfig *cfg) {
    if (!sampler) return;
    stroke_buffer_init(&sampler->buffer, capacity);
    sampler->brush_mode = BRUSH_MODE_DENSITY;
    sampler->pointer_down = false;
    sampler->last_emit_x = sampler->last_emit_y = 0.0f;
    sampler->current_x = sampler->current_y = 0.0f;
    sampler->accumulator = 0.0;
    double rate = (cfg && cfg->stroke_sample_rate > 0.0)
                      ? cfg->stroke_sample_rate
                      : DEFAULT_SAMPLE_RATE;
    sampler->sample_interval = 1.0 / rate;
    sampler->sample_spacing = (cfg && cfg->stroke_spacing > 0.0f)
                                  ? cfg->stroke_spacing
                                  : DEFAULT_SAMPLE_SPACING;
}

static void stroke_sampler_shutdown(StrokeSampler *sampler) {
    if (!sampler) return;
    stroke_buffer_shutdown(&sampler->buffer);
}

static void stroke_sampler_capture(StrokeSampler *sampler,
                                   const InputCommands *cmds,
                                   double dt) {
    if (!sampler || !cmds) return;
    sampler->accumulator += dt;

    if (cmds->brush_mode_changed) {
        sampler->brush_mode = cmds->brush_mode;
    }

    sampler->current_x = (float)cmds->mouse_x;
    sampler->current_y = (float)cmds->mouse_y;

    if (!cmds->mouse_down) {
        sampler->pointer_down = false;
        sampler->accumulator = fmin(sampler->accumulator, sampler->sample_interval);
        return;
    }

    if (!sampler->pointer_down) {
        sampler->pointer_down = true;
        sampler->last_emit_x = sampler->current_x;
        sampler->last_emit_y = sampler->current_y;
        StrokeSample sample = {
            .x = (int)lroundf(sampler->current_x),
            .y = (int)lroundf(sampler->current_y),
            .vx = 0.0f,
            .vy = 0.0f,
            .mode = sampler->brush_mode
        };
        stroke_buffer_push(&sampler->buffer, &sample);
    }

    while (sampler->accumulator >= sampler->sample_interval) {
        sampler->accumulator -= sampler->sample_interval;

        float dx = sampler->current_x - sampler->last_emit_x;
        float dy = sampler->current_y - sampler->last_emit_y;
        float dist = sqrtf(dx * dx + dy * dy);
        float spacing = (sampler->sample_spacing > 0.0f)
                            ? sampler->sample_spacing
                            : DEFAULT_SAMPLE_SPACING;
        int steps = (spacing > 0.0f) ? (int)ceilf(dist / spacing) : 1;
        if (steps < 1) steps = 1;
        float step_x = (steps > 0) ? dx / (float)steps : 0.0f;
        float step_y = (steps > 0) ? dy / (float)steps : 0.0f;

        for (int i = 0; i < steps; ++i) {
            sampler->last_emit_x += step_x;
            sampler->last_emit_y += step_y;
            StrokeSample sample = {
                .x = (int)lroundf(sampler->last_emit_x),
                .y = (int)lroundf(sampler->last_emit_y),
                .vx = step_x,
                .vy = step_y,
                .mode = sampler->brush_mode
            };
            stroke_buffer_push(&sampler->buffer, &sample);
        }
    }
}

static void stroke_sampler_apply(StrokeSampler *sampler,
                                 SceneState *scene,
                                 size_t max_samples) {
    if (!sampler || !scene) return;
    size_t processed = 0;
    StrokeSample sample;
    while (processed < max_samples &&
           stroke_buffer_pop(&sampler->buffer, &sample)) {
        scene_apply_brush_sample(scene, &sample);
        ++processed;
    }
}

static uint32_t scene_controller_count_bool(bool value) {
    return value ? 1u : 0u;
}

static void scene_controller_input_intake_phase(InputContextManager *ctx_mgr,
                                                CommandBus *bus,
                                                bool ignore_input,
                                                SceneControllerInputEventRaw *out_raw) {
    if (!out_raw) {
        return;
    }
    memset(out_raw, 0, sizeof(*out_raw));
    out_raw->ignore_input_active = ignore_input;
    out_raw->polled = input_poll_events(&out_raw->commands, bus, ctx_mgr);
    out_raw->quit_requested = out_raw->commands.quit;
    out_raw->running_after_poll = ignore_input ? true : out_raw->polled;
    out_raw->valid = true;

    if (ignore_input) {
        bool quit_requested = out_raw->commands.quit;
        memset(&out_raw->commands, 0, sizeof(out_raw->commands));
        out_raw->commands.quit = quit_requested;
        out_raw->quit_requested = quit_requested;
    }
}

static void scene_controller_input_normalize_phase(
    const SceneControllerInputEventRaw *raw,
    SceneControllerInputEventNormalized *out_normalized) {
    if (!raw || !out_normalized) {
        return;
    }
    memset(out_normalized, 0, sizeof(*out_normalized));
    out_normalized->has_quit_action = raw->commands.quit;
    out_normalized->has_pointer_actions = raw->commands.mouse_down;
    out_normalized->has_shortcut_actions = raw->commands.text_zoom_in_requested ||
                                           raw->commands.text_zoom_out_requested ||
                                           raw->commands.text_zoom_reset_requested;
    out_normalized->has_brush_mode_change = raw->commands.brush_mode_changed;
    out_normalized->action_count += scene_controller_count_bool(out_normalized->has_quit_action);
    out_normalized->action_count += scene_controller_count_bool(out_normalized->has_pointer_actions);
    out_normalized->action_count += scene_controller_count_bool(out_normalized->has_shortcut_actions);
    out_normalized->action_count += scene_controller_count_bool(out_normalized->has_brush_mode_change);
}

static void scene_controller_input_route_phase(
    const SceneControllerInputEventNormalized *normalized,
    SceneControllerInputRouteResult *out_route) {
    if (!normalized || !out_route) {
        return;
    }
    memset(out_route, 0, sizeof(*out_route));
    out_route->target_policy = SCENE_CONTROLLER_INPUT_ROUTE_TARGET_FALLBACK;

    if (normalized->has_shortcut_actions || normalized->has_quit_action) {
        out_route->routed_global_count = normalized->action_count;
        out_route->target_policy = SCENE_CONTROLLER_INPUT_ROUTE_TARGET_GLOBAL;
        out_route->consumed = true;
        return;
    }
    if (normalized->has_pointer_actions || normalized->has_brush_mode_change) {
        out_route->routed_scene_count = normalized->action_count;
        out_route->target_policy = SCENE_CONTROLLER_INPUT_ROUTE_TARGET_SCENE;
        out_route->consumed = true;
        return;
    }
    if (normalized->action_count > 0u) {
        out_route->routed_fallback_count = normalized->action_count;
        out_route->target_policy = SCENE_CONTROLLER_INPUT_ROUTE_TARGET_FALLBACK;
        out_route->consumed = true;
    }
}

static void scene_controller_input_invalidate_phase(
    const SceneControllerInputFrame *input_frame,
    SceneControllerInputInvalidationResult *out_invalidation) {
    if (!input_frame || !out_invalidation) {
        return;
    }
    memset(out_invalidation, 0, sizeof(*out_invalidation));
    if (input_frame->normalized.has_quit_action) {
        out_invalidation->invalidation_reason_bits |= SCENE_CONTROLLER_INPUT_INVALIDATE_REASON_QUIT;
        out_invalidation->full_invalidation_count += 1u;
        out_invalidation->full_invalidate = true;
    }
    if (input_frame->normalized.has_shortcut_actions) {
        out_invalidation->invalidation_reason_bits |= SCENE_CONTROLLER_INPUT_INVALIDATE_REASON_SHORTCUT;
    }
    if (input_frame->normalized.has_pointer_actions) {
        out_invalidation->invalidation_reason_bits |= SCENE_CONTROLLER_INPUT_INVALIDATE_REASON_POINTER;
    }
    if (input_frame->normalized.has_brush_mode_change) {
        out_invalidation->invalidation_reason_bits |= SCENE_CONTROLLER_INPUT_INVALIDATE_REASON_BRUSH;
    }
    out_invalidation->target_invalidation_count += input_frame->route.routed_global_count;
    out_invalidation->target_invalidation_count += input_frame->route.routed_scene_count;
    out_invalidation->target_invalidation_count += input_frame->route.routed_fallback_count;
}

static SceneControllerInputFrame scene_controller_input_phase(InputContextManager *ctx_mgr,
                                                              CommandBus *bus,
                                                              bool ignore_input,
                                                              bool headless_mode) {
    SceneControllerInputFrame frame = {0};
    frame.valid = true;
    scene_controller_input_intake_phase(ctx_mgr, bus, ignore_input, &frame.raw);
    scene_controller_input_normalize_phase(&frame.raw, &frame.normalized);
    scene_controller_input_route_phase(&frame.normalized, &frame.route);
    scene_controller_input_invalidate_phase(&frame, &frame.invalidation);
    frame.effective_commands = frame.raw.commands;
    frame.running = frame.raw.running_after_poll;
    frame.aborted = false;
    if (headless_mode && frame.effective_commands.quit) {
        frame.running = false;
        frame.aborted = true;
    }
    return frame;
}

static SceneControllerUpdateFrame scene_controller_update_phase(
    SceneState *scene,
    AppConfig *cfg,
    const SceneControllerInputFrame *input_frame,
    double dt,
    bool running,
    bool aborted,
    bool headless_mode,
    uint64_t frame_index,
    CommandBus *bus,
    StrokeSampler *sampler,
    CommandDispatchContext *dispatch_ctx,
    const SimModeHooks *mode_hooks,
    const char *snapshot_dir,
    int *snapshot_index) {
    SceneControllerUpdateFrame frame = {0};

    if (!scene || !cfg || !input_frame || !bus || !sampler || !dispatch_ctx || !snapshot_index) {
        return frame;
    }

    frame.valid = true;
    frame.running = running;
    frame.aborted = aborted;
    frame.headless_mode = headless_mode;
    frame.dt = dt;
    frame.frame_index = frame_index;

    if (input_frame->invalidation.target_invalidation_count > 0u ||
        input_frame->invalidation.full_invalidation_count > 0u) {
        frame.invalidation_reason_bits |= SCENE_CONTROLLER_INVALIDATION_INPUT;
    }

    if (!scene_controller_retained_runtime_view_active(scene)) {
        scene_apply_input(scene, &input_frame->effective_commands);
        stroke_sampler_capture(sampler, &input_frame->effective_commands, dt);
        stroke_sampler_apply(sampler, scene, MAX_SAMPLES_PER_FRAME);
    }

    {
        size_t max_commands = (cfg->command_batch_limit > 0)
                                  ? (size_t)cfg->command_batch_limit
                                  : 0;
        frame.dispatched_command_count =
            command_bus_dispatch(bus, max_commands, handle_scene_command, dispatch_ctx);
        if (frame.dispatched_command_count > 0) {
            frame.invalidation_reason_bits |= SCENE_CONTROLLER_INVALIDATION_COMMAND;
        }
    }

    if (!scene->paused) {
        scene->dt = dt;
        ts_start_timer("physics");
        {
            AppConfig step_cfg = *cfg;
            SimModeStepPolicy step_policy = sim_mode_step_policy(&scene->mode_route,
                                                                 scene->preset ? scene->preset->dimension_mode
                                                                              : SCENE_DIMENSION_MODE_2D);
            if (step_policy.constrained_3d_active) {
                if (step_cfg.physics_substeps < step_policy.min_substeps) {
                    step_cfg.physics_substeps = step_policy.min_substeps;
                }
                step_cfg.fluid_buoyancy_force *= step_policy.buoyancy_scale;
            }
            int substeps = step_cfg.physics_substeps > 0 ? step_cfg.physics_substeps : 1;
            double sub_dt = dt / (double)substeps;
            for (int i = 0; i < substeps; ++i) {
                if (mode_hooks && mode_hooks->pre_substep) {
                    mode_hooks->pre_substep(scene, sub_dt);
                }

                scene_apply_emitters(scene, sub_dt);
                scene_apply_boundary_flows(scene, sub_dt);
                scene_enforce_obstacles(scene);

                ts_start_timer("fluid_step");
                {
                    sim_runtime_backend_step(scene->backend,
                                             scene,
                                             &step_cfg,
                                             sub_dt);
                }
                ts_stop_timer("fluid_step");

                scene_enforce_obstacles(scene);
                scene_enforce_boundary_flows(scene);

                if (mode_hooks && mode_hooks->post_substep) {
                    mode_hooks->post_substep(scene, sub_dt);
                }

                object_manager_step(&scene->objects, sub_dt, &step_cfg, scene->objects_gravity_enabled);
                for (size_t ii = 0; ii < scene->import_shape_count; ++ii) {
                    int body_idx = scene->import_body_map[ii];
                    if (body_idx < 0 || body_idx >= scene->objects.count) continue;
                    RigidBody2D *b = &scene->objects.objects[body_idx].body;
                    scene->import_shapes[ii].position_x = b->position.x / (float)cfg->window_w;
                    scene->import_shapes[ii].position_y = b->position.y / (float)cfg->window_h;
                    scene->import_shapes[ii].rotation_deg = b->angle * 180.0f / (float)M_PI;
                }
                scene_rasterize_dynamic_obstacles(scene);
                sim_runtime_backend_inject_object_motion(scene->backend, scene);
                scene_backend_mark_obstacles_dirty(scene);
                scene->time += sub_dt;
            }
        }
        ts_stop_timer("physics");
        frame.invalidation_reason_bits |= SCENE_CONTROLLER_INVALIDATION_SIM_STEP;
    } else {
        scene_enforce_obstacles(scene);
    }

    frame.snapshot_requested = dispatch_ctx->snapshot_requested;
    if (dispatch_ctx->snapshot_requested) {
        char path[256];
        build_snapshot_path(path, sizeof(path), snapshot_dir, (*snapshot_index)++);
        if (!scene_export_snapshot(scene, path)) {
            fprintf(stderr, "Failed to export snapshot to %s\n", path);
        } else {
            fprintf(stderr, "Exported snapshot to %s\n", path);
            frame.snapshot_exported = true;
            frame.invalidation_reason_bits |= SCENE_CONTROLLER_INVALIDATION_SNAPSHOT_EXPORT;
        }
    }

    return frame;
}

static SceneControllerRenderDeriveFrame scene_controller_render_derive_phase(
    const SceneState *scene,
    const AppConfig *cfg,
    const SceneControllerUpdateFrame *update_frame,
    bool headless_mode,
    bool skip_present) {
    SceneControllerRenderDeriveFrame frame = {0};
    const char *quality_label = NULL;
    SimRuntimeBackendReport backend_report = {0};
    SceneFluidFieldView2D fluid_view = {0};
    SceneObstacleFieldView2D obstacle_view = {0};
    bool compatibility_slice_has_activity = false;
    bool compatibility_slice_has_obstacles = false;

    if (!scene || !cfg || !update_frame || !update_frame->valid) {
        return frame;
    }

    quality_label = quality_profile_name(cfg->quality_index);
    (void)scene_backend_report(scene, &backend_report);
    if (backend_report.compatibility_view_2d_available &&
        scene_backend_fluid_view_2d(scene, &fluid_view) &&
        fluid_view.density) {
        for (size_t i = 0; i < fluid_view.cell_count; ++i) {
            if (fluid_view.density[i] > 0.0001f) {
                compatibility_slice_has_activity = true;
                break;
            }
        }
    }
    if (backend_report.compatibility_view_2d_available &&
        scene_backend_obstacle_view_2d(scene, &obstacle_view) &&
        obstacle_view.solid_mask) {
        for (size_t i = 0; i < obstacle_view.cell_count; ++i) {
            if (obstacle_view.solid_mask[i]) {
                compatibility_slice_has_obstacles = true;
                break;
            }
        }
    }
    frame.valid = true;
    frame.headless_mode = headless_mode;
    frame.frame_index = update_frame->frame_index;
    frame.invalidation_reason_bits = update_frame->invalidation_reason_bits;
    frame.should_save_volume_frames = cfg->save_volume_frames;
    frame.should_save_render_frames = cfg->save_render_frames;
    frame.should_present = !headless_mode || !skip_present;

    frame.hud = (RendererHudInfo){
        .preset_name = (scene->preset && scene->preset->name) ? scene->preset->name : "Preset",
        .preset_is_custom = scene->preset ? scene->preset->is_custom : false,
        .grid_w = scene->config ? scene->config->grid_w : cfg->grid_w,
        .grid_h = scene->config ? scene->config->grid_h : cfg->grid_h,
        .window_w = cfg->window_w,
        .window_h = cfg->window_h,
        .paused = scene->paused,
        .sim_mode = cfg->sim_mode,
        .requested_space_mode = scene->mode_route.requested_space_mode,
        .projection_space_mode = scene->mode_route.projection_space_mode,
        .backend_lane = scene->mode_route.backend_lane,
        .backend_uses_canonical_2d_solver = scene->mode_route.backend_uses_canonical_2d_solver,
        .backend_kind = backend_report.kind,
        .backend_domain_w = backend_report.domain_w,
        .backend_domain_h = backend_report.domain_h,
        .backend_domain_d = backend_report.domain_d,
        .backend_cell_count = backend_report.cell_count,
        .backend_volumetric_emitters_free_live = backend_report.volumetric_emitters_free_live,
        .backend_volumetric_emitters_attached_live = backend_report.volumetric_emitters_attached_live,
        .backend_volumetric_obstacles_live = backend_report.volumetric_obstacles_live,
        .backend_full_3d_solver_live = backend_report.full_3d_solver_live,
        .backend_world_bounds_valid = backend_report.world_bounds_valid,
        .backend_world_min_x = backend_report.world_min_x,
        .backend_world_min_y = backend_report.world_min_y,
        .backend_world_min_z = backend_report.world_min_z,
        .backend_world_max_x = backend_report.world_max_x,
        .backend_world_max_y = backend_report.world_max_y,
        .backend_world_max_z = backend_report.world_max_z,
        .backend_voxel_size = backend_report.voxel_size,
        .backend_compatibility_view_2d_available = backend_report.compatibility_view_2d_available,
        .backend_compatibility_view_2d_derived = backend_report.compatibility_view_2d_derived,
        .backend_compatibility_slice_z = backend_report.compatibility_slice_z,
        .backend_compatibility_slice_has_activity = compatibility_slice_has_activity,
        .backend_compatibility_slice_has_obstacles = compatibility_slice_has_obstacles,
        .backend_secondary_debug_slice_stack_live = backend_report.secondary_debug_slice_stack_live,
        .backend_secondary_debug_slice_stack_radius = backend_report.secondary_debug_slice_stack_radius,
        .tunnel_inflow_speed = cfg->tunnel_inflow_speed,
        .vorticity_enabled = renderer_sdl_vorticity_enabled(),
        .pressure_enabled = renderer_sdl_pressure_enabled(),
        .velocity_overlay_enabled = renderer_sdl_velocity_vectors_enabled(),
        .particle_overlay_enabled = renderer_sdl_flow_particles_enabled(),
        .velocity_fixed_length = renderer_sdl_velocity_mode_fixed(),
        .kit_viz_density_enabled = renderer_sdl_kit_viz_density_enabled(),
        .kit_viz_density_active = renderer_sdl_density_using_kit_viz(),
        .kit_viz_velocity_enabled = renderer_sdl_kit_viz_velocity_enabled(),
        .kit_viz_velocity_active = renderer_sdl_velocity_using_kit_viz(),
        .kit_viz_pressure_enabled = renderer_sdl_kit_viz_pressure_enabled(),
        .kit_viz_pressure_active = renderer_sdl_pressure_using_kit_viz(),
        .kit_viz_vorticity_enabled = renderer_sdl_kit_viz_vorticity_enabled(),
        .kit_viz_vorticity_active = renderer_sdl_vorticity_using_kit_viz(),
        .kit_viz_particles_enabled = renderer_sdl_kit_viz_particles_enabled(),
        .kit_viz_particles_active = renderer_sdl_particles_using_kit_viz(),
        .objects_gravity_enabled = scene->objects_gravity_enabled,
        .retained_runtime_visual_active = scene_controller_retained_runtime_view_active(scene),
        .quality_name = quality_label ? quality_label : "Custom",
        .solver_iterations = cfg->fluid_solver_iterations,
        .physics_substeps = cfg->physics_substeps
    };

    if (frame.should_save_volume_frames || frame.should_save_render_frames || frame.should_present) {
        frame.invalidation_reason_bits |= SCENE_CONTROLLER_INVALIDATION_RENDER_OUTPUT;
    }

    return frame;
}

static void scene_controller_render_submit_phase(const SceneState *scene,
                                                 const SceneControllerRenderDeriveFrame *derive_frame,
                                                 bool *io_running,
                                                 bool *io_aborted) {
    if (!scene || !derive_frame || !derive_frame->valid || !io_running || !io_aborted) {
        return;
    }

    if (derive_frame->should_save_volume_frames) {
        volume_frames_write(scene, derive_frame->frame_index);
    }

    if (renderer_sdl_render_scene(scene)) {
        if (derive_frame->should_save_render_frames) {
            uint8_t *pixels = NULL;
            int pitch = 0;
            if (renderer_sdl_capture_pixels(&pixels, &pitch)) {
                render_frames_write_bmp(pixels,
                                        renderer_sdl_output_width(),
                                        renderer_sdl_output_height(),
                                        pitch,
                                        derive_frame->frame_index);
                renderer_sdl_free_capture(pixels);
            }
        }
        if (derive_frame->should_present) {
            renderer_sdl_present_with_hud(&derive_frame->hud);
        }
    }

    if (renderer_sdl_device_lost()) {
        fprintf(stderr, "[scene] Vulkan device lost; stopping simulation.\n");
        *io_aborted = true;
        *io_running = false;
    }
}

int scene_controller_run(const AppConfig *initial_cfg,
                        const FluidScenePreset *preset,
                        const SceneRuntimeLaunch *runtime_launch,
                        const ShapeAssetLibrary *shape_library,
                        const char *snapshot_dir,
                        const HeadlessOptions *headless) {
    if (!initial_cfg) {
        fprintf(stderr, "[scene] Missing configuration.\n");
        return 1;
    }

    bool sdl_initialized = false;
    if (SDL_WasInit(SDL_INIT_VIDEO) == 0) {
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
            fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
            return 1;
        }
        sdl_initialized = true;
    }

    AppConfig cfg = *initial_cfg;
    const FluidScenePreset *preset_fallback = preset ? preset : scene_presets_get_default();
    FluidScenePreset runtime_preset = preset_fallback ? *preset_fallback : (FluidScenePreset){0};
    SimModeRoute mode_route = sim_mode_resolve_route(cfg.sim_mode, cfg.space_mode);
    const SimModeHooks *mode_hooks = mode_route.hooks;
    if (mode_hooks && mode_hooks->configure_app) {
        mode_hooks->configure_app(&cfg, &runtime_preset);
    }
    if (mode_route.fallback_to_2d_projection) {
        fprintf(stderr,
                "[scene] Space mode 3D requested; using controlled 3D lane with scaffold backend and compatibility XY projection.\n");
    }

    if (!renderer_sdl_init(cfg.window_w, cfg.window_h, cfg.grid_w, cfg.grid_h)) {
        if (sdl_initialized) {
            SDL_Quit();
        }
        return 1;
    }

    SceneState scene = scene_create(&cfg, &runtime_preset, shape_library, &mode_route);
    if (runtime_launch &&
        runtime_launch->has_retained_scene &&
        runtime_launch->retained_runtime_scene_path[0] &&
        mode_route.requested_space_mode == SPACE_MODE_3D) {
        (void)scene_load_runtime_visual_bootstrap(&scene,
                                                  runtime_launch->retained_runtime_scene_path);
        (void)retained_runtime_scene_overlay_frame_view(&scene, cfg.window_w, cfg.window_h);
    }
    if (!sim_runtime_backend_valid(scene.backend)) {
        fprintf(stderr, "[scene] Fluid grid failed to initialize.\n");
    }
    if (mode_hooks && mode_hooks->prepare_scene) {
        mode_hooks->prepare_scene(&scene);
    }

    FrameTimer timer;
    timing_init(&timer);

    CommandBus bus;
    command_bus_init(&bus, 64);

    StrokeSampler sampler;
    stroke_sampler_init(&sampler, 4096, &cfg);

    InputContextManager ctx_mgr;
    input_context_manager_init(&ctx_mgr);
    if (scene_controller_retained_runtime_view_active(&scene)) {
        InputContext runtime_view_ctx = {
            .on_pointer_down = scene_controller_runtime_pointer_down,
            .on_pointer_up = scene_controller_runtime_pointer_up,
            .on_pointer_move = scene_controller_runtime_pointer_move,
            .on_wheel = scene_controller_runtime_wheel,
            .on_key_down = scene_controller_runtime_key_down,
            .on_key_up = scene_controller_runtime_key_up,
            .user_data = &scene
        };
        input_context_manager_push(&ctx_mgr, &runtime_view_ctx);
    }

    bool running = true;
    bool aborted = false;
    int snapshot_index = 0;
    uint64_t frame_index = 0;
    (void)snapshot_dir;

    bool headless_mode = headless && headless->enabled;
    int interactive_frame_limit = (!headless_mode && cfg.headless_frame_count > 0)
                                      ? cfg.headless_frame_count
                                      : 0;
    SceneControllerRs1DiagTotals rs1_diag_totals = {0};
    SceneControllerIr1DiagTotals ir1_diag_totals = {0};
    while (running) {
        ts_frame_start();
        ts_start_timer("frame");

        bool ignore_input = headless_mode && headless->ignore_input;
        SceneControllerInputFrame input_frame =
            scene_controller_input_phase(&ctx_mgr, &bus, ignore_input, headless_mode);
        running = input_frame.running;
        if (input_frame.aborted) {
            aborted = true;
        }

        double dt = timing_begin_frame(&timer, &cfg);
        CommandDispatchContext dispatch_ctx = {
            .scene = &scene,
            .snapshot_requested = false,
        };
        SceneControllerUpdateFrame update_frame =
            scene_controller_update_phase(&scene,
                                          &cfg,
                                          &input_frame,
                                          dt,
                                          running,
                                          aborted,
                                          headless_mode,
                                          frame_index,
                                          &bus,
                                          &sampler,
                                          &dispatch_ctx,
                                          mode_hooks,
                                          snapshot_dir,
                                          &snapshot_index);
        ir1_diag_totals.frame_count += 1u;
        ir1_diag_totals.routed_global_total += input_frame.route.routed_global_count;
        ir1_diag_totals.routed_scene_total += input_frame.route.routed_scene_count;
        ir1_diag_totals.routed_fallback_total += input_frame.route.routed_fallback_count;
        ir1_diag_totals.invalidation_reason_bits_total += input_frame.invalidation.invalidation_reason_bits;
        if (scene_controller_ir1_diag_enabled()) {
            printf("[ir1] physics_sim frame=%llu actions=%u route(global=%u scene=%u fallback=%u target=%d) "
                   "invalidate(bits=0x%x target=%u full=%u) totals(frames=%llu global=%llu scene=%llu fallback=%llu invalid_bits_sum=%llu)\n",
                   (unsigned long long)frame_index,
                   (unsigned int)input_frame.normalized.action_count,
                   (unsigned int)input_frame.route.routed_global_count,
                   (unsigned int)input_frame.route.routed_scene_count,
                   (unsigned int)input_frame.route.routed_fallback_count,
                   (int)input_frame.route.target_policy,
                   (unsigned int)input_frame.invalidation.invalidation_reason_bits,
                   (unsigned int)input_frame.invalidation.target_invalidation_count,
                   (unsigned int)input_frame.invalidation.full_invalidation_count,
                   (unsigned long long)ir1_diag_totals.frame_count,
                   (unsigned long long)ir1_diag_totals.routed_global_total,
                   (unsigned long long)ir1_diag_totals.routed_scene_total,
                   (unsigned long long)ir1_diag_totals.routed_fallback_total,
                   (unsigned long long)ir1_diag_totals.invalidation_reason_bits_total);
        }

        uint64_t derive_begin = SDL_GetPerformanceCounter();
        SceneControllerRenderDeriveFrame derive_frame =
            scene_controller_render_derive_phase(&scene,
                                                &cfg,
                                                &update_frame,
                                                headless_mode,
                                                headless_mode && headless && headless->skip_present);
        uint64_t derive_end = SDL_GetPerformanceCounter();

        scene_controller_render_submit_phase(&scene, &derive_frame, &running, &aborted);
        uint64_t submit_end = SDL_GetPerformanceCounter();

        {
            uint64_t perf_freq = SDL_GetPerformanceFrequency();
            uint64_t derive_ns = 0;
            uint64_t submit_ns = 0;
            if (perf_freq > 0) {
                derive_ns = (uint64_t)((derive_end - derive_begin) * 1000000000ull / perf_freq);
                submit_ns = (uint64_t)((submit_end - derive_end) * 1000000000ull / perf_freq);
            }
            rs1_diag_totals.frame_count += 1u;
            rs1_diag_totals.derive_ns_total += derive_ns;
            rs1_diag_totals.submit_ns_total += submit_ns;
            rs1_diag_totals.invalidation_reason_bits_total += derive_frame.invalidation_reason_bits;
            rs1_diag_totals.dispatched_commands_total += update_frame.dispatched_command_count;
            if (scene_controller_rs1_diag_enabled()) {
                printf("[rs1] physics_sim frame=%llu derive_ns=%llu submit_ns=%llu invalidation_bits=0x%x "
                       "commands=%llu totals(frames=%llu derive_ns=%llu submit_ns=%llu invalid_bits_sum=%llu commands=%llu)\n",
                       (unsigned long long)rs1_diag_totals.frame_count,
                       (unsigned long long)derive_ns,
                       (unsigned long long)submit_ns,
                       (unsigned int)derive_frame.invalidation_reason_bits,
                       (unsigned long long)update_frame.dispatched_command_count,
                       (unsigned long long)rs1_diag_totals.frame_count,
                       (unsigned long long)rs1_diag_totals.derive_ns_total,
                       (unsigned long long)rs1_diag_totals.submit_ns_total,
                       (unsigned long long)rs1_diag_totals.invalidation_reason_bits_total,
                       (unsigned long long)rs1_diag_totals.dispatched_commands_total);
            }
        }


        frame_index++;
        if (headless_mode && headless->frame_limit > 0 &&
            frame_index >= (uint64_t)headless->frame_limit) {
            running = false;
        }
        if (!headless_mode && interactive_frame_limit > 0 &&
            frame_index >= (uint64_t)interactive_frame_limit) {
            running = false;
        }
        ts_stop_timer("frame");
        ts_frame_end();
    }

    scene_destroy(&scene);
    stroke_sampler_shutdown(&sampler);
    command_bus_shutdown(&bus);
    renderer_sdl_shutdown();
    return aborted ? 2 : 0;
}
