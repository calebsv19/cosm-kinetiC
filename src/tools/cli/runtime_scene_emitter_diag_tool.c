#include "app/app_config.h"
#include "app/scene_state.h"
#include "app/sim_runtime_3d_anchor.h"
#include "app/sim_runtime_3d_space.h"
#include "app/sim_runtime_backend.h"
#include "app/sim_runtime_backend_3d_emitter_shapes.h"
#include "app/sim_runtime_emitter.h"
#include "import/runtime_scene_bridge.h"
#include "render/retained_runtime_scene_overlay_geom.h"

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct DiagWorldBounds {
    bool valid;
    double min_x;
    double max_x;
    double min_y;
    double max_y;
    double min_z;
    double max_z;
} DiagWorldBounds;

typedef struct DiagDensityStats {
    DiagWorldBounds bounds;
    size_t active_cells;
    double total_density;
    double centroid_x;
    double centroid_y;
    double centroid_z;
} DiagDensityStats;

SimRuntimeBackend *sim_runtime_backend_2d_create(const AppConfig *cfg,
                                                 const FluidScenePreset *preset,
                                                 const SimModeRoute *mode_route,
                                                 const PhysicsSimRuntimeVisualBootstrap *runtime_visual) {
    (void)cfg;
    (void)preset;
    (void)mode_route;
    (void)runtime_visual;
    return NULL;
}

static void usage(const char *argv0) {
    fprintf(stderr, "usage: %s <runtime_scene.json> [emitter_index]\n", argv0);
}

static double axis_span(double world_min, double world_max) {
    double span = world_max - world_min;
    return span > 0.0 ? span : 0.0;
}

static double authored_half_extent_to_world(double value, double world_min, double world_max) {
    double span = axis_span(world_min, world_max);
    if (!(value > 0.0)) return 0.0;
    if (value <= 1.0) return value * span;
    return value;
}

static double cell_world_min(double world_min, double voxel_size, int index) {
    return world_min + (double)index * voxel_size;
}

static double cell_world_max(double world_min, double voxel_size, int index) {
    return world_min + (double)(index + 1) * voxel_size;
}

static double cell_world_center(double world_min, double voxel_size, int index) {
    return world_min + ((double)index + 0.5) * voxel_size;
}

static void diag_bounds_include_cell(DiagWorldBounds *bounds,
                                     const SceneDebugVolumeView3D *view,
                                     int x,
                                     int y,
                                     int z) {
    double min_x = 0.0;
    double max_x = 0.0;
    double min_y = 0.0;
    double max_y = 0.0;
    double min_z = 0.0;
    double max_z = 0.0;
    if (!bounds || !view) return;
    min_x = cell_world_min(view->world_min_x, view->voxel_size, x);
    max_x = cell_world_max(view->world_min_x, view->voxel_size, x);
    min_y = cell_world_min(view->world_min_y, view->voxel_size, y);
    max_y = cell_world_max(view->world_min_y, view->voxel_size, y);
    min_z = cell_world_min(view->world_min_z, view->voxel_size, z);
    max_z = cell_world_max(view->world_min_z, view->voxel_size, z);
    if (!bounds->valid) {
        bounds->valid = true;
        bounds->min_x = min_x;
        bounds->max_x = max_x;
        bounds->min_y = min_y;
        bounds->max_y = max_y;
        bounds->min_z = min_z;
        bounds->max_z = max_z;
        return;
    }
    if (min_x < bounds->min_x) bounds->min_x = min_x;
    if (max_x > bounds->max_x) bounds->max_x = max_x;
    if (min_y < bounds->min_y) bounds->min_y = min_y;
    if (max_y > bounds->max_y) bounds->max_y = max_y;
    if (min_z < bounds->min_z) bounds->min_z = min_z;
    if (max_z > bounds->max_z) bounds->max_z = max_z;
}

