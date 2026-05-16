#include "app/atmospheric/atmospheric_warm_start.h"

#include "app/scene_state.h"
#include "core_pack.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct WarmStartVf3dRawHeaderV1 {
    uint32_t magic;
    uint32_t version;
    uint32_t grid_w;
    uint32_t grid_h;
    uint32_t grid_d;
    double time_seconds;
    uint64_t frame_index;
    double dt_seconds;
    float origin_x;
    float origin_y;
    float origin_z;
    float voxel_size;
    float scene_up_x;
    float scene_up_y;
    float scene_up_z;
    uint32_t solid_mask_crc32;
    uint32_t reserved[3];
} WarmStartVf3dRawHeaderV1;

typedef struct WarmStartVf3hCanonical {
    uint32_t version;
    uint32_t grid_w;
    uint32_t grid_h;
    uint32_t grid_d;
    double time_seconds;
    uint64_t frame_index;
    double dt_seconds;
    float origin_x;
    float origin_y;
    float origin_z;
    float voxel_size;
    float scene_up_x;
    float scene_up_y;
    float scene_up_z;
    uint32_t solid_mask_crc32;
} WarmStartVf3hCanonical;

static const uint32_t ATMOSPHERIC_WARM_START_VF3D_MAGIC =
    ('V' << 24) | ('F' << 16) | ('3' << 8) | ('D');
static const uint32_t ATMOSPHERIC_WARM_START_VF3D_VERSION_V1 = 1u;
static const float ATMOSPHERIC_WARM_START_DENSITY_EPSILON = 0.0001f;

static void set_error(char *error, size_t error_size, const char *message) {
    if (!error || error_size == 0u) return;
    snprintf(error, error_size, "%s", message ? message : "unknown warm-start error");
}

static void set_runtime_report_path(AtmosphericWarmStartRuntimeReport3D *report,
                                    const char *path) {
    if (!report) return;
    snprintf(report->path, sizeof(report->path), "%s", path ? path : "");
}

static void set_runtime_report_rejected(AtmosphericWarmStartRuntimeReport3D *report,
                                        const char *path,
                                        const char *reason) {
    if (!report) return;
    memset(report, 0, sizeof(*report));
    report->status = ATMOSPHERIC_WARM_START_RUNTIME_REJECTED;
    set_runtime_report_path(report, path);
    snprintf(report->rejection_reason,
             sizeof(report->rejection_reason),
             "%s",
             reason && reason[0] ? reason : "unknown warm-start rejection");
}

static bool checked_cell_count(uint32_t w,
                               uint32_t h,
                               uint32_t d,
                               size_t *out_count,
                               size_t *out_float_bytes,
                               size_t *out_solid_bytes) {
    size_t count = 0;
    if (!out_count || !out_float_bytes || !out_solid_bytes) return false;
    if (w == 0u || h == 0u || d == 0u) return false;
    if ((size_t)w > SIZE_MAX / (size_t)h) return false;
    count = (size_t)w * (size_t)h;
    if (count > SIZE_MAX / (size_t)d) return false;
    count *= (size_t)d;
    if (count > SIZE_MAX / sizeof(float)) return false;
    *out_count = count;
    *out_float_bytes = count * sizeof(float);
    *out_solid_bytes = count * sizeof(uint8_t);
    return true;
}

static bool allocate_volume_arrays(AtmosphericWarmStartVolume3D *volume,
                                   size_t float_bytes,
                                   size_t solid_bytes) {
    if (!volume) return false;
    volume->density = (float *)calloc(1u, float_bytes);
    volume->velocity_x = (float *)calloc(1u, float_bytes);
    volume->velocity_y = (float *)calloc(1u, float_bytes);
    volume->velocity_z = (float *)calloc(1u, float_bytes);
    volume->pressure = (float *)calloc(1u, float_bytes);
    volume->solid_mask = (uint8_t *)calloc(1u, solid_bytes);
    if (!volume->density ||
        !volume->velocity_x ||
        !volume->velocity_y ||
        !volume->velocity_z ||
        !volume->pressure ||
        !volume->solid_mask) {
        atmospheric_warm_start_volume_3d_free(volume);
        return false;
    }
    return true;
}

