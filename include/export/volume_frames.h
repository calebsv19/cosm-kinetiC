#ifndef VOLUME_FRAMES_H
#define VOLUME_FRAMES_H

#include <stdbool.h>
#include <stdint.h>

// Disk format (v2):
// uint32 magic        = 'VFRM'
// uint32 version      = 2
// uint32 grid_w
// uint32 grid_h
// double time_seconds
// uint64 frame_index
// double dt_seconds
// float  origin_x
// float  origin_y
// float  cell_size
// uint32 obstacle_mask_crc32
// uint32 reserved[3]
//
// Then density, velX, velY arrays (float32, count=grid_w*grid_h each).
//
// v1 reader remains supported for backward compatibility (version=1 and no
// extended fields). v2 writes only.

struct SceneState;

bool volume_frames_write(const struct SceneState *scene,
                         uint64_t frame_index);

// Build a minimal CoreDataset JSON sidecar from a .vf2d frame header.
// If dataset_json_path is NULL/empty, the output path defaults to
// "<vf2d stem>.dataset.json". manifest_path is optional metadata.
bool volume_frame_write_core_dataset_json(const char *vf2d_path,
                                          const char *dataset_json_path,
                                          const char *manifest_path);

typedef struct VolumeFrameInfo {
    uint32_t version;
    uint32_t grid_w;
    uint32_t grid_h;
    double   time_seconds;
    uint64_t frame_index;
    double   dt_seconds;
    float    origin_x;
    float    origin_y;
    float    cell_size;
    uint32_t obstacle_mask_crc32;
} VolumeFrameInfo;

// Lightweight header reader for v1/v2 files (data not loaded).
bool volume_frame_read_header(const char *path, VolumeFrameInfo *out_info);

#endif // VOLUME_FRAMES_H