static bool collect_solid_bounds(const SceneDebugVolumeView3D *view,
                                 DiagWorldBounds *out_bounds,
                                 size_t *out_active_cells) {
    size_t active_cells = 0;
    DiagWorldBounds bounds = {0};
    if (!view || !out_bounds || !out_active_cells || !view->solid_mask) return false;
    for (int z = 0; z < view->depth; ++z) {
        for (int y = 0; y < view->height; ++y) {
            for (int x = 0; x < view->width; ++x) {
                size_t idx = (size_t)z * (size_t)view->width * (size_t)view->height +
                             (size_t)y * (size_t)view->width +
                             (size_t)x;
                if (!view->solid_mask[idx]) continue;
                diag_bounds_include_cell(&bounds, view, x, y, z);
                active_cells++;
            }
        }
    }
    *out_bounds = bounds;
    *out_active_cells = active_cells;
    return bounds.valid;
}

static bool collect_density_stats(const SceneDebugVolumeView3D *view,
                                  DiagDensityStats *out_stats) {
    DiagDensityStats stats = {0};
    if (!view || !out_stats || !view->density) return false;
    for (int z = 0; z < view->depth; ++z) {
        for (int y = 0; y < view->height; ++y) {
            for (int x = 0; x < view->width; ++x) {
                size_t idx = (size_t)z * (size_t)view->width * (size_t)view->height +
                             (size_t)y * (size_t)view->width +
                             (size_t)x;
                double density = (double)view->density[idx];
                if (!(density > 0.0)) continue;
                diag_bounds_include_cell(&stats.bounds, view, x, y, z);
                stats.active_cells++;
                stats.total_density += density;
                stats.centroid_x += cell_world_center(view->world_min_x, view->voxel_size, x) * density;
                stats.centroid_y += cell_world_center(view->world_min_y, view->voxel_size, y) * density;
                stats.centroid_z += cell_world_center(view->world_min_z, view->voxel_size, z) * density;
            }
        }
    }
    if (!(stats.total_density > 0.0)) return false;
    stats.centroid_x /= stats.total_density;
    stats.centroid_y /= stats.total_density;
    stats.centroid_z /= stats.total_density;
    *out_stats = stats;
    return true;
}

static void include_world_point(DiagWorldBounds *bounds, CoreObjectVec3 point) {
    if (!bounds) return;
    if (!bounds->valid) {
        bounds->valid = true;
        bounds->min_x = bounds->max_x = point.x;
        bounds->min_y = bounds->max_y = point.y;
        bounds->min_z = bounds->max_z = point.z;
        return;
    }
    if (point.x < bounds->min_x) bounds->min_x = point.x;
    if (point.x > bounds->max_x) bounds->max_x = point.x;
    if (point.y < bounds->min_y) bounds->min_y = point.y;
    if (point.y > bounds->max_y) bounds->max_y = point.y;
    if (point.z < bounds->min_z) bounds->min_z = point.z;
    if (point.z > bounds->max_z) bounds->max_z = point.z;
}

static bool retained_prism_bounds(const CoreSceneObjectContract *object, DiagWorldBounds *out_bounds) {
    CoreObjectVec3 corners[8];
    DiagWorldBounds bounds = {0};
    if (!object || !out_bounds || !object->has_rect_prism_primitive) return false;
    retained_runtime_overlay_fill_prism_corners(&object->rect_prism_primitive, corners);
    for (int i = 0; i < 8; ++i) {
        include_world_point(&bounds, corners[i]);
    }
    *out_bounds = bounds;
    return bounds.valid;
}

static void print_bounds(const char *label, const DiagWorldBounds *bounds) {
    if (!label || !bounds || !bounds->valid) {
        fprintf(stdout, "%s: <none>\n", label ? label : "bounds");
        return;
    }
    fprintf(stdout,
            "%s: x=[%.6f, %.6f] y=[%.6f, %.6f] z=[%.6f, %.6f]\n",
            label,
            bounds->min_x,
            bounds->max_x,
            bounds->min_y,
            bounds->max_y,
            bounds->min_z,
            bounds->max_z);
}

