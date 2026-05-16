#include "app/atmospheric/atmospheric_warm_start.h"
#include "app/scene_state.h"
#include "app/sim_runtime_backend.h"
#include "core_io.h"
#include "export/export_paths.h"
#include "export/volume_frames.h"
#include "sim_runtime_backend_3d_test_support.h"

#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

extern char *mkdtemp(char *);

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

static bool nearly_equal(float a, float b) {
    float diff = a - b;
    if (diff < 0.0f) diff = -diff;
    return diff < 0.0001f;
}

static bool fail_with_message(const char *message) {
    fprintf(stderr, "atmospheric_warm_start_contract_test: %s\n", message);
    return false;
}

static bool apply_and_verify(const AppConfig *cfg,
                             const FluidScenePreset *preset,
                             const SimModeRoute *route,
                             const PhysicsSimRuntimeVisualBootstrap *visual,
                             const AtmosphericWarmStartVolume3D *volume) {
    SimRuntimeBackend *target = NULL;
    AtmosphericWarmStartStats3D stats = {0};
    SimRuntimeBackendReport report = {0};
    SimRuntimeBackend3DScaffoldTestView view = {0};
    char error[160] = {0};
    size_t idx = 0u;
    bool ok = false;

    target = sim_runtime_backend_create(cfg, preset, route, visual);
    if (!target) return fail_with_message("target backend create failed");
    if (!atmospheric_warm_start_apply_3d(target, volume, &stats, error, sizeof(error))) {
        fprintf(stderr, "warm-start apply error: %s\n", error);
        goto done;
    }
    if (stats.active_density_cells != 2u ||
        stats.solid_cells == 0u ||
        !nearly_equal(stats.max_density, 3.5f)) {
        ok = fail_with_message("unexpected warm-start apply stats");
        goto done;
    }
    if (!sim_runtime_backend_get_report(target, &report) ||
        !report.atmospheric_warm_start_loaded ||
        report.atmospheric_seeded ||
        report.atmospheric_warm_start_source_kind != (int)volume->metadata.source_kind ||
        report.atmospheric_warm_start_w != volume->metadata.width ||
        report.atmospheric_warm_start_h != volume->metadata.height ||
        report.atmospheric_warm_start_d != volume->metadata.depth ||
        report.atmospheric_warm_start_active_density_cells != 2u ||
        !nearly_equal(report.atmospheric_warm_start_max_density, 3.5f)) {
        ok = fail_with_message("unexpected backend warm-start report");
        goto done;
    }
    if (!sim_runtime_backend_3d_test_view_refresh(target, &view)) {
        ok = fail_with_message("target export view refresh failed");
        goto done;
    }
    idx = sim_runtime_3d_volume_index(&view.volume.desc, 2, 3, 4);
    if (!nearly_equal(view.volume.density[idx], 3.5f) ||
        !nearly_equal(view.volume.velocity_x[idx], 1.25f) ||
        !nearly_equal(view.volume.velocity_y[idx], -2.5f) ||
        !nearly_equal(view.volume.velocity_z[idx], 0.75f) ||
        !view.obstacle_occupancy[idx]) {
        ok = fail_with_message("primary warm-start cell mismatch");
        goto done;
    }
    idx = sim_runtime_3d_volume_index(&view.volume.desc, 4, 3, 2);
    if (!nearly_equal(view.volume.density[idx], 1.25f) ||
        !nearly_equal(view.volume.velocity_x[idx], -0.5f) ||
        !nearly_equal(view.volume.velocity_y[idx], 0.25f) ||
        !nearly_equal(view.volume.velocity_z[idx], 2.0f) ||
        view.obstacle_occupancy[idx]) {
        ok = fail_with_message("secondary warm-start cell mismatch");
        goto done;
    }
    ok = true;

done:
    sim_runtime_backend_destroy(target);
    return ok;
}