static void metadata_from_raw_header(AtmosphericWarmStartMetadata3D *metadata,
                                     const WarmStartVf3dRawHeaderV1 *header) {
    if (!metadata || !header) return;
    metadata->source_kind = ATMOSPHERIC_WARM_START_SOURCE_VF3D_RAW;
    metadata->width = (int)header->grid_w;
    metadata->height = (int)header->grid_h;
    metadata->depth = (int)header->grid_d;
    metadata->cell_count =
        (size_t)header->grid_w * (size_t)header->grid_h * (size_t)header->grid_d;
    metadata->time_seconds = header->time_seconds;
    metadata->frame_index = header->frame_index;
    metadata->dt_seconds = header->dt_seconds;
    metadata->origin_x = header->origin_x;
    metadata->origin_y = header->origin_y;
    metadata->origin_z = header->origin_z;
    metadata->voxel_size = header->voxel_size;
    metadata->scene_up_x = header->scene_up_x;
    metadata->scene_up_y = header->scene_up_y;
    metadata->scene_up_z = header->scene_up_z;
    metadata->solid_mask_crc32 = header->solid_mask_crc32;
}

static void metadata_from_pack_header(AtmosphericWarmStartMetadata3D *metadata,
                                      const WarmStartVf3hCanonical *header) {
    if (!metadata || !header) return;
    metadata->source_kind = ATMOSPHERIC_WARM_START_SOURCE_VF3H_PACK;
    metadata->width = (int)header->grid_w;
    metadata->height = (int)header->grid_h;
    metadata->depth = (int)header->grid_d;
    metadata->cell_count =
        (size_t)header->grid_w * (size_t)header->grid_h * (size_t)header->grid_d;
    metadata->time_seconds = header->time_seconds;
    metadata->frame_index = header->frame_index;
    metadata->dt_seconds = header->dt_seconds;
    metadata->origin_x = header->origin_x;
    metadata->origin_y = header->origin_y;
    metadata->origin_z = header->origin_z;
    metadata->voxel_size = header->voxel_size;
    metadata->scene_up_x = header->scene_up_x;
    metadata->scene_up_y = header->scene_up_y;
    metadata->scene_up_z = header->scene_up_z;
    metadata->solid_mask_crc32 = header->solid_mask_crc32;
}

static bool read_raw_vf3d(const char *path,
                          AtmosphericWarmStartVolume3D *out_volume,
                          char *error,
                          size_t error_size) {
    FILE *f = NULL;
    WarmStartVf3dRawHeaderV1 header = {0};
    size_t cell_count = 0u;
    size_t float_bytes = 0u;
    size_t solid_bytes = 0u;

    f = fopen(path, "rb");
    if (!f) {
        set_error(error, error_size, "failed to open vf3d warm-start file");
        return false;
    }
    if (fread(&header, sizeof(header), 1u, f) != 1u) {
        fclose(f);
        set_error(error, error_size, "failed to read vf3d warm-start header");
        return false;
    }
    if (header.magic != ATMOSPHERIC_WARM_START_VF3D_MAGIC ||
        header.version != ATMOSPHERIC_WARM_START_VF3D_VERSION_V1) {
        fclose(f);
        set_error(error, error_size, "unsupported vf3d warm-start header");
        return false;
    }
    if (!checked_cell_count(header.grid_w,
                            header.grid_h,
                            header.grid_d,
                            &cell_count,
                            &float_bytes,
                            &solid_bytes)) {
        fclose(f);
        set_error(error, error_size, "invalid vf3d warm-start dimensions");
        return false;
    }
    if (!allocate_volume_arrays(out_volume, float_bytes, solid_bytes)) {
        fclose(f);
        set_error(error, error_size, "out of memory loading vf3d warm-start");
        return false;
    }
    metadata_from_raw_header(&out_volume->metadata, &header);
    out_volume->metadata.cell_count = cell_count;

    if (fread(out_volume->density, 1u, float_bytes, f) != float_bytes ||
        fread(out_volume->velocity_x, 1u, float_bytes, f) != float_bytes ||
        fread(out_volume->velocity_y, 1u, float_bytes, f) != float_bytes ||
        fread(out_volume->velocity_z, 1u, float_bytes, f) != float_bytes ||
        fread(out_volume->pressure, 1u, float_bytes, f) != float_bytes ||
        fread(out_volume->solid_mask, 1u, solid_bytes, f) != solid_bytes) {
        fclose(f);
        atmospheric_warm_start_volume_3d_free(out_volume);
        set_error(error, error_size, "failed to read vf3d warm-start arrays");
        return false;
    }
    fclose(f);
    return true;
}