static void print_object_world_footprint(const SimRuntime3DDomainDesc *desc,
                                         const PresetObject *object) {
    CoreObjectVec3 center = {0};
    double half_x = 0.0;
    double half_y = 0.0;
    double half_z = 0.0;
    DiagWorldBounds bounds = {0};
    if (!desc || !object) return;
    center.x = sim_runtime_3d_space_resolve_world_axis(object->position_x,
                                                       desc->world_min_x,
                                                       desc->world_max_x);
    center.y = sim_runtime_3d_space_resolve_world_axis(object->position_y,
                                                       desc->world_min_y,
                                                       desc->world_max_y);
    if (object->position_z < desc->world_min_z) {
        center.z = desc->world_min_z;
    } else if (object->position_z > desc->world_max_z) {
        center.z = desc->world_max_z;
    } else {
        center.z = object->position_z;
    }
    half_x = authored_half_extent_to_world(object->size_x, desc->world_min_x, desc->world_max_x);
    half_y = authored_half_extent_to_world(object->size_y, desc->world_min_y, desc->world_max_y);
    half_z = authored_half_extent_to_world(object->size_z, desc->world_min_z, desc->world_max_z);
    bounds.valid = true;
    bounds.min_x = center.x - half_x;
    bounds.max_x = center.x + half_x;
    bounds.min_y = center.y - half_y;
    bounds.max_y = center.y + half_y;
    bounds.min_z = center.z - half_z;
    bounds.max_z = center.z + half_z;
    fprintf(stdout,
            "projected preset object center: (%.6f, %.6f, %.6f)\n",
            center.x,
            center.y,
            center.z);
    fprintf(stdout,
            "projected preset half-extents: (%.6f, %.6f, %.6f)\n",
            half_x,
            half_y,
            half_z);
    print_bounds("projected preset world bounds", &bounds);
}