static bool apply_scene_path_and_verify(const AppConfig *cfg,
                                        const FluidScenePreset *preset,
                                        const SimModeRoute *route,
                                        const PhysicsSimRuntimeVisualBootstrap *visual,
                                        const char *path) {
    SimRuntimeBackend *target = NULL;
    SceneState scene = {0};
    AtmosphericWarmStartStats3D stats = {0};
    SimRuntimeBackendReport report = {0};
    SimRuntimeBackend3DScaffoldTestView view = {0};
    char error[160] = {0};
    size_t idx = 0u;
    bool ok = false;

    target = sim_runtime_backend_create(cfg, preset, route, visual);
    if (!target) return fail_with_message("scene wrapper target backend create failed");
    scene.mode_route = *route;
    scene.backend = target;
    scene.preset = preset;
    scene.config = cfg;
    scene.runtime_visual = *visual;
    if (!atmospheric_warm_start_apply_scene_3d(&scene, path, &stats, error, sizeof(error))) {
        fprintf(stderr, "scene warm-start apply error: %s\n", error);
        goto done;
    }
    if (stats.active_density_cells != 2u ||
        stats.solid_cells == 0u ||
        !nearly_equal(stats.max_density, 3.5f)) {
        ok = fail_with_message("unexpected scene warm-start stats");
        goto done;
    }
    if (scene.atmospheric_warm_start.status != ATMOSPHERIC_WARM_START_RUNTIME_APPLIED ||
        scene.atmospheric_warm_start.metadata.source_kind != ATMOSPHERIC_WARM_START_SOURCE_VF3D_RAW ||
        strcmp(scene.atmospheric_warm_start.path, path) != 0 ||
        scene.atmospheric_warm_start.stats.active_density_cells != 2u) {
        ok = fail_with_message("unexpected scene warm-start runtime report");
        goto done;
    }
    if (!sim_runtime_backend_get_report(target, &report) ||
        !report.atmospheric_warm_start_loaded ||
        report.atmospheric_seeded ||
        report.atmospheric_warm_start_source_kind != ATMOSPHERIC_WARM_START_SOURCE_VF3D_RAW ||
        report.atmospheric_warm_start_w != scene.atmospheric_warm_start.metadata.width ||
        report.atmospheric_warm_start_h != scene.atmospheric_warm_start.metadata.height ||
        report.atmospheric_warm_start_d != scene.atmospheric_warm_start.metadata.depth ||
        report.atmospheric_warm_start_active_density_cells !=
            scene.atmospheric_warm_start.stats.active_density_cells) {
        ok = fail_with_message("unexpected scene backend warm-start report");
        goto done;
    }
    if (!sim_runtime_backend_3d_test_view_refresh(target, &view)) {
        ok = fail_with_message("scene wrapper target export view refresh failed");
        goto done;
    }
    idx = sim_runtime_3d_volume_index(&view.volume.desc, 2, 3, 4);
    if (!nearly_equal(view.volume.density[idx], 3.5f) ||
        !nearly_equal(view.volume.velocity_x[idx], 1.25f) ||
        !nearly_equal(view.volume.velocity_y[idx], -2.5f) ||
        !nearly_equal(view.volume.velocity_z[idx], 0.75f) ||
        !view.obstacle_occupancy[idx]) {
        ok = fail_with_message("scene wrapper primary cell mismatch");
        goto done;
    }
    ok = true;

done:
    sim_runtime_backend_destroy(target);
    return ok;
}