static bool read_pack_chunk(CorePackReader *reader,
                            const char type[4],
                            void *dst,
                            uint64_t expected_size,
                            char *error,
                            size_t error_size) {
    CorePackChunkInfo chunk = {0};
    if (core_pack_reader_find_chunk(reader, type, 0u, &chunk).code != CORE_OK) {
        set_error(error, error_size, "missing warm-start pack chunk");
        return false;
    }
    if (chunk.size != expected_size) {
        set_error(error, error_size, "unexpected warm-start pack chunk size");
        return false;
    }
    if (core_pack_reader_read_chunk_data(reader, &chunk, dst, expected_size).code != CORE_OK) {
        set_error(error, error_size, "failed to read warm-start pack chunk");
        return false;
    }
    return true;
}

static bool read_pack_vf3h(const char *path,
                           AtmosphericWarmStartVolume3D *out_volume,
                           char *error,
                           size_t error_size) {
    CorePackReader reader = {0};
    WarmStartVf3hCanonical header = {0};
    size_t cell_count = 0u;
    size_t float_bytes = 0u;
    size_t solid_bytes = 0u;
    bool ok = false;

    if (core_pack_reader_open(path, &reader).code != CORE_OK) {
        set_error(error, error_size, "failed to open pack warm-start file");
        return false;
    }
    if (!read_pack_chunk(&reader,
                         "VF3H",
                         &header,
                         (uint64_t)sizeof(header),
                         error,
                         error_size)) {
        goto done;
    }
    if (header.version != ATMOSPHERIC_WARM_START_VF3D_VERSION_V1 ||
        !checked_cell_count(header.grid_w,
                            header.grid_h,
                            header.grid_d,
                            &cell_count,
                            &float_bytes,
                            &solid_bytes)) {
        set_error(error, error_size, "invalid vf3h warm-start header");
        goto done;
    }
    if (!allocate_volume_arrays(out_volume, float_bytes, solid_bytes)) {
        set_error(error, error_size, "out of memory loading pack warm-start");
        goto done;
    }
    metadata_from_pack_header(&out_volume->metadata, &header);
    out_volume->metadata.cell_count = cell_count;

    if (!read_pack_chunk(&reader, "DENS", out_volume->density, (uint64_t)float_bytes, error, error_size) ||
        !read_pack_chunk(&reader, "VELX", out_volume->velocity_x, (uint64_t)float_bytes, error, error_size) ||
        !read_pack_chunk(&reader, "VELY", out_volume->velocity_y, (uint64_t)float_bytes, error, error_size) ||
        !read_pack_chunk(&reader, "VELZ", out_volume->velocity_z, (uint64_t)float_bytes, error, error_size) ||
        !read_pack_chunk(&reader, "PRES", out_volume->pressure, (uint64_t)float_bytes, error, error_size) ||
        !read_pack_chunk(&reader, "SOLI", out_volume->solid_mask, (uint64_t)solid_bytes, error, error_size)) {
        atmospheric_warm_start_volume_3d_free(out_volume);
        goto done;
    }
    ok = true;

done:
    core_pack_reader_close(&reader);
    return ok;
}

static bool path_has_extension(const char *path, const char *ext) {
    size_t path_len = 0u;
    size_t ext_len = 0u;
    if (!path || !ext) return false;
    path_len = strlen(path);
    ext_len = strlen(ext);
    if (path_len < ext_len) return false;
    return strcmp(path + path_len - ext_len, ext) == 0;
}

