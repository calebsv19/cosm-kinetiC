#ifndef SCENE_CONTROLLER_H
#define SCENE_CONTROLLER_H

#include "app/app_config.h"
#include "app/scene_presets.h"
#include "app/sim_runtime_backend.h"
#include "geo/shape_library.h"
#include "input/input.h"
#include "render/renderer_sdl.h"
#include <stddef.h>
#include <stdint.h>

typedef struct HeadlessProgressInfo {
    uint64_t frames_completed;
    uint64_t frames_requested;
    uint64_t frame_index;
    uint32_t sim_steps_completed_in_frame;
    uint32_t sim_steps_total_in_frame;
    const char *stage;
    bool final_update;
} HeadlessProgressInfo;

typedef void (*HeadlessProgressCallback)(void *user_data,
                                         const HeadlessProgressInfo *progress);
typedef bool (*HeadlessCancelRequestedFn)(void *user_data);
typedef void (*HeadlessFrameAnalysisCallback)(void *user_data,
                                              uint64_t frame_index,
                                              const SimRuntimeBackendReport *backend_report);

typedef enum HeadlessWindShotCameraProfile {
    HEADLESS_WIND_SHOT_CAMERA_RUNTIME_DEFAULT = 0,
    HEADLESS_WIND_SHOT_CAMERA_THREE_QUARTER = 1,
    HEADLESS_WIND_SHOT_CAMERA_SIDE = 2,
    HEADLESS_WIND_SHOT_CAMERA_TOP = 3,
    HEADLESS_WIND_SHOT_CAMERA_DOWNSTREAM = 4
} HeadlessWindShotCameraProfile;

typedef struct HeadlessOptions {
    bool enabled;
    int  frame_limit;
    int  sim_steps_per_frame;
    bool skip_present;
    bool ignore_input;
    bool preserve_input;
    bool preserve_sdl_state;
    uint32_t progress_interval_frames;
    HeadlessProgressCallback progress_callback;
    void *progress_user_data;
    HeadlessCancelRequestedFn cancel_requested;
    void *cancel_user_data;
    HeadlessFrameAnalysisCallback frame_analysis_callback;
    void *frame_analysis_user_data;
    HeadlessWindShotCameraProfile wind_shot_camera_profile;
    bool save_wind_projection_frames;
    SimRuntimeBackendReport *final_backend_report;
} HeadlessOptions;

typedef struct SceneRuntimeLaunch {
    bool has_retained_scene;
    char retained_runtime_scene_path[512];
} SceneRuntimeLaunch;

typedef enum SceneControllerInvalidationReasonBits {
    SCENE_CONTROLLER_INVALIDATION_INPUT = 1u << 0,
    SCENE_CONTROLLER_INVALIDATION_COMMAND = 1u << 1,
    SCENE_CONTROLLER_INVALIDATION_SIM_STEP = 1u << 2,
    SCENE_CONTROLLER_INVALIDATION_SNAPSHOT_EXPORT = 1u << 3,
    SCENE_CONTROLLER_INVALIDATION_RENDER_OUTPUT = 1u << 4
} SceneControllerInvalidationReasonBits;

typedef enum SceneControllerInputInvalidateReasonBits {
    SCENE_CONTROLLER_INPUT_INVALIDATE_REASON_QUIT = 1u << 0,
    SCENE_CONTROLLER_INPUT_INVALIDATE_REASON_SHORTCUT = 1u << 1,
    SCENE_CONTROLLER_INPUT_INVALIDATE_REASON_POINTER = 1u << 2,
    SCENE_CONTROLLER_INPUT_INVALIDATE_REASON_BRUSH = 1u << 3
} SceneControllerInputInvalidateReasonBits;

typedef enum SceneControllerInputRouteTarget {
    SCENE_CONTROLLER_INPUT_ROUTE_TARGET_FALLBACK = 0,
    SCENE_CONTROLLER_INPUT_ROUTE_TARGET_GLOBAL = 1,
    SCENE_CONTROLLER_INPUT_ROUTE_TARGET_SCENE = 2
} SceneControllerInputRouteTarget;

typedef struct SceneControllerInputEventRaw {
    bool valid;
    bool polled;
    bool ignore_input_active;
    bool quit_requested;
    bool running_after_poll;
    uint32_t wait_blocked_ms;
    uint32_t wait_call_count;
    InputCommands commands;
} SceneControllerInputEventRaw;

typedef struct SceneControllerInputEventNormalized {
    bool has_quit_action;
    bool has_pointer_actions;
    bool has_shortcut_actions;
    bool has_brush_mode_change;
    uint32_t action_count;
} SceneControllerInputEventNormalized;

typedef struct SceneControllerInputRouteResult {
    bool consumed;
    SceneControllerInputRouteTarget target_policy;
    uint32_t routed_global_count;
    uint32_t routed_scene_count;
    uint32_t routed_fallback_count;
} SceneControllerInputRouteResult;

typedef struct SceneControllerInputInvalidationResult {
    bool full_invalidate;
    uint32_t invalidation_reason_bits;
    uint32_t target_invalidation_count;
    uint32_t full_invalidation_count;
} SceneControllerInputInvalidationResult;

typedef struct SceneControllerInputFrame {
    bool valid;
    bool running;
    bool aborted;
    InputCommands effective_commands;
    SceneControllerInputEventRaw raw;
    SceneControllerInputEventNormalized normalized;
    SceneControllerInputRouteResult route;
    SceneControllerInputInvalidationResult invalidation;
} SceneControllerInputFrame;

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
    bool should_save_wind_projection_frames;
    bool headless_mode;
    uint64_t frame_index;
    uint32_t invalidation_reason_bits;
    SimRuntimeBackendReport backend_report;
    RendererHudInfo hud;
} SceneControllerRenderDeriveFrame;

// Runs the lifetime of the application: initializes SDL subsystems via the
// renderer, owns the SceneState, pumps input, drains the command bus, and
// steps physics/rendering until the user quits.
int scene_controller_run(const AppConfig *initial_cfg,
                         const FluidScenePreset *preset,
                         const SceneRuntimeLaunch *runtime_launch,
                         const ShapeAssetLibrary *shape_library,
                         const char *snapshot_dir,
                         const HeadlessOptions *headless);

#endif // SCENE_CONTROLLER_H
