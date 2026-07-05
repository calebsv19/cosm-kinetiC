#include "export/water_surface_artifacts.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app/sim_runtime_3d_domain.h"
#include "app/water_object_coupling.h"
#include "cJSON.h"

enum {
    WATER_SURFACE_MANIFEST_VERSION = 1
};

static const float WATER_DENSITY_THRESHOLD = 0.5f;
static const float WATER_IOR_DEFAULT = 1.333f;
static const float WATER_ABSORPTION_R_DEFAULT = 0.10f;
static const float WATER_ABSORPTION_G_DEFAULT = 0.035f;
static const float WATER_ABSORPTION_B_DEFAULT = 0.015f;
static const float WATER_ABSORPTION_DISTANCE_M_DEFAULT = 4.0f;
static const float WATER_REVIEW_RIPPLE_AMPLITUDE_DEFAULT_M = 0.025f;

typedef struct WaterSurfaceFrameData {
    float *heights_y;
    float *normals_xyz;
    WaterSurfaceArtifactStatsV1 stats;
} WaterSurfaceFrameData;

static const char *path_basename_or_self(const char *path) {
    const char *last_slash = NULL;
    if (!path || !path[0]) return path;
    last_slash = strrchr(path, '/');
    if (last_slash && last_slash[1]) return last_slash + 1;
    return path;
}

static bool write_text_file(const char *path, const char *text) {
    FILE *f = NULL;
    size_t len = 0u;
    if (!path || !path[0] || !text) return false;
    f = fopen(path, "wb");
    if (!f) return false;
    len = strlen(text);
    if (fwrite(text, 1, len, f) != len) {
        fclose(f);
        return false;
    }
    fclose(f);
    return true;
}

static cJSON *read_json_file(const char *path) {
    FILE *f = NULL;
    long sz = 0;
    char *buf = NULL;
    cJSON *root = NULL;
    if (!path || !path[0]) return NULL;
    f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0) {
        fclose(f);
        return NULL;
    }
    buf = (char *)malloc((size_t)sz + 1u);
    if (!buf) {
        fclose(f);
        return NULL;
    }
    if (fread(buf, 1, (size_t)sz, f) != (size_t)sz) {
        fclose(f);
        free(buf);
        return NULL;
    }
    fclose(f);
    buf[sz] = '\0';
    root = cJSON_Parse(buf);
    free(buf);
    return root;
}

static void json_set_number(cJSON *obj, const char *name, double value) {
    if (!obj || !name) return;
    cJSON_DeleteItemFromObject(obj, name);
    cJSON_AddNumberToObject(obj, name, value);
}

static void json_set_string(cJSON *obj, const char *name, const char *value) {
    if (!obj || !name || !value) return;
    cJSON_DeleteItemFromObject(obj, name);
    cJSON_AddStringToObject(obj, name, value);
}

static cJSON *json_get_or_create_object(cJSON *parent, const char *name) {
    cJSON *obj = NULL;
    if (!parent || !name) return NULL;
    obj = cJSON_GetObjectItem(parent, name);
    if (!cJSON_IsObject(obj)) {
        cJSON_DeleteItemFromObject(parent, name);
        obj = cJSON_CreateObject();
        if (obj) cJSON_AddItemToObject(parent, name, obj);
    }
    return cJSON_IsObject(obj) ? obj : NULL;
}

static cJSON *json_get_or_create_array(cJSON *parent, const char *name) {
    cJSON *arr = NULL;
    if (!parent || !name) return NULL;
    arr = cJSON_GetObjectItem(parent, name);
    if (!cJSON_IsArray(arr)) {
        cJSON_DeleteItemFromObject(parent, name);
        arr = cJSON_CreateArray();
        if (arr) cJSON_AddItemToObject(parent, name, arr);
    }
    return cJSON_IsArray(arr) ? arr : NULL;
}

static bool add_float_array(cJSON *root, const char *name, const float *values, size_t count) {
    cJSON *arr = NULL;
    if (!root || !name || !values) return false;
    arr = cJSON_CreateArray();
    if (!arr) return false;
    for (size_t i = 0; i < count; ++i) {
        cJSON *value = cJSON_CreateNumber((double)values[i]);
        if (!value) {
            cJSON_Delete(arr);
            return false;
        }
        cJSON_AddItemToArray(arr, value);
    }
    cJSON_AddItemToObject(root, name, arr);
    return true;
}

static float clamp_surface_height(const SceneFluidVolumeExportView3D *volume, float height_y) {
    float min_y = volume->origin_y;
    float max_y = volume->origin_y + (float)volume->height * volume->voxel_size;
    if (!isfinite(height_y)) return min_y;
    if (height_y < min_y) return min_y;
    if (height_y > max_y) return max_y;
    return height_y;
}

static size_t water_surface_column_index(const SceneFluidVolumeExportView3D *volume,
                                         int x,
                                         int z) {
    return (size_t)z * (size_t)volume->width + (size_t)x;
}