void atmospheric_warm_start_volume_3d_free(AtmosphericWarmStartVolume3D *volume) {
    if (!volume) return;
    free(volume->density);
    free(volume->velocity_x);
    free(volume->velocity_y);
    free(volume->velocity_z);
    free(volume->pressure);
    free(volume->solid_mask);
    memset(volume, 0, sizeof(*volume));
}

bool atmospheric_warm_start_load_3d(const char *path,
                                    AtmosphericWarmStartVolume3D *out_volume,
                                    char *error,
                                    size_t error_size) {
    if (!path || !path[0] || !out_volume) {
        set_error(error, error_size, "invalid warm-start load arguments");
        return false;
    }
    memset(out_volume, 0, sizeof(*out_volume));
    if (path_has_extension(path, ".vf3d")) {
        return read_raw_vf3d(path, out_volume, error, error_size);
    }
    if (path_has_extension(path, ".pack")) {
        return read_pack_vf3h(path, out_volume, error, error_size);
    }
    set_error(error, error_size, "unsupported warm-start file extension");
    return false;
}

AtmosphericWarmStartStats3D atmospheric_warm_start_stats_3d(
    const AtmosphericWarmStartVolume3D *volume) {
    AtmosphericWarmStartStats3D stats = {0};
    if (!volume || !volume->density || !volume->velocity_x ||
        !volume->velocity_y || !volume->velocity_z || !volume->solid_mask) {
        return stats;
    }
    stats.cell_count = volume->metadata.cell_count;
    for (size_t i = 0u; i < volume->metadata.cell_count; ++i) {
        float density = volume->density[i];
        float vx = volume->velocity_x[i];
        float vy = volume->velocity_y[i];
        float vz = volume->velocity_z[i];
        float speed = sqrtf(vx * vx + vy * vy + vz * vz);
        if (density > ATMOSPHERIC_WARM_START_DENSITY_EPSILON) {
            stats.active_density_cells++;
        }
        if (volume->solid_mask[i]) {
            stats.solid_cells++;
        }
        if (density > stats.max_density) {
            stats.max_density = density;
        }
        if (speed > stats.max_velocity_magnitude) {
            stats.max_velocity_magnitude = speed;
        }
    }
    return stats;
}

bool atmospheric_warm_start_apply_3d(SimRuntimeBackend *backend,
                                     const AtmosphericWarmStartVolume3D *volume,
                                     AtmosphericWarmStartStats3D *out_stats,
                                     char *error,
                                     size_t error_size) {
    SimRuntime3DDomainDesc desc = {0};
    AtmosphericWarmStartStats3D stats = {0};
    size_t cell_count = 0u;
    if (out_stats) memset(out_stats, 0, sizeof(*out_stats));
    if (!backend || !volume || !volume->density || !volume->velocity_x ||
        !volume->velocity_y || !volume->velocity_z || !volume->pressure ||
        !volume->solid_mask) {
        set_error(error, error_size, "invalid warm-start apply arguments");
        return false;
    }
    if (!sim_runtime_backend_get_domain_desc_3d(backend, &desc)) {
        set_error(error, error_size, "warm-start target is not a 3d backend");
        return false;
    }
    if (desc.grid_w != volume->metadata.width ||
        desc.grid_h != volume->metadata.height ||
        desc.grid_d != volume->metadata.depth ||
        desc.cell_count != volume->metadata.cell_count) {
        set_error(error, error_size, "warm-start dimensions do not match runtime domain");
        return false;
    }
    if (!sim_runtime_backend_debug_reset_volume_truth_3d(backend)) {
        set_error(error, error_size, "failed to clear runtime before warm-start apply");
        return false;
    }

    cell_count = volume->metadata.cell_count;
    for (int z = 0; z < desc.grid_d; ++z) {
        for (int y = 0; y < desc.grid_h; ++y) {
            for (int x = 0; x < desc.grid_w; ++x) {
                size_t i = (size_t)z * desc.slice_cell_count +
                           (size_t)y * (size_t)desc.grid_w +
                           (size_t)x;
                float density = volume->density[i];
                float vx = volume->velocity_x[i];
                float vy = volume->velocity_y[i];
                float vz = volume->velocity_z[i];
                float pressure = volume->pressure[i];
                uint8_t solid = volume->solid_mask[i];
                float speed = sqrtf(vx * vx + vy * vy + vz * vz);
                if (density <= ATMOSPHERIC_WARM_START_DENSITY_EPSILON &&
                    vx == 0.0f && vy == 0.0f && vz == 0.0f &&
                    pressure == 0.0f && solid == 0u) {
                    continue;
                }
                if (!sim_runtime_backend_debug_write_volume_cell_3d(backend,
                                                                     x,
                                                                     y,
                                                                     z,
                                                                     density,
                                                                     vx,
                                                                     vy,
                                                                     vz,
                                                                     pressure,
                                                                     solid)) {
                    set_error(error, error_size, "failed to write warm-start cell");
                    return false;
                }
                if (density > ATMOSPHERIC_WARM_START_DENSITY_EPSILON) {
                    stats.active_density_cells++;
                }
                if (solid) {
                    stats.solid_cells++;
                }
                if (density > stats.max_density) {
                    stats.max_density = density;
                }
                if (speed > stats.max_velocity_magnitude) {
                    stats.max_velocity_magnitude = speed;
                }
            }
        }
    }
    stats.cell_count = cell_count;
    if (out_stats) *out_stats = stats;
    if (!sim_runtime_backend_debug_note_atmospheric_warm_start_3d(
            backend,
            (int)volume->metadata.source_kind,
            volume->metadata.width,
            volume->metadata.height,
            volume->metadata.depth,
            volume->metadata.cell_count,
            stats.active_density_cells,
            stats.solid_cells,
            stats.max_density,
            stats.max_velocity_magnitude)) {
        set_error(error, error_size, "failed to record warm-start backend report");
        return false;
    }
    return true;
}

