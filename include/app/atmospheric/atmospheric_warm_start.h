#ifndef ATMOSPHERIC_WARM_START_H
#define ATMOSPHERIC_WARM_START_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "app/sim_runtime_backend.h"

struct SceneState;

typedef enum AtmosphericWarmStartSourceKind {
    ATMOSPHERIC_WARM_START_SOURCE_NONE = 0,
    ATMOSPHERIC_WARM_START_SOURCE_VF3D_RAW,
    ATMOSPHERIC_WARM_START_SOURCE_VF3H_PACK
} AtmosphericWarmStartSourceKind;

typedef enum AtmosphericWarmStartRuntimeStatus {
    ATMOSPHERIC_WARM_START_RUNTIME_NONE = 0,
    ATMOSPHERIC_WARM_START_RUNTIME_REQUESTED,
    ATMOSPHERIC_WARM_START_RUNTIME_APPLIED,
    ATMOSPHERIC_WARM_START_RUNTIME_REJECTED
} AtmosphericWarmStartRuntimeStatus;

#define ATMOSPHERIC_WARM_START_PATH_CAPACITY 512u
#define ATMOSPHERIC_WARM_START_ERROR_CAPACITY 160u

typedef struct AtmosphericWarmStartMetadata3D {
    AtmosphericWarmStartSourceKind source_kind;
    int width;
    int height;
    int depth;
    size_t cell_count;
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
} AtmosphericWarmStartMetadata3D;

typedef struct AtmosphericWarmStartStats3D {
    size_t cell_count;
    size_t active_density_cells;
    size_t solid_cells;
    float max_density;
    float max_velocity_magnitude;
} AtmosphericWarmStartStats3D;

typedef struct AtmosphericWarmStartVolume3D {
    AtmosphericWarmStartMetadata3D metadata;
    float *density;
    float *velocity_x;
    float *velocity_y;
    float *velocity_z;
    float *pressure;
    uint8_t *solid_mask;
} AtmosphericWarmStartVolume3D;

typedef struct AtmosphericWarmStartRuntimeReport3D {
    AtmosphericWarmStartRuntimeStatus status;
    AtmosphericWarmStartMetadata3D metadata;
    AtmosphericWarmStartStats3D stats;
    char path[ATMOSPHERIC_WARM_START_PATH_CAPACITY];
    char rejection_reason[ATMOSPHERIC_WARM_START_ERROR_CAPACITY];
} AtmosphericWarmStartRuntimeReport3D;

void atmospheric_warm_start_volume_3d_free(AtmosphericWarmStartVolume3D *volume);

bool atmospheric_warm_start_load_3d(const char *path,
                                    AtmosphericWarmStartVolume3D *out_volume,
                                    char *error,
                                    size_t error_size);

bool atmospheric_warm_start_apply_3d(SimRuntimeBackend *backend,
                                     const AtmosphericWarmStartVolume3D *volume,
                                     AtmosphericWarmStartStats3D *out_stats,
                                     char *error,
                                     size_t error_size);

bool atmospheric_warm_start_apply_scene_3d(struct SceneState *scene,
                                           const char *path,
                                           AtmosphericWarmStartStats3D *out_stats,
                                           char *error,
                                           size_t error_size);

AtmosphericWarmStartStats3D atmospheric_warm_start_stats_3d(
    const AtmosphericWarmStartVolume3D *volume);

#endif
