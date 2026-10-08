#ifndef PHYSICS_SIM_VF3D_CONTRACT_H
#define PHYSICS_SIM_VF3D_CONTRACT_H
#include <stdint.h>
#include <stddef.h>
/* Existing native-endian, native-layout v1 exporter contract. */
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

/* Historical solid_mask_crc32 field: FNV-1a, not CRC32. Preserve v1 bytes. */
#define VOLUME_FRAME_VF3D_MASK_HASH_INITIAL UINT32_C(2166136261)
static inline uint32_t volume_frame_vf3d_mask_hash_update(uint32_t hash, const uint8_t *bytes, size_t count) {
    for (size_t i = 0; i < count; ++i) { hash ^= bytes[i]; hash *= UINT32_C(16777619); }
    return hash;
}
#endif
