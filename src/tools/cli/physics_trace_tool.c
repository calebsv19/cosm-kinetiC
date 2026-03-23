#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core_io.h"
#include "core_scene.h"
#include "core_trace.h"
#include "cJSON.h"

typedef struct VolumeFrameHeaderV2 {
    uint32_t magic;
    uint32_t version;
    uint32_t grid_w;
    uint32_t grid_h;
    double time_seconds;
    uint64_t frame_index;
    double dt_seconds;
    float origin_x;
    float origin_y;
    float cell_size;
    uint32_t obstacle_mask_crc32;
    uint32_t reserved[3];
} VolumeFrameHeaderV2;

typedef struct VolumeFrameHeaderV1 {
    uint32_t magic;
    uint32_t version;
    uint32_t grid_w;
    uint32_t grid_h;
    double time_seconds;
    uint64_t frame_index;
} VolumeFrameHeaderV1;

static const uint32_t VOLUME_MAGIC = ('V' << 24) | ('F' << 16) | ('R' << 8) | ('M');
static const uint32_t VOLUME_VERSION_V2 = 2u;
static const uint32_t VOLUME_VERSION_V1 = 1u;

typedef struct FrameSummary {
    double time_seconds;
    double dt_seconds;
    float density_avg;
    uint64_t frame_index;
} FrameSummary;

static int read_entire_file(const char *path, char **out_buf) {
    CoreBuffer file_data = {0};
    CoreResult r;
    size_t size = 0;
    char *buf = NULL;
    if (!path || !out_buf) return 0;
    r = core_io_read_all(path, &file_data);
    if (r.code != CORE_OK || !file_data.data || file_data.size == 0u) return 0;

    size = file_data.size;
    buf = (char *)malloc(size + 1u);
    if (!buf) {
        core_io_buffer_free(&file_data);
        return 0;
    }
    memcpy(buf, file_data.data, size);
    buf[size] = '\0';
    core_io_buffer_free(&file_data);
    *out_buf = buf;
    return 1;
}

static int file_exists(const char *path) {
    if (!path || !path[0]) return 0;
    return core_io_path_exists(path) ? 1 : 0;
}

static int read_frame_summary(const char *frame_path, FrameSummary *out_summary) {
    FILE *f = NULL;
    uint32_t magic = 0;
    uint32_t version = 0;
    size_t grid_count = 0;
    float *density = NULL;
    double sum = 0.0;

    if (!frame_path || !out_summary) return 0;
    memset(out_summary, 0, sizeof(*out_summary));

    f = fopen(frame_path, "rb");
    if (!f) return 0;
    if (fread(&magic, sizeof(magic), 1, f) != 1 || magic != VOLUME_MAGIC) {
        fclose(f);
        return 0;
    }
    if (fread(&version, sizeof(version), 1, f) != 1) {
        fclose(f);
        return 0;
    }

    if (version == VOLUME_VERSION_V2) {
        VolumeFrameHeaderV2 h;
        h.magic = magic;
        h.version = version;
        if (fread(&h.grid_w, sizeof(h) - sizeof(h.magic) - sizeof(h.version), 1, f) != 1) {
            fclose(f);
            return 0;
        }
        out_summary->time_seconds = h.time_seconds;
        out_summary->dt_seconds = h.dt_seconds;
        out_summary->frame_index = h.frame_index;
        grid_count = (size_t)h.grid_w * (size_t)h.grid_h;
    } else if (version == VOLUME_VERSION_V1) {
        VolumeFrameHeaderV1 h;
        h.magic = magic;
        h.version = version;
        if (fread(&h.grid_w, sizeof(h) - sizeof(h.magic) - sizeof(h.version), 1, f) != 1) {
            fclose(f);
            return 0;
        }
        out_summary->time_seconds = h.time_seconds;
        out_summary->dt_seconds = 0.0;
        out_summary->frame_index = h.frame_index;
        grid_count = (size_t)h.grid_w * (size_t)h.grid_h;
    } else {
        fclose(f);
        return 0;
    }

    if (grid_count == 0u) {
        fclose(f);
        return 0;
    }

    density = (float *)malloc(grid_count * sizeof(float));
    if (!density) {
        fclose(f);
        return 0;
    }
    if (fread(density, sizeof(float), grid_count, f) != grid_count) {
        free(density);
        fclose(f);
        return 0;
    }
    fclose(f);

    for (size_t i = 0; i < grid_count; ++i) {
        sum += (double)density[i];
    }
    out_summary->density_avg = (float)(sum / (double)grid_count);
    free(density);
    return 1;
}

static void usage(const char *argv0) {
    fprintf(stderr, "usage: %s <manifest_json> <trace_pack_path> [solver_iterations]\n", argv0);
}