static float water_surface_review_ripple_amplitude(const SceneState *scene,
                                                   const SceneFluidVolumeExportView3D *volume) {
    float amplitude = WATER_REVIEW_RIPPLE_AMPLITUDE_DEFAULT_M;
    if (scene && scene->config && scene->config->water_review_ripple_amplitude_m > 0.0f) {
        amplitude = scene->config->water_review_ripple_amplitude_m;
    }
    if (volume && volume->voxel_size > 0.0f && amplitude > 0.45f * volume->voxel_size) {
        amplitude = 0.45f * volume->voxel_size;
    }
    return amplitude;
}

static float water_surface_review_ripple_delta(const SceneFluidVolumeExportView3D *volume,
                                               const VolumeFrameHeaderVf3dV1 *volume_header,
                                               int x,
                                               int z,
                                               float amplitude_m) {
    float sample_x = 0.0f;
    float sample_z = 0.0f;
    float span_x = 1.0f;
    float span_z = 1.0f;
    float center_x = 0.0f;
    float center_z = 0.0f;
    float local_x = 0.0f;
    float local_z = 0.0f;
    float radius = 0.0f;
    float t = volume_header ? (float)volume_header->time_seconds : 0.0f;
    float wave = 0.0f;
    float ring = 0.0f;
    float dimple_a = 0.0f;
    float dimple_b = 0.0f;
    float detail = 0.0f;
    float delta = 0.0f;

    if (!volume || volume->voxel_size <= 0.0f || amplitude_m <= 0.0f) return 0.0f;
    span_x = (float)volume->width * volume->voxel_size;
    span_z = (float)volume->depth * volume->voxel_size;
    sample_x = volume->origin_x + ((float)x + 0.5f) * volume->voxel_size;
    sample_z = volume->origin_z + ((float)z + 0.5f) * volume->voxel_size;
    center_x = volume->origin_x + 0.5f * span_x;
    center_z = volume->origin_z + 0.5f * span_z;
    local_x = span_x > 0.0f ? (sample_x - center_x) / span_x : 0.0f;
    local_z = span_z > 0.0f ? (sample_z - center_z) / span_z : 0.0f;
    radius = sqrtf(local_x * local_x + local_z * local_z);

    wave = 0.52f * sinf((sample_x * 4.1f) + (sample_z * 2.7f) + (t * 1.3f)) +
           0.31f * sinf((sample_x * -2.9f) + (sample_z * 5.4f) + (t * 0.9f));
    ring = 0.48f * sinf((radius * 44.0f) - (t * 2.1f)) * expf(-radius * radius * 5.0f);
    dimple_a = -0.78f * expf(-(((local_x - 0.18f) * (local_x - 0.18f)) / 0.018f +
                                ((local_z + 0.13f) * (local_z + 0.13f)) / 0.030f));
    dimple_b = -0.55f * expf(-(((local_x + 0.24f) * (local_x + 0.24f)) / 0.026f +
                                ((local_z - 0.19f) * (local_z - 0.19f)) / 0.022f));
    detail = 0.14f * sinf(((float)x * 0.73f) + ((float)z * 1.21f) + (t * 0.4f));
    delta = amplitude_m * (wave + ring + dimple_a + dimple_b + detail);
    if (delta > amplitude_m) delta = amplitude_m;
    if (delta < -amplitude_m) delta = -amplitude_m;
    return delta;
}

