#include "app/scene_state.h"
#include "app/sim_runtime_backend.h"
#include "app/sim_runtime_backend_3d_scaffold_internal.h"
#include "core_io.h"
#include "core_pack.h"
#include "export/volume_frames.h"
#include "export/volume_frames_vf3d.h"

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "cJSON.h"

extern char *mkdtemp(char *);

typedef struct Vf3dHeaderCanonical {
    uint32_t version;
    uint32_t grid_w;
    uint32_t grid_h;
    uint32_t grid_d;
    double   time_seconds;
    uint64_t frame_index;
    double   dt_seconds;
    float    origin_x;
    float    origin_y;
    float    origin_z;
    float    voxel_size;
    float    scene_up_x;
    float    scene_up_y;
    float    scene_up_z;
    uint32_t solid_mask_crc32;
} Vf3dHeaderCanonical;

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
    fprintf(stderr, "volume_frames_3d_tiny_parity_contract_test: %s\n", message);
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

static cJSON *read_pack_json_chunk(const char *pack_path) {
    CorePackReader reader = {0};
    CorePackChunkInfo json_chunk = {0};
    char *json_buf = NULL;
    cJSON *root = NULL;
    if (core_pack_reader_open(pack_path, &reader).code != CORE_OK) return NULL;
    if (core_pack_reader_find_chunk(&reader, "JSON", 0, &json_chunk).code != CORE_OK) {
        core_pack_reader_close(&reader);
        return NULL;
    }
    json_buf = (char *)malloc((size_t)json_chunk.size + 1u);
    if (!json_buf) {
        core_pack_reader_close(&reader);
        return NULL;
    }
    if (core_pack_reader_read_chunk_data(&reader, &json_chunk, json_buf, json_chunk.size).code != CORE_OK) {
        free(json_buf);
        core_pack_reader_close(&reader);
        return NULL;
    }
    json_buf[json_chunk.size] = '\0';
    root = cJSON_ParseWithLength(json_buf, (size_t)json_chunk.size);
    free(json_buf);
    core_pack_reader_close(&reader);
    return root;
}

static size_t count_nonzero_f32(const float *values, size_t count) {
    size_t total = 0;
    if (!values) return 0;
    for (size_t i = 0; i < count; ++i) {
        if (values[i] != 0.0f) total += 1u;
    }
    return total;
}

static size_t count_nonzero_u8(const uint8_t *values, size_t count) {
    size_t total = 0;
    if (!values) return 0;
    for (size_t i = 0; i < count; ++i) {
        if (values[i] != 0u) total += 1u;
    }
    return total;
}

static bool read_raw_vf3d(const char *path,
                          VolumeFrameHeaderVf3dV1 *out_header,
                          float *density,
                          float *velx,
                          float *vely,
                          float *velz,
                          float *pressure,
                          uint8_t *solid,
                          size_t cell_count) {
    FILE *f = NULL;
    if (!path || !out_header || !density || !velx || !vely || !velz || !pressure || !solid) return false;
    f = fopen(path, "rb");
    if (!f) return false;
    if (fread(out_header, sizeof(*out_header), 1, f) != 1 ||
        fread(density, sizeof(float), cell_count, f) != cell_count ||
        fread(velx, sizeof(float), cell_count, f) != cell_count ||
        fread(vely, sizeof(float), cell_count, f) != cell_count ||
        fread(velz, sizeof(float), cell_count, f) != cell_count ||
        fread(pressure, sizeof(float), cell_count, f) != cell_count ||
        fread(solid, sizeof(uint8_t), cell_count, f) != cell_count) {
        fclose(f);
        return false;
    }
    fclose(f);
    return true;
}

static bool read_pack_chunk_f32(CorePackReader *reader,
                                const char type[4],
                                float *dst,
                                size_t cell_count) {
    CorePackChunkInfo chunk = {0};
    if (!reader || !dst) return false;
    if (core_pack_reader_find_chunk(reader, type, 0, &chunk).code != CORE_OK) return false;
    if (chunk.size != (uint64_t)(cell_count * sizeof(float))) return false;
    return core_pack_reader_read_chunk_data(reader, &chunk, dst, chunk.size).code == CORE_OK;
}

