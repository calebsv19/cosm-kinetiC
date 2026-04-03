#ifndef SCENE_CONTROLLER_H
#define SCENE_CONTROLLER_H

#include "app/app_config.h"
#include "app/scene_presets.h"
#include "geo/shape_library.h"
#include "render/renderer_sdl.h"
#include <stddef.h>
#include <stdint.h>

typedef struct HeadlessOptions {
    bool enabled;
    int  frame_limit;
    bool skip_present;
    bool ignore_input;
    bool preserve_input;
    bool preserve_sdl_state;
} HeadlessOptions;

typedef enum SceneControllerInvalidationReasonBits {
    SCENE_CONTROLLER_INVALIDATION_INPUT = 1u << 0,
    SCENE_CONTROLLER_INVALIDATION_COMMAND = 1u << 1,
    SCENE_CONTROLLER_INVALIDATION_SIM_STEP = 1u << 2,
    SCENE_CONTROLLER_INVALIDATION_SNAPSHOT_EXPORT = 1u << 3,
    SCENE_CONTROLLER_INVALIDATION_RENDER_OUTPUT = 1u << 4
} SceneControllerInvalidationReasonBits;

typedef struct SceneControllerUpdateFrame {
    bool valid;
    bool running;
    bool aborted;
    bool headless_mode;
    bool snapshot_requested;
    bool snapshot_exported;
    double dt;
    uint64_t frame_index;
    uint32_t invalidation_reason_bits;
    size_t dispatched_command_count;
} SceneControllerUpdateFrame;

typedef struct SceneControllerRenderDeriveFrame {
    bool valid;
    bool should_present;
    bool should_save_render_frames;
    bool should_save_volume_frames;
    bool headless_mode;
    uint64_t frame_index;
    uint32_t invalidation_reason_bits;
    RendererHudInfo hud;
} SceneControllerRenderDeriveFrame;

// Runs the lifetime of the application: initializes SDL subsystems via the
// renderer, owns the SceneState, pumps input, drains the command bus, and
// steps physics/rendering until the user quits.
int scene_controller_run(const AppConfig *initial_cfg,
                         const FluidScenePreset *preset,
                         const ShapeAssetLibrary *shape_library,
                         const char *snapshot_dir,
                         const HeadlessOptions *headless);

#endif // SCENE_CONTROLLER_H