static bool extract_heightfield(const SceneState *scene,
                                const SceneFluidVolumeExportView3D *volume,
                                const VolumeFrameHeaderVf3dV1 *volume_header,
                                WaterSurfaceFrameData *out_frame) {
    WaterSurfaceArtifactStatsV1 stats = {0};
    size_t sample_count = 0u;
    float sum_y = 0.0f;
    bool minmax_initialized = false;
    bool review_ripples = scene && scene->config && scene->config->water_review_ripples;
    float review_amplitude_m = water_surface_review_ripple_amplitude(scene, volume);
    bool review_delta_initialized = false;
    WaterObjectCouplingGridBounds object_bounds = {0};
    bool object_coupling =
        scene && scene->config &&
        water_object_coupling_grid_bounds(scene->config, volume, &object_bounds);
    bool object_delta_initialized = false;
    float object_delta_sq_sum = 0.0f;
    size_t object_zone_sample_count = 0u;
    float object_zone_height_sum = 0.0f;
    float object_zone_height_sq_sum = 0.0f;
    bool object_zone_height_initialized = false;

    if (!volume || !out_frame || !volume->density || !volume->solid_mask) return false;
    if (volume->width <= 0 || volume->height <= 0 || volume->depth <= 0 ||
        volume->voxel_size <= 0.0f) {
        return false;
    }
    sample_count = (size_t)volume->width * (size_t)volume->depth;
    out_frame->heights_y = (float *)calloc(sample_count, sizeof(float));
    out_frame->normals_xyz = (float *)calloc(sample_count * 3u, sizeof(float));
    if (!out_frame->heights_y || !out_frame->normals_xyz) return false;

    stats.grid_w = volume->width;
    stats.grid_d = volume->depth;
    stats.sample_count = sample_count;
    stats.finite_normals = true;
    stats.review_ripple_amplitude_m = review_ripples ? review_amplitude_m : 0.0f;

    for (int z = 0; z < volume->depth; ++z) {
        for (int x = 0; x < volume->width; ++x) {
            int highest_water_y = -1;
            int column_solid_cells = 0;
            float height_y = volume->origin_y;
            size_t surface_idx = water_surface_column_index(volume, x, z);
            for (int y = 0; y < volume->height; ++y) {
                SimRuntime3DDomainDesc desc = {
                    .grid_w = volume->width,
                    .grid_h = volume->height,
                    .grid_d = volume->depth,
                    .slice_cell_count = (size_t)volume->width * (size_t)volume->height,
                };
                size_t idx = sim_runtime_3d_volume_index(&desc, x, y, z);
                if (volume->solid_mask[idx] != 0u) {
                    column_solid_cells++;
                    continue;
                }
                if (volume->density[idx] >= WATER_DENSITY_THRESHOLD) {
                    highest_water_y = y;
                    stats.water_cells++;
                }
            }
            if (column_solid_cells >= volume->height) stats.solid_columns++;
            if (highest_water_y >= 0) {
                height_y = volume->origin_y + ((float)highest_water_y + 1.0f) * volume->voxel_size;
                stats.wet_columns++;
                if (review_ripples) {
                    float delta_m = water_surface_review_ripple_delta(volume,
                                                                      volume_header,
                                                                      x,
                                                                      z,
                                                                      review_amplitude_m);
                    height_y += delta_m;
                    stats.review_ripples_applied = true;
                    if (!review_delta_initialized) {
                        stats.review_ripple_delta_min_m = delta_m;
                        stats.review_ripple_delta_max_m = delta_m;
                        review_delta_initialized = true;
                    } else {
                        if (delta_m < stats.review_ripple_delta_min_m) {
                            stats.review_ripple_delta_min_m = delta_m;
                        }
                        if (delta_m > stats.review_ripple_delta_max_m) {
                            stats.review_ripple_delta_max_m = delta_m;
                        }
                    }
                }
                if (object_coupling) {
                    float weight = 0.0f;
                    float delta_m = 0.0f;
                    bool was_capped = false;
                    if (stats.object_coupling.displaced_volume_m3 <= 0.0f) {
                        water_object_coupling_accumulate_diagnostics(scene->config,
                                                                     volume,
                                                                     volume_header,
                                                                     &stats.object_coupling);
                    }
                    delta_m = water_object_coupling_surface_delta(
                        volume,
                        volume_header,
                        &object_bounds,
                        x,
                        z,
                        stats.object_coupling.displaced_volume_m3,
                        &weight,
                        &was_capped);
                    if (weight > 0.05f) {
                        float abs_delta_m = fabsf(delta_m);
                        stats.object_coupling.displacement_sample_count++;
                        stats.object_coupling.displacement_weight_sum += weight;
                        if (weight > stats.object_coupling.displacement_weight_max) {
                            stats.object_coupling.displacement_weight_max = weight;
                        }
                        stats.object_coupling.displacement_delta_sum_m += delta_m;
                        stats.object_coupling.displacement_delta_abs_sum_m += abs_delta_m;
                        object_delta_sq_sum += delta_m * delta_m;
                        if (was_capped) {
                            stats.object_coupling.displacement_capped_sample_count++;
                        }
                        if (!object_delta_initialized) {
                            stats.object_coupling.displacement_delta_min_m = delta_m;
                            stats.object_coupling.displacement_delta_max_m = delta_m;
                            object_delta_initialized = true;
                        } else {
                            if (delta_m < stats.object_coupling.displacement_delta_min_m) {
                                stats.object_coupling.displacement_delta_min_m = delta_m;
                            }
                            if (delta_m > stats.object_coupling.displacement_delta_max_m) {
                                stats.object_coupling.displacement_delta_max_m = delta_m;
                            }
                        }
                        if (delta_m != 0.0f) {
                            height_y += delta_m;
                            stats.object_coupling.displacement_applied = true;
                        }
                    }
                }
            } else {
                stats.dry_columns++;
            }
            height_y = clamp_surface_height(volume, height_y);
            out_frame->heights_y[surface_idx] = height_y;
            if (!minmax_initialized) {
                stats.surface_min_y = height_y;
                stats.surface_max_y = height_y;
                minmax_initialized = true;
            } else {
                if (height_y < stats.surface_min_y) stats.surface_min_y = height_y;
                if (height_y > stats.surface_max_y) stats.surface_max_y = height_y;
            }
            sum_y += height_y;
        }
    }

    stats.surface_avg_y = sample_count > 0u ? sum_y / (float)sample_count : volume->origin_y;
    if (object_coupling && !stats.object_coupling.fixture_active) {
        water_object_coupling_accumulate_diagnostics(scene->config,
                                                     volume,
                                                     volume_header,
                                                     &stats.object_coupling);
    }
    if (stats.object_coupling.displacement_sample_count > 0u) {
        stats.object_coupling.displacement_delta_rms_m =
            sqrtf(object_delta_sq_sum /
                  (float)stats.object_coupling.displacement_sample_count);
    }

    for (int z = 0; z < volume->depth; ++z) {
        for (int x = 0; x < volume->width; ++x) {
            int lx = x > 0 ? x - 1 : x;
            int rx = x + 1 < volume->width ? x + 1 : x;
            int fz = z > 0 ? z - 1 : z;
            int bz = z + 1 < volume->depth ? z + 1 : z;
            size_t idx = water_surface_column_index(volume, x, z);
            size_t left_idx = water_surface_column_index(volume, lx, z);
            size_t right_idx = water_surface_column_index(volume, rx, z);
            size_t front_idx = water_surface_column_index(volume, x, fz);
            size_t back_idx = water_surface_column_index(volume, x, bz);
            float dx = (float)(rx - lx) * volume->voxel_size;
            float dz = (float)(bz - fz) * volume->voxel_size;
            float dhdx = dx > 0.0f
                             ? (out_frame->heights_y[right_idx] - out_frame->heights_y[left_idx]) / dx
                             : 0.0f;
            float dhdz = dz > 0.0f
                             ? (out_frame->heights_y[back_idx] - out_frame->heights_y[front_idx]) / dz
                             : 0.0f;
            float nx = -dhdx;
            float ny = 1.0f;
            float nz = -dhdz;
            float len = sqrtf(nx * nx + ny * ny + nz * nz);
            float slope = sqrtf(dhdx * dhdx + dhdz * dhdz);
            if (slope > stats.max_slope) stats.max_slope = slope;
            if (object_coupling &&
                stats.object_coupling.displaced_volume_m3 > 0.0f) {
                float zone_weight = 0.0f;
                bool zone_capped = false;
                (void)water_object_coupling_surface_delta(
                    volume,
                    volume_header,
                    &object_bounds,
                    x,
                    z,
                    stats.object_coupling.displaced_volume_m3,
                    &zone_weight,
                    &zone_capped);
                if (zone_weight > 0.05f) {
                    float h = out_frame->heights_y[idx];
                    float wet_threshold_y = volume->origin_y + 0.5f * volume->voxel_size;
                    if (h <= volume->origin_y + 0.5f * volume->voxel_size) {
                        continue;
                    }
                    object_zone_sample_count++;
                    object_zone_height_sum += h;
                    object_zone_height_sq_sum += h * h;
                    if (!object_zone_height_initialized) {
                        stats.object_coupling.object_zone_height_min_y = h;
                        stats.object_coupling.object_zone_height_max_y = h;
                        object_zone_height_initialized = true;
                    } else {
                        if (h < stats.object_coupling.object_zone_height_min_y) {
                            stats.object_coupling.object_zone_height_min_y = h;
                        }
                        if (h > stats.object_coupling.object_zone_height_max_y) {
                            stats.object_coupling.object_zone_height_max_y = h;
                        }
                    }
                    if (out_frame->heights_y[left_idx] > wet_threshold_y &&
                        out_frame->heights_y[right_idx] > wet_threshold_y &&
                        out_frame->heights_y[front_idx] > wet_threshold_y &&
                        out_frame->heights_y[back_idx] > wet_threshold_y &&
                        slope > stats.object_coupling.object_zone_max_slope) {
                        stats.object_coupling.object_zone_max_slope = slope;
                    }
                }
            }
            if (!isfinite(len) || len <= 0.0f) {
                nx = 0.0f;
                ny = 1.0f;
                nz = 0.0f;
                stats.finite_normals = false;
            } else {
                nx /= len;
                ny /= len;
                nz /= len;
            }
            out_frame->normals_xyz[idx * 3u + 0u] = nx;
            out_frame->normals_xyz[idx * 3u + 1u] = ny;
            out_frame->normals_xyz[idx * 3u + 2u] = nz;
        }
    }
    if (object_zone_sample_count > 0u) {
        float mean = object_zone_height_sum / (float)object_zone_sample_count;
        float variance =
            (object_zone_height_sq_sum / (float)object_zone_sample_count) -
            (mean * mean);
        if (variance < 0.0f && variance > -0.000001f) variance = 0.0f;
        stats.object_coupling.object_zone_height_avg_y = mean;
        stats.object_coupling.object_zone_height_stddev_m =
            variance > 0.0f ? sqrtf(variance) : 0.0f;
    }

    out_frame->stats = stats;
    return true;
}