int main(int argc, char **argv) {
    const char *runtime_scene_path = NULL;
    int emitter_index = 0;
    RuntimeSceneBridgePreflight summary = {0};
    char bootstrap_diagnostics[256];
    AppConfig cfg = app_config_default();
    FluidScenePreset preset = {0};
    FluidScenePreset selected_preset = {0};
    const FluidScenePreset *base = scene_presets_get_default();
    PhysicsSimRuntimeVisualBootstrap visual = {0};
    SimModeRoute route = {
        .backend_lane = SIM_BACKEND_CONTROLLED_3D,
        .requested_space_mode = SPACE_MODE_3D,
        .projection_space_mode = SPACE_MODE_2D,
    };
    SceneState scene = {0};
    SimRuntimeBackend *backend = NULL;
    SceneDebugVolumeView3D view = {0};
    SimRuntimeBackendReport report = {0};
    SimRuntime3DDomainDesc desc = {0};
    SimRuntimeEmitterResolved resolved = {0};
    SimRuntimeEmitterPlacement3D placement = {0};
    SimRuntimeEmitterOrientedBox3D object_box = {0};
    CoreObjectVec3 anchor_world = {0};
    DiagWorldBounds solid_bounds = {0};
    DiagWorldBounds retained_bounds = {0};
    DiagDensityStats density_stats = {0};
    size_t solid_cells = 0;
    const PresetObject *attached_object = NULL;
    const CoreSceneObjectContract *retained_object = NULL;
    bool have_object_box = false;
    bool ok = false;

    if (argc < 2 || argc > 3) {
        usage(argv[0]);
        return 1;
    }
    runtime_scene_path = argv[1];
    if (argc == 3) {
        emitter_index = atoi(argv[2]);
    }

    preset = base ? *base : (FluidScenePreset){0};
    ok = runtime_scene_bridge_apply_file(runtime_scene_path, &cfg, &preset, &summary);
    if (!ok) {
        fprintf(stderr, "runtime_scene_emitter_diag_tool: bridge apply failed: %s\n", summary.diagnostics);
        return 1;
    }
    ok = runtime_scene_bridge_load_visual_bootstrap_file(runtime_scene_path,
                                                         &visual,
                                                         bootstrap_diagnostics,
                                                         sizeof(bootstrap_diagnostics));
    if (!ok) {
        fprintf(stderr,
                "runtime_scene_emitter_diag_tool: visual bootstrap failed: %s\n",
                bootstrap_diagnostics);
        return 1;
    }
    if (cfg.space_mode != SPACE_MODE_3D || preset.dimension_mode != SCENE_DIMENSION_MODE_3D) {
        fprintf(stderr,
                "runtime_scene_emitter_diag_tool: scene is not projected as 3D (space_mode=%d dim=%d)\n",
                cfg.space_mode,
                preset.dimension_mode);
        return 1;
    }
    if (preset.emitter_count == 0) {
        fprintf(stderr, "runtime_scene_emitter_diag_tool: no emitters projected from runtime scene\n");
        return 1;
    }
    if (emitter_index < 0 || emitter_index >= (int)preset.emitter_count) {
        fprintf(stderr,
                "runtime_scene_emitter_diag_tool: emitter_index=%d out of range [0,%zu)\n",
                emitter_index,
                preset.emitter_count);
        return 1;
    }

    selected_preset = preset;
    selected_preset.emitter_count = 1;
    selected_preset.emitters[0] = preset.emitters[emitter_index];

    backend = sim_runtime_backend_create(&cfg, &selected_preset, &route, &visual);
    if (!backend) {
        fprintf(stderr, "runtime_scene_emitter_diag_tool: backend create failed\n");
        return 1;
    }

    scene.backend = backend;
    scene.preset = &selected_preset;
    scene.config = &cfg;
    scene.emitters_enabled = true;
    scene.runtime_visual = visual;
    scene.import_shape_count = selected_preset.import_shape_count;
    for (size_t i = 0; i < scene.import_shape_count && i < MAX_IMPORTED_SHAPES; ++i) {
        scene.import_shapes[i] = selected_preset.import_shapes[i];
    }

    if (!sim_runtime_emitter_resolve(&selected_preset, 0, &resolved)) {
        fprintf(stderr, "runtime_scene_emitter_diag_tool: failed to resolve emitter\n");
        sim_runtime_backend_destroy(backend);
        return 1;
    }
    if (!sim_runtime_backend_get_report(backend, &report)) {
        fprintf(stderr, "runtime_scene_emitter_diag_tool: failed to get backend report\n");
        sim_runtime_backend_destroy(backend);
        return 1;
    }

    fprintf(stdout, "scene_id: %s\n", summary.scene_id);
    fprintf(stdout, "runtime_scene_path: %s\n", runtime_scene_path);
    fprintf(stdout, "selected_emitter_index: %d (of %zu projected emitters)\n", emitter_index, preset.emitter_count);
    fprintf(stdout,
            "domain: %dx%dx%d voxel=%.6f world=[(%.6f, %.6f, %.6f) -> (%.6f, %.6f, %.6f)]\n",
            report.domain_w,
            report.domain_h,
            report.domain_d,
            report.voxel_size,
            report.world_min_x,
            report.world_min_y,
            report.world_min_z,
            report.world_max_x,
            report.world_max_y,
            report.world_max_z);
    if (report.scene_up_valid) {
        fprintf(stdout,
                "scene up: source=%s dir=(%.6f, %.6f, %.6f)\n",
                physics_sim_runtime_scene_up_source_label(report.scene_up_source),
                report.scene_up_x,
                report.scene_up_y,
                report.scene_up_z);
    }
    if (report.debug_volume_view_3d_available) {
        if (report.debug_volume_scene_up_velocity_valid) {
            fprintf(stdout,
                    "debug volume report: active_density=%zu solid=%zu rho_max=%.6f speed_max=%.6f scene_up_vel(avg/peak)=%.6f/%.6f\n",
                    report.debug_volume_active_density_cells,
                    report.debug_volume_solid_cells,
                    report.debug_volume_max_density,
                    report.debug_volume_max_velocity_magnitude,
                    report.debug_volume_scene_up_velocity_avg,
                    report.debug_volume_scene_up_velocity_peak);
        } else {
            fprintf(stdout,
                    "debug volume report: active_density=%zu solid=%zu rho_max=%.6f speed_max=%.6f\n",
                    report.debug_volume_active_density_cells,
                    report.debug_volume_solid_cells,
                    report.debug_volume_max_density,
                    report.debug_volume_max_velocity_magnitude);
        }
    }
    fprintf(stdout,
            "resolved emitter: source=%s footprint=%s attached_object=%d attached_import=%d radius=%.6f strength=%.6f dir=(%.6f, %.6f, %.6f)\n",
            sim_runtime_emitter_source_kind_label(resolved.source_kind),
            sim_runtime_emitter_footprint_kind_label(resolved.primary_footprint),
            resolved.attached_object,
            resolved.attached_import,
            resolved.radius,
            resolved.strength,
            resolved.dir_x,
            resolved.dir_y,
            resolved.dir_z);

    desc = (SimRuntime3DDomainDesc){
        .grid_w = report.domain_w,
        .grid_h = report.domain_h,
        .grid_d = report.domain_d,
        .cell_count = report.cell_count,
        .slice_cell_count = (size_t)report.domain_w * (size_t)report.domain_h,
        .world_min_x = report.world_min_x,
        .world_min_y = report.world_min_y,
        .world_min_z = report.world_min_z,
        .world_max_x = report.world_max_x,
        .world_max_y = report.world_max_y,
        .world_max_z = report.world_max_z,
        .voxel_size = report.voxel_size,
    };
    if (!sim_runtime_3d_anchor_resolve_resolved_emitter_world_anchor(&scene,
                                                                     &desc,
                                                                     &resolved,
                                                                     &anchor_world)) {
        fprintf(stderr, "runtime_scene_emitter_diag_tool: failed to resolve world anchor\n");
        sim_runtime_backend_destroy(backend);
        return 1;
    }

    {
        if (!backend_3d_scaffold_resolve_emitter_placement(&scene, &desc, &resolved, &placement)) {
            fprintf(stderr, "runtime_scene_emitter_diag_tool: failed to resolve emitter placement\n");
            sim_runtime_backend_destroy(backend);
            return 1;
        }
        fprintf(stdout,
                "resolved anchor world: (%.6f, %.6f, %.6f)\n",
                anchor_world.x,
                anchor_world.y,
                anchor_world.z);
        fprintf(stdout,
                "resolved anchor grid: center=(%d, %d, %d) sphere_radius_cells=%d sphere_bounds=[x:%d..%d y:%d..%d z:%d..%d]\n",
                placement.center_x,
                placement.center_y,
                placement.center_z,
                placement.radius_cells,
                placement.min_x,
                placement.max_x,
                placement.min_y,
                placement.max_y,
                placement.min_z,
                placement.max_z);

        if (resolved.attached_object >= 0 &&
            resolved.attached_object < (int)selected_preset.object_count) {
            attached_object = &selected_preset.objects[resolved.attached_object];
            print_object_world_footprint(&desc, attached_object);
            if (resolved.attached_object < visual.retained_scene.retained_object_count) {
                retained_object = &visual.retained_scene.objects[resolved.attached_object];
                if (retained_prism_bounds(retained_object, &retained_bounds)) {
                    print_bounds("retained prism render bounds", &retained_bounds);
                }
            }
            if (backend_3d_scaffold_build_object_box(&desc, &placement, attached_object, &object_box)) {
                DiagWorldBounds voxel_box_bounds = {
                    .valid = true,
                    .min_x = cell_world_min(desc.world_min_x, desc.voxel_size, object_box.min_x),
                    .max_x = cell_world_max(desc.world_min_x, desc.voxel_size, object_box.max_x),
                    .min_y = cell_world_min(desc.world_min_y, desc.voxel_size, object_box.min_y),
                    .max_y = cell_world_max(desc.world_min_y, desc.voxel_size, object_box.max_y),
                    .min_z = cell_world_min(desc.world_min_z, desc.voxel_size, object_box.min_z),
                    .max_z = cell_world_max(desc.world_min_z, desc.voxel_size, object_box.max_z),
                };
                have_object_box = true;
                fprintf(stdout,
                        "attached object emitter box grid: center=(%d, %d, %d) half=(%.3f, %.3f, %.3f) bounds=[x:%d..%d y:%d..%d z:%d..%d]\n",
                        object_box.center_x,
                        object_box.center_y,
                        object_box.center_z,
                        object_box.half_u_cells,
                        object_box.half_v_cells,
                        object_box.half_w_cells,
                        object_box.min_x,
                        object_box.max_x,
                        object_box.min_y,
                        object_box.max_y,
                        object_box.min_z,
                        object_box.max_z);
                print_bounds("attached object emitter box voxel AABB", &voxel_box_bounds);
            }
        }
    }

    sim_runtime_backend_build_obstacles(backend, &scene);
    if (!sim_runtime_backend_get_debug_volume_view_3d(backend, &view)) {
        fprintf(stderr, "runtime_scene_emitter_diag_tool: failed to fetch pre-emit debug view\n");
        sim_runtime_backend_destroy(backend);
        return 1;
    }
    if (collect_solid_bounds(&view, &solid_bounds, &solid_cells)) {
        fprintf(stdout, "solid occupancy cells before emitter apply: %zu\n", solid_cells);
        print_bounds("solid occupancy bounds before emitter apply", &solid_bounds);
    } else {
        fprintf(stdout, "solid occupancy cells before emitter apply: 0\n");
    }

    sim_runtime_backend_apply_emitters(backend, &scene, 0.1);
    if (!sim_runtime_backend_get_report(backend, &report)) {
        fprintf(stderr, "runtime_scene_emitter_diag_tool: failed to fetch post-emit report\n");
        sim_runtime_backend_destroy(backend);
        return 1;
    }
    if (report.debug_volume_view_3d_available) {
        if (report.debug_volume_scene_up_velocity_valid) {
            fprintf(stdout,
                    "post-step debug volume report: active_density=%zu solid=%zu rho_max=%.6f speed_max=%.6f scene_up_vel(avg/peak)=%.6f/%.6f\n",
                    report.debug_volume_active_density_cells,
                    report.debug_volume_solid_cells,
                    report.debug_volume_max_density,
                    report.debug_volume_max_velocity_magnitude,
                    report.debug_volume_scene_up_velocity_avg,
                    report.debug_volume_scene_up_velocity_peak);
        } else {
            fprintf(stdout,
                    "post-step debug volume report: active_density=%zu solid=%zu rho_max=%.6f speed_max=%.6f\n",
                    report.debug_volume_active_density_cells,
                    report.debug_volume_solid_cells,
                    report.debug_volume_max_density,
                    report.debug_volume_max_velocity_magnitude);
        }
    }
    if (!sim_runtime_backend_get_debug_volume_view_3d(backend, &view)) {
        fprintf(stderr, "runtime_scene_emitter_diag_tool: failed to fetch post-emit debug view\n");
        sim_runtime_backend_destroy(backend);
        return 1;
    }
    if (collect_density_stats(&view, &density_stats)) {
        fprintf(stdout,
                "post-emit density: active_cells=%zu total_density=%.6f centroid=(%.6f, %.6f, %.6f)\n",
                density_stats.active_cells,
                density_stats.total_density,
                density_stats.centroid_x,
                density_stats.centroid_y,
                density_stats.centroid_z);
        print_bounds("post-emit density bounds", &density_stats.bounds);
        fprintf(stdout,
                "centroid-anchor delta: (%.6f, %.6f, %.6f)\n",
                density_stats.centroid_x - anchor_world.x,
                density_stats.centroid_y - anchor_world.y,
                density_stats.centroid_z - anchor_world.z);
    } else {
        fprintf(stdout, "post-emit density: no active cells\n");
    }

    fprintf(stdout,
            "emitter step stats: emitters=%zu attached=%zu free=%zu affected_cells=%zu last_footprint=%zu density_delta=%.6f velocity_delta=%.6f\n",
            report.emitter_step_emitters_applied,
            report.emitter_step_attached_emitters_applied,
            report.emitter_step_free_emitters_applied,
            report.emitter_step_affected_cells,
            report.emitter_step_last_footprint_cells,
            report.emitter_step_density_delta,
            report.emitter_step_velocity_magnitude_delta);

    if (!have_object_box && resolved.source_kind == SIM_RUNTIME_EMITTER_SOURCE_ATTACHED_OBJECT) {
        fprintf(stdout, "attached object emitter box: unavailable\n");
    }

    sim_runtime_backend_destroy(backend);
    return 0;
}