bool atmospheric_warm_start_apply_scene_3d(SceneState *scene,
                                           const char *path,
                                           AtmosphericWarmStartStats3D *out_stats,
                                           char *error,
                                           size_t error_size) {
    AtmosphericWarmStartVolume3D volume = {0};
    bool ok = false;
    AtmosphericWarmStartRuntimeReport3D *report = NULL;
    if (out_stats) memset(out_stats, 0, sizeof(*out_stats));
    if (scene) {
        report = &scene->atmospheric_warm_start;
        memset(report, 0, sizeof(*report));
        report->status = ATMOSPHERIC_WARM_START_RUNTIME_REQUESTED;
        set_runtime_report_path(report, path);
    }
    if (!scene || !scene->backend || !path || !path[0]) {
        set_error(error, error_size, "invalid scene warm-start arguments");
        set_runtime_report_rejected(report, path, "invalid scene warm-start arguments");
        return false;
    }
    if (scene->mode_route.requested_space_mode != SPACE_MODE_3D) {
        set_error(error, error_size, "scene warm-start requires 3d space mode");
        set_runtime_report_rejected(report, path, "scene warm-start requires 3d space mode");
        return false;
    }
    if (!atmospheric_warm_start_load_3d(path, &volume, error, error_size)) {
        set_runtime_report_rejected(report,
                                    path,
                                    error && error[0] ? error : "failed to load warm-start");
        return false;
    }
    ok = atmospheric_warm_start_apply_3d(scene->backend,
                                         &volume,
                                         out_stats,
                                         error,
                                         error_size);
    if (report) {
        report->metadata = volume.metadata;
        if (ok) {
            report->status = ATMOSPHERIC_WARM_START_RUNTIME_APPLIED;
            report->stats = out_stats ? *out_stats : atmospheric_warm_start_stats_3d(&volume);
            report->rejection_reason[0] = '\0';
        } else {
            report->status = ATMOSPHERIC_WARM_START_RUNTIME_REJECTED;
            snprintf(report->rejection_reason,
                     sizeof(report->rejection_reason),
                     "%s",
                     error && error[0] ? error : "failed to apply warm-start");
        }
    }
    atmospheric_warm_start_volume_3d_free(&volume);
    return ok;
}