static void free_frame_data(WaterSurfaceFrameData *frame) {
    if (!frame) return;
    free(frame->heights_y);
    free(frame->normals_xyz);
    memset(frame, 0, sizeof(*frame));
}

static void add_summary_object(cJSON *parent, const WaterSurfaceArtifactStatsV1 *stats) {
    cJSON *summary = NULL;
    cJSON *object = NULL;
    if (!parent || !stats) return;
    summary = cJSON_CreateObject();
    if (!summary) return;
    cJSON_AddNumberToObject(summary, "wet_columns", (double)stats->wet_columns);
    cJSON_AddNumberToObject(summary, "dry_columns", (double)stats->dry_columns);
    cJSON_AddNumberToObject(summary, "solid_columns", (double)stats->solid_columns);
    cJSON_AddNumberToObject(summary, "water_cells", (double)stats->water_cells);
    cJSON_AddNumberToObject(summary, "surface_min_y", (double)stats->surface_min_y);
    cJSON_AddNumberToObject(summary, "surface_max_y", (double)stats->surface_max_y);
    cJSON_AddNumberToObject(summary, "surface_avg_y", (double)stats->surface_avg_y);
    cJSON_AddNumberToObject(summary, "max_slope", (double)stats->max_slope);
    cJSON_AddBoolToObject(summary, "review_ripples_applied", stats->review_ripples_applied ? 1 : 0);
    cJSON_AddNumberToObject(summary,
                            "review_ripple_amplitude_m",
                            (double)stats->review_ripple_amplitude_m);
    cJSON_AddNumberToObject(summary,
                            "review_ripple_delta_min_m",
                            (double)stats->review_ripple_delta_min_m);
    cJSON_AddNumberToObject(summary,
                            "review_ripple_delta_max_m",
                            (double)stats->review_ripple_delta_max_m);
    object = cJSON_CreateObject();
    if (object) {
        cJSON_AddBoolToObject(object, "enabled", stats->object_coupling.enabled ? 1 : 0);
        cJSON_AddBoolToObject(object,
                              "fixture_active",
                              stats->object_coupling.fixture_active ? 1 : 0);
        cJSON_AddStringToObject(object,
                                "fixture_id",
                                stats->object_coupling.enabled
                                    ? "water_pool_submerged_solid"
                                    : "none");
        cJSON_AddNumberToObject(object,
                                "object_solid_cells",
                                (double)stats->object_coupling.object_solid_cells);
        cJSON_AddNumberToObject(object,
                                "object_footprint_columns",
                                (double)stats->object_coupling.object_footprint_columns);
        cJSON_AddNumberToObject(object,
                                "object_wet_overlap_cells",
                                (double)stats->object_coupling.object_wet_overlap_cells);
        cJSON_AddNumberToObject(object,
                                "displaced_volume_m3",
                                (double)stats->object_coupling.displaced_volume_m3);
        cJSON_AddBoolToObject(object,
                              "displacement_applied",
                              stats->object_coupling.displacement_applied ? 1 : 0);
        cJSON_AddNumberToObject(object,
                                "displacement_delta_min_m",
                                (double)stats->object_coupling.displacement_delta_min_m);
        cJSON_AddNumberToObject(object,
                                "displacement_delta_max_m",
                                (double)stats->object_coupling.displacement_delta_max_m);
        cJSON_AddNumberToObject(object,
                                "displacement_delta_sum_m",
                                (double)stats->object_coupling.displacement_delta_sum_m);
        cJSON_AddNumberToObject(object,
                                "displacement_delta_abs_sum_m",
                                (double)stats->object_coupling.displacement_delta_abs_sum_m);
        cJSON_AddNumberToObject(object,
                                "displacement_delta_rms_m",
                                (double)stats->object_coupling.displacement_delta_rms_m);
        cJSON_AddNumberToObject(object,
                                "displacement_sample_count",
                                (double)stats->object_coupling.displacement_sample_count);
        cJSON_AddNumberToObject(object,
                                "displacement_capped_sample_count",
                                (double)stats->object_coupling.displacement_capped_sample_count);
        cJSON_AddNumberToObject(object,
                                "displacement_weight_sum",
                                (double)stats->object_coupling.displacement_weight_sum);
        cJSON_AddNumberToObject(object,
                                "displacement_weight_max",
                                (double)stats->object_coupling.displacement_weight_max);
        cJSON_AddNumberToObject(object,
                                "object_zone_height_min_y",
                                (double)stats->object_coupling.object_zone_height_min_y);
        cJSON_AddNumberToObject(object,
                                "object_zone_height_max_y",
                                (double)stats->object_coupling.object_zone_height_max_y);
        cJSON_AddNumberToObject(object,
                                "object_zone_height_avg_y",
                                (double)stats->object_coupling.object_zone_height_avg_y);
        cJSON_AddNumberToObject(object,
                                "object_zone_height_stddev_m",
                                (double)stats->object_coupling.object_zone_height_stddev_m);
        cJSON_AddNumberToObject(object,
                                "object_zone_max_slope",
                                (double)stats->object_coupling.object_zone_max_slope);
        cJSON_AddNumberToObject(object,
                                "affected_min_x",
                                (double)stats->object_coupling.affected_min_x);
        cJSON_AddNumberToObject(object,
                                "affected_max_x",
                                (double)stats->object_coupling.affected_max_x);
        cJSON_AddNumberToObject(object,
                                "affected_min_z",
                                (double)stats->object_coupling.affected_min_z);
        cJSON_AddNumberToObject(object,
                                "affected_max_z",
                                (double)stats->object_coupling.affected_max_z);
        cJSON_AddItemToObject(summary, "object_coupling", object);
    }
    cJSON_AddBoolToObject(summary, "finite_normals", stats->finite_normals ? 1 : 0);
    cJSON_AddItemToObject(parent, "summary", summary);
}

