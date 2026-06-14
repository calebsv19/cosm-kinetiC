#include "app/app_config.h"
#include "app/scene_controller.h"
#include "app/scene_presets.h"
#include "geo/shape_library.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include <errno.h>
#include <dirent.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define PHYSICS_SIM_HEADLESS_PATH_MAX 1024

typedef enum PhysicsSimHeadlessOutputPolicy {
    PHYSICS_SIM_HEADLESS_OUTPUT_FAIL_IF_EXISTS = 0,
    PHYSICS_SIM_HEADLESS_OUTPUT_OVERWRITE = 1
} PhysicsSimHeadlessOutputPolicy;

typedef struct PhysicsSimHeadlessCliOptions {
    const char *runtime_scene_path;
    const char *output_root;
    const char *summary_path;
    const char *progress_path;
    const char *cancel_flag_path;
    int frames;
    int sim_steps_per_frame;
    int progress_interval;
    int grid_w;
    int grid_h;
    int grid_d;
    bool grid_override;
    bool save_volume_frames;
    bool save_render_frames;
    bool save_wind_projection_frames;
    bool skip_present;
    WindVisualMode wind_visual_mode;
    HeadlessWindShotCameraProfile wind_shot_camera_profile;
    PhysicsSimHeadlessOutputPolicy output_policy;
} PhysicsSimHeadlessCliOptions;

typedef struct PhysicsSimHeadlessProgressSink {
    const char *progress_path;
    const PhysicsSimHeadlessCliOptions *opts;
} PhysicsSimHeadlessProgressSink;

typedef struct PhysicsSimHeadlessWindAnalysisSink {
    const char *timeseries_path;
    const PhysicsSimHeadlessCliOptions *opts;
} PhysicsSimHeadlessWindAnalysisSink;

typedef struct PhysicsSimHeadlessCancelProbe {
    const char *cancel_flag_path;
} PhysicsSimHeadlessCancelProbe;

static bool utc_now_string(char *out, size_t out_size);
static double progress_ratio_for(const HeadlessProgressInfo *progress);

static bool join_path(char *out, size_t out_size, const char *dir, const char *name);

static void print_usage(const char *argv0) {
    fprintf(stderr,
            "usage: %s --runtime-scene <scene_runtime.json> --frames <n> "
            "--output-root <dir> [--save-volume-frames] [--save-render-frames] "
            "[--save-wind-projection-frames] "
            "[--summary <run_summary.json>] [--progress <run_progress.json>] "
            "[--cancel-flag <cancel_requested.flag>] "
            "[--progress-interval <n>] [--sim-steps-per-frame <n>] "
            "[--grid <width>x<height>x<depth>] "
            "[--wind-shot-camera <three_quarter|side|top|downstream|runtime_default>] "
            "[--wind-visual-mode <flow|speed|speed_deficit|vorticity|object_mask|slice_speed_deficit|slice_vorticity|volume_speed_deficit|volume_vorticity>] "
            "[--overwrite] [--present]\n",
            argv0 ? argv0 : "physics_sim_headless");
}

static bool parse_int_arg(const char *text, int *out) {
    char *end = NULL;
    long value = 0;
    if (!text || !text[0] || !out) return false;
    errno = 0;
    value = strtol(text, &end, 10);
    if (errno != 0 || !end || *end != '\0' || value < 0 || value > 100000000L) {
        return false;
    }
    *out = (int)value;
    return true;
}

static bool parse_grid_arg(const char *text, int *out_w, int *out_h, int *out_d) {
    const char *p = text;
    char *end = NULL;
    long values[3] = {0, 0, 0};
    if (!text || !text[0] || !out_w || !out_h || !out_d) return false;
    for (int axis = 0; axis < 3; ++axis) {
        errno = 0;
        values[axis] = strtol(p, &end, 10);
        if (errno != 0 || end == p || values[axis] < 4 || values[axis] > 512) {
            return false;
        }
        if (axis < 2) {
            if (*end != 'x' && *end != 'X') return false;
            p = end + 1;
        } else if (*end != '\0') {
            return false;
        }
    }
    *out_w = (int)values[0];
    *out_h = (int)values[1];
    *out_d = (int)values[2];
    return true;
}

static const char *wind_visual_mode_label(WindVisualMode mode) {
    switch (mode) {
    case WIND_VISUAL_MODE_FLOW: return "flow";
    case WIND_VISUAL_MODE_SPEED: return "speed";
    case WIND_VISUAL_MODE_SPEED_DEFICIT: return "speed_deficit";
    case WIND_VISUAL_MODE_VORTICITY: return "vorticity";
    case WIND_VISUAL_MODE_OBJECT_MASK: return "object_mask";
    case WIND_VISUAL_MODE_SLICE_SPEED_DEFICIT: return "slice_speed_deficit";
    case WIND_VISUAL_MODE_SLICE_VORTICITY: return "slice_vorticity";
    case WIND_VISUAL_MODE_VOLUME_SPEED_DEFICIT: return "volume_speed_deficit";
    case WIND_VISUAL_MODE_VOLUME_VORTICITY: return "volume_vorticity";
    default: return "flow";
    }
}