int main(int argc, char **argv) {
    const char *manifest_path = NULL;
    const char *trace_pack_path = NULL;
    int solver_iterations = 20;
    char *json_buf = NULL;
    cJSON *root = NULL;
    cJSON *frames = NULL;
    int frame_count = 0;
    CoreTraceSession session;
    CoreTraceConfig cfg;
    CoreResult r;
    char manifest_dir[512];

    if (argc < 3 || argc > 4) {
        usage(argv[0]);
        return 1;
    }
    manifest_path = argv[1];
    trace_pack_path = argv[2];
    if (argc == 4) {
        solver_iterations = atoi(argv[3]);
        if (solver_iterations < 1) solver_iterations = 1;
    }

    if (!read_entire_file(manifest_path, &json_buf)) {
        fprintf(stderr, "physics_trace_tool: failed to read manifest: %s\n", manifest_path);
        return 1;
    }
    root = cJSON_Parse(json_buf);
    free(json_buf);
    if (!root) {
        fprintf(stderr, "physics_trace_tool: failed to parse manifest json\n");
        return 1;
    }

    frames = cJSON_GetObjectItem(root, "frames");
    if (!cJSON_IsArray(frames)) {
        cJSON_Delete(root);
        fprintf(stderr, "physics_trace_tool: manifest missing frames array\n");
        return 1;
    }
    frame_count = cJSON_GetArraySize(frames);
    if (frame_count <= 0) {
        cJSON_Delete(root);
        fprintf(stderr, "physics_trace_tool: manifest frames array is empty\n");
        return 1;
    }

    r = core_scene_dirname(manifest_path, manifest_dir, sizeof(manifest_dir));
    if (r.code != CORE_OK) {
        cJSON_Delete(root);
        fprintf(stderr, "physics_trace_tool: failed to resolve manifest dir (%s)\n", r.message);
        return 1;
    }

    memset(&session, 0, sizeof(session));
    cfg.sample_capacity = (size_t)frame_count * 3u;
    cfg.marker_capacity = 4u;
    r = core_trace_session_init(&session, &cfg);
    if (r.code != CORE_OK) {
        cJSON_Delete(root);
        fprintf(stderr, "physics_trace_tool: failed to init trace session (%s)\n", r.message);
        return 1;
    }

    for (int i = 0; i < frame_count; ++i) {
        cJSON *entry = cJSON_GetArrayItem(frames, i);
        cJSON *path = cJSON_IsObject(entry) ? cJSON_GetObjectItem(entry, "path") : NULL;
        const char *frame_rel = (cJSON_IsString(path) && path->valuestring) ? path->valuestring : NULL;
        char frame_path[1024];
        FrameSummary summary;

        if (!frame_rel || !frame_rel[0]) {
            fprintf(stderr, "physics_trace_tool: frame[%d] missing path\n", i);
            core_trace_session_reset(&session);
            cJSON_Delete(root);
            return 1;
        }

        if (frame_rel[0] == '/') {
            snprintf(frame_path, sizeof(frame_path), "%s", frame_rel);
        } else if (file_exists(frame_rel)) {
            snprintf(frame_path, sizeof(frame_path), "%s", frame_rel);
        } else {
            r = core_scene_resolve_path(manifest_dir, frame_rel, frame_path, sizeof(frame_path));
            if (r.code != CORE_OK) {
                fprintf(stderr, "physics_trace_tool: failed to resolve frame path for %s (%s)\n", frame_rel, r.message);
                core_trace_session_reset(&session);
                cJSON_Delete(root);
                return 1;
            }
        }

        if (!read_frame_summary(frame_path, &summary)) {
            fprintf(stderr, "physics_trace_tool: failed to read frame summary from %s (%s)\n",
                    frame_path, strerror(errno));
            core_trace_session_reset(&session);
            cJSON_Delete(root);
            return 1;
        }

        r = core_trace_emit_sample_f32(&session, "frame_dt", summary.time_seconds, (float)summary.dt_seconds);
        if (r.code != CORE_OK) {
            fprintf(stderr, "physics_trace_tool: emit frame_dt failed (%s)\n", r.message);
            core_trace_session_reset(&session);
            cJSON_Delete(root);
            return 1;
        }
        r = core_trace_emit_sample_f32(&session, "solver_iterations", summary.time_seconds, (float)solver_iterations);
        if (r.code != CORE_OK) {
            fprintf(stderr, "physics_trace_tool: emit solver_iterations failed (%s)\n", r.message);
            core_trace_session_reset(&session);
            cJSON_Delete(root);
            return 1;
        }
        r = core_trace_emit_sample_f32(&session, "density_avg", summary.time_seconds, summary.density_avg);
        if (r.code != CORE_OK) {
            fprintf(stderr, "physics_trace_tool: emit density_avg failed (%s)\n", r.message);
            core_trace_session_reset(&session);
            cJSON_Delete(root);
            return 1;
        }

        if (i == 0) {
            r = core_trace_emit_marker(&session, "events", summary.time_seconds, "trace_start");
            if (r.code != CORE_OK) {
                fprintf(stderr, "physics_trace_tool: emit start marker failed (%s)\n", r.message);
                core_trace_session_reset(&session);
                cJSON_Delete(root);
                return 1;
            }
        }
        if (i == frame_count - 1) {
            r = core_trace_emit_marker(&session, "events", summary.time_seconds, "trace_end");
            if (r.code != CORE_OK) {
                fprintf(stderr, "physics_trace_tool: emit end marker failed (%s)\n", r.message);
                core_trace_session_reset(&session);
                cJSON_Delete(root);
                return 1;
            }
        }
    }

    r = core_trace_finalize(&session);
    if (r.code != CORE_OK) {
        fprintf(stderr, "physics_trace_tool: finalize failed (%s)\n", r.message);
        core_trace_session_reset(&session);
        cJSON_Delete(root);
        return 1;
    }

    r = core_trace_export_pack(&session, trace_pack_path);
    if (r.code != CORE_OK) {
        fprintf(stderr, "physics_trace_tool: export failed (%s)\n", r.message);
        core_trace_session_reset(&session);
        cJSON_Delete(root);
        return 1;
    }

    printf("wrote trace pack: %s\n", trace_pack_path);
    printf("source manifest: %s\n", manifest_path);
    printf("frames: %d\n", frame_count);
    printf("lanes: frame_dt, solver_iterations, density_avg\n");

    core_trace_session_reset(&session);
    cJSON_Delete(root);
    return 0;
}