static bool write_surface_frame_json(const SceneFluidVolumeExportView3D *volume,
                                     const VolumeFrameHeaderVf3dV1 *volume_header,
                                     const WaterSurfaceFrameData *frame,
                                     const char *surface_path) {
    cJSON *root = NULL;
    char *text = NULL;
    bool ok = false;
    if (!volume || !volume_header || !frame || !surface_path || !surface_path[0]) return false;

    root = cJSON_CreateObject();
    if (!root) return false;

    cJSON_AddStringToObject(root, "schema", "physics_sim_water_surface_heightfield_v1");
    cJSON_AddNumberToObject(root, "version", WATER_SURFACE_MANIFEST_VERSION);
    cJSON_AddNumberToObject(root, "frame_index", (double)volume_header->frame_index);
    cJSON_AddNumberToObject(root, "time_seconds", volume_header->time_seconds);
    cJSON_AddNumberToObject(root, "dt_seconds", volume_header->dt_seconds);
    cJSON_AddStringToObject(root, "surface_representation", "heightfield");
    cJSON_AddStringToObject(root, "layout", "row_major_z_x");
    cJSON_AddStringToObject(root, "surface_axis", "y");
    cJSON_AddStringToObject(root, "height_units", "meters");
    cJSON_AddNumberToObject(root, "grid_w", volume->width);
    cJSON_AddNumberToObject(root, "grid_d", volume->depth);
    cJSON_AddNumberToObject(root, "sample_count", (double)frame->stats.sample_count);
    cJSON_AddNumberToObject(root, "volume_grid_w", volume->width);
    cJSON_AddNumberToObject(root, "volume_grid_h", volume->height);
    cJSON_AddNumberToObject(root, "volume_grid_d", volume->depth);
    cJSON_AddNumberToObject(root, "origin_x", (double)volume->origin_x);
    cJSON_AddNumberToObject(root, "origin_y", (double)volume->origin_y);
    cJSON_AddNumberToObject(root, "origin_z", (double)volume->origin_z);
    cJSON_AddNumberToObject(root, "sample_origin_x", (double)(volume->origin_x + 0.5f * volume->voxel_size));
    cJSON_AddNumberToObject(root, "sample_origin_z", (double)(volume->origin_z + 0.5f * volume->voxel_size));
    cJSON_AddNumberToObject(root, "sample_spacing_x", (double)volume->voxel_size);
    cJSON_AddNumberToObject(root, "sample_spacing_z", (double)volume->voxel_size);
    cJSON_AddNumberToObject(root, "density_threshold", (double)WATER_DENSITY_THRESHOLD);
    add_summary_object(root, &frame->stats);
    if (!add_float_array(root, "heights_y", frame->heights_y, frame->stats.sample_count) ||
        !add_float_array(root, "normals_xyz", frame->normals_xyz, frame->stats.sample_count * 3u)) {
        cJSON_Delete(root);
        return false;
    }

    text = cJSON_Print(root);
    cJSON_Delete(root);
    if (!text) return false;
    ok = write_text_file(surface_path, text);
    free(text);
    return ok;
}

