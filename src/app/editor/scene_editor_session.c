#include "app/editor/scene_editor_session.h"
#include "app/editor/scene_editor_wind_setup.h"

#include <json-c/json.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static void session_diag(char *out_diagnostics, size_t out_diagnostics_size, const char *message) {
    if (!out_diagnostics || out_diagnostics_size == 0 || !message) return;
    snprintf(out_diagnostics, out_diagnostics_size, "%s", message);
}

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
        radius = fmax(object->plane_primitive.width, object->plane_primitive.height);
    } else if (object->has_rect_prism_primitive) {
        radius = fmax(object->rect_prism_primitive.width,
                      fmax(object->rect_prism_primitive.height,
                           object->rect_prism_primitive.depth));
    } else {
        radius = fmax(object->object.transform.scale.x, object->object.transform.scale.y);
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

static bool session_is_legacy_sideways_direction(CoreObjectVec3 direction) {
    return fabs(direction.x) <= 1e-9 && fabs(direction.y - (-1.0)) <= 1e-9 &&
           fabs(direction.z) <= 1e-9;
}

static bool session_text_is_role(const char *text, const char *a, const char *b) {
    if (!text || !text[0]) return false;
    if (a && strcmp(text, a) == 0) return true;
    if (b && strcmp(text, b) == 0) return true;
    return false;
}

static const char *session_wind_outlet_policy_label(WindTunnel3DOutletPolicy policy) {
    switch (policy) {
        case WIND_TUNNEL_3D_OUTLET_CLEAR: return "clear";
        case WIND_TUNNEL_3D_OUTLET_RECEIVE:
        default: return "receive";
    }
}

static const char *session_wind_wall_policy_label(WindTunnel3DWallPolicy policy) {
    switch (policy) {
        case WIND_TUNNEL_3D_WALL_OPEN: return "open";
        case WIND_TUNNEL_3D_WALL_SLIP: return "slip";
        case WIND_TUNNEL_3D_WALL_NO_SLIP:
        default: return "no_slip";
    }
}

static bool session_json_number_field(json_object *obj, const char *key, double *out_value) {
    json_object *node = NULL;
    if (!obj || !key || !out_value) return false;
    if (!json_object_object_get_ex(obj, key, &node) ||
        (!json_object_is_type(node, json_type_double) &&
         !json_object_is_type(node, json_type_int))) {
        return false;
    }
    *out_value = json_object_get_double(node);
    return true;
}

static bool session_add_wind_tunnel_json(json_object *physics_ext,
                                         const WindTunnel3DConfig *config) {
    json_object *wind = NULL;
    if (!physics_ext || !config || !wind_tunnel_3d_config_validate(config)) return true;
    wind = json_object_new_object();
    if (!wind) return false;
    json_object_object_add(wind, "active", json_object_new_boolean(config->active ? 1 : 0));
    json_object_object_add(wind, "inlet_face", json_object_new_string(wind_tunnel_3d_face_label(config->inlet_face)));
    json_object_object_add(wind, "outlet_face", json_object_new_string(wind_tunnel_3d_face_label(config->outlet_face)));
    json_object_object_add(wind, "inflow_speed", json_object_new_double(config->inflow_speed));
    json_object_object_add(wind, "inflow_density", json_object_new_double(config->inflow_density));
    json_object_object_add(wind, "inlet_slab_cells", json_object_new_int(config->inlet_slab_cells));
    json_object_object_add(wind, "outlet_policy", json_object_new_string(session_wind_outlet_policy_label(config->outlet_policy)));
    json_object_object_add(wind, "wall_policy", json_object_new_string(session_wind_wall_policy_label(config->wall_policy)));
    json_object_object_add(physics_ext, "wind_tunnel", wind);
    return true;
}

static bool session_hydrate_wind_tunnel_from_physics_ext(PhysicsSimEditorSession *session,
                                                         json_object *physics_ext) {
    json_object *wind = NULL;
    json_object *node = NULL;
    WindTunnel3DConfig config;
    double number = 0.0;
    if (!session || !physics_ext) return false;
    if (!json_object_object_get_ex(physics_ext, "wind_tunnel", &wind) ||
        !json_object_is_type(wind, json_type_object)) {
        return false;
    }

    config = session->has_wind_tunnel_config
                 ? session->wind_tunnel
                 : wind_tunnel_3d_config_default(NULL);
    if (json_object_object_get_ex(wind, "active", &node)) {
        config.active = json_object_get_boolean(node) ? true : false;
    }
    if (json_object_object_get_ex(wind, "inlet_face", &node) &&
        json_object_is_type(node, json_type_string)) {
        (void)wind_tunnel_3d_face_from_label(json_object_get_string(node), &config.inlet_face);
    }
    if (json_object_object_get_ex(wind, "outlet_face", &node) &&
        json_object_is_type(node, json_type_string)) {
        (void)wind_tunnel_3d_face_from_label(json_object_get_string(node), &config.outlet_face);
    }
    if (session_json_number_field(wind, "inflow_speed", &number)) {
        config.inflow_speed = (float)number;
    }
    if (session_json_number_field(wind, "inflow_density", &number)) {
        config.inflow_density = (float)number;
    }
    if (session_json_number_field(wind, "inlet_slab_cells", &number)) {
        config.inlet_slab_cells = (int)number;
    }
    if (json_object_object_get_ex(wind, "outlet_policy", &node) &&
        json_object_is_type(node, json_type_string)) {
        (void)wind_tunnel_3d_outlet_policy_from_label(json_object_get_string(node), &config.outlet_policy);
    }
    if (json_object_object_get_ex(wind, "wall_policy", &node) &&
        json_object_is_type(node, json_type_string)) {
        (void)wind_tunnel_3d_wall_policy_from_label(json_object_get_string(node), &config.wall_policy);
    }
    if (!wind_tunnel_3d_config_validate(&config)) return false;
    session->wind_tunnel = config;
    session->has_wind_tunnel_config = true;
    session->wind_tunnel_authored = true;
    return true;
}

static PhysicsSimRuntimeMeshEditorRole session_mesh_role_from_overlay_text(const char *text) {
    if (session_text_is_role(text, "solid_obstacle", "solid") ||
        session_text_is_role(text, "Solid", "SolidObstacle")) {
        return PHYSICS_SIM_RUNTIME_MESH_EDITOR_ROLE_SOLID;
    }
    if (session_text_is_role(text, "visual_only", "none") ||
        session_text_is_role(text, "Visual", "VisualOnly")) {
        return PHYSICS_SIM_RUNTIME_MESH_EDITOR_ROLE_VISUAL_ONLY;
    }
    if (session_text_is_role(text, "surface_heat_emitter", "heat_emitter") ||
        session_text_is_role(text, "Heat", "SurfaceHeatEmitter")) {
        return PHYSICS_SIM_RUNTIME_MESH_EDITOR_ROLE_SURFACE_HEAT_EMITTER;
    }
    if (session_text_is_role(text, "boundary_flow_emitter", "velocity_emitter") ||
        session_text_is_role(text, "Flow", "BoundaryFlowEmitter")) {
        return PHYSICS_SIM_RUNTIME_MESH_EDITOR_ROLE_BOUNDARY_FLOW_EMITTER;
    }
    if (session_text_is_role(text, "surface_emitter", "emitter") ||
        session_text_is_role(text, "Surface", "SurfaceEmitter")) {
        return PHYSICS_SIM_RUNTIME_MESH_EDITOR_ROLE_SURFACE_EMITTER;
    }
    return PHYSICS_SIM_RUNTIME_MESH_EDITOR_ROLE_SOLID;
}

static PhysicsSimRuntimeMeshEditorRole session_mesh_role_from_behavior(const char *behavior,
                                                                       bool fluid_obstacle_enabled,
                                                                       bool fluid_emitter_enabled,
                                                                       FluidEmitter3DSourceMode source_mode,
                                                                       FluidEmitterType emitter_type) {
    if (session_text_is_role(behavior, "visual_only", "none")) {
        return PHYSICS_SIM_RUNTIME_MESH_EDITOR_ROLE_VISUAL_ONLY;
    }
    if (fluid_emitter_enabled ||
        session_text_is_role(behavior, "surface_emitter", "emitter") ||
        session_text_is_role(behavior, "surface_heat_emitter", "heat_emitter") ||
        session_text_is_role(behavior, "boundary_flow_emitter", "velocity_emitter")) {
        if (session_text_is_role(behavior, "surface_heat_emitter", "heat_emitter") ||
            source_mode == EMITTER_3D_SOURCE_MODE_HEATED_OBSTACLE) {
            return PHYSICS_SIM_RUNTIME_MESH_EDITOR_ROLE_SURFACE_HEAT_EMITTER;
        }
        if (session_text_is_role(behavior, "boundary_flow_emitter", "velocity_emitter") ||
            emitter_type == EMITTER_VELOCITY_JET) {
            return PHYSICS_SIM_RUNTIME_MESH_EDITOR_ROLE_BOUNDARY_FLOW_EMITTER;
        }
        return PHYSICS_SIM_RUNTIME_MESH_EDITOR_ROLE_SURFACE_EMITTER;
    }
    if (fluid_obstacle_enabled || session_text_is_role(behavior, "solid_obstacle", "solid")) {
        return PHYSICS_SIM_RUNTIME_MESH_EDITOR_ROLE_SOLID;
    }
    return PHYSICS_SIM_RUNTIME_MESH_EDITOR_ROLE_VISUAL_ONLY;
}

static const char *session_mesh_role_behavior(PhysicsSimRuntimeMeshEditorRole role) {
    switch (role) {
    case PHYSICS_SIM_RUNTIME_MESH_EDITOR_ROLE_VISUAL_ONLY:
        return "visual_only";
    case PHYSICS_SIM_RUNTIME_MESH_EDITOR_ROLE_SURFACE_EMITTER:
        return "surface_emitter";
    case PHYSICS_SIM_RUNTIME_MESH_EDITOR_ROLE_SURFACE_HEAT_EMITTER:
        return "surface_heat_emitter";
    case PHYSICS_SIM_RUNTIME_MESH_EDITOR_ROLE_BOUNDARY_FLOW_EMITTER:
        return "boundary_flow_emitter";
    case PHYSICS_SIM_RUNTIME_MESH_EDITOR_ROLE_SOLID:
    default:
        return "solid_obstacle";
    }
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

static void session_mesh_overlay_apply_role_defaults(PhysicsSimRuntimeMeshOverlay *overlay) {
    bool had_active_emitter = false;
    if (!overlay) return;
    had_active_emitter = overlay->emitter.active;
    overlay->emitter.active = false;
    switch (overlay->role) {
    case PHYSICS_SIM_RUNTIME_MESH_EDITOR_ROLE_SURFACE_EMITTER:
        overlay->emitter.active = true;
        if (!had_active_emitter) {
            overlay->emitter.type = EMITTER_DENSITY_SOURCE;
            overlay->emitter.source_mode_3d = EMITTER_3D_SOURCE_MODE_SURFACE_SHELL;
            overlay->emitter.surface_3d = EMITTER_3D_SURFACE_ALL_FACES;
            overlay->emitter.obstacle_mode_3d = EMITTER_3D_OBSTACLE_MODE_CLEAR_ATTACHED;
            overlay->emitter.thermal_buoyancy_3d = 0.0f;
        }
        break;
    case PHYSICS_SIM_RUNTIME_MESH_EDITOR_ROLE_SURFACE_HEAT_EMITTER:
        overlay->emitter.active = true;
        if (!had_active_emitter) {
            overlay->emitter.type = EMITTER_DENSITY_SOURCE;
            overlay->emitter.source_mode_3d = EMITTER_3D_SOURCE_MODE_HEATED_OBSTACLE;
            overlay->emitter.surface_3d = EMITTER_3D_SURFACE_ALL_FACES;
            overlay->emitter.obstacle_mode_3d = EMITTER_3D_OBSTACLE_MODE_CLEAR_ATTACHED;
        }
        if (overlay->emitter.thermal_buoyancy_3d <= 0.0f) {
            overlay->emitter.thermal_buoyancy_3d = 6.0f;
        }
        break;
    case PHYSICS_SIM_RUNTIME_MESH_EDITOR_ROLE_BOUNDARY_FLOW_EMITTER:
        overlay->emitter.active = true;
        if (!had_active_emitter) {
            overlay->emitter.type = EMITTER_VELOCITY_JET;
            overlay->emitter.source_mode_3d = EMITTER_3D_SOURCE_MODE_SURFACE_SHELL;
            overlay->emitter.surface_3d = EMITTER_3D_SURFACE_ALL_FACES;
            overlay->emitter.obstacle_mode_3d = EMITTER_3D_OBSTACLE_MODE_CLEAR_ATTACHED;
            overlay->emitter.thermal_buoyancy_3d = 0.0f;
        }
        break;
    case PHYSICS_SIM_RUNTIME_MESH_EDITOR_ROLE_SOLID:
    case PHYSICS_SIM_RUNTIME_MESH_EDITOR_ROLE_VISUAL_ONLY:
    default:
        break;
    }
    if (overlay->emitter.radius <= 0.0f) {
        overlay->emitter.radius = 0.08f;
    }
    overlay->emitter.radius = session_clamp_emitter_radius(overlay->emitter.radius);
    if (overlay->emitter.strength <= 0.0f) {
        overlay->emitter.strength = session_emitter_default_strength(overlay->emitter.type);
    }
    overlay->emitter.strength = session_clamp_emitter_strength(overlay->emitter.strength);
    if (overlay->emitter.direction.x == 0.0 &&
        overlay->emitter.direction.y == 0.0 &&
        overlay->emitter.direction.z == 0.0) {
        overlay->emitter.direction = (CoreObjectVec3){0.0, 0.0, 1.0};
    }
}

static bool session_compute_retained_bounds(const PhysicsSimRetainedRuntimeScene *retained,
                                            CoreObjectVec3 *out_min,
                                            CoreObjectVec3 *out_max,
                                            bool *out_seeded_from_retained_bounds) {
    CoreObjectVec3 corners[8];
    bool have = false;
    int i = 0;
    if (out_seeded_from_retained_bounds) *out_seeded_from_retained_bounds = false;
    if (!retained || !out_min || !out_max) return false;

    if (retained->has_line_drawing_scene3d && retained->bounds.enabled) {
        *out_min = retained->bounds.min;
        *out_max = retained->bounds.max;
        if (out_seeded_from_retained_bounds) *out_seeded_from_retained_bounds = true;
        return true;
    }

    for (i = 0; i < retained->retained_object_count; ++i) {
        const CoreSceneObjectContract *object = &retained->objects[i];
        int corner_count = 0;
        if (object->has_plane_primitive) {
            const CoreScenePlanePrimitive *plane = &object->plane_primitive;
            const double half_width = plane->width * 0.5;
            const double half_height = plane->height * 0.5;
            CoreObjectVec3 origin = plane->frame.origin;
            CoreObjectVec3 u_plus = origin;
            CoreObjectVec3 u_minus = origin;
            u_plus.x += plane->frame.axis_u.x * half_width;
            u_plus.y += plane->frame.axis_u.y * half_width;
            u_plus.z += plane->frame.axis_u.z * half_width;
            u_minus.x -= plane->frame.axis_u.x * half_width;
            u_minus.y -= plane->frame.axis_u.y * half_width;
            u_minus.z -= plane->frame.axis_u.z * half_width;
            corners[0] = u_minus;
            corners[0].x -= plane->frame.axis_v.x * half_height;
            corners[0].y -= plane->frame.axis_v.y * half_height;
            corners[0].z -= plane->frame.axis_v.z * half_height;
            corners[1] = u_plus;
            corners[1].x -= plane->frame.axis_v.x * half_height;
            corners[1].y -= plane->frame.axis_v.y * half_height;
            corners[1].z -= plane->frame.axis_v.z * half_height;
            corners[2] = u_plus;
            corners[2].x += plane->frame.axis_v.x * half_height;
            corners[2].y += plane->frame.axis_v.y * half_height;
            corners[2].z += plane->frame.axis_v.z * half_height;
            corners[3] = u_minus;
            corners[3].x += plane->frame.axis_v.x * half_height;
            corners[3].y += plane->frame.axis_v.y * half_height;
            corners[3].z += plane->frame.axis_v.z * half_height;
            corner_count = 4;
        } else if (object->has_rect_prism_primitive) {
            const CoreSceneRectPrismPrimitive *prism = &object->rect_prism_primitive;
            const double half_width = prism->width * 0.5;
            const double half_height = prism->height * 0.5;
            const double half_depth = prism->depth * 0.5;
            int corner_index = 0;
            for (int sx = -1; sx <= 1; sx += 2) {
                for (int sy = -1; sy <= 1; sy += 2) {
                    for (int sz = -1; sz <= 1; sz += 2) {
                        CoreObjectVec3 corner = prism->frame.origin;
                        corner.x += prism->frame.axis_u.x * half_width * (double)sx;
                        corner.y += prism->frame.axis_u.y * half_width * (double)sx;
                        corner.z += prism->frame.axis_u.z * half_width * (double)sx;
                        corner.x += prism->frame.axis_v.x * half_height * (double)sy;
                        corner.y += prism->frame.axis_v.y * half_height * (double)sy;
                        corner.z += prism->frame.axis_v.z * half_height * (double)sy;
                        corner.x += prism->frame.normal.x * half_depth * (double)sz;
                        corner.y += prism->frame.normal.y * half_depth * (double)sz;
                        corner.z += prism->frame.normal.z * half_depth * (double)sz;
                        corners[corner_index++] = corner;
                    }
                }
            }
            corner_count = 8;
        } else {
            corners[0] = object->object.transform.position;
            corner_count = 1;
        }

        for (int c = 0; c < corner_count; ++c) {
            const CoreObjectVec3 point = corners[c];
            if (!have) {
                *out_min = point;
                *out_max = point;
                have = true;
            } else {
                if (point.x < out_min->x) out_min->x = point.x;
                if (point.y < out_min->y) out_min->y = point.y;
                if (point.z < out_min->z) out_min->z = point.z;
                if (point.x > out_max->x) out_max->x = point.x;
                if (point.y > out_max->y) out_max->y = point.y;
                if (point.z > out_max->z) out_max->z = point.z;
            }
        }
    }

    return have;
}

static void session_seed_default_scene_domain(PhysicsSimEditorSession *session) {
    CoreObjectVec3 min = {0};
    CoreObjectVec3 max = {0};
    bool seeded_from_retained_bounds = false;
    if (!session || !session->has_retained_scene) return;

    session->physics_overlay.scene_domain.active = true;
    session->physics_overlay.scene_domain.shape = PHYSICS_SIM_DOMAIN_SHAPE_BOX;
    session->physics_overlay.scene_domain.derived_defaults = true;
    session->physics_overlay.scene_domain.logical_clock = 0;
    if (session_compute_retained_bounds(&session->retained_scene,
                                        &min,
                                        &max,
                                        &seeded_from_retained_bounds)) {
        session->physics_overlay.scene_domain.min = min;
        session->physics_overlay.scene_domain.max = max;
        session->physics_overlay.scene_domain.seeded_from_retained_bounds = seeded_from_retained_bounds;
    }
}

static void physics_sim_editor_session_seed_default_overlay(PhysicsSimEditorSession *session) {
    int i = 0;
    if (!session || !session->has_retained_scene) return;

    memset(&session->physics_overlay, 0, sizeof(session->physics_overlay));
    session->has_physics_overlay = true;
    session->physics_overlay.active = true;
    session->physics_overlay.derived_defaults = true;
    session->physics_overlay.logical_clock = 0;
    session_seed_default_scene_domain(session);

    for (i = 0; i < session->retained_scene.retained_object_count; ++i) {
        const CoreSceneObjectContract *object = &session->retained_scene.objects[i];
        PhysicsSimObjectOverlay *overlay = &session->physics_overlay.object_overlays[session->physics_overlay.object_overlay_count];
        overlay->active = true;
        overlay->retained_object_index = i;
        snprintf(overlay->object_id, sizeof(overlay->object_id), "%s", object->object.object_id);
        overlay->motion_mode = object->object.flags.locked
                                   ? PHYSICS_SIM_OVERLAY_MOTION_STATIC
                                   : PHYSICS_SIM_OVERLAY_MOTION_DYNAMIC;
        overlay->initial_velocity = (CoreObjectVec3){0};
        overlay->emitter.active = false;
        overlay->emitter.type = EMITTER_DENSITY_SOURCE;
        overlay->emitter.radius = session_default_emitter_radius_for_object(object);
        overlay->emitter.strength = session_emitter_default_strength(EMITTER_DENSITY_SOURCE);
        overlay->emitter.direction = session_default_emitter_direction_for_object(object);
        overlay->emitter.source_mode_3d = session_default_emitter_source_mode_3d();
        overlay->emitter.surface_3d = session_default_emitter_surface_3d();
        overlay->emitter.obstacle_mode_3d = session_default_emitter_obstacle_mode_3d();
        overlay->emitter.thermal_buoyancy_3d =
            session_default_emitter_thermal_buoyancy_3d(EMITTER_DENSITY_SOURCE);
        session->physics_overlay.object_overlay_count++;
    }

    for (i = 0; i < session->mesh_previews.instance_count &&
                i < PHYSICS_SIM_RUNTIME_MESH_PREVIEW_MAX_INSTANCES; ++i) {
        const PhysicsSimRuntimeMeshPreviewInstance *preview = &session->mesh_previews.instances[i];
        PhysicsSimRuntimeMeshOverlay *overlay =
            &session->physics_overlay.mesh_overlays[session->physics_overlay.mesh_overlay_count];
        overlay->active = true;
        overlay->mesh_instance_index = i;
        overlay->scene_object_index = preview->scene_object_index;
        snprintf(overlay->object_id, sizeof(overlay->object_id), "%s", preview->object_id);
        overlay->role = session_mesh_role_from_behavior(preview->fluid_behavior,
                                                        preview->fluid_obstacle_enabled,
                                                        preview->fluid_emitter_enabled,
                                                        preview->emitter_source_mode_3d,
                                                        preview->emitter_type);
        overlay->emitter.active = preview->fluid_emitter_enabled;
        overlay->emitter.type = preview->emitter_type;
        overlay->emitter.radius = preview->emitter_radius;
        overlay->emitter.strength = preview->emitter_strength;
        overlay->emitter.direction = preview->emitter_direction;
        overlay->emitter.source_mode_3d = preview->emitter_source_mode_3d;
        overlay->emitter.surface_3d = preview->emitter_surface_3d;
        overlay->emitter.obstacle_mode_3d = preview->emitter_obstacle_mode_3d;
        overlay->emitter.thermal_buoyancy_3d = preview->emitter_thermal_buoyancy_3d;
        session_mesh_overlay_apply_role_defaults(overlay);
        session->physics_overlay.mesh_overlay_count++;
    }
}

static PhysicsSimObjectOverlay *physics_sim_editor_session_find_overlay_by_object_id_mut(PhysicsSimEditorSession *session,
                                                                                          const char *object_id) {
    int i = 0;
    if (!session || !session->has_physics_overlay || !object_id || !object_id[0]) return NULL;
    for (i = 0; i < session->physics_overlay.object_overlay_count; ++i) {
        PhysicsSimObjectOverlay *overlay = &session->physics_overlay.object_overlays[i];
        if (!overlay->active) continue;
        if (strcmp(overlay->object_id, object_id) == 0) {
            return overlay;
        }
    }
    return NULL;
}

static PhysicsSimRuntimeMeshOverlay *physics_sim_editor_session_find_mesh_overlay_by_object_id_mut(
    PhysicsSimEditorSession *session,
    const char *object_id) {
    int i = 0;
    if (!session || !session->has_physics_overlay || !object_id || !object_id[0]) return NULL;
    for (i = 0; i < session->physics_overlay.mesh_overlay_count; ++i) {
        PhysicsSimRuntimeMeshOverlay *overlay = &session->physics_overlay.mesh_overlays[i];
        if (!overlay->active) continue;
        if (strcmp(overlay->object_id, object_id) == 0) {
            return overlay;
        }
    }
    return NULL;
}

static void physics_sim_editor_session_clear(PhysicsSimEditorSession *session) {
    if (!session) return;
    memset(session, 0, sizeof(*session));
    session->selection.retained_object_index = -1;
    session->selection.retained_object_id[0] = '\0';
    session->legacy_selection.kind = SELECTION_NONE;
    session->legacy_selection.emitter_index = -1;
    session->legacy_selection.object_index = -1;
    session->legacy_selection.import_index = -1;
}

void physics_sim_editor_session_init(PhysicsSimEditorSession *session,
                                     FluidScenePreset *working_preset,
                                     const SceneEditorBootstrap *bootstrap) {
    if (!session) return;

    physics_sim_editor_session_clear(session);
    session->using_legacy_preset_adapter = (working_preset != NULL);
    session->working_preset = working_preset;

    if (!bootstrap || !bootstrap->has_retained_scene) {
        return;
    }
    if (!bootstrap->retained_scene.valid_contract) {
        return;
    }

    session->retained_scene = bootstrap->retained_scene;
    session->has_retained_scene = true;
    session->mesh_previews = bootstrap->mesh_previews;
    if (bootstrap->wind_tunnel_authored &&
        wind_tunnel_3d_config_validate(&bootstrap->wind_tunnel)) {
        session->has_wind_tunnel_config = true;
        session->wind_tunnel_authored = true;
        session->wind_tunnel = bootstrap->wind_tunnel;
    }
    physics_sim_editor_session_seed_default_overlay(session);
    physics_sim_editor_session_select_retained_index(session, 0);
}

void physics_sim_editor_session_select_retained_index(PhysicsSimEditorSession *session, int index) {
    const CoreSceneObjectContract *object = NULL;
    if (!session || !session->has_retained_scene) return;

    session->selection.retained_object_index = -1;
    session->selection.retained_object_id[0] = '\0';

    if (index < 0 || index >= session->retained_scene.retained_object_count) {
        return;
    }

    object = &session->retained_scene.objects[index];
    session->selection.retained_object_index = index;
    snprintf(session->selection.retained_object_id,
             sizeof(session->selection.retained_object_id),
             "%s",
             object->object.object_id);
}

void physics_sim_editor_session_set_legacy_selection(PhysicsSimEditorSession *session,
                                                     EditorSelectionKind kind,
                                                     int emitter_index,
                                                     int object_index,
                                                     int import_index) {
    if (!session) return;

    session->legacy_selection.kind = kind;
    session->legacy_selection.emitter_index = emitter_index;
    session->legacy_selection.object_index = object_index;
    session->legacy_selection.import_index = import_index;
}

bool physics_sim_editor_session_has_retained_scene(const PhysicsSimEditorSession *session) {
    return session && session->has_retained_scene;
}

bool physics_sim_editor_session_has_wind_tunnel_config(const PhysicsSimEditorSession *session) {
    return session && session->has_retained_scene && session->has_wind_tunnel_config;
}

bool physics_sim_editor_session_wind_tunnel_authored(const PhysicsSimEditorSession *session) {
    return physics_sim_editor_session_has_wind_tunnel_config(session) &&
           session->wind_tunnel_authored;
}

const WindTunnel3DConfig *physics_sim_editor_session_wind_tunnel_config(const PhysicsSimEditorSession *session) {
    if (!physics_sim_editor_session_has_wind_tunnel_config(session)) return NULL;
    return &session->wind_tunnel;
}

bool physics_sim_editor_session_set_wind_tunnel_faces(PhysicsSimEditorSession *session,
                                                      const AppConfig *cfg,
                                                      WindTunnel3DFace inlet_face,
                                                      WindTunnel3DFace outlet_face) {
    WindTunnel3DConfig config;
    if (!session || !session->has_retained_scene) return false;
    if (inlet_face == WIND_TUNNEL_3D_FACE_NONE ||
        outlet_face == WIND_TUNNEL_3D_FACE_NONE ||
        inlet_face == outlet_face) {
        return false;
    }
    config = session->has_wind_tunnel_config
                 ? session->wind_tunnel
                 : wind_tunnel_3d_config_default(cfg);
    config.active = true;
    config.inlet_face = inlet_face;
    config.outlet_face = outlet_face;
    if (!wind_tunnel_3d_config_validate(&config)) return false;
    session->wind_tunnel = config;
    session->has_wind_tunnel_config = true;
    session->wind_tunnel_authored = false;
    return true;
}

bool physics_sim_editor_session_has_physics_overlay(const PhysicsSimEditorSession *session) {
    return session && session->has_physics_overlay && session->physics_overlay.active;
}

int physics_sim_editor_session_retained_object_count(const PhysicsSimEditorSession *session) {
    if (!session || !session->has_retained_scene) return 0;
    return session->retained_scene.retained_object_count;
}

const char *physics_sim_editor_session_scene_id(const PhysicsSimEditorSession *session) {
    if (!session || !session->has_retained_scene) return "";
    return session->retained_scene.root.scene_id;
}

const CoreSceneObjectContract *physics_sim_editor_session_object_at(const PhysicsSimEditorSession *session,
                                                                    int index) {
    if (!session || !session->has_retained_scene) return NULL;
    if (index < 0 || index >= session->retained_scene.retained_object_count) return NULL;
    return &session->retained_scene.objects[index];
}

const CoreSceneObjectContract *physics_sim_editor_session_selected_object(const PhysicsSimEditorSession *session) {
    if (!session) return NULL;
    return physics_sim_editor_session_object_at(session, session->selection.retained_object_index);
}

const PhysicsSimObjectOverlay *physics_sim_editor_session_object_overlay_at(const PhysicsSimEditorSession *session,
                                                                            int index) {
    if (!session || !session->has_physics_overlay) return NULL;
    if (index < 0 || index >= session->physics_overlay.object_overlay_count) return NULL;
    return &session->physics_overlay.object_overlays[index];
}

const PhysicsSimObjectOverlay *physics_sim_editor_session_selected_object_overlay(const PhysicsSimEditorSession *session) {
    if (!session) return NULL;
    return physics_sim_editor_session_object_overlay_at(session, session->selection.retained_object_index);
}

const PhysicsSimEmitterOverlay *physics_sim_editor_session_object_emitter_at(const PhysicsSimEditorSession *session,
                                                                              int index) {
    const PhysicsSimObjectOverlay *overlay = physics_sim_editor_session_object_overlay_at(session, index);
    if (!overlay || !overlay->emitter.active) return NULL;
    return &overlay->emitter;
}

const PhysicsSimEmitterOverlay *physics_sim_editor_session_selected_object_emitter(const PhysicsSimEditorSession *session) {
    if (!session) return NULL;
    return physics_sim_editor_session_object_emitter_at(session, session->selection.retained_object_index);
}

const PhysicsSimRuntimeMeshPreviewInstance *physics_sim_editor_session_runtime_mesh_preview_at(
    const PhysicsSimEditorSession *session,
    int index) {
    if (!session || !session->has_retained_scene || !session->mesh_previews.valid_contract) return NULL;
    if (index < 0 || index >= session->mesh_previews.instance_count) return NULL;
    return &session->mesh_previews.instances[index];
}

const PhysicsSimRuntimeMeshPreviewInstance *physics_sim_editor_session_selected_runtime_mesh_preview(
    const PhysicsSimEditorSession *session) {
    const CoreSceneObjectContract *object = physics_sim_editor_session_selected_object(session);
    if (!session || !object || object->kind != CORE_SCENE_OBJECT_KIND_MESH_ASSET_INSTANCE) return NULL;
    if (!object->object.object_id[0]) return NULL;
    for (int i = 0; i < session->mesh_previews.instance_count; ++i) {
        const PhysicsSimRuntimeMeshPreviewInstance *preview = &session->mesh_previews.instances[i];
        if (strcmp(preview->object_id, object->object.object_id) == 0) {
            return preview;
        }
    }
    return NULL;
}

const PhysicsSimRuntimeMeshOverlay *physics_sim_editor_session_runtime_mesh_overlay_at(
    const PhysicsSimEditorSession *session,
    int index) {
    if (!session || !session->has_physics_overlay) return NULL;
    if (index < 0 || index >= session->physics_overlay.mesh_overlay_count) return NULL;
    if (!session->physics_overlay.mesh_overlays[index].active) return NULL;
    return &session->physics_overlay.mesh_overlays[index];
}

const PhysicsSimRuntimeMeshOverlay *physics_sim_editor_session_selected_runtime_mesh_overlay(
    const PhysicsSimEditorSession *session) {
    const PhysicsSimRuntimeMeshPreviewInstance *preview =
        physics_sim_editor_session_selected_runtime_mesh_preview(session);
    if (!session || !preview) return NULL;
    for (int i = 0; i < session->physics_overlay.mesh_overlay_count; ++i) {
        const PhysicsSimRuntimeMeshOverlay *overlay = &session->physics_overlay.mesh_overlays[i];
        if (!overlay->active) continue;
        if (strcmp(overlay->object_id, preview->object_id) == 0) {
            return overlay;
        }
    }
    return NULL;
}

const PhysicsSimEmitterOverlay *physics_sim_editor_session_selected_runtime_mesh_emitter(
    const PhysicsSimEditorSession *session) {
    const PhysicsSimRuntimeMeshOverlay *overlay =
        physics_sim_editor_session_selected_runtime_mesh_overlay(session);
    if (!overlay || !overlay->emitter.active) return NULL;
    return &overlay->emitter;
}

const PhysicsSimDomainOverlay *physics_sim_editor_session_scene_domain(const PhysicsSimEditorSession *session) {
    if (!session || !session->has_physics_overlay || !session->physics_overlay.scene_domain.active) return NULL;
    return &session->physics_overlay.scene_domain;
}

void physics_sim_editor_session_scene_domain_dimensions(const PhysicsSimEditorSession *session,
                                                        double *out_width,
                                                        double *out_height,
                                                        double *out_depth) {
    const PhysicsSimDomainOverlay *domain = physics_sim_editor_session_scene_domain(session);
    if (out_width) *out_width = 0.0;
    if (out_height) *out_height = 0.0;
    if (out_depth) *out_depth = 0.0;
    if (!domain) return;
    if (out_width) *out_width = fabs(domain->max.x - domain->min.x);
    if (out_height) *out_height = fabs(domain->max.y - domain->min.y);
    if (out_depth) *out_depth = fabs(domain->max.z - domain->min.z);
}

bool physics_sim_editor_session_build_overlay_json(const PhysicsSimEditorSession *session,
                                                   char **out_overlay_json,
                                                   char *out_diagnostics,
                                                   size_t out_diagnostics_size) {
    json_object *root = NULL;
    json_object *overlay_meta = NULL;
    json_object *extensions = NULL;
    json_object *physics_ext = NULL;
    json_object *scene_domain = NULL;
    json_object *object_overlays = NULL;
    json_object *mesh_overlays = NULL;
    const char *serialized = NULL;
    char *out = NULL;
    size_t out_len = 0;
    int i = 0;

    if (out_overlay_json) *out_overlay_json = NULL;
    session_diag(out_diagnostics, out_diagnostics_size, "invalid input");
    if (!session || !out_overlay_json) return false;
    if (!session->has_retained_scene || !session->has_physics_overlay || !session->physics_overlay.active) {
        session_diag(out_diagnostics, out_diagnostics_size, "physics overlay unavailable");
        return false;
    }

    root = json_object_new_object();
    overlay_meta = json_object_new_object();
    extensions = json_object_new_object();
    physics_ext = json_object_new_object();
    scene_domain = json_object_new_object();
    object_overlays = json_object_new_array();
    mesh_overlays = json_object_new_array();
    if (!root || !overlay_meta || !extensions || !physics_ext || !scene_domain || !object_overlays ||
        !mesh_overlays) {
        session_diag(out_diagnostics, out_diagnostics_size, "out of memory");
        if (root) json_object_put(root);
        if (overlay_meta) json_object_put(overlay_meta);
        if (extensions) json_object_put(extensions);
        if (physics_ext) json_object_put(physics_ext);
        if (scene_domain) json_object_put(scene_domain);
        if (object_overlays) json_object_put(object_overlays);
        if (mesh_overlays) json_object_put(mesh_overlays);
        return false;
    }

    json_object_object_add(overlay_meta, "producer", json_object_new_string("physics_sim"));
    json_object_object_add(overlay_meta,
                           "logical_clock",
                           json_object_new_int(session->physics_overlay.logical_clock + 1));
    json_object_object_add(root, "overlay_meta", overlay_meta);

    json_object_object_add(physics_ext,
                           "overlay_variant",
                           json_object_new_string("retained_object_overlay_v1"));
    json_object_object_add(physics_ext,
                           "scene_id",
                           json_object_new_string(session->retained_scene.root.scene_id));
    json_object_object_add(physics_ext,
                           "derived_defaults",
                           json_object_new_boolean(session->physics_overlay.derived_defaults ? 1 : 0));
    json_object_object_add(scene_domain,
                           "active",
                           json_object_new_boolean(session->physics_overlay.scene_domain.active ? 1 : 0));
    json_object_object_add(scene_domain,
                           "shape",
                           json_object_new_string("box"));
    {
        json_object *min_obj = json_object_new_object();
        json_object *max_obj = json_object_new_object();
        if (!min_obj || !max_obj) {
            session_diag(out_diagnostics, out_diagnostics_size, "out of memory");
            if (min_obj) json_object_put(min_obj);
            if (max_obj) json_object_put(max_obj);
            json_object_put(root);
            return false;
        }
        json_object_object_add(min_obj, "x", json_object_new_double(session->physics_overlay.scene_domain.min.x));
        json_object_object_add(min_obj, "y", json_object_new_double(session->physics_overlay.scene_domain.min.y));
        json_object_object_add(min_obj, "z", json_object_new_double(session->physics_overlay.scene_domain.min.z));
        json_object_object_add(max_obj, "x", json_object_new_double(session->physics_overlay.scene_domain.max.x));
        json_object_object_add(max_obj, "y", json_object_new_double(session->physics_overlay.scene_domain.max.y));
        json_object_object_add(max_obj, "z", json_object_new_double(session->physics_overlay.scene_domain.max.z));
        json_object_object_add(scene_domain, "min", min_obj);
        json_object_object_add(scene_domain, "max", max_obj);
    }
    json_object_object_add(scene_domain,
                           "seeded_from_retained_bounds",
                           json_object_new_boolean(session->physics_overlay.scene_domain.seeded_from_retained_bounds ? 1 : 0));
    json_object_object_add(physics_ext, "scene_domain", scene_domain);

    if (session->has_wind_tunnel_config &&
        !session_add_wind_tunnel_json(physics_ext, &session->wind_tunnel)) {
        session_diag(out_diagnostics, out_diagnostics_size, "failed to serialize wind tunnel setup");
        json_object_put(root);
        return false;
    }

    for (i = 0; i < session->physics_overlay.object_overlay_count; ++i) {
        const PhysicsSimObjectOverlay *overlay = &session->physics_overlay.object_overlays[i];
        json_object *overlay_obj = NULL;
        json_object *velocity = NULL;
        json_object *emitter = NULL;
        json_object *direction = NULL;
        if (!overlay->active) continue;

        overlay_obj = json_object_new_object();
        velocity = json_object_new_object();
        if (!overlay_obj || !velocity) {
            session_diag(out_diagnostics, out_diagnostics_size, "out of memory");
            if (overlay_obj) json_object_put(overlay_obj);
            if (velocity) json_object_put(velocity);
            json_object_put(root);
            return false;
        }

        json_object_object_add(overlay_obj, "object_id", json_object_new_string(overlay->object_id));
        json_object_object_add(overlay_obj,
                               "motion_mode",
                               json_object_new_string(physics_sim_editor_session_motion_mode_label(overlay->motion_mode)));
        json_object_object_add(velocity, "x", json_object_new_double(overlay->initial_velocity.x));
        json_object_object_add(velocity, "y", json_object_new_double(overlay->initial_velocity.y));
        json_object_object_add(velocity, "z", json_object_new_double(overlay->initial_velocity.z));
        json_object_object_add(overlay_obj, "initial_velocity", velocity);
        if (overlay->emitter.active) {
            emitter = json_object_new_object();
            direction = json_object_new_object();
            if (!emitter || !direction) {
                session_diag(out_diagnostics, out_diagnostics_size, "out of memory");
                if (emitter) json_object_put(emitter);
                if (direction) json_object_put(direction);
                if (overlay_obj) json_object_put(overlay_obj);
                json_object_put(root);
                return false;
            }
            json_object_object_add(emitter, "active", json_object_new_boolean(1));
            json_object_object_add(emitter,
                                   "type",
                                   json_object_new_string(
                                       physics_sim_editor_session_emitter_type_label(overlay->emitter.type)));
            json_object_object_add(emitter, "radius", json_object_new_double(overlay->emitter.radius));
            json_object_object_add(emitter, "strength", json_object_new_double(overlay->emitter.strength));
            json_object_object_add(
                emitter,
                "mode_3d",
                json_object_new_string(
                    physics_sim_editor_session_emitter_source_mode_3d_label(
                        overlay->emitter.source_mode_3d)));
            json_object_object_add(
                emitter,
                "surface_3d",
                json_object_new_string(
                    physics_sim_editor_session_emitter_surface_3d_label(
                        overlay->emitter.surface_3d)));
            json_object_object_add(
                emitter,
                "obstacle_mode_3d",
                json_object_new_string(
                    physics_sim_editor_session_emitter_obstacle_mode_3d_label(
                        overlay->emitter.obstacle_mode_3d)));
            json_object_object_add(emitter,
                                   "thermal_buoyancy_3d",
                                   json_object_new_double(overlay->emitter.thermal_buoyancy_3d));
            json_object_object_add(direction, "x", json_object_new_double(overlay->emitter.direction.x));
            json_object_object_add(direction, "y", json_object_new_double(overlay->emitter.direction.y));
            json_object_object_add(direction, "z", json_object_new_double(overlay->emitter.direction.z));
            json_object_object_add(emitter, "direction", direction);
            json_object_object_add(overlay_obj, "emitter", emitter);
        }
        json_object_array_add(object_overlays, overlay_obj);
    }

    json_object_object_add(physics_ext, "object_overlays", object_overlays);

    for (i = 0; i < session->physics_overlay.mesh_overlay_count; ++i) {
        const PhysicsSimRuntimeMeshOverlay *overlay = &session->physics_overlay.mesh_overlays[i];
        json_object *overlay_obj = NULL;
        json_object *emitter = NULL;
        json_object *direction = NULL;
        if (!overlay->active) continue;

        overlay_obj = json_object_new_object();
        if (!overlay_obj) {
            session_diag(out_diagnostics, out_diagnostics_size, "out of memory");
            json_object_put(root);
            return false;
        }

        json_object_object_add(overlay_obj, "object_id", json_object_new_string(overlay->object_id));
        json_object_object_add(overlay_obj, "mesh_instance_index", json_object_new_int(overlay->mesh_instance_index));
        json_object_object_add(overlay_obj, "scene_object_index", json_object_new_int(overlay->scene_object_index));
        json_object_object_add(overlay_obj,
                               "role",
                               json_object_new_string(
                                   physics_sim_editor_session_runtime_mesh_role_label(overlay->role)));
        json_object_object_add(overlay_obj,
                               "fluid_behavior",
                               json_object_new_string(session_mesh_role_behavior(overlay->role)));
        json_object_object_add(overlay_obj,
                               "fluid_obstacle",
                               json_object_new_boolean(
                                   overlay->role == PHYSICS_SIM_RUNTIME_MESH_EDITOR_ROLE_SOLID ? 1 : 0));
        if (overlay->emitter.active) {
            emitter = json_object_new_object();
            direction = json_object_new_object();
            if (!emitter || !direction) {
                session_diag(out_diagnostics, out_diagnostics_size, "out of memory");
                if (emitter) json_object_put(emitter);
                if (direction) json_object_put(direction);
                json_object_put(overlay_obj);
                json_object_put(root);
                return false;
            }
            json_object_object_add(emitter, "active", json_object_new_boolean(1));
            json_object_object_add(emitter,
                                   "type",
                                   json_object_new_string(
                                       physics_sim_editor_session_emitter_type_label(overlay->emitter.type)));
            json_object_object_add(emitter, "radius", json_object_new_double(overlay->emitter.radius));
            json_object_object_add(emitter, "strength", json_object_new_double(overlay->emitter.strength));
            json_object_object_add(
                emitter,
                "mode_3d",
                json_object_new_string(
                    physics_sim_editor_session_emitter_source_mode_3d_label(
                        overlay->emitter.source_mode_3d)));
            json_object_object_add(
                emitter,
                "surface_3d",
                json_object_new_string(
                    physics_sim_editor_session_emitter_surface_3d_label(
                        overlay->emitter.surface_3d)));
            json_object_object_add(
                emitter,
                "obstacle_mode_3d",
                json_object_new_string(
                    physics_sim_editor_session_emitter_obstacle_mode_3d_label(
                        overlay->emitter.obstacle_mode_3d)));
            json_object_object_add(emitter,
                                   "thermal_buoyancy_3d",
                                   json_object_new_double(overlay->emitter.thermal_buoyancy_3d));
            json_object_object_add(direction, "x", json_object_new_double(overlay->emitter.direction.x));
            json_object_object_add(direction, "y", json_object_new_double(overlay->emitter.direction.y));
            json_object_object_add(direction, "z", json_object_new_double(overlay->emitter.direction.z));
            json_object_object_add(emitter, "direction", direction);
            json_object_object_add(overlay_obj, "emitter", emitter);
        }
        json_object_array_add(mesh_overlays, overlay_obj);
    }

    json_object_object_add(physics_ext, "mesh_overlays", mesh_overlays);
    json_object_object_add(extensions, "physics_sim", physics_ext);
    json_object_object_add(root, "extensions", extensions);

    serialized = json_object_to_json_string_ext(root,
                                                JSON_C_TO_STRING_PRETTY | JSON_C_TO_STRING_NOSLASHESCAPE);
    if (!serialized) {
        session_diag(out_diagnostics, out_diagnostics_size, "failed to serialize overlay json");
        json_object_put(root);
        return false;
    }

    out_len = strlen(serialized);
    out = (char *)malloc(out_len + 1u);
    if (!out) {
        session_diag(out_diagnostics, out_diagnostics_size, "out of memory");
        json_object_put(root);
        return false;
    }
    memcpy(out, serialized, out_len + 1u);
    *out_overlay_json = out;
    session_diag(out_diagnostics, out_diagnostics_size, "ok");
    json_object_put(root);
    return true;
}

bool physics_sim_editor_session_hydrate_overlay_from_runtime_scene_json(PhysicsSimEditorSession *session,
                                                                        const char *runtime_scene_json,
                                                                        char *out_diagnostics,
                                                                        size_t out_diagnostics_size) {
    json_object *root = NULL;
    json_object *extensions = NULL;
    json_object *physics_ext = NULL;
    json_object *overlay_merge = NULL;
    json_object *producer_clocks = NULL;
    json_object *overlay_meta = NULL;
    json_object *scene_domain = NULL;
    json_object *object_overlays = NULL;
    json_object *mesh_overlays = NULL;
    json_object *logical_clock = NULL;
    json_object *derived_defaults = NULL;
    int i = 0;
    bool found_any = false;
    bool has_derived_defaults = false;

    session_diag(out_diagnostics, out_diagnostics_size, "invalid input");
    if (!session || !runtime_scene_json) return false;
    if (!session->has_retained_scene || !session->has_physics_overlay || !session->physics_overlay.active) {
        session_diag(out_diagnostics, out_diagnostics_size, "physics overlay unavailable");
        return false;
    }

    root = json_tokener_parse(runtime_scene_json);
    if (!root || !json_object_is_type(root, json_type_object)) {
        if (root) json_object_put(root);
        session_diag(out_diagnostics, out_diagnostics_size, "failed to parse runtime scene json");
        return false;
    }

    if (!json_object_object_get_ex(root, "extensions", &extensions) ||
        !json_object_is_type(extensions, json_type_object) ||
        !json_object_object_get_ex(extensions, "physics_sim", &physics_ext) ||
        !json_object_is_type(physics_ext, json_type_object)) {
        session_diag(out_diagnostics, out_diagnostics_size, "ok");
        json_object_put(root);
        return true;
    }

    if (json_object_object_get_ex(root, "overlay_meta", &overlay_meta) &&
        json_object_is_type(overlay_meta, json_type_object) &&
        json_object_object_get_ex(overlay_meta, "logical_clock", &logical_clock) &&
        json_object_is_type(logical_clock, json_type_int)) {
        session->physics_overlay.logical_clock = json_object_get_int(logical_clock);
        session->physics_overlay.scene_domain.logical_clock = session->physics_overlay.logical_clock;
    } else if (json_object_object_get_ex(extensions, "overlay_merge", &overlay_merge) &&
               json_object_is_type(overlay_merge, json_type_object) &&
               json_object_object_get_ex(overlay_merge, "producer_clocks", &producer_clocks) &&
               json_object_is_type(producer_clocks, json_type_object) &&
               json_object_object_get_ex(producer_clocks, "physics_sim", &logical_clock) &&
               json_object_is_type(logical_clock, json_type_int)) {
        session->physics_overlay.logical_clock = json_object_get_int(logical_clock);
        session->physics_overlay.scene_domain.logical_clock = session->physics_overlay.logical_clock;
    }
    if (json_object_object_get_ex(physics_ext, "derived_defaults", &derived_defaults) &&
        json_object_is_type(derived_defaults, json_type_boolean)) {
        session->physics_overlay.derived_defaults = json_object_get_boolean(derived_defaults) ? true : false;
        has_derived_defaults = true;
    }
    if (session_hydrate_wind_tunnel_from_physics_ext(session, physics_ext)) {
        found_any = true;
    }

    if (json_object_object_get_ex(physics_ext, "scene_domain", &scene_domain) &&
        json_object_is_type(scene_domain, json_type_object)) {
        json_object *active = NULL;
        json_object *shape = NULL;
        json_object *seeded = NULL;
        json_object *min_obj = NULL;
        json_object *max_obj = NULL;
        if (json_object_object_get_ex(scene_domain, "active", &active)) {
            session->physics_overlay.scene_domain.active = json_object_get_boolean(active) ? true : false;
        }
        if (json_object_object_get_ex(scene_domain, "shape", &shape) &&
            json_object_is_type(shape, json_type_string)) {
            const char *shape_text = json_object_get_string(shape);
            session->physics_overlay.scene_domain.shape =
                (shape_text && strcmp(shape_text, "box") == 0)
                    ? PHYSICS_SIM_DOMAIN_SHAPE_BOX
                    : PHYSICS_SIM_DOMAIN_SHAPE_BOX;
        }
        if (json_object_object_get_ex(scene_domain, "seeded_from_retained_bounds", &seeded)) {
            session->physics_overlay.scene_domain.seeded_from_retained_bounds =
                json_object_get_boolean(seeded) ? true : false;
        }
        if (json_object_object_get_ex(scene_domain, "min", &min_obj) &&
            json_object_is_type(min_obj, json_type_object)) {
            json_object *vx = NULL;
            json_object *vy = NULL;
            json_object *vz = NULL;
            if (json_object_object_get_ex(min_obj, "x", &vx) &&
                (json_object_is_type(vx, json_type_double) || json_object_is_type(vx, json_type_int))) {
                session->physics_overlay.scene_domain.min.x = json_object_get_double(vx);
            }
            if (json_object_object_get_ex(min_obj, "y", &vy) &&
                (json_object_is_type(vy, json_type_double) || json_object_is_type(vy, json_type_int))) {
                session->physics_overlay.scene_domain.min.y = json_object_get_double(vy);
            }
            if (json_object_object_get_ex(min_obj, "z", &vz) &&
                (json_object_is_type(vz, json_type_double) || json_object_is_type(vz, json_type_int))) {
                session->physics_overlay.scene_domain.min.z = json_object_get_double(vz);
            }
        }
        if (json_object_object_get_ex(scene_domain, "max", &max_obj) &&
            json_object_is_type(max_obj, json_type_object)) {
            json_object *vx = NULL;
            json_object *vy = NULL;
            json_object *vz = NULL;
            if (json_object_object_get_ex(max_obj, "x", &vx) &&
                (json_object_is_type(vx, json_type_double) || json_object_is_type(vx, json_type_int))) {
                session->physics_overlay.scene_domain.max.x = json_object_get_double(vx);
            }
            if (json_object_object_get_ex(max_obj, "y", &vy) &&
                (json_object_is_type(vy, json_type_double) || json_object_is_type(vy, json_type_int))) {
                session->physics_overlay.scene_domain.max.y = json_object_get_double(vy);
            }
            if (json_object_object_get_ex(max_obj, "z", &vz) &&
                (json_object_is_type(vz, json_type_double) || json_object_is_type(vz, json_type_int))) {
                session->physics_overlay.scene_domain.max.z = json_object_get_double(vz);
            }
        }
        if (!has_derived_defaults) {
            session->physics_overlay.scene_domain.derived_defaults = false;
        }
        found_any = true;
    }

    if (json_object_object_get_ex(physics_ext, "object_overlays", &object_overlays) &&
        json_object_is_type(object_overlays, json_type_array)) {
        int overlay_count = json_object_array_length(object_overlays);
        for (i = 0; i < overlay_count; ++i) {
            json_object *overlay_obj = json_object_array_get_idx(object_overlays, i);
            json_object *object_id = NULL;
            json_object *motion_mode = NULL;
            json_object *initial_velocity = NULL;
            json_object *emitter = NULL;
            PhysicsSimObjectOverlay *overlay = NULL;
            const CoreSceneObjectContract *object = NULL;
            if (!overlay_obj || !json_object_is_type(overlay_obj, json_type_object)) continue;
            if (!json_object_object_get_ex(overlay_obj, "object_id", &object_id) ||
                !json_object_is_type(object_id, json_type_string)) {
                continue;
            }
            overlay = physics_sim_editor_session_find_overlay_by_object_id_mut(session,
                                                                               json_object_get_string(object_id));
            if (!overlay) continue;
            object = physics_sim_editor_session_object_at(session, overlay->retained_object_index);
            if (json_object_object_get_ex(overlay_obj, "motion_mode", &motion_mode) &&
                json_object_is_type(motion_mode, json_type_string)) {
                const char *mode_text = json_object_get_string(motion_mode);
                overlay->motion_mode = (mode_text && strcmp(mode_text, "Static") == 0)
                                           ? PHYSICS_SIM_OVERLAY_MOTION_STATIC
                                           : PHYSICS_SIM_OVERLAY_MOTION_DYNAMIC;
            }
            if (json_object_object_get_ex(overlay_obj, "initial_velocity", &initial_velocity) &&
                json_object_is_type(initial_velocity, json_type_object)) {
                json_object *vx = NULL;
                json_object *vy = NULL;
                json_object *vz = NULL;
                if (json_object_object_get_ex(initial_velocity, "x", &vx) &&
                    (json_object_is_type(vx, json_type_double) || json_object_is_type(vx, json_type_int))) {
                    overlay->initial_velocity.x = json_object_get_double(vx);
                }
                if (json_object_object_get_ex(initial_velocity, "y", &vy) &&
                    (json_object_is_type(vy, json_type_double) || json_object_is_type(vy, json_type_int))) {
                    overlay->initial_velocity.y = json_object_get_double(vy);
                }
                if (json_object_object_get_ex(initial_velocity, "z", &vz) &&
                    (json_object_is_type(vz, json_type_double) || json_object_is_type(vz, json_type_int))) {
                    overlay->initial_velocity.z = json_object_get_double(vz);
                }
            }
            overlay->emitter.active = false;
            if (json_object_object_get_ex(overlay_obj, "emitter", &emitter) &&
                json_object_is_type(emitter, json_type_object)) {
                json_object *active = NULL;
                json_object *type = NULL;
                json_object *radius = NULL;
                json_object *strength = NULL;
                json_object *mode_3d = NULL;
                json_object *surface_3d = NULL;
                json_object *obstacle_mode_3d = NULL;
                json_object *thermal_buoyancy_3d = NULL;
                json_object *direction = NULL;
                if (json_object_object_get_ex(emitter, "active", &active) &&
                    json_object_get_boolean(active)) {
                    overlay->emitter.active = true;
                }
                if (json_object_object_get_ex(emitter, "type", &type) &&
                    json_object_is_type(type, json_type_string)) {
                    const char *type_text = json_object_get_string(type);
                    overlay->emitter.type = EMITTER_DENSITY_SOURCE;
                    if (type_text && strcmp(type_text, "Jet") == 0) {
                        overlay->emitter.type = EMITTER_VELOCITY_JET;
                    } else if (type_text && strcmp(type_text, "Sink") == 0) {
                        overlay->emitter.type = EMITTER_SINK;
                    }
                }
                if (json_object_object_get_ex(emitter, "radius", &radius) &&
                    (json_object_is_type(radius, json_type_double) || json_object_is_type(radius, json_type_int))) {
                    overlay->emitter.radius = session_clamp_emitter_radius((float)json_object_get_double(radius));
                }
                if (json_object_object_get_ex(emitter, "strength", &strength) &&
                    (json_object_is_type(strength, json_type_double) || json_object_is_type(strength, json_type_int))) {
                    overlay->emitter.strength = session_clamp_emitter_strength((float)json_object_get_double(strength));
                }
                if (json_object_object_get_ex(emitter, "mode_3d", &mode_3d) &&
                    json_object_is_type(mode_3d, json_type_string)) {
                    const char *mode_text = json_object_get_string(mode_3d);
                    overlay->emitter.source_mode_3d = EMITTER_3D_SOURCE_MODE_LEGACY_COMPAT;
                    if (mode_text && strcmp(mode_text, "VolumeFill") == 0) {
                        overlay->emitter.source_mode_3d = EMITTER_3D_SOURCE_MODE_VOLUME_FILL;
                    } else if (mode_text && strcmp(mode_text, "SurfacePatch") == 0) {
                        overlay->emitter.source_mode_3d = EMITTER_3D_SOURCE_MODE_SURFACE_PATCH;
                    } else if (mode_text && strcmp(mode_text, "SurfaceShell") == 0) {
                        overlay->emitter.source_mode_3d = EMITTER_3D_SOURCE_MODE_SURFACE_SHELL;
                    } else if (mode_text && strcmp(mode_text, "HeatedObstacle") == 0) {
                        overlay->emitter.source_mode_3d = EMITTER_3D_SOURCE_MODE_HEATED_OBSTACLE;
                    }
                }
                if (json_object_object_get_ex(emitter, "surface_3d", &surface_3d) &&
                    json_object_is_type(surface_3d, json_type_string)) {
                    const char *surface_text = json_object_get_string(surface_3d);
                    overlay->emitter.surface_3d = EMITTER_3D_SURFACE_AUTO;
                    if (surface_text && strcmp(surface_text, "Top") == 0) {
                        overlay->emitter.surface_3d = EMITTER_3D_SURFACE_TOP;
                    } else if (surface_text && strcmp(surface_text, "Bottom") == 0) {
                        overlay->emitter.surface_3d = EMITTER_3D_SURFACE_BOTTOM;
                    } else if (surface_text && strcmp(surface_text, "Left") == 0) {
                        overlay->emitter.surface_3d = EMITTER_3D_SURFACE_LEFT;
                    } else if (surface_text && strcmp(surface_text, "Right") == 0) {
                        overlay->emitter.surface_3d = EMITTER_3D_SURFACE_RIGHT;
                    } else if (surface_text && strcmp(surface_text, "Front") == 0) {
                        overlay->emitter.surface_3d = EMITTER_3D_SURFACE_FRONT;
                    } else if (surface_text && strcmp(surface_text, "Back") == 0) {
                        overlay->emitter.surface_3d = EMITTER_3D_SURFACE_BACK;
                    } else if (surface_text && strcmp(surface_text, "AllFaces") == 0) {
                        overlay->emitter.surface_3d = EMITTER_3D_SURFACE_ALL_FACES;
                    }
                }
                if (json_object_object_get_ex(emitter, "obstacle_mode_3d", &obstacle_mode_3d) &&
                    json_object_is_type(obstacle_mode_3d, json_type_string)) {
                    const char *obstacle_text = json_object_get_string(obstacle_mode_3d);
                    overlay->emitter.obstacle_mode_3d = EMITTER_3D_OBSTACLE_MODE_AUTO;
                    if (obstacle_text && strcmp(obstacle_text, "ClearAttached") == 0) {
                        overlay->emitter.obstacle_mode_3d = EMITTER_3D_OBSTACLE_MODE_CLEAR_ATTACHED;
                    } else if (obstacle_text && strcmp(obstacle_text, "RetainAttached") == 0) {
                        overlay->emitter.obstacle_mode_3d = EMITTER_3D_OBSTACLE_MODE_RETAIN_ATTACHED;
                    }
                }
                if (json_object_object_get_ex(emitter, "thermal_buoyancy_3d", &thermal_buoyancy_3d) &&
                    (json_object_is_type(thermal_buoyancy_3d, json_type_double) ||
                     json_object_is_type(thermal_buoyancy_3d, json_type_int))) {
                    overlay->emitter.thermal_buoyancy_3d =
                        session_clamp_emitter_thermal_buoyancy(
                            (float)json_object_get_double(thermal_buoyancy_3d));
                }
                if (json_object_object_get_ex(emitter, "direction", &direction) &&
                    json_object_is_type(direction, json_type_object)) {
                    json_object *vx = NULL;
                    json_object *vy = NULL;
                    json_object *vz = NULL;
                    if (json_object_object_get_ex(direction, "x", &vx) &&
                        (json_object_is_type(vx, json_type_double) || json_object_is_type(vx, json_type_int))) {
                        overlay->emitter.direction.x = json_object_get_double(vx);
                    }
                    if (json_object_object_get_ex(direction, "y", &vy) &&
                        (json_object_is_type(vy, json_type_double) || json_object_is_type(vy, json_type_int))) {
                        overlay->emitter.direction.y = json_object_get_double(vy);
                    }
                    if (json_object_object_get_ex(direction, "z", &vz) &&
                        (json_object_is_type(vz, json_type_double) || json_object_is_type(vz, json_type_int))) {
                        overlay->emitter.direction.z = json_object_get_double(vz);
                    }
                }
                if ((overlay->emitter.direction.x == 0.0 &&
                     overlay->emitter.direction.y == 0.0 &&
                     overlay->emitter.direction.z == 0.0) ||
                    session_is_legacy_sideways_direction(overlay->emitter.direction)) {
                    overlay->emitter.direction = session_default_emitter_direction_for_object(object);
                }
            }
            found_any = true;
        }
    }

    if (json_object_object_get_ex(physics_ext, "mesh_overlays", &mesh_overlays) &&
        json_object_is_type(mesh_overlays, json_type_array)) {
        int overlay_count = json_object_array_length(mesh_overlays);
        for (i = 0; i < overlay_count; ++i) {
            json_object *overlay_obj = json_object_array_get_idx(mesh_overlays, i);
            json_object *object_id = NULL;
            json_object *role = NULL;
            json_object *fluid_behavior = NULL;
            json_object *emitter = NULL;
            PhysicsSimRuntimeMeshOverlay *overlay = NULL;
            if (!overlay_obj || !json_object_is_type(overlay_obj, json_type_object)) continue;
            if (!json_object_object_get_ex(overlay_obj, "object_id", &object_id) ||
                !json_object_is_type(object_id, json_type_string)) {
                continue;
            }
            overlay = physics_sim_editor_session_find_mesh_overlay_by_object_id_mut(
                session,
                json_object_get_string(object_id));
            if (!overlay) continue;

            if (json_object_object_get_ex(overlay_obj, "fluid_behavior", &fluid_behavior) &&
                json_object_is_type(fluid_behavior, json_type_string)) {
                overlay->role = session_mesh_role_from_overlay_text(json_object_get_string(fluid_behavior));
            } else if (json_object_object_get_ex(overlay_obj, "role", &role) &&
                       json_object_is_type(role, json_type_string)) {
                overlay->role = session_mesh_role_from_overlay_text(json_object_get_string(role));
            }

            overlay->emitter.active = false;
            if (json_object_object_get_ex(overlay_obj, "emitter", &emitter) &&
                json_object_is_type(emitter, json_type_object)) {
                json_object *active = NULL;
                json_object *type = NULL;
                json_object *radius = NULL;
                json_object *strength = NULL;
                json_object *mode_3d = NULL;
                json_object *surface_3d = NULL;
                json_object *obstacle_mode_3d = NULL;
                json_object *thermal_buoyancy_3d = NULL;
                json_object *direction = NULL;
                if (json_object_object_get_ex(emitter, "active", &active) &&
                    json_object_get_boolean(active)) {
                    overlay->emitter.active = true;
                }
                if (json_object_object_get_ex(emitter, "type", &type) &&
                    json_object_is_type(type, json_type_string)) {
                    const char *type_text = json_object_get_string(type);
                    overlay->emitter.type = EMITTER_DENSITY_SOURCE;
                    if (type_text && strcmp(type_text, "Jet") == 0) {
                        overlay->emitter.type = EMITTER_VELOCITY_JET;
                    } else if (type_text && strcmp(type_text, "Sink") == 0) {
                        overlay->emitter.type = EMITTER_SINK;
                    }
                }
                if (json_object_object_get_ex(emitter, "radius", &radius) &&
                    (json_object_is_type(radius, json_type_double) || json_object_is_type(radius, json_type_int))) {
                    overlay->emitter.radius = session_clamp_emitter_radius((float)json_object_get_double(radius));
                }
                if (json_object_object_get_ex(emitter, "strength", &strength) &&
                    (json_object_is_type(strength, json_type_double) || json_object_is_type(strength, json_type_int))) {
                    overlay->emitter.strength = session_clamp_emitter_strength((float)json_object_get_double(strength));
                }
                if (json_object_object_get_ex(emitter, "mode_3d", &mode_3d) &&
                    json_object_is_type(mode_3d, json_type_string)) {
                    const char *mode_text = json_object_get_string(mode_3d);
                    overlay->emitter.source_mode_3d = EMITTER_3D_SOURCE_MODE_LEGACY_COMPAT;
                    if (mode_text && strcmp(mode_text, "VolumeFill") == 0) {
                        overlay->emitter.source_mode_3d = EMITTER_3D_SOURCE_MODE_VOLUME_FILL;
                    } else if (mode_text && strcmp(mode_text, "SurfacePatch") == 0) {
                        overlay->emitter.source_mode_3d = EMITTER_3D_SOURCE_MODE_SURFACE_PATCH;
                    } else if (mode_text && strcmp(mode_text, "SurfaceShell") == 0) {
                        overlay->emitter.source_mode_3d = EMITTER_3D_SOURCE_MODE_SURFACE_SHELL;
                    } else if (mode_text && strcmp(mode_text, "HeatedObstacle") == 0) {
                        overlay->emitter.source_mode_3d = EMITTER_3D_SOURCE_MODE_HEATED_OBSTACLE;
                    }
                }
                if (json_object_object_get_ex(emitter, "surface_3d", &surface_3d) &&
                    json_object_is_type(surface_3d, json_type_string)) {
                    const char *surface_text = json_object_get_string(surface_3d);
                    overlay->emitter.surface_3d = EMITTER_3D_SURFACE_AUTO;
                    if (surface_text && strcmp(surface_text, "Top") == 0) {
                        overlay->emitter.surface_3d = EMITTER_3D_SURFACE_TOP;
                    } else if (surface_text && strcmp(surface_text, "Bottom") == 0) {
                        overlay->emitter.surface_3d = EMITTER_3D_SURFACE_BOTTOM;
                    } else if (surface_text && strcmp(surface_text, "Left") == 0) {
                        overlay->emitter.surface_3d = EMITTER_3D_SURFACE_LEFT;
                    } else if (surface_text && strcmp(surface_text, "Right") == 0) {
                        overlay->emitter.surface_3d = EMITTER_3D_SURFACE_RIGHT;
                    } else if (surface_text && strcmp(surface_text, "Front") == 0) {
                        overlay->emitter.surface_3d = EMITTER_3D_SURFACE_FRONT;
                    } else if (surface_text && strcmp(surface_text, "Back") == 0) {
                        overlay->emitter.surface_3d = EMITTER_3D_SURFACE_BACK;
                    } else if (surface_text && strcmp(surface_text, "AllFaces") == 0) {
                        overlay->emitter.surface_3d = EMITTER_3D_SURFACE_ALL_FACES;
                    }
                }
                if (json_object_object_get_ex(emitter, "obstacle_mode_3d", &obstacle_mode_3d) &&
                    json_object_is_type(obstacle_mode_3d, json_type_string)) {
                    const char *obstacle_text = json_object_get_string(obstacle_mode_3d);
                    overlay->emitter.obstacle_mode_3d = EMITTER_3D_OBSTACLE_MODE_AUTO;
                    if (obstacle_text && strcmp(obstacle_text, "ClearAttached") == 0) {
                        overlay->emitter.obstacle_mode_3d = EMITTER_3D_OBSTACLE_MODE_CLEAR_ATTACHED;
                    } else if (obstacle_text && strcmp(obstacle_text, "RetainAttached") == 0) {
                        overlay->emitter.obstacle_mode_3d = EMITTER_3D_OBSTACLE_MODE_RETAIN_ATTACHED;
                    }
                }
                if (json_object_object_get_ex(emitter, "thermal_buoyancy_3d", &thermal_buoyancy_3d) &&
                    (json_object_is_type(thermal_buoyancy_3d, json_type_double) ||
                     json_object_is_type(thermal_buoyancy_3d, json_type_int))) {
                    overlay->emitter.thermal_buoyancy_3d =
                        session_clamp_emitter_thermal_buoyancy(
                            (float)json_object_get_double(thermal_buoyancy_3d));
                }
                if (json_object_object_get_ex(emitter, "direction", &direction) &&
                    json_object_is_type(direction, json_type_object)) {
                    json_object *vx = NULL;
                    json_object *vy = NULL;
                    json_object *vz = NULL;
                    if (json_object_object_get_ex(direction, "x", &vx) &&
                        (json_object_is_type(vx, json_type_double) || json_object_is_type(vx, json_type_int))) {
                        overlay->emitter.direction.x = json_object_get_double(vx);
                    }
                    if (json_object_object_get_ex(direction, "y", &vy) &&
                        (json_object_is_type(vy, json_type_double) || json_object_is_type(vy, json_type_int))) {
                        overlay->emitter.direction.y = json_object_get_double(vy);
                    }
                    if (json_object_object_get_ex(direction, "z", &vz) &&
                        (json_object_is_type(vz, json_type_double) || json_object_is_type(vz, json_type_int))) {
                        overlay->emitter.direction.z = json_object_get_double(vz);
                    }
                }
            }
            session_mesh_overlay_apply_role_defaults(overlay);
            found_any = true;
        }
    }

    if (found_any && !has_derived_defaults) {
        session->physics_overlay.derived_defaults = false;
    }

    session_diag(out_diagnostics, out_diagnostics_size, "ok");
    json_object_put(root);
    return true;
}

bool physics_sim_editor_session_mark_overlay_applied(PhysicsSimEditorSession *session) {
    if (!session || !session->has_physics_overlay || !session->physics_overlay.active) return false;
    session->physics_overlay.logical_clock += 1;
    session->physics_overlay.derived_defaults = false;
    session->physics_overlay.scene_domain.logical_clock = session->physics_overlay.logical_clock;
    session->physics_overlay.scene_domain.derived_defaults = false;
    if (session->has_wind_tunnel_config &&
        wind_tunnel_3d_config_validate(&session->wind_tunnel)) {
        session->wind_tunnel_authored = true;
    }
    return true;
}