static bool parse_wind_visual_mode_arg(const char *text, WindVisualMode *out_mode) {
    if (!text || !text[0] || !out_mode) return false;
    if (strcmp(text, "flow") == 0) {
        *out_mode = WIND_VISUAL_MODE_FLOW;
    } else if (strcmp(text, "speed") == 0) {
        *out_mode = WIND_VISUAL_MODE_SPEED;
    } else if (strcmp(text, "speed_deficit") == 0) {
        *out_mode = WIND_VISUAL_MODE_SPEED_DEFICIT;
    } else if (strcmp(text, "vorticity") == 0) {
        *out_mode = WIND_VISUAL_MODE_VORTICITY;
    } else if (strcmp(text, "object_mask") == 0) {
        *out_mode = WIND_VISUAL_MODE_OBJECT_MASK;
    } else if (strcmp(text, "slice_speed_deficit") == 0) {
        *out_mode = WIND_VISUAL_MODE_SLICE_SPEED_DEFICIT;
    } else if (strcmp(text, "slice_vorticity") == 0) {
        *out_mode = WIND_VISUAL_MODE_SLICE_VORTICITY;
    } else if (strcmp(text, "volume_speed_deficit") == 0) {
        *out_mode = WIND_VISUAL_MODE_VOLUME_SPEED_DEFICIT;
    } else if (strcmp(text, "volume_vorticity") == 0) {
        *out_mode = WIND_VISUAL_MODE_VOLUME_VORTICITY;
    } else {
        return false;
    }
    return true;
}

static bool parse_args(int argc, char **argv, PhysicsSimHeadlessCliOptions *out) {
    if (!out) return false;
    memset(out, 0, sizeof(*out));
    out->frames = 1;
    out->sim_steps_per_frame = 1;
    out->progress_interval = 60;
    out->skip_present = true;
    out->wind_visual_mode = WIND_VISUAL_MODE_FLOW;
    out->wind_shot_camera_profile = HEADLESS_WIND_SHOT_CAMERA_THREE_QUARTER;
    out->output_policy = PHYSICS_SIM_HEADLESS_OUTPUT_FAIL_IF_EXISTS;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--runtime-scene") == 0) {
            if (++i >= argc || !argv[i][0]) return false;
            out->runtime_scene_path = argv[i];
        } else if (strcmp(argv[i], "--frames") == 0) {
            if (++i >= argc || !parse_int_arg(argv[i], &out->frames)) return false;
        } else if (strcmp(argv[i], "--output-root") == 0) {
            if (++i >= argc || !argv[i][0]) return false;
            out->output_root = argv[i];
        } else if (strcmp(argv[i], "--summary") == 0) {
            if (++i >= argc || !argv[i][0]) return false;
            out->summary_path = argv[i];
        } else if (strcmp(argv[i], "--progress") == 0) {
            if (++i >= argc || !argv[i][0]) return false;
            out->progress_path = argv[i];
        } else if (strcmp(argv[i], "--cancel-flag") == 0) {
            if (++i >= argc || !argv[i][0]) return false;
            out->cancel_flag_path = argv[i];
        } else if (strcmp(argv[i], "--progress-interval") == 0) {
            if (++i >= argc || !parse_int_arg(argv[i], &out->progress_interval)) return false;
        } else if (strcmp(argv[i], "--sim-steps-per-frame") == 0) {
            if (++i >= argc || !parse_int_arg(argv[i], &out->sim_steps_per_frame) ||
                out->sim_steps_per_frame <= 0) {
                return false;
            }
        } else if (strcmp(argv[i], "--grid") == 0) {
            if (++i >= argc || !parse_grid_arg(argv[i],
                                               &out->grid_w,
                                               &out->grid_h,
                                               &out->grid_d)) {
                return false;
            }
            out->grid_override = true;
        } else if (strcmp(argv[i], "--wind-shot-camera") == 0) {
            if (++i >= argc || !argv[i][0]) return false;
            if (strcmp(argv[i], "three_quarter") == 0) {
                out->wind_shot_camera_profile = HEADLESS_WIND_SHOT_CAMERA_THREE_QUARTER;
            } else if (strcmp(argv[i], "side") == 0) {
                out->wind_shot_camera_profile = HEADLESS_WIND_SHOT_CAMERA_SIDE;
            } else if (strcmp(argv[i], "top") == 0) {
                out->wind_shot_camera_profile = HEADLESS_WIND_SHOT_CAMERA_TOP;
            } else if (strcmp(argv[i], "downstream") == 0) {
                out->wind_shot_camera_profile = HEADLESS_WIND_SHOT_CAMERA_DOWNSTREAM;
            } else if (strcmp(argv[i], "runtime_default") == 0) {
                out->wind_shot_camera_profile = HEADLESS_WIND_SHOT_CAMERA_RUNTIME_DEFAULT;
            } else {
                return false;
            }
        } else if (strcmp(argv[i], "--wind-visual-mode") == 0) {
            if (++i >= argc || !parse_wind_visual_mode_arg(argv[i], &out->wind_visual_mode)) {
                return false;
            }
        } else if (strcmp(argv[i], "--save-volume-frames") == 0) {
            out->save_volume_frames = true;
        } else if (strcmp(argv[i], "--save-render-frames") == 0) {
            out->save_render_frames = true;
        } else if (strcmp(argv[i], "--save-wind-projection-frames") == 0) {
            out->save_wind_projection_frames = true;
        } else if (strcmp(argv[i], "--overwrite") == 0) {
            out->output_policy = PHYSICS_SIM_HEADLESS_OUTPUT_OVERWRITE;
        } else if (strcmp(argv[i], "--resume") == 0) {
            fprintf(stderr,
                    "[physics_sim_headless] ERROR: --resume is not supported yet; use a new output root or --overwrite.\n");
            return false;
        } else if (strcmp(argv[i], "--present") == 0) {
            out->skip_present = false;
        } else if (strcmp(argv[i], "--skip-present") == 0) {
            out->skip_present = true;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            exit(0);
        } else {
            return false;
        }
    }

    return out->runtime_scene_path && out->runtime_scene_path[0] &&
           out->output_root && out->output_root[0] &&
           out->frames > 0;
}

