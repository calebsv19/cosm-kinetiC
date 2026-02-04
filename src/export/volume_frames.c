#include "export/volume_frames.h"

#include "app/scene_state.h"
#include "export/export_paths.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

#include "cJSON.h"

typedef struct VolumeFrameHeaderV2 {
    uint32_t magic;
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
    uint32_t reserved[3];
} VolumeFrameHeaderV2;

typedef struct VolumeFrameHeaderV1 {
    uint32_t magic;
    uint32_t version;
    uint32_t grid_w;
    uint32_t grid_h;
    double   time_seconds;
    uint64_t frame_index;
} VolumeFrameHeaderV1;

static const uint32_t VOLUME_MAGIC = ('V' << 24) | ('F' << 16) | ('R' << 8) | ('M');
static const uint32_t VOLUME_VERSION_V2 = 2;
static const uint32_t VOLUME_VERSION_V1 = 1;

static uint32_t obstacle_mask_crc32(const SceneState *scene) {
    if (!scene || !scene->obstacle_mask) return 0;
    size_t count = (size_t)scene->smoke->w * (size_t)scene->smoke->h;
    uint32_t hash = 2166136261u; // FNV-1a
    for (size_t i = 0; i < count; ++i) {
        hash ^= (uint32_t)scene->obstacle_mask[i];
        hash *= 16777619u;
    }
    return hash;
}

static bool manifest_append(const SceneState *scene,
                            const VolumeFrameHeaderV2 *header,
                            const char *frame_path,
                            const char *run_dir) {
    if (!scene || !header || !frame_path) return false;

    const char *dir = run_dir ? run_dir : export_volume_dir();
    if (!dir || !dir[0]) return false;
    char manifest_path[512];
    snprintf(manifest_path, sizeof(manifest_path), "%s/manifest.json", dir);

    FILE *f = fopen(manifest_path, "rb");
    cJSON *root = NULL;
    if (f) {
        fseek(f, 0, SEEK_END);
        long sz = ftell(f);
        fseek(f, 0, SEEK_SET);
        if (sz > 0) {
            char *buf = (char *)malloc((size_t)sz + 1);
            if (buf) {
                size_t n = fread(buf, 1, (size_t)sz, f);
                if (n == (size_t)sz) {
                    buf[sz] = '\0';
                    root = cJSON_Parse(buf);
                }
                free(buf);
            }
        }
        fclose(f);
    }

    if (!root) {
        root = cJSON_CreateObject();
        cJSON_AddNumberToObject(root, "manifest_version", 1);
        cJSON_AddNumberToObject(root, "grid_w", header->grid_w);
        cJSON_AddNumberToObject(root, "grid_h", header->grid_h);
        cJSON_AddNumberToObject(root, "cell_size", header->cell_size);
        cJSON_AddNumberToObject(root, "origin_x", header->origin_x);
        cJSON_AddNumberToObject(root, "origin_y", header->origin_y);
        cJSON_AddNumberToObject(root, "obstacle_mask_crc32", header->obstacle_mask_crc32);
        cJSON_AddStringToObject(root, "run_name", dir);
        if (scene->preset && scene->preset->name) {
            cJSON_AddStringToObject(root, "preset", scene->preset->name);
        }

        if (scene->import_shape_count > 0) {
            cJSON *imports = cJSON_AddArrayToObject(root, "imports");
            if (imports) {
                for (size_t i = 0; i < scene->import_shape_count; ++i) {
                    const ImportedShape *imp = &scene->import_shapes[i];
                    if (!imp->path[0]) continue;
                    cJSON *obj = cJSON_CreateObject();
                    if (!obj) continue;
                    cJSON_AddStringToObject(obj, "path", imp->path);
                    cJSON_AddNumberToObject(obj, "pos_x_norm", imp->position_x);
                    cJSON_AddNumberToObject(obj, "pos_y_norm", imp->position_y);
                    cJSON_AddNumberToObject(obj, "rotation_deg", imp->rotation_deg);
                    cJSON_AddNumberToObject(obj, "scale", imp->scale);
                    cJSON_AddBoolToObject(obj, "is_static", imp->is_static);
                    cJSON_AddItemToArray(imports, obj);
                }
            }
        }

        cJSON_AddArrayToObject(root, "frames");
    }

    cJSON *frames = cJSON_GetObjectItem(root, "frames");
    if (!frames) {
        frames = cJSON_AddArrayToObject(root, "frames");
    }
    if (!frames) {
        cJSON_Delete(root);
        return false;
    }

    cJSON *entry = cJSON_CreateObject();
    if (!entry) { cJSON_Delete(root); return false; }
    cJSON_AddNumberToObject(entry, "frame_index", (double)header->frame_index);
    cJSON_AddNumberToObject(entry, "time_seconds", header->time_seconds);
    cJSON_AddNumberToObject(entry, "dt_seconds", header->dt_seconds);
    cJSON_AddStringToObject(entry, "path", frame_path);
    cJSON_AddItemToArray(frames, entry);

    char *text = cJSON_Print(root);
    cJSON_Delete(root);
    if (!text) return false;

    f = fopen(manifest_path, "wb");
    if (!f) {
        free(text);
        return false;
    }
    size_t len = strlen(text);
    bool ok = fwrite(text, 1, len, f) == len;
    fclose(f);
    free(text);
    return ok;
}

