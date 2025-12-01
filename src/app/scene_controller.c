#include "app/scene_controller.h"

#include <SDL2/SDL.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "command/command_bus.h"
#include "app/scene_state.h"
#include "app/scene_presets.h"
#include "app/sim_mode.h"
#include "app/quality_profiles.h"
#include "input/input.h"
#include "input/stroke_buffer.h"
#include "physics/fluid2d/fluid2d.h"
#include "render/renderer_sdl.h"
#include "export/volume_frames.h"
#include "export/render_frames.h"
#include "timing.h"
#include "render/TimerHUD/src/api/time_scope.h"

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

static const double DEFAULT_SAMPLE_RATE = 240.0;
static const float  DEFAULT_SAMPLE_SPACING = 3.0f;
static const size_t MAX_SAMPLES_PER_FRAME = 512;

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
    default:
        return scene_handle_command(ctx->scene, cmd);
    }
}

static void build_snapshot_path(char *buffer, size_t buffer_size,
                                const char *dir, int index) {
    if (!buffer || buffer_size == 0) return;
    const char *base_dir = dir ? dir : "data/snapshots";
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

int scene_controller_run(const AppConfig *initial_cfg,
                        const FluidScenePreset *preset,
                        const ShapeAssetLibrary *shape_library,
                        const char *snapshot_dir,
                        const HeadlessOptions *headless) {
    if (!initial_cfg) {
        fprintf(stderr, "[scene] Missing configuration.\n");
        return 1;
    }

    bool preserve_sdl = (headless && headless->preserve_sdl_state);
    bool sdl_initialized = false;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }
    sdl_initialized = true;

    AppConfig cfg = *initial_cfg;
    const FluidScenePreset *preset_fallback = preset ? preset : scene_presets_get_default();
    FluidScenePreset runtime_preset = preset_fallback ? *preset_fallback : (FluidScenePreset){0};
    const SimModeHooks *mode_hooks = sim_mode_get_hooks(cfg.sim_mode);
    if (mode_hooks && mode_hooks->configure_app) {
        mode_hooks->configure_app(&cfg, &runtime_preset);
    }

    if (!renderer_sdl_init(cfg.window_w, cfg.window_h, cfg.grid_w, cfg.grid_h)) {
        SDL_Quit();
        return 1;
    }

    SceneState scene = scene_create(&cfg, &runtime_preset, shape_library);
    if (!scene.smoke) {
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

    bool running = true;
    bool aborted = false;
    int snapshot_index = 0;
    uint64_t frame_index = 0;
    (void)snapshot_dir;

    bool headless_mode = headless && headless->enabled;
    int interactive_frame_limit = (!headless_mode && cfg.headless_frame_count > 0)
                                      ? cfg.headless_frame_count
                                      : 0;
    while (running) {
        ts_frame_start();
        ts_start_timer("frame");

        InputCommands cmds;
        bool ignore_input = headless_mode && headless->ignore_input;
        bool polled = input_poll_events(&cmds, &bus, &ctx_mgr);
        if (ignore_input) {
            bool quit_requested = cmds.quit;
            memset(&cmds, 0, sizeof(cmds));
            cmds.quit = quit_requested;
            running = true;
        } else {
            running = polled;
        }

        if (headless_mode && cmds.quit) {
            running = false;
            aborted = true;
        }
        double dt = timing_begin_frame(&timer, &cfg);


        scene_apply_input(&scene, &cmds);
        stroke_sampler_capture(&sampler, &cmds, dt);


        CommandDispatchContext dispatch_ctx = {
            .scene = &scene,
            .snapshot_requested = false,
        };


        stroke_sampler_apply(&sampler, &scene, MAX_SAMPLES_PER_FRAME);


        size_t max_commands = (cfg.command_batch_limit > 0)
                                  ? (size_t)cfg.command_batch_limit
                                  : 0; // 0 => unlimited

        command_bus_dispatch(&bus, max_commands, handle_scene_command, &dispatch_ctx);

        if (!scene.paused) {
            scene.dt = dt;
            ts_start_timer("physics");
            int substeps = cfg.physics_substeps > 0 ? cfg.physics_substeps : 1;
            double sub_dt = dt / (double)substeps;
            for (int i = 0; i < substeps; ++i) {
                if (mode_hooks && mode_hooks->pre_substep) {
                    mode_hooks->pre_substep(&scene, sub_dt);
                }

                scene_apply_emitters(&scene, sub_dt);

                // ts_start_timer("boundary_flows");
                scene_apply_boundary_flows(&scene, sub_dt);
                // ts_stop_timer("boundary_flows");

                scene_enforce_obstacles(&scene);

                ts_start_timer("fluid_step");
                const BoundaryFlow *flows = scene.preset ? scene.preset->boundary_flows : NULL;
                fluid2d_step(scene.smoke,
                             sub_dt,
                             &cfg,
                             flows,
                             scene.obstacle_mask,
                             scene.obstacle_velX,
                             scene.obstacle_velY);
                ts_stop_timer("fluid_step");

                scene_enforce_obstacles(&scene);
                scene_enforce_boundary_flows(&scene);

                if (mode_hooks && mode_hooks->post_substep) {
                    mode_hooks->post_substep(&scene, sub_dt);
                }

                object_manager_step(&scene.objects, sub_dt, &cfg);
                scene.obstacle_mask_dirty = true;
                scene.time += sub_dt;
            }
            ts_stop_timer("physics");
        } else {
            scene_enforce_obstacles(&scene);
        }

        if (dispatch_ctx.snapshot_requested) {
            char path[256];
            build_snapshot_path(path, sizeof(path), snapshot_dir, snapshot_index++);
            if (!scene_export_snapshot(&scene, path)) {
                fprintf(stderr, "Failed to export snapshot to %s\n", path);
            } else {
                fprintf(stderr, "Exported snapshot to %s\n", path);
            }
        }

        const char *quality_label = quality_profile_name(cfg.quality_index);
        RendererHudInfo hud = {
            .preset_name = (scene.preset && scene.preset->name) ? scene.preset->name : "Preset",
            .preset_is_custom = scene.preset ? scene.preset->is_custom : false,
            .grid_w = scene.config ? scene.config->grid_w : cfg.grid_w,
            .grid_h = scene.config ? scene.config->grid_h : cfg.grid_h,
            .window_w = cfg.window_w,
            .window_h = cfg.window_h,
            .paused = scene.paused,
            .sim_mode = cfg.sim_mode,
            .tunnel_inflow_speed = cfg.tunnel_inflow_speed,
            .vorticity_enabled = renderer_sdl_vorticity_enabled(),
            .pressure_enabled = renderer_sdl_pressure_enabled(),
            .velocity_overlay_enabled = renderer_sdl_velocity_vectors_enabled(),
            .particle_overlay_enabled = renderer_sdl_flow_particles_enabled(),
            .velocity_fixed_length = renderer_sdl_velocity_mode_fixed(),
            .quality_name = quality_label ? quality_label : "Custom",
            .solver_iterations = cfg.fluid_solver_iterations,
            .physics_substeps = cfg.physics_substeps
        };


        if (cfg.save_volume_frames) {
            volume_frames_write(&scene, frame_index);
        }


        if (renderer_sdl_render_scene(&scene)) {
            if (cfg.save_render_frames) {
                uint8_t *pixels = NULL;
                int pitch = 0;
                if (renderer_sdl_capture_pixels(&pixels, &pitch)) {
                    render_frames_write_bmp(pixels,
                                            renderer_sdl_output_width(),
                                            renderer_sdl_output_height(),
                                            pitch,
                                            frame_index);
                    renderer_sdl_free_capture(pixels);
                }
            }
            if (!headless_mode || !headless->skip_present) {
                renderer_sdl_present_with_hud(&hud);
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
    if (!preserve_sdl && sdl_initialized) {
        SDL_Quit();
    }
    return aborted ? 2 : 0;
}
