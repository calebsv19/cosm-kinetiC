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
#include <unistd.h>

#include "cJSON.h"

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
    fprintf(stderr, "water_surface_artifacts_contract_test: %s\n", message);
    return false;
}

static cJSON *read_json_file(const char *path) {
    CoreBuffer file_data = {0};
    cJSON *root = NULL;
    CoreResult r = core_io_read_all(path, &file_data);
    if (r.code != CORE_OK || !file_data.data || file_data.size == 0u) return NULL;
    root = cJSON_ParseWithLength((const char *)file_data.data, file_data.size);
    core_io_buffer_free(&file_data);
    return root;
}

static bool seed_flat_water_with_peak(SimRuntimeBackend *backend,
                                      const SimRuntime3DDomainDesc *desc) {
    if (!backend || !desc || desc->grid_h < 3) return false;
    for (int z = 0; z < desc->grid_d; ++z) {
        for (int x = 0; x < desc->grid_w; ++x) {
            if (!sim_runtime_backend_3d_test_write_cell(backend,
                                                        x,
                                                        0,
                                                        z,
                                                        1.0f,
                                                        0.0f,
                                                        0.0f,
                                                        0.0f,
                                                        0.0f,
                                                        0u) ||
                !sim_runtime_backend_3d_test_write_cell(backend,
                                                        x,
                                                        1,
                                                        z,
                                                        1.0f,
                                                        0.0f,
                                                        0.0f,
                                                        0.0f,
                                                        0.0f,
                                                        0u)) {
                return false;
            }
        }
    }
    return sim_runtime_backend_3d_test_write_cell(backend,
                                                  0,
                                                  2,
                                                  0,
                                                  1.0f,
                                                  0.0f,
                                                  0.0f,
                                                  0.0f,
                                                  0.0f,
                                                  0u);
}