static bool test_exported_vf3d_and_pack_load_as_3d_warm_starts(void) {
    AppConfig cfg = {0};
    SimModeRoute route = {
        .backend_lane = SIM_BACKEND_CONTROLLED_3D,
        .requested_space_mode = SPACE_MODE_3D,
        .projection_space_mode = SPACE_MODE_2D,
    };
    PhysicsSimRuntimeVisualBootstrap visual = {0};
    FluidScenePreset preset = {0};
    SceneState scene = {0};
    SimRuntimeBackend *source = NULL;
    AtmosphericWarmStartVolume3D raw = {0};
    AtmosphericWarmStartVolume3D pack = {0};
    AtmosphericWarmStartStats3D raw_stats = {0};
    char temp_dir[] = "/tmp/physics_sim_atmo_warm_start_XXXXXX";
    char output_root[PATH_MAX];
    char raw_path[PATH_MAX];
    char pack_path[PATH_MAX];
    char error[160] = {0};
    bool ok = false;

    if (!mkdtemp(temp_dir)) {
        return fail_with_message("failed to create temp dir");
    }
    snprintf(output_root, sizeof(output_root), "%s/out", temp_dir);
    if (mkdir(output_root, 0755) != 0) {
        return fail_with_message("failed to create output root");
    }
    if (!export_paths_set_root(output_root)) {
        return fail_with_message("failed to bind export root");
    }

    cfg.quality_index = 5;
    cfg.grid_w = 32;
    cfg.grid_h = 32;
    cfg.grid_d = 16;
    cfg.window_w = 640;
    cfg.window_h = 480;
    cfg.space_mode = SPACE_MODE_3D;

    visual.scene_domain.enabled = true;
    visual.scene_domain_authored = true;
    visual.scene_domain.min.x = -1.0;
    visual.scene_domain.min.y = -1.0;
    visual.scene_domain.min.z = -1.0;
    visual.scene_domain.max.x = 1.0;
    visual.scene_domain.max.y = 1.0;
    visual.scene_domain.max.z = 1.0;
    visual.scene_up.valid = true;
    visual.scene_up.direction = (CoreObjectVec3){0.0, 1.0, 0.0};
    visual.scene_up.source = PHYSICS_SIM_RUNTIME_SCENE_UP_FALLBACK_POSITIVE_Z;

    preset.name = "atmo_warm_start_contract";
    preset.domain = SCENE_DOMAIN_ATMOSPHERIC;
    preset.dimension_mode = SCENE_DIMENSION_MODE_3D;

    source = sim_runtime_backend_create(&cfg, &preset, &route, &visual);
    if (!source) {
        ok = fail_with_message("source backend create failed");
        goto done;
    }
    sim_runtime_backend_build_obstacles(source, NULL);
    if (!sim_runtime_backend_3d_test_write_cell(source,
                                                2,
                                                3,
                                                4,
                                                3.5f,
                                                1.25f,
                                                -2.5f,
                                                0.75f,
                                                0.125f,
                                                1u) ||
        !sim_runtime_backend_3d_test_write_cell(source,
                                                4,
                                                3,
                                                2,
                                                1.25f,
                                                -0.5f,
                                                0.25f,
                                                2.0f,
                                                -0.25f,
                                                0u)) {
        ok = fail_with_message("source seed write failed");
        goto done;
    }

    scene.time = 1.5;
    scene.dt = 0.016;
    scene.mode_route = route;
    scene.backend = source;
    scene.preset = &preset;
    scene.config = &cfg;
    scene.runtime_visual = visual;
    if (!volume_frames_write(&scene, 7u)) {
        ok = fail_with_message("source vf3d export failed");
        goto done;
    }

    snprintf(raw_path, sizeof(raw_path),
             "%s/volume_frames/%s/frame_%06d.vf3d",
             output_root,
             preset.name,
             7);
    snprintf(pack_path, sizeof(pack_path),
             "%s/volume_frames/%s/frame_%06d.pack",
             output_root,
             preset.name,
             7);
    if (!core_io_path_exists(raw_path) || !core_io_path_exists(pack_path)) {
        ok = fail_with_message("missing exported warm-start files");
        goto done;
    }

    if (!atmospheric_warm_start_load_3d(raw_path, &raw, error, sizeof(error))) {
        fprintf(stderr, "raw warm-start load error: %s\n", error);
        goto done;
    }
    raw_stats = atmospheric_warm_start_stats_3d(&raw);
    if (raw.metadata.source_kind != ATMOSPHERIC_WARM_START_SOURCE_VF3D_RAW ||
        raw.metadata.width <= 0 ||
        raw.metadata.height <= 0 ||
        raw.metadata.depth <= 0 ||
        raw_stats.active_density_cells != 2u ||
        raw_stats.solid_cells == 0u ||
        !nearly_equal(raw_stats.max_density, 3.5f)) {
        ok = fail_with_message("unexpected raw warm-start metadata/stats");
        goto done;
    }
    if (!apply_and_verify(&cfg, &preset, &route, &visual, &raw)) {
        goto done;
    }
    if (!apply_scene_path_and_verify(&cfg, &preset, &route, &visual, raw_path)) {
        goto done;
    }

    if (!atmospheric_warm_start_load_3d(pack_path, &pack, error, sizeof(error))) {
        fprintf(stderr, "pack warm-start load error: %s\n", error);
        goto done;
    }
    if (pack.metadata.source_kind != ATMOSPHERIC_WARM_START_SOURCE_VF3H_PACK ||
        pack.metadata.width != raw.metadata.width ||
        pack.metadata.height != raw.metadata.height ||
        pack.metadata.depth != raw.metadata.depth ||
        !apply_and_verify(&cfg, &preset, &route, &visual, &pack)) {
        ok = fail_with_message("pack warm-start load/apply mismatch");
        goto done;
    }
    ok = true;

done:
    atmospheric_warm_start_volume_3d_free(&raw);
    atmospheric_warm_start_volume_3d_free(&pack);
    sim_runtime_backend_destroy(source);
    return ok;
}

