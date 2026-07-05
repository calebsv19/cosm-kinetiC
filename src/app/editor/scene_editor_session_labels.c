#include "app/editor/scene_editor_session.h"

#include <stdio.h>

const char *physics_sim_editor_session_object_kind_label(CoreSceneObjectKind kind) {
    switch (kind) {
        case CORE_SCENE_OBJECT_KIND_PLANE_PRIMITIVE:
            return "Plane Primitive";
        case CORE_SCENE_OBJECT_KIND_RECT_PRISM_PRIMITIVE:
            return "Rect Prism Primitive";
        case CORE_SCENE_OBJECT_KIND_CURVE_PATH:
            return "Curve Path";
        case CORE_SCENE_OBJECT_KIND_POINT_SET:
            return "Point Set";
        case CORE_SCENE_OBJECT_KIND_EDGE_SET:
            return "Edge Set";
        case CORE_SCENE_OBJECT_KIND_MESH_ASSET_INSTANCE:
            return "Mesh Asset Instance";
        case CORE_SCENE_OBJECT_KIND_UNKNOWN:
        default:
            return "Unknown";
    }
}

const char *physics_sim_editor_session_motion_mode_label(PhysicsSimOverlayMotionMode mode) {
    switch (mode) {
        case PHYSICS_SIM_OVERLAY_MOTION_STATIC:
            return "Static";
        case PHYSICS_SIM_OVERLAY_MOTION_DYNAMIC:
        default:
            return "Dynamic";
    }
}

const char *physics_sim_editor_session_emitter_type_label(FluidEmitterType type) {
    switch (type) {
        case EMITTER_VELOCITY_JET:
            return "Jet";
        case EMITTER_SINK:
            return "Sink";
        case EMITTER_DENSITY_SOURCE:
        default:
            return "Source";
    }
}

const char *physics_sim_editor_session_emitter_source_mode_3d_label(FluidEmitter3DSourceMode mode) {
    switch (mode) {
    case EMITTER_3D_SOURCE_MODE_VOLUME_FILL:
        return "VolumeFill";
    case EMITTER_3D_SOURCE_MODE_SURFACE_PATCH:
        return "SurfacePatch";
    case EMITTER_3D_SOURCE_MODE_SURFACE_SHELL:
        return "SurfaceShell";
    case EMITTER_3D_SOURCE_MODE_HEATED_OBSTACLE:
        return "HeatedObstacle";
    case EMITTER_3D_SOURCE_MODE_LEGACY_COMPAT:
    default:
        return "LegacyCompat";
    }
}

const char *physics_sim_editor_session_emitter_surface_3d_label(FluidEmitter3DSurface surface) {
    switch (surface) {
    case EMITTER_3D_SURFACE_TOP:
        return "Top";
    case EMITTER_3D_SURFACE_BOTTOM:
        return "Bottom";
    case EMITTER_3D_SURFACE_LEFT:
        return "Left";
    case EMITTER_3D_SURFACE_RIGHT:
        return "Right";
    case EMITTER_3D_SURFACE_FRONT:
        return "Front";
    case EMITTER_3D_SURFACE_BACK:
        return "Back";
    case EMITTER_3D_SURFACE_ALL_FACES:
        return "AllFaces";
    case EMITTER_3D_SURFACE_AUTO:
    default:
        return "Auto";
    }
}

const char *physics_sim_editor_session_emitter_obstacle_mode_3d_label(FluidEmitter3DObstacleMode mode) {
    switch (mode) {
    case EMITTER_3D_OBSTACLE_MODE_CLEAR_ATTACHED:
        return "ClearAttached";
    case EMITTER_3D_OBSTACLE_MODE_RETAIN_ATTACHED:
        return "RetainAttached";
    case EMITTER_3D_OBSTACLE_MODE_AUTO:
    default:
        return "Auto";
    }
}

const char *physics_sim_editor_session_runtime_mesh_role_label(PhysicsSimRuntimeMeshEditorRole role) {
    switch (role) {
    case PHYSICS_SIM_RUNTIME_MESH_EDITOR_ROLE_VISUAL_ONLY:
        return "VisualOnly";
    case PHYSICS_SIM_RUNTIME_MESH_EDITOR_ROLE_SURFACE_EMITTER:
        return "SurfaceEmitter";
    case PHYSICS_SIM_RUNTIME_MESH_EDITOR_ROLE_SURFACE_HEAT_EMITTER:
        return "SurfaceHeatEmitter";
    case PHYSICS_SIM_RUNTIME_MESH_EDITOR_ROLE_BOUNDARY_FLOW_EMITTER:
        return "BoundaryFlowEmitter";
    case PHYSICS_SIM_RUNTIME_MESH_EDITOR_ROLE_SOLID:
    default:
        return "Solid";
    }
}

const char *physics_sim_editor_session_legacy_selection_summary(const PhysicsSimEditorSession *session,
                                                                char *buffer,
                                                                size_t buffer_size) {
    const char *summary = "Legacy Selection: none";
    if (!buffer || buffer_size == 0) return "";
    buffer[0] = '\0';

    if (!session) {
        snprintf(buffer, buffer_size, "%s", summary);
        return buffer;
    }

    switch (session->legacy_selection.kind) {
        case SELECTION_EMITTER:
            snprintf(buffer,
                     buffer_size,
                     "Legacy Selection: emitter=%d object=%d import=%d",
                     session->legacy_selection.emitter_index,
                     session->legacy_selection.object_index,
                     session->legacy_selection.import_index);
            break;
        case SELECTION_OBJECT:
            snprintf(buffer,
                     buffer_size,
                     "Legacy Selection: object=%d emitter=%d",
                     session->legacy_selection.object_index,
                     session->legacy_selection.emitter_index);
            break;
        case SELECTION_IMPORT:
            snprintf(buffer,
                     buffer_size,
                     "Legacy Selection: import=%d emitter=%d",
                     session->legacy_selection.import_index,
                     session->legacy_selection.emitter_index);
            break;
        case SELECTION_NONE:
        default:
            snprintf(buffer, buffer_size, "%s", summary);
            break;
    }
    return buffer;
}
