#ifndef PHYSICS_SIM_SCENE_EDITOR_SESSION_H
#define PHYSICS_SIM_SCENE_EDITOR_SESSION_H

#include <stdbool.h>
#include <stddef.h>

#include "app/scene_presets.h"
#include "core_scene.h"
#include "import/runtime_scene_bridge.h"

typedef struct SceneEditorBootstrap {
    bool has_retained_scene;
    PhysicsSimRetainedRuntimeScene retained_scene;
    char retained_runtime_scene_path[512];
} SceneEditorBootstrap;

typedef enum EditorSelectionKind {
    SELECTION_NONE = 0,
    SELECTION_EMITTER,
    SELECTION_OBJECT,
    SELECTION_IMPORT
} EditorSelectionKind;

typedef struct PhysicsSimEditorSessionSelection {
    int retained_object_index;
    char retained_object_id[64];
} PhysicsSimEditorSessionSelection;

typedef struct PhysicsSimEditorSessionLegacySelection {
    EditorSelectionKind kind;
    int emitter_index;
    int object_index;
    int import_index;
} PhysicsSimEditorSessionLegacySelection;

typedef enum PhysicsSimOverlayMotionMode {
    PHYSICS_SIM_OVERLAY_MOTION_DYNAMIC = 0,
    PHYSICS_SIM_OVERLAY_MOTION_STATIC
} PhysicsSimOverlayMotionMode;

typedef enum PhysicsSimDomainShape {
    PHYSICS_SIM_DOMAIN_SHAPE_BOX = 0
} PhysicsSimDomainShape;

typedef struct PhysicsSimEmitterOverlay {
    bool active;
    FluidEmitterType type;
    float radius;
    float strength;
    CoreObjectVec3 direction;
} PhysicsSimEmitterOverlay;

typedef struct PhysicsSimDomainOverlay {
    bool active;
    PhysicsSimDomainShape shape;
    CoreObjectVec3 min;
    CoreObjectVec3 max;
    bool seeded_from_retained_bounds;
    bool derived_defaults;
    int logical_clock;
} PhysicsSimDomainOverlay;

typedef struct PhysicsSimObjectOverlay {
    bool active;
    int retained_object_index;
    char object_id[64];
    PhysicsSimOverlayMotionMode motion_mode;
    CoreObjectVec3 initial_velocity;
    PhysicsSimEmitterOverlay emitter;
} PhysicsSimObjectOverlay;

typedef struct PhysicsSimSceneOverlay {
    bool active;
    bool derived_defaults;
    int logical_clock;
    PhysicsSimDomainOverlay scene_domain;
    int object_overlay_count;
    PhysicsSimObjectOverlay object_overlays[PHYSICS_SIM_RUNTIME_SCENE_MAX_OBJECTS];
} PhysicsSimSceneOverlay;

typedef struct PhysicsSimEditorSession {
    bool using_legacy_preset_adapter;
    FluidScenePreset *working_preset;

    bool has_retained_scene;
    PhysicsSimRetainedRuntimeScene retained_scene;
    bool has_physics_overlay;
    PhysicsSimSceneOverlay physics_overlay;

    PhysicsSimEditorSessionSelection selection;
    PhysicsSimEditorSessionLegacySelection legacy_selection;
} PhysicsSimEditorSession;

void physics_sim_editor_session_init(PhysicsSimEditorSession *session,
                                     FluidScenePreset *working_preset,
                                     const SceneEditorBootstrap *bootstrap);
void physics_sim_editor_session_select_retained_index(PhysicsSimEditorSession *session, int index);
void physics_sim_editor_session_set_legacy_selection(PhysicsSimEditorSession *session,
                                                     EditorSelectionKind kind,
                                                     int emitter_index,
                                                     int object_index,
                                                     int import_index);
bool physics_sim_editor_session_has_retained_scene(const PhysicsSimEditorSession *session);
int physics_sim_editor_session_retained_object_count(const PhysicsSimEditorSession *session);
const char *physics_sim_editor_session_scene_id(const PhysicsSimEditorSession *session);
bool physics_sim_editor_session_has_physics_overlay(const PhysicsSimEditorSession *session);
const CoreSceneObjectContract *physics_sim_editor_session_object_at(const PhysicsSimEditorSession *session,
                                                                    int index);
const CoreSceneObjectContract *physics_sim_editor_session_selected_object(const PhysicsSimEditorSession *session);
const PhysicsSimObjectOverlay *physics_sim_editor_session_object_overlay_at(const PhysicsSimEditorSession *session,
                                                                            int index);
const PhysicsSimObjectOverlay *physics_sim_editor_session_selected_object_overlay(const PhysicsSimEditorSession *session);
const PhysicsSimEmitterOverlay *physics_sim_editor_session_object_emitter_at(const PhysicsSimEditorSession *session,
                                                                              int index);
const PhysicsSimEmitterOverlay *physics_sim_editor_session_selected_object_emitter(const PhysicsSimEditorSession *session);
const PhysicsSimDomainOverlay *physics_sim_editor_session_scene_domain(const PhysicsSimEditorSession *session);
void physics_sim_editor_session_scene_domain_dimensions(const PhysicsSimEditorSession *session,
                                                        double *out_width,
                                                        double *out_height,
                                                        double *out_depth);
bool physics_sim_editor_session_set_selected_motion_mode(PhysicsSimEditorSession *session,
                                                         PhysicsSimOverlayMotionMode mode);
bool physics_sim_editor_session_set_selected_emitter_type(PhysicsSimEditorSession *session,
                                                          FluidEmitterType type,
                                                          bool toggle_clear);
bool physics_sim_editor_session_set_scene_domain_size(PhysicsSimEditorSession *session,
                                                      double width,
                                                      double height,
                                                      double depth);
bool physics_sim_editor_session_nudge_selected_velocity(PhysicsSimEditorSession *session,
                                                        double dx,
                                                        double dy,
                                                        double dz);
bool physics_sim_editor_session_reset_selected_velocity(PhysicsSimEditorSession *session);
bool physics_sim_editor_session_build_overlay_json(const PhysicsSimEditorSession *session,
                                                   char **out_overlay_json,
                                                   char *out_diagnostics,
                                                   size_t out_diagnostics_size);
bool physics_sim_editor_session_hydrate_overlay_from_runtime_scene_json(PhysicsSimEditorSession *session,
                                                                        const char *runtime_scene_json,
                                                                        char *out_diagnostics,
                                                                        size_t out_diagnostics_size);
bool physics_sim_editor_session_mark_overlay_applied(PhysicsSimEditorSession *session);
const char *physics_sim_editor_session_object_kind_label(CoreSceneObjectKind kind);
const char *physics_sim_editor_session_motion_mode_label(PhysicsSimOverlayMotionMode mode);
const char *physics_sim_editor_session_emitter_type_label(FluidEmitterType type);
const char *physics_sim_editor_session_legacy_selection_summary(const PhysicsSimEditorSession *session,
                                                                char *buffer,
                                                                size_t buffer_size);

#endif