static bool test_scene_rejects_bad_warm_start_with_runtime_report(void) {
    AppConfig cfg = {0};
    FluidScenePreset preset = {0};
    SimModeRoute route = {
        .backend_lane = SIM_BACKEND_CONTROLLED_3D,
        .requested_space_mode = SPACE_MODE_3D,
        .projection_space_mode = SPACE_MODE_2D,
    };
    PhysicsSimRuntimeVisualBootstrap visual = {0};
    SimRuntimeBackend *target = NULL;
    SceneState scene = {0};
    AtmosphericWarmStartStats3D stats = {0};
    char error[160] = {0};
    const char *bad_path = "/tmp/not_a_warm_start.txt";
    bool ok = false;

    cfg.grid_w = 16;
    cfg.grid_h = 16;
    cfg.grid_d = 8;
    cfg.window_w = 320;
    cfg.window_h = 240;
    cfg.space_mode = SPACE_MODE_3D;
    preset.domain = SCENE_DOMAIN_ATMOSPHERIC;
    preset.dimension_mode = SCENE_DIMENSION_MODE_3D;
    target = sim_runtime_backend_create(&cfg, &preset, &route, &visual);
    if (!target) return fail_with_message("bad-path target backend create failed");
    scene.mode_route = route;
    scene.backend = target;
    scene.preset = &preset;
    scene.config = &cfg;

    if (atmospheric_warm_start_apply_scene_3d(&scene,
                                              bad_path,
                                              &stats,
                                              error,
                                              sizeof(error))) {
        ok = fail_with_message("bad warm-start unexpectedly applied");
        goto done;
    }
    if (scene.atmospheric_warm_start.status != ATMOSPHERIC_WARM_START_RUNTIME_REJECTED ||
        strcmp(scene.atmospheric_warm_start.path, bad_path) != 0 ||
        scene.atmospheric_warm_start.rejection_reason[0] == '\0' ||
        error[0] == '\0') {
        ok = fail_with_message("bad warm-start rejection report missing");
        goto done;
    }
    ok = true;

done:
    sim_runtime_backend_destroy(target);
    return ok;
}

int main(void) {
    if (!test_exported_vf3d_and_pack_load_as_3d_warm_starts()) {
        return 1;
    }
    if (!test_scene_rejects_bad_warm_start_with_runtime_report()) {
        return 1;
    }
    fprintf(stdout, "atmospheric_warm_start_contract_test: success\n");
    return 0;
}