static bool test_tiny_vf3d_fixture_parity_matches_backend_truth(void) {
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
    char temp_dir[] = "/tmp/physics_sim_vf3d_tiny_parity_XXXXXX";
    char raw_path[PATH_MAX];
    char pack_path[PATH_MAX];
    char manifest_path[PATH_MAX];
    VolumeFrameHeaderVf3dV1 raw_header = {0};
    Vf3dHeaderCanonical pack_header = {0};
    CorePackReader reader = {0};
    CorePackChunkInfo vf3h = {0};
    cJSON *manifest = NULL;
    cJSON *pack_json = NULL;
    cJSON *space_contract = NULL;
    cJSON *frames = NULL;
    cJSON *entry = NULL;
    float *raw_density = NULL;
    float *raw_velx = NULL;
    float *raw_vely = NULL;
    float *raw_velz = NULL;
    float *raw_pressure = NULL;
    uint8_t *raw_solid = NULL;
    float *pack_density = NULL;
    float *pack_velx = NULL;
    float *pack_vely = NULL;
    float *pack_velz = NULL;
    float *pack_pressure = NULL;
    uint8_t *pack_solid = NULL;
    size_t cell_count = 0;
    size_t idx_a = 0;
    size_t idx_b = 0;
    size_t idx_c = 0;
    size_t idx_d = 0;
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

    visual.scene_up.valid = true;
    visual.scene_up.direction = (CoreObjectVec3){0.0, 0.0, 1.0};
    visual.scene_up.source = PHYSICS_SIM_RUNTIME_SCENE_UP_FALLBACK_POSITIVE_Z;

    preset.name = "vf3d_tiny_fixture";
    preset.dimension_mode = SCENE_DIMENSION_MODE_3D;
    preset.domain_width = 4.0f;
    preset.domain_height = 2.0f;

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
    if (impl->volume.desc.grid_w != 16 || impl->volume.desc.grid_h != 8 || impl->volume.desc.grid_d != 8) {
        ok = fail_with_message("tiny domain dimensions mismatch");
        goto cleanup;
    }
    if (!nearly_equal(impl->volume.desc.voxel_size, 0.25f)) {
        ok = fail_with_message("tiny domain voxel size mismatch");
        goto cleanup;
    }

    cell_count = impl->volume.desc.cell_count;
    sim_runtime_3d_volume_clear(&impl->volume);
    memset(impl->obstacle_occupancy, 0, cell_count * sizeof(uint8_t));
    impl->obstacle_volume_dirty = false;
    impl->obstacle_slice_dirty = true;
    impl->debug_volume_stats_dirty = true;

    idx_a = sim_runtime_3d_volume_index(&impl->volume.desc, 1, 2, 3);
    idx_b = sim_runtime_3d_volume_index(&impl->volume.desc, 5, 1, 2);
    idx_c = sim_runtime_3d_volume_index(&impl->volume.desc, 9, 6, 4);
    idx_d = sim_runtime_3d_volume_index(&impl->volume.desc, 14, 4, 7);

    impl->volume.density[idx_a] = 1.0f;
    impl->volume.velocity_x[idx_a] = 0.5f;
    impl->volume.velocity_y[idx_a] = -0.25f;
    impl->volume.velocity_z[idx_a] = 2.0f;
    impl->volume.pressure[idx_a] = 4.0f;

    impl->volume.density[idx_b] = 2.5f;
    impl->volume.velocity_x[idx_b] = -1.0f;
    impl->volume.velocity_y[idx_b] = 3.0f;
    impl->volume.velocity_z[idx_b] = -2.0f;
    impl->volume.pressure[idx_b] = 5.5f;

    impl->volume.density[idx_c] = 0.75f;
    impl->volume.velocity_x[idx_c] = 1.25f;
    impl->volume.velocity_y[idx_c] = 1.5f;
    impl->volume.velocity_z[idx_c] = 0.25f;
    impl->volume.pressure[idx_c] = 6.25f;

    impl->volume.density[idx_d] = 4.0f;
    impl->volume.velocity_x[idx_d] = -3.5f;
    impl->volume.velocity_y[idx_d] = 0.75f;
    impl->volume.velocity_z[idx_d] = 1.0f;
    impl->volume.pressure[idx_d] = 7.0f;

    impl->obstacle_occupancy[idx_a] = 1u;
    impl->obstacle_occupancy[idx_c] = 1u;
    impl->obstacle_occupancy[idx_d] = 1u;

    scene.time = 1.25;
    scene.dt = 0.05;
    scene.mode_route = route;
    scene.backend = backend;
    scene.preset = &preset;
    scene.config = &cfg;
    scene.runtime_visual = visual;

    if (!volume_frames_write(&scene, 3u)) {
        ok = fail_with_message("volume frame export failed");
        goto cleanup;
    }

    snprintf(raw_path, sizeof(raw_path),
             "%s/export/volume_frames/%s/frame_%06d.vf3d",
             temp_dir,
             preset.name,
             3);
    snprintf(pack_path, sizeof(pack_path),
             "%s/export/volume_frames/%s/frame_%06d.pack",
             temp_dir,
             preset.name,
             3);
    snprintf(manifest_path, sizeof(manifest_path),
             "%s/export/volume_frames/%s/manifest.json",
             temp_dir,
             preset.name);

    if (!core_io_path_exists(raw_path) || !core_io_path_exists(pack_path) || !core_io_path_exists(manifest_path)) {
        ok = fail_with_message("tiny fixture artifacts missing");
        goto cleanup;
    }

    raw_density = (float *)malloc(cell_count * sizeof(float));
    raw_velx = (float *)malloc(cell_count * sizeof(float));
    raw_vely = (float *)malloc(cell_count * sizeof(float));
    raw_velz = (float *)malloc(cell_count * sizeof(float));
    raw_pressure = (float *)malloc(cell_count * sizeof(float));
    raw_solid = (uint8_t *)malloc(cell_count * sizeof(uint8_t));
    pack_density = (float *)malloc(cell_count * sizeof(float));
    pack_velx = (float *)malloc(cell_count * sizeof(float));
    pack_vely = (float *)malloc(cell_count * sizeof(float));
    pack_velz = (float *)malloc(cell_count * sizeof(float));
    pack_pressure = (float *)malloc(cell_count * sizeof(float));
    pack_solid = (uint8_t *)malloc(cell_count * sizeof(uint8_t));
    if (!raw_density || !raw_velx || !raw_vely || !raw_velz || !raw_pressure || !raw_solid ||
        !pack_density || !pack_velx || !pack_vely || !pack_velz || !pack_pressure || !pack_solid) {
        ok = fail_with_message("failed to allocate parity buffers");
        goto cleanup;
    }

    if (!read_raw_vf3d(raw_path,
                       &raw_header,
                       raw_density,
                       raw_velx,
                       raw_vely,
                       raw_velz,
                       raw_pressure,
                       raw_solid,
                       cell_count)) {
        ok = fail_with_message("failed to read raw tiny fixture");
        goto cleanup;
    }

    if (core_pack_reader_open(pack_path, &reader).code != CORE_OK) {
        ok = fail_with_message("failed to open tiny pack fixture");
        goto cleanup;
    }
    if (core_pack_reader_chunk_count(&reader) != 8u) {
        ok = fail_with_message("tiny pack chunk count mismatch");
        goto cleanup;
    }
    if (core_pack_reader_find_chunk(&reader, "VF3H", 0, &vf3h).code != CORE_OK) {
        ok = fail_with_message("tiny pack missing VF3H");
        goto cleanup;
    }
    if (vf3h.size != sizeof(pack_header) ||
        core_pack_reader_read_chunk_data(&reader, &vf3h, &pack_header, sizeof(pack_header)).code != CORE_OK) {
        ok = fail_with_message("failed to read VF3H header");
        goto cleanup;
    }
    if (!read_pack_chunk_f32(&reader, "DENS", pack_density, cell_count) ||
        !read_pack_chunk_f32(&reader, "VELX", pack_velx, cell_count) ||
        !read_pack_chunk_f32(&reader, "VELY", pack_vely, cell_count) ||
        !read_pack_chunk_f32(&reader, "VELZ", pack_velz, cell_count) ||
        !read_pack_chunk_f32(&reader, "PRES", pack_pressure, cell_count)) {
        ok = fail_with_message("failed to read float pack chunks");
        goto cleanup;
    }
    {
        CorePackChunkInfo solid_chunk = {0};
        if (core_pack_reader_find_chunk(&reader, "SOLI", 0, &solid_chunk).code != CORE_OK ||
            solid_chunk.size != (uint64_t)cell_count ||
            core_pack_reader_read_chunk_data(&reader, &solid_chunk, pack_solid, solid_chunk.size).code != CORE_OK) {
            ok = fail_with_message("failed to read solid pack chunk");
            goto cleanup;
        }
    }
    if (core_pack_reader_close(&reader).code != CORE_OK) {
        ok = fail_with_message("failed to close tiny pack fixture");
        goto cleanup;
    }
    memset(&reader, 0, sizeof(reader));

    if (raw_header.grid_w != 16u || raw_header.grid_h != 8u || raw_header.grid_d != 8u) {
        ok = fail_with_message("raw tiny header dimensions mismatch");
        goto cleanup;
    }
    if (!nearly_equal(raw_header.voxel_size, impl->volume.desc.voxel_size) ||
        !nearly_equal(raw_header.origin_x, impl->volume.desc.world_min_x) ||
        !nearly_equal(raw_header.origin_y, impl->volume.desc.world_min_y) ||
        !nearly_equal(raw_header.origin_z, impl->volume.desc.world_min_z)) {
        ok = fail_with_message("raw tiny header space mismatch");
        goto cleanup;
    }
    if (pack_header.grid_w != raw_header.grid_w ||
        pack_header.grid_h != raw_header.grid_h ||
        pack_header.grid_d != raw_header.grid_d ||
        !nearly_equal(pack_header.origin_x, raw_header.origin_x) ||
        !nearly_equal(pack_header.origin_y, raw_header.origin_y) ||
        !nearly_equal(pack_header.origin_z, raw_header.origin_z) ||
        !nearly_equal(pack_header.voxel_size, raw_header.voxel_size) ||
        !nearly_equal(pack_header.scene_up_z, raw_header.scene_up_z) ||
        pack_header.solid_mask_crc32 != raw_header.solid_mask_crc32) {
        ok = fail_with_message("VF3H header parity mismatch");
        goto cleanup;
    }

    if (memcmp(raw_density, impl->volume.density, cell_count * sizeof(float)) != 0 ||
        memcmp(raw_velx, impl->volume.velocity_x, cell_count * sizeof(float)) != 0 ||
        memcmp(raw_vely, impl->volume.velocity_y, cell_count * sizeof(float)) != 0 ||
        memcmp(raw_velz, impl->volume.velocity_z, cell_count * sizeof(float)) != 0 ||
        memcmp(raw_pressure, impl->volume.pressure, cell_count * sizeof(float)) != 0 ||
        memcmp(raw_solid, impl->obstacle_occupancy, cell_count * sizeof(uint8_t)) != 0) {
        ok = fail_with_message("raw fixture payload diverged from backend truth");
        goto cleanup;
    }
    if (memcmp(pack_density, raw_density, cell_count * sizeof(float)) != 0 ||
        memcmp(pack_velx, raw_velx, cell_count * sizeof(float)) != 0 ||
        memcmp(pack_vely, raw_vely, cell_count * sizeof(float)) != 0 ||
        memcmp(pack_velz, raw_velz, cell_count * sizeof(float)) != 0 ||
        memcmp(pack_pressure, raw_pressure, cell_count * sizeof(float)) != 0 ||
        memcmp(pack_solid, raw_solid, cell_count * sizeof(uint8_t)) != 0) {
        ok = fail_with_message("pack payload parity mismatch");
        goto cleanup;
    }

    if (count_nonzero_f32(raw_density, cell_count) != 4u ||
        count_nonzero_u8(raw_solid, cell_count) != 3u) {
        ok = fail_with_message("tiny fixture nonzero counts mismatch");
        goto cleanup;
    }

    manifest = read_json_file(manifest_path);
    pack_json = read_pack_json_chunk(pack_path);
    if (!manifest || !pack_json) {
        ok = fail_with_message("failed to parse manifest parity json");
        goto cleanup;
    }
    if (!cJSON_IsNumber(cJSON_GetObjectItem(manifest, "manifest_version")) ||
        cJSON_GetObjectItem(manifest, "manifest_version")->valuedouble != 2.0 ||
        !cJSON_IsString(cJSON_GetObjectItem(manifest, "frame_contract")) ||
        strcmp(cJSON_GetObjectItem(manifest, "frame_contract")->valuestring, "vf3d") != 0 ||
        !cJSON_IsNumber(cJSON_GetObjectItem(manifest, "grid_d")) ||
        cJSON_GetObjectItem(manifest, "grid_d")->valueint != 8 ||
        !cJSON_IsNumber(cJSON_GetObjectItem(manifest, "voxel_size")) ||
        !nearly_equal((float)cJSON_GetObjectItem(manifest, "voxel_size")->valuedouble, 0.25f) ||
        !cJSON_IsNumber(cJSON_GetObjectItem(manifest, "solid_mask_crc32")) ||
        (uint32_t)cJSON_GetObjectItem(manifest, "solid_mask_crc32")->valuedouble != raw_header.solid_mask_crc32) {
        ok = fail_with_message("manifest metadata mismatch");
        goto cleanup;
    }
    space_contract = cJSON_GetObjectItem(manifest, "space_contract");
    frames = cJSON_GetObjectItem(manifest, "frames");
    entry = cJSON_IsArray(frames) ? cJSON_GetArrayItem(frames, 0) : NULL;
    if (!cJSON_IsObject(space_contract) ||
        !cJSON_IsString(cJSON_GetObjectItem(space_contract, "axis_authority")) ||
        strcmp(cJSON_GetObjectItem(space_contract, "axis_authority")->valuestring, "xyz") != 0 ||
        !cJSON_IsNumber(cJSON_GetObjectItem(space_contract, "grid_w")) ||
        cJSON_GetObjectItem(space_contract, "grid_w")->valueint != 16 ||
        !cJSON_IsNumber(cJSON_GetObjectItem(space_contract, "grid_h")) ||
        cJSON_GetObjectItem(space_contract, "grid_h")->valueint != 8 ||
        !cJSON_IsNumber(cJSON_GetObjectItem(space_contract, "grid_d")) ||
        cJSON_GetObjectItem(space_contract, "grid_d")->valueint != 8 ||
        !cJSON_IsObject(entry) ||
        !cJSON_IsString(cJSON_GetObjectItem(entry, "frame_contract")) ||
        strcmp(cJSON_GetObjectItem(entry, "frame_contract")->valuestring, "vf3d") != 0) {
        ok = fail_with_message("manifest frame/space parity mismatch");
        goto cleanup;
    }
    if (!cJSON_IsNumber(cJSON_GetObjectItem(pack_json, "manifest_version")) ||
        cJSON_GetObjectItem(pack_json, "manifest_version")->valuedouble != cJSON_GetObjectItem(manifest, "manifest_version")->valuedouble ||
        !cJSON_IsString(cJSON_GetObjectItem(pack_json, "frame_contract")) ||
        strcmp(cJSON_GetObjectItem(pack_json, "frame_contract")->valuestring,
               cJSON_GetObjectItem(manifest, "frame_contract")->valuestring) != 0 ||
        !cJSON_IsNumber(cJSON_GetObjectItem(pack_json, "solid_mask_crc32")) ||
        (uint32_t)cJSON_GetObjectItem(pack_json, "solid_mask_crc32")->valuedouble != raw_header.solid_mask_crc32) {
        ok = fail_with_message("pack JSON parity mismatch");
        goto cleanup;
    }

    ok = true;

cleanup:
    cJSON_Delete(manifest);
    cJSON_Delete(pack_json);
    free(raw_density);
    free(raw_velx);
    free(raw_vely);
    free(raw_velz);
    free(raw_pressure);
    free(raw_solid);
    free(pack_density);
    free(pack_velx);
    free(pack_vely);
    free(pack_velz);
    free(pack_pressure);
    free(pack_solid);
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
    if (!test_tiny_vf3d_fixture_parity_matches_backend_truth()) {
        return 1;
    }
    fprintf(stdout, "volume_frames_3d_tiny_parity_contract_test: success\n");
    return 0;
}