static cJSON *open_or_create_water_manifest(const SceneState *scene,
                                            const VolumeFrameHeaderVf3dV1 *volume_header,
                                            const char *manifest_path) {
    cJSON *root = read_json_file(manifest_path);
    cJSON *material = NULL;
    cJSON *absorption = NULL;
    cJSON *review = NULL;
    cJSON *object = NULL;
    if (!root) root = cJSON_CreateObject();
    if (!root) return NULL;

    json_set_string(root, "schema", "physics_sim_water_manifest_v1");
    json_set_number(root, "version", WATER_SURFACE_MANIFEST_VERSION);
    json_set_string(root, "mode", "water");
    json_set_string(root, "surface_representation", "heightfield");
    json_set_string(root, "surface_axis", "y");
    json_set_string(root, "height_units", "meters");
    json_set_string(root, "frame_contract", "water_surface_heightfield_v1");
    json_set_number(root, "grid_w", volume_header->grid_w);
    json_set_number(root, "grid_d", volume_header->grid_d);
    json_set_number(root, "volume_grid_w", volume_header->grid_w);
    json_set_number(root, "volume_grid_h", volume_header->grid_h);
    json_set_number(root, "volume_grid_d", volume_header->grid_d);
    json_set_number(root, "origin_x", volume_header->origin_x);
    json_set_number(root, "origin_y", volume_header->origin_y);
    json_set_number(root, "origin_z", volume_header->origin_z);
    json_set_number(root, "voxel_size", volume_header->voxel_size);
    json_set_number(root, "scene_up_x", volume_header->scene_up_x);
    json_set_number(root, "scene_up_y", volume_header->scene_up_y);
    json_set_number(root, "scene_up_z", volume_header->scene_up_z);
    json_set_number(root, "density_threshold", WATER_DENSITY_THRESHOLD);
    if (scene && scene->preset && scene->preset->name) {
        json_set_string(root, "preset", scene->preset->name);
    }
    if (scene && scene->config) {
        json_set_number(root, "configured_water_level", scene->config->water_level);
        review = json_get_or_create_object(root, "review_surface");
        if (review) {
            cJSON_DeleteItemFromObject(review, "mode");
            cJSON_DeleteItemFromObject(review, "enabled");
            cJSON_DeleteItemFromObject(review, "configured_amplitude_m");
            cJSON_AddStringToObject(review,
                                    "mode",
                                    scene->config->water_review_ripples
                                        ? "deterministic_headless_ripples"
                                        : "none");
            cJSON_AddBoolToObject(review,
                                  "enabled",
                                  scene->config->water_review_ripples ? 1 : 0);
            cJSON_AddNumberToObject(review,
                                    "configured_amplitude_m",
                                    (double)scene->config->water_review_ripple_amplitude_m);
        }
        object = json_get_or_create_object(root, "object_coupling");
        if (object) {
            cJSON_DeleteItemFromObject(object, "enabled");
            cJSON_DeleteItemFromObject(object, "fixture_id");
            cJSON_DeleteItemFromObject(object, "response_mode");
            cJSON_AddBoolToObject(object,
                                  "enabled",
                                  scene->config->water_object_fixture ? 1 : 0);
            cJSON_AddStringToObject(object,
                                    "fixture_id",
                                    scene->config->water_object_fixture
                                        ? "water_pool_submerged_solid"
                                        : "none");
            cJSON_AddStringToObject(object,
                                    "response_mode",
                                    scene->config->water_object_fixture
                                        ? "solid_mask_plus_smoothed_export_sidecar_displacement_deterministic_wake"
                                        : "none");
        }
    }

    material = json_get_or_create_object(root, "material");
    if (material) {
        json_set_number(material, "ior", WATER_IOR_DEFAULT);
        json_set_number(material, "absorption_distance_m", WATER_ABSORPTION_DISTANCE_M_DEFAULT);
        absorption = cJSON_CreateArray();
        if (absorption) {
            cJSON_AddItemToArray(absorption, cJSON_CreateNumber(WATER_ABSORPTION_R_DEFAULT));
            cJSON_AddItemToArray(absorption, cJSON_CreateNumber(WATER_ABSORPTION_G_DEFAULT));
            cJSON_AddItemToArray(absorption, cJSON_CreateNumber(WATER_ABSORPTION_B_DEFAULT));
            cJSON_DeleteItemFromObject(material, "absorption_rgb");
            cJSON_AddItemToObject(material, "absorption_rgb", absorption);
        }
    }

    if (!json_get_or_create_array(root, "frames")) {
        cJSON_Delete(root);
        return NULL;
    }
    return root;
}