static bool ensure_dir(const char *path) {
    char tmp[PHYSICS_SIM_HEADLESS_PATH_MAX];
    size_t len = 0;
    if (!path || !path[0]) return false;
    if (snprintf(tmp, sizeof(tmp), "%s", path) >= (int)sizeof(tmp)) return false;
    len = strlen(tmp);
    while (len > 1u && tmp[len - 1u] == '/') {
        tmp[len - 1u] = '\0';
        --len;
    }
    for (char *p = tmp + 1; *p; ++p) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(tmp, 0775) != 0 && errno != EEXIST) return false;
            *p = '/';
        }
    }
    return mkdir(tmp, 0775) == 0 || errno == EEXIST;
}

static bool path_exists(const char *path) {
    struct stat st;
    if (!path || !path[0]) return false;
    return stat(path, &st) == 0;
}

static bool dir_is_empty(const char *path) {
    DIR *dir = NULL;
    struct dirent *entry = NULL;
    if (!path || !path[0]) return false;
    dir = opendir(path);
    if (!dir) return false;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        closedir(dir);
        return false;
    }
    closedir(dir);
    return true;
}

static bool remove_tree(const char *path) {
    struct stat st;
    DIR *dir = NULL;
    struct dirent *entry = NULL;
    if (!path || !path[0] || strcmp(path, "/") == 0 || strlen(path) < 8u) return false;
    if (lstat(path, &st) != 0) return errno == ENOENT;
    if (!S_ISDIR(st.st_mode)) return remove(path) == 0;

    dir = opendir(path);
    if (!dir) return false;
    while ((entry = readdir(dir)) != NULL) {
        char child[PHYSICS_SIM_HEADLESS_PATH_MAX];
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        if (!join_path(child, sizeof(child), path, entry->d_name)) {
            closedir(dir);
            return false;
        }
        if (!remove_tree(child)) {
            closedir(dir);
            return false;
        }
    }
    closedir(dir);
    return rmdir(path) == 0;
}

static bool join_path(char *out, size_t out_size, const char *dir, const char *name) {
    if (!out || out_size == 0u || !dir || !name) return false;
    return snprintf(out, out_size, "%s/%s", dir, name) < (int)out_size;
}

static void json_write_escaped(FILE *f, const char *text) {
    const unsigned char *p = (const unsigned char *)(text ? text : "");
    fputc('"', f);
    while (*p) {
        switch (*p) {
            case '\\': fputs("\\\\", f); break;
            case '"': fputs("\\\"", f); break;
            case '\n': fputs("\\n", f); break;
            case '\r': fputs("\\r", f); break;
            case '\t': fputs("\\t", f); break;
            default:
                if (*p < 0x20) {
                    fprintf(f, "\\u%04x", (unsigned int)*p);
                } else {
                    fputc(*p, f);
                }
                break;
        }
        ++p;
    }
    fputc('"', f);
}

static const char *output_policy_label(PhysicsSimHeadlessOutputPolicy policy) {
    switch (policy) {
        case PHYSICS_SIM_HEADLESS_OUTPUT_OVERWRITE: return "overwrite";
        case PHYSICS_SIM_HEADLESS_OUTPUT_FAIL_IF_EXISTS:
        default: return "fail_if_exists";
    }
}

static const char *wind_shot_camera_profile_label(HeadlessWindShotCameraProfile profile) {
    switch (profile) {
        case HEADLESS_WIND_SHOT_CAMERA_THREE_QUARTER: return "three_quarter";
        case HEADLESS_WIND_SHOT_CAMERA_SIDE: return "side";
        case HEADLESS_WIND_SHOT_CAMERA_TOP: return "top";
        case HEADLESS_WIND_SHOT_CAMERA_DOWNSTREAM: return "downstream";
        case HEADLESS_WIND_SHOT_CAMERA_RUNTIME_DEFAULT:
        default:
            return "runtime_default";
    }
}

static void wind_shot_camera_profile_values(HeadlessWindShotCameraProfile profile,
                                            float *out_yaw_deg,
                                            float *out_pitch_deg,
                                            float *out_distance_scale) {
    float yaw_deg = -35.0f;
    float pitch_deg = 24.0f;
    float distance_scale = 1.0f;
    switch (profile) {
        case HEADLESS_WIND_SHOT_CAMERA_THREE_QUARTER:
            yaw_deg = -38.0f;
            pitch_deg = 22.0f;
            distance_scale = 1.08f;
            break;
        case HEADLESS_WIND_SHOT_CAMERA_SIDE:
            yaw_deg = 0.0f;
            pitch_deg = 12.0f;
            distance_scale = 1.08f;
            break;
        case HEADLESS_WIND_SHOT_CAMERA_TOP:
            yaw_deg = 0.0f;
            pitch_deg = 82.0f;
            distance_scale = 1.16f;
            break;
        case HEADLESS_WIND_SHOT_CAMERA_DOWNSTREAM:
            yaw_deg = -90.0f;
            pitch_deg = 14.0f;
            distance_scale = 1.12f;
            break;
        case HEADLESS_WIND_SHOT_CAMERA_RUNTIME_DEFAULT:
        default:
            break;
    }
    if (out_yaw_deg) *out_yaw_deg = yaw_deg;
    if (out_pitch_deg) *out_pitch_deg = pitch_deg;
    if (out_distance_scale) *out_distance_scale = distance_scale;
}

