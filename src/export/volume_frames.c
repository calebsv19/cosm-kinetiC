#include "export/volume_frames.h"

#include "app/scene_state.h"
#include "export/export_paths.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

typedef struct VolumeFrameHeader {
    uint32_t magic;
    uint32_t version;
    uint32_t grid_w;
    uint32_t grid_h;
    double   time_seconds;
    uint64_t frame_index;
} VolumeFrameHeader;

static const uint32_t VOLUME_MAGIC = ('V' << 24) | ('F' << 16) | ('R' << 8) | ('M');
static const uint32_t VOLUME_VERSION = 1;

bool volume_frames_write(const SceneState *scene,
                         uint64_t frame_index) {
    if (!scene || !scene->smoke) return false;
    if (!export_paths_init()) return false;
    const char *dir = export_volume_dir();
    if (!dir) return false;

    char path[512];
    snprintf(path, sizeof(path), "%s/frame_%06llu.vf2d",
             dir, (unsigned long long)frame_index);

    FILE *f = fopen(path, "wb");
    if (!f) {
        fprintf(stderr, "[export] Failed to open %s (%s)\n", path, strerror(errno));
        return false;
    }

    VolumeFrameHeader header = {
        .magic = VOLUME_MAGIC,
        .version = VOLUME_VERSION,
        .grid_w = (uint32_t)scene->smoke->w,
        .grid_h = (uint32_t)scene->smoke->h,
        .time_seconds = scene->time,
        .frame_index = frame_index
    };
    size_t grid_count = (size_t)scene->smoke->w * (size_t)scene->smoke->h;

    if (fwrite(&header, sizeof(header), 1, f) != 1) {
        fprintf(stderr, "[export] Failed to write header to %s\n", path);
        fclose(f);
        return false;
    }

    if (fwrite(scene->smoke->density, sizeof(float), grid_count, f) != grid_count ||
        fwrite(scene->smoke->velX, sizeof(float), grid_count, f) != grid_count ||
        fwrite(scene->smoke->velY, sizeof(float), grid_count, f) != grid_count) {
        fprintf(stderr, "[export] Failed to write frame data to %s\n", path);
        fclose(f);
        return false;
    }

    fclose(f);
    return true;
}
