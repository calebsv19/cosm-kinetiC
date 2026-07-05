#include "render/retained_runtime_scene_overlay_geom.h"

static CoreObjectVec3 vec3_add_scaled(CoreObjectVec3 base,
                                      CoreObjectVec3 axis,
                                      double scale) {
    CoreObjectVec3 result = base;
    result.x += axis.x * scale;
    result.y += axis.y * scale;
    result.z += axis.z * scale;
    return result;
}

void retained_runtime_overlay_fill_aabb_corners(CoreObjectVec3 min,
                                                CoreObjectVec3 max,
                                                CoreObjectVec3 out_corners[8]) {
    if (!out_corners) return;
    out_corners[0] = (CoreObjectVec3){min.x, min.y, min.z};
    out_corners[1] = (CoreObjectVec3){max.x, min.y, min.z};
    out_corners[2] = (CoreObjectVec3){max.x, max.y, min.z};
    out_corners[3] = (CoreObjectVec3){min.x, max.y, min.z};
    out_corners[4] = (CoreObjectVec3){min.x, min.y, max.z};
    out_corners[5] = (CoreObjectVec3){max.x, min.y, max.z};
    out_corners[6] = (CoreObjectVec3){max.x, max.y, max.z};
    out_corners[7] = (CoreObjectVec3){min.x, max.y, max.z};
}

void retained_runtime_overlay_fill_plane_corners(const CoreScenePlanePrimitive *plane,
                                                 CoreObjectVec3 out_corners[4]) {
    CoreObjectVec3 origin = {0};
    CoreObjectVec3 u_plus = {0};
    CoreObjectVec3 u_minus = {0};
    double half_width = 0.0;
    double half_height = 0.0;
    if (!plane || !out_corners) return;
    half_width = plane->width * 0.5;
    half_height = plane->height * 0.5;
    origin = plane->frame.origin;
    u_plus = vec3_add_scaled(origin, plane->frame.axis_u, half_width);
    u_minus = vec3_add_scaled(origin, plane->frame.axis_u, -half_width);
    out_corners[0] = vec3_add_scaled(u_minus, plane->frame.axis_v, -half_height);
    out_corners[1] = vec3_add_scaled(u_plus, plane->frame.axis_v, -half_height);
    out_corners[2] = vec3_add_scaled(u_plus, plane->frame.axis_v, half_height);
    out_corners[3] = vec3_add_scaled(u_minus, plane->frame.axis_v, half_height);
}

void retained_runtime_overlay_fill_prism_corners(const CoreSceneRectPrismPrimitive *prism,
                                                 CoreObjectVec3 out_corners[8]) {
    CoreObjectVec3 base = {0};
    double half_width = 0.0;
    double half_height = 0.0;
    double half_depth = 0.0;
    if (!prism || !out_corners) return;
    base = prism->frame.origin;
    half_width = prism->width * 0.5;
    half_height = prism->height * 0.5;
    half_depth = prism->depth * 0.5;

    out_corners[0] = vec3_add_scaled(vec3_add_scaled(vec3_add_scaled(base, prism->frame.axis_u, -half_width),
                                                     prism->frame.axis_v, -half_height),
                                     prism->frame.normal, -half_depth);
    out_corners[1] = vec3_add_scaled(vec3_add_scaled(vec3_add_scaled(base, prism->frame.axis_u, half_width),
                                                     prism->frame.axis_v, -half_height),
                                     prism->frame.normal, -half_depth);
    out_corners[2] = vec3_add_scaled(vec3_add_scaled(vec3_add_scaled(base, prism->frame.axis_u, half_width),
                                                     prism->frame.axis_v, half_height),
                                     prism->frame.normal, -half_depth);
    out_corners[3] = vec3_add_scaled(vec3_add_scaled(vec3_add_scaled(base, prism->frame.axis_u, -half_width),
                                                     prism->frame.axis_v, half_height),
                                     prism->frame.normal, -half_depth);
    out_corners[4] = vec3_add_scaled(vec3_add_scaled(vec3_add_scaled(base, prism->frame.axis_u, -half_width),
                                                     prism->frame.axis_v, -half_height),
                                     prism->frame.normal, half_depth);
    out_corners[5] = vec3_add_scaled(vec3_add_scaled(vec3_add_scaled(base, prism->frame.axis_u, half_width),
                                                     prism->frame.axis_v, -half_height),
                                     prism->frame.normal, half_depth);
    out_corners[6] = vec3_add_scaled(vec3_add_scaled(vec3_add_scaled(base, prism->frame.axis_u, half_width),
                                                     prism->frame.axis_v, half_height),
                                     prism->frame.normal, half_depth);
    out_corners[7] = vec3_add_scaled(vec3_add_scaled(vec3_add_scaled(base, prism->frame.axis_u, -half_width),
                                                     prism->frame.axis_v, half_height),
                                     prism->frame.normal, half_depth);
}