static bool utc_now_string(char *out, size_t out_size) {
    time_t now = 0;
    struct tm tm_utc;
    if (!out || out_size == 0u) return false;
    out[0] = '\0';
    now = time(NULL);
    if (now == (time_t)-1) return false;
#if defined(__APPLE__) || defined(__unix__)
    if (gmtime_r(&now, &tm_utc) == NULL) return false;
#else
    {
        struct tm *tmp = gmtime(&now);
        if (!tmp) return false;
        tm_utc = *tmp;
    }
#endif
    return strftime(out, out_size, "%Y-%m-%dT%H:%M:%SZ", &tm_utc) > 0u;
}

static double progress_ratio_for(const HeadlessProgressInfo *progress) {
    double completed = 0.0;
    if (!progress) return 0.0;
    completed = (double)progress->frames_completed;
    if (progress->sim_steps_total_in_frame > 0u) {
        completed +=
            (double)progress->sim_steps_completed_in_frame /
            (double)progress->sim_steps_total_in_frame;
    }
    if (progress->frames_requested > 0u) {
        double ratio = completed / (double)progress->frames_requested;
        if (ratio > 1.0) ratio = 1.0;
        return ratio;
    }
    return 0.0;
}

static bool write_progress_json(const char *progress_path,
                                const PhysicsSimHeadlessCliOptions *opts,
                                const HeadlessProgressInfo *progress,
                                const char *status) {
    FILE *f = NULL;
    double progress_ratio = 0.0;
    char updated_at_utc[32];
    if (!progress_path || !progress_path[0] || !opts || !progress || !status) return false;
    progress_ratio = progress_ratio_for(progress);
    if (!utc_now_string(updated_at_utc, sizeof(updated_at_utc))) return false;
    f = fopen(progress_path, "wb");
    if (!f) return false;
    fputs("{\n", f);
    fputs("  \"schema\": \"physics_sim_headless_run_progress_v2\",\n", f);
    fputs("  \"runtime_scene\": ", f);
    json_write_escaped(f, opts->runtime_scene_path);
    fputs(",\n  \"output_root\": ", f);
    json_write_escaped(f, opts->output_root);
    if (opts->grid_override) {
        fprintf(f,
                ",\n  \"grid_override\": true,\n"
                "  \"grid\": {\"width\": %d, \"height\": %d, \"depth\": %d}",
                opts->grid_w,
                opts->grid_h,
                opts->grid_d);
    } else {
        fputs(",\n  \"grid_override\": false", f);
    }
    fprintf(f,
            ",\n  \"frames_requested\": %llu,\n"
            "  \"frames_completed\": %llu,\n"
            "  \"frame_index\": %llu,\n"
            "  \"sim_steps_per_frame\": %d,\n"
            "  \"sim_steps_completed_in_frame\": %u,\n"
            "  \"sim_steps_total_in_frame\": %u,\n"
            "  \"progress_ratio\": %.6f,\n"
            "  \"percent_complete\": %.6f,\n"
            "  \"stage\": ",
            (unsigned long long)progress->frames_requested,
            (unsigned long long)progress->frames_completed,
            (unsigned long long)progress->frame_index,
            opts->sim_steps_per_frame,
            progress->sim_steps_completed_in_frame,
            progress->sim_steps_total_in_frame,
            progress_ratio,
            progress_ratio * 100.0);
    json_write_escaped(f, progress->stage ? progress->stage : "running");
    fprintf(f, ",\n  \"updated_at_utc\": ");
    json_write_escaped(f, updated_at_utc);
    fprintf(f, ",\n  \"status\": ");
    json_write_escaped(f, status);
    fputs("\n}\n", f);
    return fclose(f) == 0;
}

