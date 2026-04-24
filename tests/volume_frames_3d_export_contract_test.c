#include "app/scene_state.h"
#include "app/sim_runtime_backend.h"
#include "app/sim_runtime_backend_3d_scaffold_internal.h"
#include "core_io.h"
#include "core_pack.h"
#include "export/volume_frames.h"
#include "export/volume_frames_vf3d.h"

#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
    fprintf(stderr, "volume_frames_3d_export_contract_test: %s\n", message);
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

static bool test_volume_frames_write_routes_authoritative_3d_runs_to_vf3d(void) {
    AppConfig cfg = {0};
    SimModeRoute route = {
        .backend_lane = SIM_BACKEND_CONTROLLED_3D,
        .requested_space_mode = SPACE_MODE_3D,
        .projection_space_mode = SPACE_MODE_2D,
    };
    PhysicsSimRuntimeVisualBootstrap visual = {0};
    FluidScenePreset preset = {0};
    SceneState scene = {0};
    SimRuntimeBackend *backend = NULL;
    SimRuntimeBackend3DScaffold *impl = NULL;
    char original_cwd[PATH_MAX];
    char temp_dir[] = "/tmp/physics_sim_vf3d_export_contract_XXXXXX";
    char raw_path[PATH_MAX];
    char pack_path[PATH_MAX];
    char manifest_path[PATH_MAX];
    char bundle_path[PATH_MAX];
    FILE *f = NULL;
    VolumeFrameHeaderVf3dV1 header = {0};
    size_t cell_count = 0;
    float *density = NULL;
    float *velx = NULL;
    float *vely = NULL;
    float *velz = NULL;
    float *pressure = NULL;
    uint8_t *solid = NULL;
    size_t idx = 0;
    CorePackReader reader = {0};
    CorePackChunkInfo info = {0};
    cJSON *manifest = NULL;
    cJSON *bundle = NULL;
    cJSON *frames = NULL;
    cJSON *entry = NULL;
    cJSON *space_contract = NULL;
    cJSON *fluid_source = NULL;
    bool ok = false;

    if (!getcwd(original_cwd, sizeof(original_cwd))) {
        return fail_with_message("failed to capture cwd");
    }
    if (!mkdtemp(temp_dir)) {
        return fail_with_message("failed to create temp dir");
    }
    if (chdir(temp_dir) != 0) {
        return fail_with_message("failed to enter temp dir");
    }

    cfg.quality_index = 5;
    cfg.grid_w = 64;
    cfg.grid_h = 64;
    cfg.window_w = 640;
    cfg.window_h = 480;

    visual.scene_domain.enabled = true;
    visual.scene_domain_authored = true;
    visual.scene_domain.min.x = -1.0;
    visual.scene_domain.min.y = -2.0;
    visual.scene_domain.min.z = -3.0;
    visual.scene_domain.max.x = 1.0;
    visual.scene_domain.max.y = 2.0;
    visual.scene_domain.max.z = 3.0;
    visual.scene_up.valid = true;
    visual.scene_up.direction = (CoreObjectVec3){0.0, 0.0, 1.0};
    visual.scene_up.source = PHYSICS_SIM_RUNTIME_SCENE_UP_FALLBACK_POSITIVE_Z;

    preset.name = "vf3d_contract_test";
    preset.dimension_mode = SCENE_DIMENSION_MODE_3D;

    backend = sim_runtime_backend_create(&cfg, &preset, &route, &visual);
    if (!backend) {
        ok = fail_with_message("backend create failed");
        goto cleanup;
    }
    impl = (SimRuntimeBackend3DScaffold *)backend->impl;
    if (!impl) {
        ok = fail_with_message("backend impl missing");
        goto cleanup;
    }

    sim_runtime_backend_build_obstacles(backend, NULL);
    idx = sim_runtime_3d_volume_index(&impl->volume.desc, 2, 3, 4);
    impl->volume.density[idx] = 1.25f;
    impl->volume.velocity_x[idx] = 2.0f;
    impl->volume.velocity_y[idx] = -3.0f;
    impl->volume.velocity_z[idx] = 4.5f;
    impl->volume.pressure[idx] = 6.0f;
    impl->obstacle_occupancy[idx] = 1u;

    scene.time = 2.5;
    scene.dt = 0.033;
    scene.mode_route = route;
    scene.backend = backend;
    scene.preset = &preset;
    scene.config = &cfg;
    scene.runtime_visual = visual;

    if (!volume_frames_write(&scene, 11u)) {
        ok = fail_with_message("volume frame export failed");
        goto cleanup;
    }

    snprintf(raw_path, sizeof(raw_path),
             "%s/export/volume_frames/%s/frame_%06d.vf3d",
             temp_dir,
             preset.name,
             11);
    snprintf(pack_path, sizeof(pack_path),
             "%s/export/volume_frames/%s/frame_%06d.pack",
             temp_dir,
             preset.name,
             11);
    snprintf(manifest_path, sizeof(manifest_path),
             "%s/export/volume_frames/%s/manifest.json",
             temp_dir,
             preset.name);
    snprintf(bundle_path, sizeof(bundle_path),
             "%s/export/volume_frames/%s/scene_bundle.json",
             temp_dir,
             preset.name);

    if (!core_io_path_exists(raw_path)) {
        ok = fail_with_message("missing vf3d raw export");
        goto cleanup;
    }
    if (!core_io_path_exists(pack_path)) {
        ok = fail_with_message("missing vf3d pack export");
        goto cleanup;
    }
    if (!core_io_path_exists(manifest_path)) {
        ok = fail_with_message("missing manifest");
        goto cleanup;
    }
    if (!core_io_path_exists(bundle_path)) {
        ok = fail_with_message("missing scene bundle");
        goto cleanup;
    }

    f = fopen(raw_path, "rb");
    if (!f) {
        ok = fail_with_message("failed to open raw vf3d");
        goto cleanup;
    }
    if (fread(&header, sizeof(header), 1, f) != 1) {
        fclose(f);
        ok = fail_with_message("failed to read vf3d header");
        goto cleanup;
    }
    cell_count = (size_t)header.grid_w * (size_t)header.grid_h * (size_t)header.grid_d;
    density = (float *)malloc(cell_count * sizeof(float));
    velx = (float *)malloc(cell_count * sizeof(float));
    vely = (float *)malloc(cell_count * sizeof(float));
    velz = (float *)malloc(cell_count * sizeof(float));
    pressure = (float *)malloc(cell_count * sizeof(float));
    solid = (uint8_t *)malloc(cell_count * sizeof(uint8_t));
    if (!density || !velx || !vely || !velz || !pressure || !solid) {
        fclose(f);
        ok = fail_with_message("failed to allocate raw buffers");
        goto cleanup;
    }
    if (fread(density, sizeof(float), cell_count, f) != cell_count ||
        fread(velx, sizeof(float), cell_count, f) != cell_count ||
        fread(vely, sizeof(float), cell_count, f) != cell_count ||
        fread(velz, sizeof(float), cell_count, f) != cell_count ||
        fread(pressure, sizeof(float), cell_count, f) != cell_count ||
        fread(solid, sizeof(uint8_t), cell_count, f) != cell_count) {
        fclose(f);
        ok = fail_with_message("failed to read raw vf3d payload");
        goto cleanup;
    }
    fclose(f);
    f = NULL;

    if (header.magic != (('V' << 24) | ('F' << 16) | ('3' << 8) | ('D'))) {
        ok = fail_with_message("raw vf3d magic mismatch");
        goto cleanup;
    }
    if (header.version != 1u) {
        ok = fail_with_message("raw vf3d version mismatch");
        goto cleanup;
    }
    if (header.grid_w != (uint32_t)impl->volume.desc.grid_w ||
        header.grid_h != (uint32_t)impl->volume.desc.grid_h ||
        header.grid_d != (uint32_t)impl->volume.desc.grid_d) {
        ok = fail_with_message("raw vf3d dimensions mismatch");
        goto cleanup;
    }
    if (!nearly_equal(header.origin_x, impl->volume.desc.world_min_x) ||
        !nearly_equal(header.origin_y, impl->volume.desc.world_min_y) ||
        !nearly_equal(header.origin_z, impl->volume.desc.world_min_z) ||
        !nearly_equal(header.voxel_size, impl->volume.desc.voxel_size)) {
        ok = fail_with_message("raw vf3d spatial header mismatch");
        goto cleanup;
    }
    if (!nearly_equal(header.scene_up_z, 1.0f)) {
        ok = fail_with_message("raw vf3d scene up mismatch");
        goto cleanup;
    }
    if (!nearly_equal(density[idx], 1.25f) ||
        !nearly_equal(velx[idx], 2.0f) ||
        !nearly_equal(vely[idx], -3.0f) ||
        !nearly_equal(velz[idx], 4.5f) ||
        !nearly_equal(pressure[idx], 6.0f) ||
        solid[idx] != 1u) {
        ok = fail_with_message("raw vf3d payload mismatch");
        goto cleanup;
    }

    if (core_pack_reader_open(pack_path, &reader).code != CORE_OK) {
        ok = fail_with_message("failed to open vf3d pack");
        goto cleanup;
    }
    if (core_pack_reader_chunk_count(&reader) != 8u) {
        ok = fail_with_message("vf3d pack chunk count mismatch");
        goto cleanup;
    }
    if (core_pack_reader_find_chunk(&reader, "VF3H", 0, &info).code != CORE_OK ||
        core_pack_reader_find_chunk(&reader, "DENS", 0, &info).code != CORE_OK ||
        core_pack_reader_find_chunk(&reader, "VELX", 0, &info).code != CORE_OK ||
        core_pack_reader_find_chunk(&reader, "VELY", 0, &info).code != CORE_OK ||
        core_pack_reader_find_chunk(&reader, "VELZ", 0, &info).code != CORE_OK ||
        core_pack_reader_find_chunk(&reader, "PRES", 0, &info).code != CORE_OK ||
        core_pack_reader_find_chunk(&reader, "SOLI", 0, &info).code != CORE_OK ||
        core_pack_reader_find_chunk(&reader, "JSON", 0, &info).code != CORE_OK) {
        ok = fail_with_message("vf3d pack missing required chunks");
        goto cleanup;
    }
    if (core_pack_reader_close(&reader).code != CORE_OK) {
        ok = fail_with_message("failed to close vf3d pack");
        goto cleanup;
    }
    memset(&reader, 0, sizeof(reader));

    manifest = read_json_file(manifest_path);
    if (!manifest) {
        ok = fail_with_message("failed to parse manifest json");
        goto cleanup;
    }
    space_contract = cJSON_GetObjectItem(manifest, "space_contract");
    frames = cJSON_GetObjectItem(manifest, "frames");
    entry = cJSON_IsArray(frames) ? cJSON_GetArrayItem(frames, 0) : NULL;
    if (!cJSON_IsNumber(cJSON_GetObjectItem(manifest, "manifest_version")) ||
        cJSON_GetObjectItem(manifest, "manifest_version")->valuedouble != 2.0 ||
        !cJSON_IsString(cJSON_GetObjectItem(manifest, "frame_contract")) ||
        strcmp(cJSON_GetObjectItem(manifest, "frame_contract")->valuestring, "vf3d") != 0 ||
        !cJSON_IsString(cJSON_GetObjectItem(manifest, "space_mode")) ||
        strcmp(cJSON_GetObjectItem(manifest, "space_mode")->valuestring, "3d") != 0) {
        ok = fail_with_message("manifest root contract mismatch");
        goto cleanup;
    }
    if (!cJSON_IsObject(space_contract) ||
        !cJSON_IsString(cJSON_GetObjectItem(space_contract, "axis_authority")) ||
        strcmp(cJSON_GetObjectItem(space_contract, "axis_authority")->valuestring, "xyz") != 0 ||
        !cJSON_IsNumber(cJSON_GetObjectItem(space_contract, "grid_d")) ||
        !cJSON_IsNumber(cJSON_GetObjectItem(space_contract, "origin_z")) ||
        !cJSON_IsNumber(cJSON_GetObjectItem(space_contract, "voxel_size"))) {
        ok = fail_with_message("manifest space_contract mismatch");
        goto cleanup;
    }
    if (!cJSON_IsObject(entry) ||
        !cJSON_IsString(cJSON_GetObjectItem(entry, "frame_contract")) ||
        strcmp(cJSON_GetObjectItem(entry, "frame_contract")->valuestring, "vf3d") != 0) {
        ok = fail_with_message("manifest frame entry mismatch");
        goto cleanup;
    }

    bundle = read_json_file(bundle_path);
    if (!bundle) {
        ok = fail_with_message("failed to parse scene bundle json");
        goto cleanup;
    }
    fluid_source = cJSON_GetObjectItem(bundle, "fluid_source");
    space_contract = cJSON_GetObjectItem(cJSON_GetObjectItem(bundle, "scene_metadata"), "space_contract");
    if (!cJSON_IsObject(fluid_source) ||
        !cJSON_IsString(cJSON_GetObjectItem(fluid_source, "contract")) ||
        strcmp(cJSON_GetObjectItem(fluid_source, "contract")->valuestring, "vf3d") != 0) {
        ok = fail_with_message("scene bundle fluid_source contract mismatch");
        goto cleanup;
    }
    if (!cJSON_IsObject(space_contract) ||
        !cJSON_IsString(cJSON_GetObjectItem(space_contract, "space_mode")) ||
        strcmp(cJSON_GetObjectItem(space_contract, "space_mode")->valuestring, "3d") != 0 ||
        !cJSON_IsString(cJSON_GetObjectItem(space_contract, "axis_authority")) ||
        strcmp(cJSON_GetObjectItem(space_contract, "axis_authority")->valuestring, "xyz") != 0) {
        ok = fail_with_message("scene bundle space_contract mismatch");
        goto cleanup;
    }

    ok = true;

cleanup:
    if (f) fclose(f);
    free(density);
    free(velx);
    free(vely);
    free(velz);
    free(pressure);
    free(solid);
    cJSON_Delete(manifest);
    cJSON_Delete(bundle);
    if (reader.file) {
        core_pack_reader_close(&reader);
    }
    if (backend) {
        sim_runtime_backend_destroy(backend);
    }
    chdir(original_cwd);
    return ok;
}

int main(void) {
    if (!test_volume_frames_write_routes_authoritative_3d_runs_to_vf3d()) {
        return 1;
    }
    fprintf(stdout, "volume_frames_3d_export_contract_test: success\n");
    return 0;
}
