#include "app/editor/scene_editor_session.h"

static float session_clamp_emitter_strength(float strength) {
    if (strength < 0.1f) strength = 0.1f;
    if (strength > 5000.0f) strength = 5000.0f;
    return strength;
}

static float session_clamp_emitter_radius(float radius) {
    if (radius < 0.02f) radius = 0.02f;
    if (radius > 0.6f) radius = 0.6f;
    return radius;
}

static float session_clamp_emitter_thermal_buoyancy(float thermal_buoyancy) {
    if (thermal_buoyancy < 0.0f) thermal_buoyancy = 0.0f;
    if (thermal_buoyancy > 1000.0f) thermal_buoyancy = 1000.0f;
    return thermal_buoyancy;
}

static float session_emitter_default_strength(FluidEmitterType type) {
    switch (type) {
        case EMITTER_DENSITY_SOURCE: return 8.0f;
        case EMITTER_VELOCITY_JET:   return 40.0f;
        case EMITTER_SINK:           return 25.0f;
        default:                     return 8.0f;
    }
}

static float session_default_emitter_radius_for_object(const CoreSceneObjectContract *object) {
    double radius = 0.08;
    if (!object) return 0.08f;
    if (object->has_plane_primitive) {
        radius = object->plane_primitive.width > object->plane_primitive.height
                     ? object->plane_primitive.width
                     : object->plane_primitive.height;
    } else if (object->has_rect_prism_primitive) {
        radius = object->rect_prism_primitive.width;
        if (object->rect_prism_primitive.height > radius) {
            radius = object->rect_prism_primitive.height;
        }
        if (object->rect_prism_primitive.depth > radius) {
            radius = object->rect_prism_primitive.depth;
        }
    } else {
        radius = object->object.transform.scale.x > object->object.transform.scale.y
                     ? object->object.transform.scale.x
                     : object->object.transform.scale.y;
        if (radius <= 0.0) radius = 0.08;
    }
    return session_clamp_emitter_radius((float)radius);
}

static CoreObjectVec3 session_default_emitter_direction_for_object(const CoreSceneObjectContract *object) {
    CoreObjectVec3 direction = {0.0, 0.0, 1.0};
    if (!object) return direction;
    if (object->has_plane_primitive) {
        direction = object->plane_primitive.frame.normal;
    } else if (object->has_rect_prism_primitive) {
        direction = object->rect_prism_primitive.frame.normal;
    }
    return direction;
}

static FluidEmitter3DSourceMode session_default_emitter_source_mode_3d(void) {
    return EMITTER_3D_SOURCE_MODE_SURFACE_PATCH;
}

static FluidEmitter3DSurface session_default_emitter_surface_3d(void) {
    return EMITTER_3D_SURFACE_TOP;
}

static FluidEmitter3DObstacleMode session_default_emitter_obstacle_mode_3d(void) {
    return EMITTER_3D_OBSTACLE_MODE_RETAIN_ATTACHED;
}

static float session_default_emitter_thermal_buoyancy_3d(FluidEmitterType type) {
    return (type == EMITTER_DENSITY_SOURCE) ? 6.0f : 0.0f;
}

static PhysicsSimObjectOverlay *physics_sim_editor_session_selected_overlay_mut(PhysicsSimEditorSession *session) {
    if (!session || !session->has_physics_overlay) return NULL;
    if (session->selection.retained_object_index < 0 ||
        session->selection.retained_object_index >= session->physics_overlay.object_overlay_count) {
        return NULL;
    }
    return &session->physics_overlay.object_overlays[session->selection.retained_object_index];
}

bool physics_sim_editor_session_set_selected_motion_mode(PhysicsSimEditorSession *session,
                                                         PhysicsSimOverlayMotionMode mode) {
    PhysicsSimObjectOverlay *overlay = physics_sim_editor_session_selected_overlay_mut(session);
    if (!overlay) return false;
    overlay->motion_mode = mode;
    session->physics_overlay.derived_defaults = false;
    return true;
}