static bool append_water_manifest_frame(const SceneState *scene,
                                        const VolumeFrameHeaderVf3dV1 *volume_header,
                                        const WaterSurfaceFrameData *frame,
                                        const char *run_dir,
                                        const char *surface_path) {
    char manifest_path[512];
    cJSON *root = NULL;
    cJSON *frames = NULL;
    cJSON *entry = NULL;
    char *text = NULL;
    bool ok = false;

    if (!volume_header || !frame || !run_dir || !run_dir[0] ||
        !surface_path || !surface_path[0]) {
        return false;
    }
    snprintf(manifest_path, sizeof(manifest_path), "%s/water_manifest_v1.json", run_dir);
    root = open_or_create_water_manifest(scene, volume_header, manifest_path);
    if (!root) return false;
    frames = json_get_or_create_array(root, "frames");
    if (!frames) {
        cJSON_Delete(root);
        return false;
    }

    entry = cJSON_CreateObject();
    if (!entry) {
        cJSON_Delete(root);
        return false;
    }
    cJSON_AddNumberToObject(entry, "frame_index", (double)volume_header->frame_index);
    cJSON_AddNumberToObject(entry, "time_seconds", volume_header->time_seconds);
    cJSON_AddNumberToObject(entry, "dt_seconds", volume_header->dt_seconds);
    cJSON_AddStringToObject(entry, "path", path_basename_or_self(surface_path));
    cJSON_AddStringToObject(entry, "frame_contract", "water_surface_heightfield_v1");
    cJSON_AddNumberToObject(entry, "surface_min_y", (double)frame->stats.surface_min_y);
    cJSON_AddNumberToObject(entry, "surface_max_y", (double)frame->stats.surface_max_y);
    cJSON_AddNumberToObject(entry, "surface_avg_y", (double)frame->stats.surface_avg_y);
    cJSON_AddNumberToObject(entry, "wet_columns", (double)frame->stats.wet_columns);
    cJSON_AddNumberToObject(entry, "dry_columns", (double)frame->stats.dry_columns);
    cJSON_AddNumberToObject(entry, "solid_columns", (double)frame->stats.solid_columns);
    cJSON_AddNumberToObject(entry, "water_cells", (double)frame->stats.water_cells);
    cJSON_AddBoolToObject(entry,
                          "review_ripples_applied",
                          frame->stats.review_ripples_applied ? 1 : 0);
    cJSON_AddNumberToObject(entry,
                            "review_ripple_amplitude_m",
                            (double)frame->stats.review_ripple_amplitude_m);
    cJSON_AddNumberToObject(entry,
                            "review_ripple_delta_min_m",
                            (double)frame->stats.review_ripple_delta_min_m);
    cJSON_AddNumberToObject(entry,
                            "review_ripple_delta_max_m",
                            (double)frame->stats.review_ripple_delta_max_m);
    cJSON_AddNumberToObject(entry,
                            "object_solid_cells",
                            (double)frame->stats.object_coupling.object_solid_cells);
    cJSON_AddNumberToObject(entry,
                            "object_footprint_columns",
                            (double)frame->stats.object_coupling.object_footprint_columns);
    cJSON_AddNumberToObject(entry,
                            "object_wet_overlap_cells",
                            (double)frame->stats.object_coupling.object_wet_overlap_cells);
    cJSON_AddNumberToObject(entry,
                            "displaced_volume_m3",
                            (double)frame->stats.object_coupling.displaced_volume_m3);
    cJSON_AddBoolToObject(entry,
                          "object_displacement_applied",
                          frame->stats.object_coupling.displacement_applied ? 1 : 0);
    cJSON_AddNumberToObject(entry,
                            "object_displacement_sample_count",
                            (double)frame->stats.object_coupling.displacement_sample_count);
    cJSON_AddNumberToObject(entry,
                            "object_displacement_capped_sample_count",
                            (double)frame->stats.object_coupling.displacement_capped_sample_count);
    cJSON_AddNumberToObject(entry,
                            "object_displacement_delta_rms_m",
                            (double)frame->stats.object_coupling.displacement_delta_rms_m);
    cJSON_AddNumberToObject(entry,
                            "object_displacement_delta_abs_sum_m",
                            (double)frame->stats.object_coupling.displacement_delta_abs_sum_m);
    cJSON_AddNumberToObject(entry,
                            "object_zone_height_stddev_m",
                            (double)frame->stats.object_coupling.object_zone_height_stddev_m);
    cJSON_AddNumberToObject(entry,
                            "object_zone_max_slope",
                            (double)frame->stats.object_coupling.object_zone_max_slope);
    cJSON_AddBoolToObject(entry, "finite_normals", frame->stats.finite_normals ? 1 : 0);
    cJSON_AddItemToArray(frames, entry);

    text = cJSON_Print(root);
    cJSON_Delete(root);
    if (!text) return false;
    ok = write_text_file(manifest_path, text);
    free(text);
    return ok;
}