static bool write_wind_shot_manifest(const char *manifest_path,
                                     const char *timeseries_path,
                                     const char *summary_path,
                                     const char *progress_path,
                                     const PhysicsSimHeadlessCliOptions *opts) {
    FILE *f = NULL;
    char created_at_utc[32];
    float camera_yaw_deg = 0.0f;
    float camera_pitch_deg = 0.0f;
    float camera_distance_scale = 1.0f;
    if (!manifest_path || !manifest_path[0] || !timeseries_path || !summary_path ||
        !progress_path || !opts) {
        return false;
    }
    if (!utc_now_string(created_at_utc, sizeof(created_at_utc))) return false;
    wind_shot_camera_profile_values(opts->wind_shot_camera_profile,
                                    &camera_yaw_deg,
                                    &camera_pitch_deg,
                                    &camera_distance_scale);
    f = fopen(manifest_path, "wb");
    if (!f) return false;
    fputs("{\n", f);
    fputs("  \"schema\": \"physics_sim_wind_shot_manifest_v1\",\n", f);
    fputs("  \"runtime_scene\": ", f);
    json_write_escaped(f, opts->runtime_scene_path);
    fputs(",\n  \"output_root\": ", f);
    json_write_escaped(f, opts->output_root);
    fputs(",\n  \"summary\": ", f);
    json_write_escaped(f, summary_path);
    fputs(",\n  \"progress\": ", f);
    json_write_escaped(f, progress_path);
    fputs(",\n  \"wind_analysis_timeseries\": ", f);
    json_write_escaped(f, timeseries_path);
    fputs(",\n  \"wind_projection_frames\": ", f);
    json_write_escaped(f, opts->save_wind_projection_frames
                              ? "wind_projection_frames/frame_%06d.bmp"
                              : "");
    if (opts->grid_override) {
        fprintf(f,
                ",\n  \"grid_override\": true,\n"
                "  \"grid\": {\"width\": %d, \"height\": %d, \"depth\": %d}",
                opts->grid_w,
                opts->grid_h,
                opts->grid_d);
    } else {
        fputs(",\n  \"grid_override\": false", f);
    }
    fprintf(f,
            ",\n  \"frames_requested\": %d,\n"
            "  \"sim_steps_per_frame\": %d,\n"
            "  \"save_volume_frames\": %s,\n"
            "  \"save_render_frames\": %s,\n"
            "  \"save_wind_projection_frames\": %s,\n"
            "  \"skip_present\": %s,\n"
            "  \"output_policy\": \"%s\",\n",
            opts->frames,
            opts->sim_steps_per_frame,
            opts->save_volume_frames ? "true" : "false",
            opts->save_render_frames ? "true" : "false",
            opts->save_wind_projection_frames ? "true" : "false",
            opts->skip_present ? "true" : "false",
            output_policy_label(opts->output_policy));
    fputs("  \"camera_source\": \"wind_shot_profile\",\n", f);
    fputs("  \"camera_profile\": ", f);
    json_write_escaped(f, wind_shot_camera_profile_label(opts->wind_shot_camera_profile));
    fputs(",\n  \"wind_visual_mode\": ", f);
    json_write_escaped(f, wind_visual_mode_label(opts->wind_visual_mode));
    fprintf(f,
            ",\n  \"camera_yaw_deg\": %.9g,\n"
            "  \"camera_pitch_deg\": %.9g,\n"
            "  \"camera_distance_scale\": %.9g,\n",
            camera_yaw_deg,
            camera_pitch_deg,
            camera_distance_scale);
    fputs("  \"analysis_schema\": \"physics_sim_wind_analysis_frame_v1\",\n", f);
    fputs("  \"analysis_fields\": [\n", f);
    fputs("    \"sampled_cells\",\n", f);
    fputs("    \"pressure_delta\",\n", f);
    fputs("    \"inlet_pressure_avg\",\n", f);
    fputs("    \"outlet_pressure_avg\",\n", f);
    fputs("    \"inlet_throughput\",\n", f);
    fputs("    \"outlet_throughput\",\n", f);
    fputs("    \"throughput_delta\",\n", f);
    fputs("    \"drag_pressure_proxy\",\n", f);
    fputs("    \"object_drag_available\",\n", f);
    fputs("    \"object_solid_cells\",\n", f);
    fputs("    \"object_projected_area\",\n", f);
    fputs("    \"object_upstream_pressure_avg\",\n", f);
    fputs("    \"object_downstream_pressure_avg\",\n", f);
    fputs("    \"object_pressure_delta\",\n", f);
    fputs("    \"object_drag_pressure_proxy\",\n", f);
    fputs("    \"vorticity_avg\",\n", f);
    fputs("    \"vorticity_max\"\n", f);
    fputs("  ],\n", f);
    fputs("  \"created_at_utc\": ", f);
    json_write_escaped(f, created_at_utc);
    fputs("\n}\n", f);
    return fclose(f) == 0;
}

static bool reset_wind_analysis_timeseries(const char *timeseries_path) {
    FILE *f = NULL;
    if (!timeseries_path || !timeseries_path[0]) return false;
    f = fopen(timeseries_path, "wb");
    if (!f) return false;
    return fclose(f) == 0;
}

static void wind_analysis_frame_callback(void *user_data,
                                         uint64_t frame_index,
                                         const SimRuntimeBackendReport *backend_report) {
    PhysicsSimHeadlessWindAnalysisSink *sink =
        (PhysicsSimHeadlessWindAnalysisSink *)user_data;
    FILE *f = NULL;
    if (!sink || !sink->timeseries_path || !sink->opts || !backend_report) return;
    f = fopen(sink->timeseries_path, "ab");
    if (!f) return;
    fprintf(f,
            "{\"schema\":\"physics_sim_wind_analysis_frame_v1\","
            "\"frame_index\":%llu,"
            "\"sim_steps_per_frame\":%d,"
            "\"available\":%s",
            (unsigned long long)frame_index,
            sink->opts->sim_steps_per_frame,
            backend_report->wind_analysis_available ? "true" : "false");
    if (backend_report->wind_analysis_available) {
        fprintf(f,
                ",\"sampled_cells\":%zu,"
                "\"pressure_delta\":%.9g,"
                "\"inlet_pressure_avg\":%.9g,"
                "\"outlet_pressure_avg\":%.9g,"
                "\"inlet_throughput\":%.9g,"
                "\"outlet_throughput\":%.9g,"
                "\"throughput_delta\":%.9g,"
                "\"drag_pressure_proxy\":%.9g,"
                "\"object_drag_available\":%s,"
                "\"object_solid_cells\":%zu,"
                "\"object_projected_area\":%.9g,"
                "\"object_upstream_pressure_avg\":%.9g,"
                "\"object_downstream_pressure_avg\":%.9g,"
                "\"object_pressure_delta\":%.9g,"
                "\"object_drag_pressure_proxy\":%.9g,"
                "\"vorticity_avg\":%.9g,"
                "\"vorticity_max\":%.9g",
                backend_report->wind_analysis_sampled_cells,
                backend_report->wind_analysis_pressure_delta,
                backend_report->wind_analysis_inlet_pressure_avg,
                backend_report->wind_analysis_outlet_pressure_avg,
                backend_report->wind_analysis_inlet_throughput,
                backend_report->wind_analysis_outlet_throughput,
                backend_report->wind_analysis_throughput_delta,
                backend_report->wind_analysis_drag_pressure_proxy,
                backend_report->wind_analysis_object_drag_available ? "true" : "false",
                backend_report->wind_analysis_object_solid_cells,
                backend_report->wind_analysis_object_projected_area,
                backend_report->wind_analysis_object_upstream_pressure_avg,
                backend_report->wind_analysis_object_downstream_pressure_avg,
                backend_report->wind_analysis_object_pressure_delta,
                backend_report->wind_analysis_object_drag_pressure_proxy,
                backend_report->wind_analysis_vorticity_avg,
                backend_report->wind_analysis_vorticity_max);
    }
    fputs("}\n", f);
    (void)fclose(f);
}