static bool test_water_mode_writes_heightfield_manifest_and_bundle_link(void) {
    AppConfig cfg = {0};
    SimModeRoute route = {
        .backend_lane = SIM_BACKEND_CONTROLLED_3D,
        .requested_space_mode = SPACE_MODE_3D,
        .projection_space_mode = SPACE_MODE_2D,
        .water_mode_active = true,
    };
    PhysicsSimRuntimeVisualBootstrap visual = {0};
    FluidScenePreset preset = {0};
    SceneState scene = {0};
    SimRuntimeBackend *backend = NULL;
    SimRuntimeBackend3DScaffoldTestView impl = {0};
    char original_cwd[PATH_MAX];
    char temp_dir[] = "/tmp/physics_sim_water_surface_contract_XXXXXX";
    char output_root[PATH_MAX];
    char manifest_path[PATH_MAX];
    char surface_path[PATH_MAX];
    char bundle_path[PATH_MAX];
    cJSON *manifest = NULL;
    cJSON *surface = NULL;
    cJSON *bundle = NULL;
    cJSON *frames = NULL;
    cJSON *entry = NULL;
    cJSON *summary = NULL;
    cJSON *heights = NULL;
    cJSON *normals = NULL;
    cJSON *water_source = NULL;
    bool ok = false;
    float expected_min_y = 0.0f;
    float expected_max_y = 0.0f;
    size_t expected_samples = 0u;

    if (!getcwd(original_cwd, sizeof(original_cwd))) {
        return fail_with_message("failed to capture cwd");
    }
    if (!mkdtemp(temp_dir)) {
        return fail_with_message("failed to create temp dir");
    }
    if (chdir(temp_dir) != 0) {
        return fail_with_message("failed to enter temp dir");
    }
    snprintf(output_root, sizeof(output_root), "%s/output", temp_dir);
    if (mkdir(output_root, 0755) != 0) {
        return fail_with_message("failed to create output dir");
    }
    if (!export_paths_set_root(output_root)) {
        return fail_with_message("failed to bind export root");
    }

    cfg.sim_mode = SIM_MODE_WATER;
    cfg.space_mode = SPACE_MODE_3D;
    cfg.water_level = 0.5f;
    cfg.grid_w = 8;
    cfg.grid_h = 8;
    cfg.grid_d = 4;
    cfg.window_w = 320;
    cfg.window_h = 240;

    preset.name = "Water Surface Contract";
    preset.domain = SCENE_DOMAIN_WATER;
    preset.dimension_mode = SCENE_DIMENSION_MODE_3D;
    preset.domain_width = 4.0f;
    preset.domain_height = 1.0f;

    backend = sim_runtime_backend_create(&cfg, &preset, &route, &visual);
    if (!backend) {
        ok = fail_with_message("backend create failed");
        goto cleanup;
    }
    sim_runtime_backend_build_obstacles(backend, NULL);
    if (!sim_runtime_backend_3d_test_view_refresh(backend, &impl)) {
        ok = fail_with_message("backend view refresh failed");
        goto cleanup;
    }
    if (!seed_flat_water_with_peak(backend, &impl.volume.desc)) {
        ok = fail_with_message("water seed failed");
        goto cleanup;
    }

    scene.time = 1.25;
    scene.dt = 0.05;
    scene.mode_route = route;
    scene.backend = backend;
    scene.preset = &preset;
    scene.config = &cfg;

    if (!volume_frames_write(&scene, 5u)) {
        ok = fail_with_message("volume frame export failed");
        goto cleanup;
    }

    snprintf(manifest_path,
             sizeof(manifest_path),
             "%s/volume_frames/%s/water_manifest_v1.json",
             output_root,
             preset.name);
    snprintf(surface_path,
             sizeof(surface_path),
             "%s/volume_frames/%s/water_surface_000005.json",
             output_root,
             preset.name);
    snprintf(bundle_path,
             sizeof(bundle_path),
             "%s/volume_frames/%s/scene_bundle.json",
             output_root,
             preset.name);

    if (!core_io_path_exists(manifest_path) ||
        !core_io_path_exists(surface_path) ||
        !core_io_path_exists(bundle_path)) {
        ok = fail_with_message("missing water artifacts");
        goto cleanup;
    }

    manifest = read_json_file(manifest_path);
    surface = read_json_file(surface_path);
    bundle = read_json_file(bundle_path);
    if (!manifest || !surface || !bundle) {
        ok = fail_with_message("failed to parse water artifact json");
        goto cleanup;
    }

    frames = cJSON_GetObjectItem(manifest, "frames");
    entry = cJSON_IsArray(frames) ? cJSON_GetArrayItem(frames, 0) : NULL;
    if (!cJSON_IsString(cJSON_GetObjectItem(manifest, "schema")) ||
        strcmp(cJSON_GetObjectItem(manifest, "schema")->valuestring,
               "physics_sim_water_manifest_v1") != 0 ||
        !cJSON_IsString(cJSON_GetObjectItem(manifest, "frame_contract")) ||
        strcmp(cJSON_GetObjectItem(manifest, "frame_contract")->valuestring,
               "water_surface_heightfield_v1") != 0 ||
        !cJSON_IsString(cJSON_GetObjectItem(manifest, "surface_axis")) ||
        strcmp(cJSON_GetObjectItem(manifest, "surface_axis")->valuestring, "y") != 0 ||
        !cJSON_IsObject(entry) ||
        !cJSON_IsString(cJSON_GetObjectItem(entry, "path")) ||
        strcmp(cJSON_GetObjectItem(entry, "path")->valuestring,
               "water_surface_000005.json") != 0) {
        ok = fail_with_message("water manifest contract mismatch");
        goto cleanup;
    }

    summary = cJSON_GetObjectItem(surface, "summary");
    heights = cJSON_GetObjectItem(surface, "heights_y");
    normals = cJSON_GetObjectItem(surface, "normals_xyz");
    expected_samples = (size_t)impl.volume.desc.grid_w * (size_t)impl.volume.desc.grid_d;
    expected_min_y = impl.volume.desc.world_min_y + 2.0f * impl.volume.desc.voxel_size;
    expected_max_y = impl.volume.desc.world_min_y + 3.0f * impl.volume.desc.voxel_size;
    if (!cJSON_IsString(cJSON_GetObjectItem(surface, "schema")) ||
        strcmp(cJSON_GetObjectItem(surface, "schema")->valuestring,
               "physics_sim_water_surface_heightfield_v1") != 0 ||
        !cJSON_IsArray(heights) ||
        cJSON_GetArraySize(heights) != (int)expected_samples ||
        !cJSON_IsArray(normals) ||
        cJSON_GetArraySize(normals) != (int)(expected_samples * 3u) ||
        !cJSON_IsObject(summary) ||
        !cJSON_IsNumber(cJSON_GetObjectItem(summary, "wet_columns")) ||
        (size_t)cJSON_GetObjectItem(summary, "wet_columns")->valuedouble != expected_samples ||
        !cJSON_IsNumber(cJSON_GetObjectItem(summary, "dry_columns")) ||
        cJSON_GetObjectItem(summary, "dry_columns")->valuedouble != 0.0 ||
        !cJSON_IsBool(cJSON_GetObjectItem(summary, "finite_normals")) ||
        !cJSON_IsTrue(cJSON_GetObjectItem(summary, "finite_normals")) ||
        !nearly_equal((float)cJSON_GetObjectItem(summary, "surface_min_y")->valuedouble,
                      expected_min_y) ||
        !nearly_equal((float)cJSON_GetObjectItem(summary, "surface_max_y")->valuedouble,
                      expected_max_y)) {
        ok = fail_with_message("water surface frame contract mismatch");
        goto cleanup;
    }

    water_source = cJSON_GetObjectItem(bundle, "water_source");
    if (!cJSON_IsObject(water_source) ||
        !cJSON_IsString(cJSON_GetObjectItem(water_source, "kind")) ||
        strcmp(cJSON_GetObjectItem(water_source, "kind")->valuestring,
               "water_manifest") != 0 ||
        !cJSON_IsString(cJSON_GetObjectItem(water_source, "path")) ||
        strcmp(cJSON_GetObjectItem(water_source, "path")->valuestring,
               "water_manifest_v1.json") != 0 ||
        !cJSON_IsString(cJSON_GetObjectItem(water_source, "surface_representation")) ||
        strcmp(cJSON_GetObjectItem(water_source, "surface_representation")->valuestring,
               "heightfield") != 0) {
        ok = fail_with_message("scene bundle water_source mismatch");
        goto cleanup;
    }

    ok = true;

cleanup:
    cJSON_Delete(manifest);
    cJSON_Delete(surface);
    cJSON_Delete(bundle);
    if (backend) {
        sim_runtime_backend_destroy(backend);
    }
    chdir(original_cwd);
    return ok;
}

int main(void) {
    if (!test_water_mode_writes_heightfield_manifest_and_bundle_link()) {
        return 1;
    }
    fprintf(stdout, "water_surface_artifacts_contract_test: success\n");
    return 0;
}