bool volume_frames_write(const SceneState *scene,
                         uint64_t frame_index) {
    if (!scene || !scene->smoke) return false;
    if (!export_paths_init()) return false;
    char run_dir[512] = {0};
    const char *run_name = scene->preset && scene->preset->name ? scene->preset->name : "run";
    if (!export_paths_volume_run(run_name, run_dir, sizeof(run_dir))) {
        return false;
    }
    const char *dir = run_dir;

    char path[512];
    snprintf(path, sizeof(path), "%s/frame_%06llu.vf2d",
             dir, (unsigned long long)frame_index);

    FILE *f = fopen(path, "wb");
    if (!f) {
        fprintf(stderr, "[export] Failed to open %s (%s)\n", path, strerror(errno));
        return false;
    }

    VolumeFrameHeaderV2 header = {
        .magic = VOLUME_MAGIC,
        .version = VOLUME_VERSION_V2,
        .grid_w = (uint32_t)scene->smoke->w,
        .grid_h = (uint32_t)scene->smoke->h,
        .time_seconds = scene->time,
        .frame_index = frame_index,
        .dt_seconds = scene->dt,
        .origin_x = 0.0f,
        .origin_y = 0.0f,
        .cell_size = 1.0f,
        .obstacle_mask_crc32 = obstacle_mask_crc32(scene),
        .reserved = {0, 0, 0}
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
    manifest_append(scene, &header, path, dir);
    return true;
}

bool volume_frame_read_header(const char *path, VolumeFrameInfo *out_info) {
    if (!path || !out_info) return false;
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    uint32_t magic = 0;
    if (fread(&magic, sizeof(magic), 1, f) != 1) { fclose(f); return false; }
    if (magic != VOLUME_MAGIC) { fclose(f); return false; }

    uint32_t version = 0;
    if (fread(&version, sizeof(version), 1, f) != 1) { fclose(f); return false; }

    memset(out_info, 0, sizeof(*out_info));
    out_info->version = version;

    if (version == VOLUME_VERSION_V1) {
        VolumeFrameHeaderV1 h = {0};
        h.magic = magic;
        h.version = version;
        if (fread(&h.grid_w, sizeof(h) - sizeof(h.magic) - sizeof(h.version), 1, f) != 1) {
            fclose(f);
            return false;
        }
        out_info->grid_w = h.grid_w;
        out_info->grid_h = h.grid_h;
        out_info->time_seconds = h.time_seconds;
        out_info->frame_index = h.frame_index;
        out_info->dt_seconds = 0.0;
        out_info->cell_size = 1.0f;
        out_info->origin_x = 0.0f;
        out_info->origin_y = 0.0f;
        out_info->obstacle_mask_crc32 = 0;
        fclose(f);
        return true;
    } else if (version == VOLUME_VERSION_V2) {
        VolumeFrameHeaderV2 h = {0};
        h.magic = magic;
        h.version = version;
        if (fread(&h.grid_w, sizeof(h) - sizeof(h.magic) - sizeof(h.version), 1, f) != 1) {
            fclose(f);
            return false;
        }
        out_info->grid_w = h.grid_w;
        out_info->grid_h = h.grid_h;
        out_info->time_seconds = h.time_seconds;
        out_info->frame_index = h.frame_index;
        out_info->dt_seconds = h.dt_seconds;
        out_info->origin_x = h.origin_x;
        out_info->origin_y = h.origin_y;
        out_info->cell_size = h.cell_size;
        out_info->obstacle_mask_crc32 = h.obstacle_mask_crc32;
        fclose(f);
        return true;
    }

    fclose(f);
    return false;
}