bool physics_sim_editor_session_set_selected_emitter_type(PhysicsSimEditorSession *session,
                                                          FluidEmitterType type,
                                                          bool toggle_clear) {
    PhysicsSimObjectOverlay *overlay = physics_sim_editor_session_selected_overlay_mut(session);
    const CoreSceneObjectContract *object = NULL;
    if (!overlay || !session || !session->has_retained_scene) return false;

    if (toggle_clear && overlay->emitter.active && overlay->emitter.type == type) {
        overlay->emitter.active = false;
        session->physics_overlay.derived_defaults = false;
        return true;
    }

    object = physics_sim_editor_session_selected_object(session);
    overlay->emitter.active = true;
    overlay->emitter.type = type;
    overlay->motion_mode = PHYSICS_SIM_OVERLAY_MOTION_DYNAMIC;
    if (overlay->emitter.radius <= 0.0f) {
        overlay->emitter.radius = session_default_emitter_radius_for_object(object);
    }
    overlay->emitter.radius = session_clamp_emitter_radius(overlay->emitter.radius);
    overlay->emitter.strength = session_clamp_emitter_strength(session_emitter_default_strength(type));
    if (overlay->emitter.direction.x == 0.0 &&
        overlay->emitter.direction.y == 0.0 &&
        overlay->emitter.direction.z == 0.0) {
        overlay->emitter.direction = session_default_emitter_direction_for_object(object);
    }
    overlay->emitter.source_mode_3d = session_default_emitter_source_mode_3d();
    overlay->emitter.surface_3d = session_default_emitter_surface_3d();
    overlay->emitter.obstacle_mode_3d = session_default_emitter_obstacle_mode_3d();
    overlay->emitter.thermal_buoyancy_3d = session_default_emitter_thermal_buoyancy_3d(type);
    session->physics_overlay.derived_defaults = false;
    return true;
}

bool physics_sim_editor_session_set_selected_emitter_radius(PhysicsSimEditorSession *session,
                                                            float radius) {
    PhysicsSimObjectOverlay *overlay = physics_sim_editor_session_selected_overlay_mut(session);
    if (!overlay || !overlay->emitter.active) return false;
    overlay->emitter.radius = session_clamp_emitter_radius(radius);
    session->physics_overlay.derived_defaults = false;
    return true;
}

bool physics_sim_editor_session_set_selected_emitter_strength(PhysicsSimEditorSession *session,
                                                              float strength) {
    PhysicsSimObjectOverlay *overlay = physics_sim_editor_session_selected_overlay_mut(session);
    if (!overlay || !overlay->emitter.active) return false;
    overlay->emitter.strength = session_clamp_emitter_strength(strength);
    session->physics_overlay.derived_defaults = false;
    return true;
}

bool physics_sim_editor_session_set_selected_emitter_thermal_buoyancy_3d(PhysicsSimEditorSession *session,
                                                                          float thermal_buoyancy) {
    PhysicsSimObjectOverlay *overlay = physics_sim_editor_session_selected_overlay_mut(session);
    if (!overlay || !overlay->emitter.active) return false;
    overlay->emitter.thermal_buoyancy_3d = session_clamp_emitter_thermal_buoyancy(thermal_buoyancy);
    session->physics_overlay.derived_defaults = false;
    return true;
}

bool physics_sim_editor_session_cycle_selected_emitter_source_mode_3d(PhysicsSimEditorSession *session) {
    PhysicsSimObjectOverlay *overlay = physics_sim_editor_session_selected_overlay_mut(session);
    if (!overlay || !overlay->emitter.active) return false;
    switch (overlay->emitter.source_mode_3d) {
    case EMITTER_3D_SOURCE_MODE_SURFACE_PATCH:
        overlay->emitter.source_mode_3d = EMITTER_3D_SOURCE_MODE_SURFACE_SHELL;
        break;
    case EMITTER_3D_SOURCE_MODE_SURFACE_SHELL:
        overlay->emitter.source_mode_3d = EMITTER_3D_SOURCE_MODE_HEATED_OBSTACLE;
        break;
    case EMITTER_3D_SOURCE_MODE_HEATED_OBSTACLE:
        overlay->emitter.source_mode_3d = EMITTER_3D_SOURCE_MODE_VOLUME_FILL;
        break;
    case EMITTER_3D_SOURCE_MODE_VOLUME_FILL:
        overlay->emitter.source_mode_3d = EMITTER_3D_SOURCE_MODE_LEGACY_COMPAT;
        break;
    case EMITTER_3D_SOURCE_MODE_LEGACY_COMPAT:
    default:
        overlay->emitter.source_mode_3d = EMITTER_3D_SOURCE_MODE_SURFACE_PATCH;
        break;
    }
    session->physics_overlay.derived_defaults = false;
    return true;
}

