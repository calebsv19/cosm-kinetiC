#include "app/scene_controller.h"

#include <SDL2/SDL.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>

#include "command/command_bus.h"
#include "app/scene_state.h"
#include "app/scene_presets.h"
#include "input/input.h"
#include "input/stroke_buffer.h"
#include "physics/fluid2d/fluid2d.h"
#include "render/renderer_sdl.h"
#include "timing.h"

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
} StrokeSampler;

static const double SAMPLE_INTERVAL = 1.0 / 240.0;
static const float  SAMPLE_SPACING  = 3.0f;
static const size_t MAX_SAMPLES_PER_FRAME = 512;

static bool handle_scene_command(const Command *cmd, void *user_data) {
    CommandDispatchContext *ctx = (CommandDispatchContext *)user_data;
    if (!ctx || !cmd) return false;

    switch (cmd->type) {
    case COMMAND_EXPORT_SNAPSHOT:
        ctx->snapshot_requested = true;
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

static void stroke_sampler_init(StrokeSampler *sampler, size_t capacity) {
    if (!sampler) return;
    stroke_buffer_init(&sampler->buffer, capacity);
    sampler->brush_mode = BRUSH_MODE_DENSITY;
    sampler->pointer_down = false;
    sampler->last_emit_x = sampler->last_emit_y = 0.0f;
    sampler->current_x = sampler->current_y = 0.0f;
    sampler->accumulator = 0.0;
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
        sampler->accumulator = fmin(sampler->accumulator, SAMPLE_INTERVAL);
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

    while (sampler->accumulator >= SAMPLE_INTERVAL) {
        sampler->accumulator -= SAMPLE_INTERVAL;

        float dx = sampler->current_x - sampler->last_emit_x;
        float dy = sampler->current_y - sampler->last_emit_y;
        float dist = sqrtf(dx * dx + dy * dy);
        int steps = (int)ceilf(dist / SAMPLE_SPACING);
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
                        const char *snapshot_dir) {
    if (!initial_cfg) {
        fprintf(stderr, "[scene] Missing configuration.\n");
        return 1;
    }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    AppConfig cfg = *initial_cfg;

    if (!renderer_sdl_init(cfg.window_w, cfg.window_h, cfg.grid_w, cfg.grid_h)) {
        SDL_Quit();
        return 1;
    }

    SceneState scene = scene_create(&cfg, preset);
    if (!scene.smoke) {
        fprintf(stderr, "[scene] Fluid grid failed to initialize.\n");
    }

    FrameTimer timer;
    timing_init(&timer);

    CommandBus bus;
    command_bus_init(&bus, 64);

    StrokeSampler sampler;
    stroke_sampler_init(&sampler, 4096);

    bool running = true;
    int snapshot_index = 0;

    InputHandlers handlers = {0};
    while (running) {
        InputCommands cmds;
        running = input_poll_events(&cmds, &bus, &handlers);

        double dt = timing_begin_frame(&timer, &cfg);
        scene.dt = dt;

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
            int substeps = cfg.physics_substeps;
            double sub_dt = dt / (double)(substeps > 0 ? substeps : 1);
            for (int i = 0; i < substeps; ++i) {
                scene_apply_emitters(&scene, sub_dt);
                fluid2d_step(scene.smoke, sub_dt, &cfg);
                scene.time += sub_dt;
            }
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

        renderer_sdl_draw(&scene);
    }

    scene_destroy(&scene);
    stroke_sampler_shutdown(&sampler);
    command_bus_shutdown(&bus);
    renderer_sdl_shutdown();
    SDL_Quit();
    return 0;
}