static void progress_callback(void *user_data, const HeadlessProgressInfo *progress) {
    PhysicsSimHeadlessProgressSink *sink = (PhysicsSimHeadlessProgressSink *)user_data;
    const char *status = NULL;
    if (!sink || !sink->progress_path || !sink->opts || !progress) return;
    if (progress->final_update && progress->stage &&
        strcmp(progress->stage, "canceled") == 0) {
        status = "canceled";
    } else if (progress->final_update) {
        status = "finishing";
    } else if (progress->stage && strcmp(progress->stage, "pending") == 0) {
        status = "pending";
    } else {
        status = "running";
    }
    (void)write_progress_json(sink->progress_path,
                              sink->opts,
                              progress,
                              status);
    fprintf(stderr,
            "[physics_sim_headless] progress: frame=%llu/%llu step=%u/%u stage=%s%s\n",
            (unsigned long long)progress->frames_completed,
            (unsigned long long)progress->frames_requested,
            progress->sim_steps_completed_in_frame,
            progress->sim_steps_total_in_frame,
            progress->stage ? progress->stage : "running",
            progress->final_update ? " final" : "");
}

static bool cancel_requested_callback(void *user_data) {
    const PhysicsSimHeadlessCancelProbe *probe = (const PhysicsSimHeadlessCancelProbe *)user_data;
    return probe && probe->cancel_flag_path && probe->cancel_flag_path[0] &&
           path_exists(probe->cancel_flag_path);
}

static bool write_run_summary(const char *summary_path,
                              const PhysicsSimHeadlessCliOptions *opts,
                              const SimRuntimeBackendReport *backend_report,
                              int result_code) {
    FILE *f = NULL;
    if (!summary_path || !summary_path[0] || !opts) return false;
    f = fopen(summary_path, "wb");
    if (!f) return false;
    fputs("{\n", f);
    fputs("  \"schema\": \"physics_sim_headless_run_summary_v1\",\n", f);
    fputs("  \"runtime_scene\": ", f);
    json_write_escaped(f, opts->runtime_scene_path);
    fputs(",\n  \"output_root\": ", f);
    json_write_escaped(f, opts->output_root);
    if (opts->grid_override) {
        fprintf(f,
                ",\n  \"grid_override\": true,\n"
                "  \"grid\": {\"width\": %d, \"height\": %d, \"depth\": %d}",
                opts->grid_w,
                opts->grid_h,
                opts->grid_d);
    } else {
        fputs(",\n  \"grid_override\": false", f);
    }
    fprintf(f,
            ",\n  \"frames_requested\": %d,\n"
            "  \"frames_completed\": %d,\n"
            "  \"sim_steps_per_frame\": %d,\n"
            "  \"save_volume_frames\": %s,\n"
            "  \"save_render_frames\": %s,\n"
            "  \"save_wind_projection_frames\": %s,\n"
            "  \"skip_present\": %s,\n"
            "  \"output_policy\": \"%s\",\n"
            "  \"result_code\": %d,\n"
            "  \"status\": \"%s\"",
            opts->frames,
            result_code == 0 ? opts->frames : 0,
            opts->sim_steps_per_frame,
            opts->save_volume_frames ? "true" : "false",
            opts->save_render_frames ? "true" : "false",
            opts->save_wind_projection_frames ? "true" : "false",
            opts->skip_present ? "true" : "false",
            output_policy_label(opts->output_policy),
            result_code,
            result_code == 0 ? "passed" : (result_code == 2 ? "canceled" : "failed"));
    fputs(",\n  \"wind_visual_mode\": ", f);
    json_write_escaped(f, wind_visual_mode_label(opts->wind_visual_mode));
    if (backend_report && backend_report->wind_analysis_available) {
        fprintf(f,
                ",\n  \"wind_analysis\": {\n"
                "    \"schema\": \"physics_sim_wind_analysis_v1\",\n"
                "    \"sampled_cells\": %zu,\n"
                "    \"pressure_delta\": %.9g,\n"
                "    \"inlet_pressure_avg\": %.9g,\n"
                "    \"outlet_pressure_avg\": %.9g,\n"
                "    \"inlet_throughput\": %.9g,\n"
                "    \"outlet_throughput\": %.9g,\n"
                "    \"throughput_delta\": %.9g,\n"
                "    \"drag_pressure_proxy\": %.9g,\n"
                "    \"object_drag_available\": %s,\n"
                "    \"object_solid_cells\": %zu,\n"
                "    \"object_projected_area\": %.9g,\n"
                "    \"object_upstream_pressure_avg\": %.9g,\n"
                "    \"object_downstream_pressure_avg\": %.9g,\n"
                "    \"object_pressure_delta\": %.9g,\n"
                "    \"object_drag_pressure_proxy\": %.9g,\n"
                "    \"vorticity_avg\": %.9g,\n"
                "    \"vorticity_max\": %.9g\n"
                "  }\n",
                backend_report->wind_analysis_sampled_cells,
                backend_report->wind_analysis_pressure_delta,
                backend_report->wind_analysis_inlet_pressure_avg,
                backend_report->wind_analysis_outlet_pressure_avg,
                backend_report->wind_analysis_inlet_throughput,
                backend_report->wind_analysis_outlet_throughput,
                backend_report->wind_analysis_throughput_delta,
                backend_report->wind_analysis_drag_pressure_proxy,
                backend_report->wind_analysis_object_drag_available ? "true" : "false",
                backend_report->wind_analysis_object_solid_cells,
                backend_report->wind_analysis_object_projected_area,
                backend_report->wind_analysis_object_upstream_pressure_avg,
                backend_report->wind_analysis_object_downstream_pressure_avg,
                backend_report->wind_analysis_object_pressure_delta,
                backend_report->wind_analysis_object_drag_pressure_proxy,
                backend_report->wind_analysis_vorticity_avg,
                backend_report->wind_analysis_vorticity_max);
    } else {
        fputc('\n', f);
    }
    fputs("}\n", f);
    return fclose(f) == 0;
}