bool physics_sim_editor_session_cycle_selected_emitter_surface_3d(PhysicsSimEditorSession *session) {
    PhysicsSimObjectOverlay *overlay = physics_sim_editor_session_selected_overlay_mut(session);
    if (!overlay || !overlay->emitter.active) return false;
    switch (overlay->emitter.surface_3d) {
    case EMITTER_3D_SURFACE_TOP:
        overlay->emitter.surface_3d = EMITTER_3D_SURFACE_BOTTOM;
        break;
    case EMITTER_3D_SURFACE_BOTTOM:
        overlay->emitter.surface_3d = EMITTER_3D_SURFACE_LEFT;
        break;
    case EMITTER_3D_SURFACE_LEFT:
        overlay->emitter.surface_3d = EMITTER_3D_SURFACE_RIGHT;
        break;
    case EMITTER_3D_SURFACE_RIGHT:
        overlay->emitter.surface_3d = EMITTER_3D_SURFACE_FRONT;
        break;
    case EMITTER_3D_SURFACE_FRONT:
        overlay->emitter.surface_3d = EMITTER_3D_SURFACE_BACK;
        break;
    case EMITTER_3D_SURFACE_BACK:
        overlay->emitter.surface_3d = EMITTER_3D_SURFACE_ALL_FACES;
        break;
    case EMITTER_3D_SURFACE_ALL_FACES:
        overlay->emitter.surface_3d = EMITTER_3D_SURFACE_AUTO;
        break;
    case EMITTER_3D_SURFACE_AUTO:
    default:
        overlay->emitter.surface_3d = EMITTER_3D_SURFACE_TOP;
        break;
    }
    session->physics_overlay.derived_defaults = false;
    return true;
}

bool physics_sim_editor_session_cycle_selected_emitter_obstacle_mode_3d(PhysicsSimEditorSession *session) {
    PhysicsSimObjectOverlay *overlay = physics_sim_editor_session_selected_overlay_mut(session);
    if (!overlay || !overlay->emitter.active) return false;
    switch (overlay->emitter.obstacle_mode_3d) {
    case EMITTER_3D_OBSTACLE_MODE_RETAIN_ATTACHED:
        overlay->emitter.obstacle_mode_3d = EMITTER_3D_OBSTACLE_MODE_CLEAR_ATTACHED;
        break;
    case EMITTER_3D_OBSTACLE_MODE_CLEAR_ATTACHED:
        overlay->emitter.obstacle_mode_3d = EMITTER_3D_OBSTACLE_MODE_AUTO;
        break;
    case EMITTER_3D_OBSTACLE_MODE_AUTO:
    default:
        overlay->emitter.obstacle_mode_3d = EMITTER_3D_OBSTACLE_MODE_RETAIN_ATTACHED;
        break;
    }
    session->physics_overlay.derived_defaults = false;
    return true;
}

bool physics_sim_editor_session_set_scene_domain_size(PhysicsSimEditorSession *session,
                                                      double width,
                                                      double height,
                                                      double depth) {
    PhysicsSimDomainOverlay *domain = NULL;
    CoreObjectVec3 center = {0};
    if (!session || !session->has_physics_overlay) return false;
    if (width <= 0.0 || height <= 0.0 || depth <= 0.0) return false;
    domain = &session->physics_overlay.scene_domain;
    if (!domain->active) return false;

    center.x = 0.5 * (domain->min.x + domain->max.x);
    center.y = 0.5 * (domain->min.y + domain->max.y);
    center.z = 0.5 * (domain->min.z + domain->max.z);
    domain->min.x = center.x - width * 0.5;
    domain->max.x = center.x + width * 0.5;
    domain->min.y = center.y - height * 0.5;
    domain->max.y = center.y + height * 0.5;
    domain->min.z = center.z - depth * 0.5;
    domain->max.z = center.z + depth * 0.5;
    domain->seeded_from_retained_bounds = false;
    domain->derived_defaults = false;
    session->physics_overlay.derived_defaults = false;
    return true;
}

bool physics_sim_editor_session_nudge_selected_velocity(PhysicsSimEditorSession *session,
                                                        double dx,
                                                        double dy,
                                                        double dz) {
    PhysicsSimObjectOverlay *overlay = physics_sim_editor_session_selected_overlay_mut(session);
    if (!overlay) return false;
    overlay->initial_velocity.x += dx;
    overlay->initial_velocity.y += dy;
    overlay->initial_velocity.z += dz;
    session->physics_overlay.derived_defaults = false;
    return true;
}

bool physics_sim_editor_session_reset_selected_velocity(PhysicsSimEditorSession *session) {
    PhysicsSimObjectOverlay *overlay = physics_sim_editor_session_selected_overlay_mut(session);
    if (!overlay) return false;
    overlay->initial_velocity = (CoreObjectVec3){0};
    session->physics_overlay.derived_defaults = false;
    return true;
}
