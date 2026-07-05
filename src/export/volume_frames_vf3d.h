#ifndef PHYSICS_SIM_VOLUME_FRAMES_VF3D_H
#define PHYSICS_SIM_VOLUME_FRAMES_VF3D_H

#include <stdbool.h>
#include <stdint.h>

#include "app/scene_state.h"

typedef struct VolumeFrameHeaderVf3dV1 {
    uint32_t magic;
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
    uint32_t reserved[3];
} VolumeFrameHeaderVf3dV1;

bool volume_frames_should_export_vf3d(const SceneState *scene,
                                      SimRuntimeBackendReport *out_report);
bool volume_frame_write_vf3d_raw(const SceneState *scene,
                                 uint64_t frame_index,
                                 const char *path,
                                 const SimRuntimeBackendReport *report,
                                 VolumeFrameHeaderVf3dV1 *out_header);
bool volume_frame_manifest_append_vf3d(const SceneState *scene,
                                       const VolumeFrameHeaderVf3dV1 *header,
                                       const char *frame_path,
                                       const char *run_dir);
bool volume_frame_write_scene_bundle_vf3d(const SceneState *scene,
                                          const VolumeFrameHeaderVf3dV1 *header,
                                          const char *run_dir);

#endif
