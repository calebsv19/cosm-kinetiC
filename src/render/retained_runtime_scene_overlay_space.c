#include "render/retained_runtime_scene_overlay_space.h"

#include <math.h>

#include "app/scene_state.h"
#include "app/sim_runtime_3d_anchor.h"
#include "app/sim_runtime_3d_space.h"
#include "render/retained_runtime_scene_overlay_geom.h"

static bool compute_retained_bounds(const PhysicsSimRetainedRuntimeScene *retained,
                                    CoreObjectVec3 *out_min,
                                    CoreObjectVec3 *out_max) {
    bool have = false;
    if (!retained || !out_min || !out_max) return false;
    if (retained->has_line_drawing_scene3d && retained->bounds.enabled) {
        *out_min = retained->bounds.min;
        *out_max = retained->bounds.max;
        return true;
    }
    for (int i = 0; i < retained->retained_object_count; ++i) {
        const CoreSceneObjectContract *object = &retained->objects[i];
        CoreObjectVec3 corners[8];
        int corner_count = 0;
        if (object->has_plane_primitive) {
            retained_runtime_overlay_fill_plane_corners(&object->plane_primitive, corners);
            corner_count = 4;
        } else if (object->has_rect_prism_primitive) {
            retained_runtime_overlay_fill_prism_corners(&object->rect_prism_primitive, corners);
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

bool retained_runtime_overlay_compute_visual_bounds(const SceneState *scene,
                                                    CoreObjectVec3 *out_min,
                                                    CoreObjectVec3 *out_max) {
    if (!scene || !out_min || !out_max) return false;
    if (scene->runtime_visual.scene_domain.enabled) {
        *out_min = scene->runtime_visual.scene_domain.min;
        *out_max = scene->runtime_visual.scene_domain.max;
        return true;
    }
    return compute_retained_bounds(&scene->runtime_visual.retained_scene, out_min, out_max);
}

double retained_runtime_overlay_slice_world_z_for_index(const SceneState *scene,
                                                        CoreObjectVec3 visual_min,
                                                        CoreObjectVec3 visual_max,
                                                        int slice_z) {
    SimRuntimeBackendReport report = {0};
    if (scene_backend_report(scene, &report) &&
        report.compatibility_view_2d_derived &&
        report.domain_d > 0 &&
        report.world_bounds_valid &&
        report.voxel_size > 0.0f) {
        return sim_runtime_3d_space_slice_world_z(report.world_min_z,
                                                  report.voxel_size,
                                                  report.domain_d,
                                                  slice_z);
    }
    (void)slice_z;
    return visual_min.z + (visual_max.z - visual_min.z) * 0.5;
}

double retained_runtime_overlay_slice_z(const SceneState *scene,
                                        CoreObjectVec3 visual_min,
                                        CoreObjectVec3 visual_max) {
    SimRuntimeBackendReport report = {0};
    if (scene_backend_report(scene, &report) &&
        report.compatibility_view_2d_derived &&
        report.domain_d > 0 &&
        report.world_bounds_valid &&
        report.voxel_size > 0.0f) {
        return sim_runtime_3d_space_slice_world_z(report.world_min_z,
                                                  report.voxel_size,
                                                  report.domain_d,
                                                  report.compatibility_slice_z);
    }
    return visual_min.z + (visual_max.z - visual_min.z) * 0.5;
}

double retained_runtime_overlay_slice_tolerance(const SceneState *scene) {
    SimRuntimeBackendReport report = {0};
    if (scene_backend_report(scene, &report) &&
        report.compatibility_view_2d_derived &&
        report.voxel_size > 0.0f) {
        return (double)report.voxel_size * 0.5;
    }
    return 0.05;
}

static void bounds_from_points(const CoreObjectVec3 *points,
                               int point_count,
                               double *out_min_z,
                               double *out_max_z) {
    double min_z = 0.0;
    double max_z = 0.0;
    if (!points || point_count <= 0 || !out_min_z || !out_max_z) return;
    min_z = max_z = points[0].z;
    for (int i = 1; i < point_count; ++i) {
        if (points[i].z < min_z) min_z = points[i].z;
        if (points[i].z > max_z) max_z = points[i].z;
    }
    *out_min_z = min_z;
    *out_max_z = max_z;
}

bool retained_runtime_overlay_object_slice_intersects(const SceneState *scene,
                                                      const CoreSceneObjectContract *object,
                                                      double slice_z) {
    CoreObjectVec3 corners[8];
    double min_z = 0.0;
    double max_z = 0.0;
    double tolerance = retained_runtime_overlay_slice_tolerance(scene);
    if (!object) return false;
    if (object->has_plane_primitive) {
        retained_runtime_overlay_fill_plane_corners(&object->plane_primitive, corners);
        bounds_from_points(corners, 4, &min_z, &max_z);
        return slice_z >= (min_z - tolerance) && slice_z <= (max_z + tolerance);
    }
    if (object->has_rect_prism_primitive) {
        retained_runtime_overlay_fill_prism_corners(&object->rect_prism_primitive, corners);
        bounds_from_points(corners, 8, &min_z, &max_z);
        return slice_z >= (min_z - tolerance) && slice_z <= (max_z + tolerance);
    }
    return fabs(object->object.transform.position.z - slice_z) <= tolerance;
}

bool retained_runtime_overlay_emitter_actual_and_slice_points(const SceneState *scene,
                                                              const FluidEmitter *emitter,
                                                              CoreObjectVec3 *out_actual,
                                                              CoreObjectVec3 *out_slice) {
    CoreObjectVec3 min = {0};
    CoreObjectVec3 max = {0};
    CoreObjectVec3 actual = {0};
    CoreObjectVec3 slice = {0};
    if (!scene || !scene->preset || !emitter || !out_actual || !out_slice) return false;
    if (!retained_runtime_overlay_compute_visual_bounds(scene, &min, &max)) return false;
    if (!sim_runtime_3d_anchor_resolve_preset_emitter_world_anchor(scene, emitter, &min, &max, &actual)) {
        return false;
    }
    slice = actual;
    slice.z = retained_runtime_overlay_slice_z(scene, min, max);
    *out_actual = actual;
    *out_slice = slice;
    return true;
}