bool water_surface_artifacts_should_export(const SceneState *scene) {
    return scene &&
           scene->mode_route.water_mode_active &&
           scene->mode_route.requested_space_mode == SPACE_MODE_3D;
}

bool water_surface_artifacts_write_frame(const SceneState *scene,
                                         const VolumeFrameHeaderVf3dV1 *volume_header,
                                         const char *run_dir,
                                         WaterSurfaceArtifactStatsV1 *out_stats) {
    SceneFluidVolumeExportView3D volume = {0};
    WaterSurfaceFrameData frame = {0};
    char surface_path[512];
    bool ok = false;

    if (out_stats) memset(out_stats, 0, sizeof(*out_stats));
    if (!water_surface_artifacts_should_export(scene) ||
        !volume_header ||
        !run_dir ||
        !run_dir[0]) {
        return false;
    }
    if (!scene_backend_volume_export_view_3d(scene, &volume)) return false;
    if ((uint32_t)volume.width != volume_header->grid_w ||
        (uint32_t)volume.height != volume_header->grid_h ||
        (uint32_t)volume.depth != volume_header->grid_d) {
        return false;
    }
    if (!extract_heightfield(scene, &volume, volume_header, &frame)) {
        free_frame_data(&frame);
        return false;
    }

    snprintf(surface_path,
             sizeof(surface_path),
             "%s/water_surface_%06llu.json",
             run_dir,
             (unsigned long long)volume_header->frame_index);
    ok = write_surface_frame_json(&volume, volume_header, &frame, surface_path) &&
         append_water_manifest_frame(scene, volume_header, &frame, run_dir, surface_path);

    if (ok && out_stats) *out_stats = frame.stats;
    free_frame_data(&frame);
    return ok;
}