int main(int argc, char **argv) {
    PhysicsSimHeadlessCliOptions opts;
    char summary_path[PHYSICS_SIM_HEADLESS_PATH_MAX];
    char progress_path[PHYSICS_SIM_HEADLESS_PATH_MAX];
    char wind_shot_manifest_path[PHYSICS_SIM_HEADLESS_PATH_MAX];
    char wind_analysis_timeseries_path[PHYSICS_SIM_HEADLESS_PATH_MAX];
    PhysicsSimHeadlessProgressSink progress_sink;
    PhysicsSimHeadlessWindAnalysisSink wind_analysis_sink;
    PhysicsSimHeadlessCancelProbe cancel_probe;
    HeadlessProgressInfo initial_progress = {0};
    SimRuntimeBackendReport final_backend_report = {0};
    if (!parse_args(argc, argv, &opts)) {
        print_usage(argv[0]);
        return 2;
    }
    if (path_exists(opts.output_root)) {
        if (opts.output_policy == PHYSICS_SIM_HEADLESS_OUTPUT_FAIL_IF_EXISTS &&
            !dir_is_empty(opts.output_root)) {
            fprintf(stderr,
                    "[physics_sim_headless] ERROR: output root already exists and is not empty: %s\n"
                    "[physics_sim_headless]        choose a new output root or pass --overwrite.\n",
                    opts.output_root);
            return 1;
        }
        if (opts.output_policy == PHYSICS_SIM_HEADLESS_OUTPUT_OVERWRITE &&
            !remove_tree(opts.output_root)) {
            fprintf(stderr,
                    "[physics_sim_headless] ERROR: failed to clear output root for overwrite: %s\n",
                    opts.output_root);
            return 1;
        }
    }
    if (!ensure_dir(opts.output_root)) {
        fprintf(stderr, "[physics_sim_headless] ERROR: failed to create output root: %s\n", opts.output_root);
        return 1;
    }
    summary_path[0] = '\0';
    if (opts.summary_path && opts.summary_path[0]) {
        if (snprintf(summary_path, sizeof(summary_path), "%s", opts.summary_path) >= (int)sizeof(summary_path)) {
            fprintf(stderr, "[physics_sim_headless] ERROR: summary path too long\n");
            return 1;
        }
    } else if (!join_path(summary_path, sizeof(summary_path), opts.output_root, "run_summary.json")) {
        fprintf(stderr, "[physics_sim_headless] ERROR: default summary path too long\n");
        return 1;
    }
    progress_path[0] = '\0';
    if (opts.progress_path && opts.progress_path[0]) {
        if (snprintf(progress_path, sizeof(progress_path), "%s", opts.progress_path) >= (int)sizeof(progress_path)) {
            fprintf(stderr, "[physics_sim_headless] ERROR: progress path too long\n");
            return 1;
        }
    } else if (!join_path(progress_path, sizeof(progress_path), opts.output_root, "run_progress.json")) {
        fprintf(stderr, "[physics_sim_headless] ERROR: default progress path too long\n");
        return 1;
    }
    opts.progress_path = progress_path;
    if (!join_path(wind_shot_manifest_path,
                   sizeof(wind_shot_manifest_path),
                   opts.output_root,
                   "wind_shot_manifest.json")) {
        fprintf(stderr, "[physics_sim_headless] ERROR: default wind shot manifest path too long\n");
        return 1;
    }
    if (!join_path(wind_analysis_timeseries_path,
                   sizeof(wind_analysis_timeseries_path),
                   opts.output_root,
                   "wind_analysis_timeseries.jsonl")) {
        fprintf(stderr, "[physics_sim_headless] ERROR: default wind analysis timeseries path too long\n");
        return 1;
    }
    if (!write_wind_shot_manifest(wind_shot_manifest_path,
                                  wind_analysis_timeseries_path,
                                  summary_path,
                                  progress_path,
                                  &opts)) {
        fprintf(stderr,
                "[physics_sim_headless] ERROR: failed to write wind shot manifest: %s\n",
                wind_shot_manifest_path);
        return 1;
    }
    if (!reset_wind_analysis_timeseries(wind_analysis_timeseries_path)) {
        fprintf(stderr,
                "[physics_sim_headless] ERROR: failed to initialize wind analysis timeseries: %s\n",
                wind_analysis_timeseries_path);
        return 1;
    }
    progress_sink = (PhysicsSimHeadlessProgressSink){
        .progress_path = progress_path,
        .opts = &opts
    };
    wind_analysis_sink = (PhysicsSimHeadlessWindAnalysisSink){
        .timeseries_path = wind_analysis_timeseries_path,
        .opts = &opts
    };
    cancel_probe = (PhysicsSimHeadlessCancelProbe){
        .cancel_flag_path = opts.cancel_flag_path
    };
    initial_progress.frames_requested = (uint64_t)opts.frames;
    initial_progress.stage = "pending";
    if (!write_progress_json(progress_path,
                             &opts,
                             &initial_progress,
                             "pending")) {
        fprintf(stderr, "[physics_sim_headless] ERROR: failed to write progress: %s\n", progress_path);
        return 1;
    }

    AppConfig cfg = app_config_default();
    cfg.space_mode = SPACE_MODE_3D;
    cfg.sim_mode = SIM_MODE_BOX;
    cfg.headless_enabled = true;
    cfg.headless_frame_count = opts.frames;
    cfg.headless_skip_present = opts.skip_present;
    cfg.save_volume_frames = opts.save_volume_frames;
    cfg.save_render_frames = opts.save_render_frames;
    cfg.wind_visual_mode = opts.wind_visual_mode;
    if (opts.grid_override) {
        cfg.grid_w = opts.grid_w;
        cfg.grid_h = opts.grid_h;
        cfg.grid_d = opts.grid_d;
    }
    snprintf(cfg.headless_output_dir, sizeof(cfg.headless_output_dir), "%s", opts.output_root);

    const FluidScenePreset *base_preset = scene_presets_get_default();
    FluidScenePreset preset = base_preset ? *base_preset : (FluidScenePreset){0};
    ShapeAssetLibrary shape_library;
    memset(&shape_library, 0, sizeof(shape_library));
    SceneRuntimeLaunch runtime_launch = {0};
    runtime_launch.has_retained_scene = true;
    snprintf(runtime_launch.retained_runtime_scene_path,
             sizeof(runtime_launch.retained_runtime_scene_path),
             "%s",
             opts.runtime_scene_path);
    HeadlessOptions headless = {
        .enabled = true,
        .frame_limit = opts.frames,
        .sim_steps_per_frame = opts.sim_steps_per_frame,
        .skip_present = opts.skip_present,
        .ignore_input = true,
        .preserve_input = false,
        .preserve_sdl_state = false,
        .progress_interval_frames = opts.progress_interval > 0 ? (uint32_t)opts.progress_interval : 0u,
        .progress_callback = progress_callback,
        .progress_user_data = &progress_sink,
        .cancel_requested = cancel_requested_callback,
        .cancel_user_data = &cancel_probe,
        .frame_analysis_callback = wind_analysis_frame_callback,
        .frame_analysis_user_data = &wind_analysis_sink,
        .wind_shot_camera_profile = opts.wind_shot_camera_profile,
        .save_wind_projection_frames = opts.save_wind_projection_frames,
        .final_backend_report = &final_backend_report
    };

    int result = scene_controller_run(&cfg,
                                      &preset,
                                      &runtime_launch,
                                      &shape_library,
                                      opts.output_root,
                                      &headless);
    if (!write_run_summary(summary_path, &opts, &final_backend_report, result)) {
        fprintf(stderr, "[physics_sim_headless] ERROR: failed to write summary: %s\n", summary_path);
        result = result == 0 ? 1 : result;
    }
    if (!write_progress_json(progress_path,
                             &opts,
                             &(HeadlessProgressInfo){
                                 .frames_completed = result == 0 ? (uint64_t)opts.frames : 0u,
                                 .frames_requested = (uint64_t)opts.frames,
                                 .frame_index = result == 0 && opts.frames > 0
                                                    ? (uint64_t)(opts.frames - 1)
                                                    : 0u,
                                 .stage = result == 0 ? "completed" :
                                          (result == 2 ? "canceled" : "failed"),
                                 .final_update = true
                             },
                             result == 0 ? "passed" : (result == 2 ? "canceled" : "failed"))) {
        fprintf(stderr, "[physics_sim_headless] ERROR: failed to write final progress: %s\n", progress_path);
        result = result == 0 ? 1 : result;
    }

    if (TTF_WasInit()) {
        TTF_Quit();
    }
    if (SDL_WasInit(0)) {
        SDL_Quit();
    }

    if (result == 0) {
        printf("[physics_sim_headless] PASS\n");
    } else {
        printf("[physics_sim_headless] FAIL result=%d\n", result);
    }
    printf("[physics_sim_headless] runtime: %s\n", opts.runtime_scene_path);
    printf("[physics_sim_headless] output:  %s\n", opts.output_root);
    printf("[physics_sim_headless] summary: %s\n", summary_path);
    printf("[physics_sim_headless] progress: %s\n", progress_path);
    return result;
}
