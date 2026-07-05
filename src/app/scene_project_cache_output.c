#include "app/scene_project_cache_output.h"

#include "app/physics_sim_json_helpers.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

static void set_error(char *error, size_t error_size, const char *message, const char *path) {
    if (!error || error_size == 0u) return;
    if (path && path[0]) {
        snprintf(error, error_size, "%s: %s", message ? message : "error", path);
    } else {
        snprintf(error, error_size, "%s", message ? message : "error");
    }
}

static bool path_join(char *out, size_t out_size, const char *a, const char *b) {
    if (!out || out_size == 0u || !a || !a[0] || !b || !b[0]) return false;
    return snprintf(out, out_size, "%s/%s", a, b) < (int)out_size;
}

static bool path_exists_kind(const char *path, bool want_dir) {
    struct stat st;
    if (!path || !path[0]) return false;
    if (stat(path, &st) != 0) return false;
    return want_dir ? S_ISDIR(st.st_mode) : S_ISREG(st.st_mode);
}

static bool path_dirname(const char *path, char *out, size_t out_size) {
    const char *slash = NULL;
    size_t len = 0u;
    if (!path || !path[0] || !out || out_size == 0u) return false;
    slash = strrchr(path, '/');
    if (!slash || slash == path) return false;
    len = (size_t)(slash - path);
    if (len == 0u || len >= out_size) return false;
    memcpy(out, path, len);
    out[len] = '\0';
    return true;
}

static bool ensure_dir(const char *path) {
    char tmp[SCENE_PROJECT_CACHE_OUTPUT_PATH_MAX];
    size_t len = 0u;
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
        char child[SCENE_PROJECT_CACHE_OUTPUT_PATH_MAX];
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        if (!path_join(child, sizeof(child), path, entry->d_name)) {
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

static bool copy_file(const char *src, const char *dst) {
    FILE *in = NULL;
    FILE *out = NULL;
    char buffer[16384];
    size_t n = 0u;
    in = fopen(src, "rb");
    if (!in) return false;
    out = fopen(dst, "wb");
    if (!out) {
        fclose(in);
        return false;
    }
    while ((n = fread(buffer, 1u, sizeof(buffer), in)) > 0u) {
        if (fwrite(buffer, 1u, n, out) != n) {
            fclose(in);
            fclose(out);
            return false;
        }
    }
    if (ferror(in)) {
        fclose(in);
        fclose(out);
        return false;
    }
    return fclose(in) == 0 && fclose(out) == 0;
}

static bool has_suffix(const char *text, const char *suffix) {
    size_t text_len = 0u;
    size_t suffix_len = 0u;
    if (!text || !suffix) return false;
    text_len = strlen(text);
    suffix_len = strlen(suffix);
    return text_len >= suffix_len && strcmp(text + text_len - suffix_len, suffix) == 0;
}

static bool should_copy_to_vf3d(const char *name) {
    return has_suffix(name, ".vf3d") || has_suffix(name, ".pack") ||
           strcmp(name, "manifest.json") == 0;
}

static bool should_copy_to_physics(const char *name) {
    return strcmp(name, "scene_bundle.json") == 0 ||
           strcmp(name, "manifest.json") == 0 ||
           strcmp(name, "water_manifest_v1.json") == 0 ||
           (strncmp(name, "water_surface_", 14u) == 0 && has_suffix(name, ".json"));
}

static bool copy_selected_files(const char *src_dir,
                                const char *dst_dir,
                                bool (*predicate)(const char *name)) {
    DIR *dir = NULL;
    struct dirent *entry = NULL;
    if (!ensure_dir(dst_dir)) return false;
    dir = opendir(src_dir);
    if (!dir) return false;
    while ((entry = readdir(dir)) != NULL) {
        char src[SCENE_PROJECT_CACHE_OUTPUT_PATH_MAX];
        char dst[SCENE_PROJECT_CACHE_OUTPUT_PATH_MAX];
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        if (!predicate(entry->d_name)) continue;
        if (!path_join(src, sizeof(src), src_dir, entry->d_name) ||
            !path_join(dst, sizeof(dst), dst_dir, entry->d_name)) {
            closedir(dir);
            return false;
        }
        if (!path_exists_kind(src, false)) continue;
        if (!copy_file(src, dst)) {
            closedir(dir);
            return false;
        }
    }
    closedir(dir);
    return true;
}

static bool find_volume_output_dir(const char *run_output_root, char *out, size_t out_size) {
    char volume_root[SCENE_PROJECT_CACHE_OUTPUT_PATH_MAX];
    DIR *dir = NULL;
    struct dirent *entry = NULL;
    if (!path_join(volume_root, sizeof(volume_root), run_output_root, "volume_frames")) return false;
    dir = opendir(volume_root);
    if (!dir) return false;
    while ((entry = readdir(dir)) != NULL) {
        char candidate[SCENE_PROJECT_CACHE_OUTPUT_PATH_MAX];
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        if (!path_join(candidate, sizeof(candidate), volume_root, entry->d_name)) {
            closedir(dir);
            return false;
        }
        if (path_exists_kind(candidate, true)) {
            bool ok = snprintf(out, out_size, "%s", candidate) < (int)out_size;
            closedir(dir);
            return ok;
        }
    }
    closedir(dir);
    return false;
}

static bool utc_created_at(char *out, size_t out_size, const char *format) {
    time_t now = 0;
    struct tm tm_utc;
    if (!out || out_size == 0u || !format) return false;
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
    return strftime(out, out_size, format, &tm_utc) > 0u;
}

static bool read_small_text_file(const char *path, char *out, size_t out_size) {
    FILE *f = NULL;
    size_t n = 0u;
    if (!path || !out || out_size == 0u) return false;
    f = fopen(path, "rb");
    if (!f) return false;
    n = fread(out, 1u, out_size - 1u, f);
    if (ferror(f)) {
        fclose(f);
        return false;
    }
    out[n] = '\0';
    fclose(f);
    return true;
}

static bool json_extract_string_field(const char *json,
                                      const char *key,
                                      char *out,
                                      size_t out_size) {
    char pattern[96];
    const char *p = NULL;
    size_t written = 0u;
    if (!json || !key || !out || out_size == 0u) return false;
    if (snprintf(pattern, sizeof(pattern), "\"%s\"", key) >= (int)sizeof(pattern)) return false;
    p = strstr(json, pattern);
    if (!p) return false;
    p = strchr(p + strlen(pattern), ':');
    if (!p) return false;
    ++p;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') ++p;
    if (*p != '"') return false;
    ++p;
    while (*p && *p != '"' && written + 1u < out_size) {
        if (*p == '\\' && p[1] != '\0') {
            ++p;
        }
        out[written++] = *p++;
    }
    out[written] = '\0';
    return written > 0u;
}

static bool json_extract_int_field(const char *json, const char *key, int *out) {
    char pattern[96];
    const char *p = NULL;
    char *end = NULL;
    long value = 0;
    if (!json || !key || !out) return false;
    if (snprintf(pattern, sizeof(pattern), "\"%s\"", key) >= (int)sizeof(pattern)) return false;
    p = strstr(json, pattern);
    if (!p) return false;
    p = strchr(p + strlen(pattern), ':');
    if (!p) return false;
    ++p;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') ++p;
    value = strtol(p, &end, 10);
    if (end == p) return false;
    *out = (int)value;
    return true;
}

static bool resolve_manifest_relative_path(const char *project_root,
                                           const char *manifest_json,
                                           const char *key,
                                           char *out,
                                           size_t out_size) {
    char relative[SCENE_PROJECT_CACHE_OUTPUT_PATH_MAX];
    if (!project_root || !project_root[0] || !manifest_json || !key || !out || out_size == 0u) {
        return false;
    }
    if (!json_extract_string_field(manifest_json, key, relative, sizeof(relative))) return false;
    if (relative[0] == '/') {
        return snprintf(out, out_size, "%s", relative) < (int)out_size;
    }
    return path_join(out, out_size, project_root, relative);
}

static void append_missing_cache_piece(char *out, size_t out_size, const char *label) {
    size_t len = 0u;
    if (!out || out_size == 0u || !label || !label[0]) return;
    len = strlen(out);
    if (len + 1u >= out_size) return;
    (void)snprintf(out + len,
                   out_size - len,
                   "%s%s",
                   len > 0u ? ", " : "",
                   label);
}

static void set_cache_target_summary(SceneProjectCacheOutputStatus *out) {
    char missing[96];
    if (!out) return;
    if (!out->has_active_manifest && !out->has_compat_manifest) {
        snprintf(out->cache_target_summary,
                 sizeof(out->cache_target_summary),
                 "Cache Target: no active cache yet");
        return;
    }
    out->active_cache_ready = out->has_vf3d_active_dir &&
                              out->has_physics_active_dir &&
                              out->has_scene_bundle;
    if (out->active_cache_ready) {
        snprintf(out->cache_target_summary,
                 sizeof(out->cache_target_summary),
                 "Cache Target: VF3D active + physics bundle ready");
        return;
    }
    missing[0] = '\0';
    if (!out->has_vf3d_active_dir) {
        append_missing_cache_piece(missing, sizeof(missing), "VF3D");
    }
    if (!out->has_physics_active_dir) {
        append_missing_cache_piece(missing, sizeof(missing), "physics");
    }
    if (!out->has_scene_bundle) {
        append_missing_cache_piece(missing, sizeof(missing), "scene_bundle");
    }
    snprintf(out->cache_target_summary,
             sizeof(out->cache_target_summary),
             "Cache Target: manifest present, missing %s",
             missing[0] ? missing : "active artifacts");
}

static void set_cache_run_summary(SceneProjectCacheOutputStatus *out) {
    char frame_bits[96];
    if (!out) return;
    frame_bits[0] = '\0';
    if (out->frame_count > 0) {
        if (out->export_stride > 1 || out->export_start_frame > 0 || out->export_max_frames > 0) {
            if (out->export_max_frames > 0) {
                snprintf(frame_bits,
                         sizeof(frame_bits),
                         "%d frames, start %d, stride %d, max %d",
                         out->frame_count,
                         out->export_start_frame,
                         out->export_stride > 0 ? out->export_stride : 1,
                         out->export_max_frames);
            } else {
                snprintf(frame_bits,
                         sizeof(frame_bits),
                         "%d frames, start %d, stride %d",
                         out->frame_count,
                         out->export_start_frame,
                         out->export_stride > 0 ? out->export_stride : 1);
            }
        } else {
            snprintf(frame_bits, sizeof(frame_bits), "%d frames", out->frame_count);
        }
    }
    if (out->active_run_id[0]) {
        if (frame_bits[0]) {
            snprintf(out->summary,
                     sizeof(out->summary),
                     "Active Run: %s (%s)",
                     out->active_run_id,
                     frame_bits);
        } else {
            snprintf(out->summary,
                     sizeof(out->summary),
                     "Active Run: %s",
                     out->active_run_id);
        }
    } else if (out->has_active_manifest || out->has_compat_manifest) {
        snprintf(out->summary,
                 sizeof(out->summary),
                 "Active Run: manifest present, run id unreadable");
    } else {
        snprintf(out->summary,
                 sizeof(out->summary),
                 "Active Run: none yet");
    }
}

static bool append_shell_quoted(char *out, size_t out_size, size_t *pos, const char *text) {
    if (!out || out_size == 0u || !pos || !text) return false;
    if (*pos + 1u >= out_size) return false;
    out[(*pos)++] = '"';
    for (const char *p = text; *p; ++p) {
        if (*p == '"' || *p == '\\' || *p == '$' || *p == '`') {
            if (*pos + 1u >= out_size) return false;
            out[(*pos)++] = '\\';
        }
        if (*pos + 1u >= out_size) return false;
        out[(*pos)++] = *p;
    }
    if (*pos + 1u >= out_size) return false;
    out[(*pos)++] = '"';
    out[*pos] = '\0';
    return true;
}

static bool append_text(char *out, size_t out_size, size_t *pos, const char *text) {
    size_t len = 0u;
    if (!out || out_size == 0u || !pos || !text) return false;
    len = strlen(text);
    if (*pos + len >= out_size) return false;
    memcpy(out + *pos, text, len);
    *pos += len;
    out[*pos] = '\0';
    return true;
}

static void write_retained_frame_indices(FILE *f,
                                         int frames,
                                         int start,
                                         int stride,
                                         int max_frames) {
    int written = 0;
    if (start < 0) start = 0;
    if (stride <= 0) stride = 1;
    fputc('[', f);
    for (int frame = start; frame < frames; frame += stride) {
        if (max_frames > 0 && written >= max_frames) break;
        fprintf(f, "%s%d", written > 0 ? ", " : "", frame);
        ++written;
    }
    fputc(']', f);
}

static bool write_cache_manifest_file(const char *path,
                                      const char *run_id,
                                      int source_frame_count,
                                      int frame_count,
                                      int export_start_frame,
                                      int export_stride,
                                      int export_max_frames,
                                      const char *created_at) {
    FILE *f = fopen(path, "wb");
    if (!f) return false;
    fputs("{\n", f);
    fputs("  \"schema\": \"physics_sim_active_cache_manifest_v1\",\n", f);
    fputs("  \"project_root\": \".\",\n", f);
    fputs("  \"runtime_scene\": \"scene_runtime.json\",\n", f);
    fputs("  \"active_run_id\": ", f);
    physics_sim_json_write_string(f, run_id);
    fputs(",\n  \"vf3d_active_dir\": \"assets/vf3d/active\",\n", f);
    fputs("  \"physics_active_dir\": \"assets/physics/active\",\n", f);
    fputs("  \"scene_bundle\": \"assets/physics/active/scene_bundle.json\",\n", f);
    fprintf(f, "  \"frame_count\": %d,\n", frame_count);
    fputs("  \"retained_frame_indices\": ", f);
    write_retained_frame_indices(f,
                                 source_frame_count,
                                 export_start_frame,
                                 export_stride,
                                 export_max_frames);
    fprintf(f,
            ",\n  \"export_start_frame\": %d,\n"
            "  \"export_stride\": %d,\n"
            "  \"export_max_frames\": %d,\n"
            "  \"created_at\": ",
            export_start_frame,
            export_stride,
            export_max_frames);
    physics_sim_json_write_string(f, created_at);
    fputs("\n}\n", f);
    return fclose(f) == 0;
}

bool scene_project_cache_output_resolve(const char *project_root,
                                        SceneProjectCacheOutputResolved *out,
                                        char *error,
                                        size_t error_size) {
    if (!project_root || !project_root[0] || !out) {
        set_error(error, error_size, "missing scene project root", NULL);
        return false;
    }
    memset(out, 0, sizeof(*out));
    if (snprintf(out->project_root, sizeof(out->project_root), "%s", project_root) >=
        (int)sizeof(out->project_root)) {
        set_error(error, error_size, "scene project root path too long", project_root);
        return false;
    }
    if (!path_exists_kind(out->project_root, true)) {
        set_error(error, error_size, "scene project root is not a directory", out->project_root);
        return false;
    }
    if (!path_join(out->scene_runtime_path,
                   sizeof(out->scene_runtime_path),
                   out->project_root,
                   "scene_runtime.json") ||
        !path_join(out->scene_authoring_path,
                   sizeof(out->scene_authoring_path),
                   out->project_root,
                   "scene_authoring.json") ||
        !path_join(out->scene_project_path,
                   sizeof(out->scene_project_path),
                   out->project_root,
                   "scene_project.json")) {
        set_error(error, error_size, "scene project required path too long", out->project_root);
        return false;
    }
    if (!path_exists_kind(out->scene_runtime_path, false)) {
        set_error(error, error_size, "missing required scene_runtime.json", out->scene_runtime_path);
        return false;
    }
    if (!path_exists_kind(out->scene_authoring_path, false)) {
        set_error(error, error_size, "missing required scene_authoring.json", out->scene_authoring_path);
        return false;
    }
    out->has_scene_project = path_exists_kind(out->scene_project_path, false);
    return true;
}

bool scene_project_cache_output_make_update_command(const char *project_root,
                                                    int frames,
                                                    int grid_w,
                                                    int grid_h,
                                                    int grid_d,
                                                    char *out,
                                                    size_t out_size) {
    size_t pos = 0u;
    char numeric[96];
    if (!project_root || !project_root[0] || !out || out_size == 0u) return false;
    out[0] = '\0';
    if (!append_text(out, out_size, &pos, "physics_sim/physics_sim_headless --scene-project ")) {
        return false;
    }
    if (!append_shell_quoted(out, out_size, &pos, project_root)) return false;
    if (frames > 0) {
        if (snprintf(numeric, sizeof(numeric), " --frames %d", frames) >= (int)sizeof(numeric) ||
            !append_text(out, out_size, &pos, numeric)) {
            return false;
        }
    }
    if (grid_w > 0 && grid_h > 0 && grid_d > 0) {
        if (snprintf(numeric, sizeof(numeric), " --grid %dx%dx%d", grid_w, grid_h, grid_d) >=
                (int)sizeof(numeric) ||
            !append_text(out, out_size, &pos, numeric)) {
            return false;
        }
    }
    return append_text(out, out_size, &pos, " --save-volume-frames --overwrite");
}

bool scene_project_cache_output_status_from_project(const char *project_root,
                                                    SceneProjectCacheOutputStatus *out,
                                                    char *error,
                                                    size_t error_size) {
    SceneProjectCacheOutputResolved resolved = {0};
    char active_manifest[SCENE_PROJECT_CACHE_OUTPUT_PATH_MAX];
    char compat_manifest[SCENE_PROJECT_CACHE_OUTPUT_PATH_MAX];
    char manifest_json[16384];
    const char *manifest_to_read = NULL;
    if (!out) {
        set_error(error, error_size, "missing scene project cache status output", NULL);
        return false;
    }
    memset(out, 0, sizeof(*out));
    if (!scene_project_cache_output_resolve(project_root, &resolved, error, error_size)) {
        return false;
    }
    out->is_scene_project = true;
    snprintf(out->project_root, sizeof(out->project_root), "%s", resolved.project_root);
    if (snprintf(active_manifest,
                 sizeof(active_manifest),
                 "%s/physics_sim/active_cache_manifest.json",
                 resolved.project_root) >= (int)sizeof(active_manifest) ||
        snprintf(compat_manifest,
                 sizeof(compat_manifest),
                 "%s/physics_sim/cache_manifest.json",
                 resolved.project_root) >= (int)sizeof(compat_manifest)) {
        set_error(error, error_size, "scene project cache manifest path too long", resolved.project_root);
        return false;
    }
    out->has_active_manifest = path_exists_kind(active_manifest, false);
    out->has_compat_manifest = path_exists_kind(compat_manifest, false);
    manifest_to_read = out->has_active_manifest ? active_manifest :
                       (out->has_compat_manifest ? compat_manifest : NULL);
    if (manifest_to_read) {
        char vf3d_active_dir[SCENE_PROJECT_CACHE_OUTPUT_PATH_MAX];
        char physics_active_dir[SCENE_PROJECT_CACHE_OUTPUT_PATH_MAX];
        char scene_bundle_path[SCENE_PROJECT_CACHE_OUTPUT_PATH_MAX];
        snprintf(out->manifest_path, sizeof(out->manifest_path), "%s", manifest_to_read);
        if (read_small_text_file(manifest_to_read, manifest_json, sizeof(manifest_json))) {
            (void)json_extract_string_field(manifest_json,
                                            "active_run_id",
                                            out->active_run_id,
                                            sizeof(out->active_run_id));
            (void)json_extract_int_field(manifest_json, "frame_count", &out->frame_count);
            (void)json_extract_int_field(manifest_json,
                                         "export_start_frame",
                                         &out->export_start_frame);
            (void)json_extract_int_field(manifest_json, "export_stride", &out->export_stride);
            (void)json_extract_int_field(manifest_json,
                                         "export_max_frames",
                                         &out->export_max_frames);
            if (resolve_manifest_relative_path(resolved.project_root,
                                               manifest_json,
                                               "vf3d_active_dir",
                                               vf3d_active_dir,
                                               sizeof(vf3d_active_dir))) {
                out->has_vf3d_active_dir = path_exists_kind(vf3d_active_dir, true);
            }
            if (resolve_manifest_relative_path(resolved.project_root,
                                               manifest_json,
                                               "physics_active_dir",
                                               physics_active_dir,
                                               sizeof(physics_active_dir))) {
                out->has_physics_active_dir = path_exists_kind(physics_active_dir, true);
            }
            if (resolve_manifest_relative_path(resolved.project_root,
                                               manifest_json,
                                               "scene_bundle",
                                               scene_bundle_path,
                                               sizeof(scene_bundle_path))) {
                out->has_scene_bundle = path_exists_kind(scene_bundle_path, false);
            }
        }
    }
    set_cache_target_summary(out);
    set_cache_run_summary(out);
    (void)scene_project_cache_output_make_update_command(resolved.project_root,
                                                         0,
                                                         0,
                                                         0,
                                                         0,
                                                         out->update_command,
                                                         sizeof(out->update_command));
    return true;
}

bool scene_project_cache_output_status_from_runtime_scene(const char *runtime_scene_path,
                                                          SceneProjectCacheOutputStatus *out,
                                                          char *error,
                                                          size_t error_size) {
    char project_root[SCENE_PROJECT_CACHE_OUTPUT_PATH_MAX];
    if (!runtime_scene_path || !runtime_scene_path[0]) {
        set_error(error, error_size, "missing retained runtime scene path", NULL);
        return false;
    }
    if (!has_suffix(runtime_scene_path, "/scene_runtime.json")) {
        set_error(error, error_size, "retained scene is not a scene-project runtime path", runtime_scene_path);
        return false;
    }
    if (!path_dirname(runtime_scene_path, project_root, sizeof(project_root))) {
        set_error(error, error_size, "failed to resolve scene project root", runtime_scene_path);
        return false;
    }
    return scene_project_cache_output_status_from_project(project_root, out, error, error_size);
}

bool scene_project_cache_output_make_run_id(char *out,
                                            size_t out_size,
                                            char *created_at,
                                            size_t created_at_size) {
    const char *override = getenv("PHYSICS_SIM_PROJECT_CACHE_RUN_ID");
    char compact_time[32];
    if (!out || out_size == 0u) return false;
    if (created_at && created_at_size > 0u &&
        !utc_created_at(created_at, created_at_size, "%Y-%m-%dT%H:%M:%SZ")) {
        return false;
    }
    if (override && override[0]) {
        return snprintf(out, out_size, "%s", override) < (int)out_size;
    }
    if (!utc_created_at(compact_time, sizeof(compact_time), "%Y%m%dT%H%M%SZ")) return false;
    return snprintf(out, out_size, "physics-run-%s", compact_time) < (int)out_size;
}

bool scene_project_cache_output_default_run_root(const char *project_root,
                                                 const char *run_id,
                                                 char *out,
                                                 size_t out_size) {
    if (!project_root || !project_root[0] || !run_id || !run_id[0] || !out || out_size == 0u) {
        return false;
    }
    return snprintf(out, out_size, "%s/physics_sim/runs/%s", project_root, run_id) < (int)out_size;
}

bool scene_project_cache_output_publish(const SceneProjectCacheOutputPublishRequest *request,
                                        char *error,
                                        size_t error_size) {
    char source_dir[SCENE_PROJECT_CACHE_OUTPUT_PATH_MAX];
    char vf3d_run_dir[SCENE_PROJECT_CACHE_OUTPUT_PATH_MAX];
    char vf3d_active_dir[SCENE_PROJECT_CACHE_OUTPUT_PATH_MAX];
    char physics_run_dir[SCENE_PROJECT_CACHE_OUTPUT_PATH_MAX];
    char physics_active_dir[SCENE_PROJECT_CACHE_OUTPUT_PATH_MAX];
    char manifest_path[SCENE_PROJECT_CACHE_OUTPUT_PATH_MAX];
    char created_at[32];
    if (!request || !request->project || !request->run_id || !request->run_output_root) {
        set_error(error, error_size, "invalid project cache publish request", NULL);
        return false;
    }
    if (!find_volume_output_dir(request->run_output_root, source_dir, sizeof(source_dir))) {
        set_error(error, error_size, "no volume frame output directory found", request->run_output_root);
        return false;
    }
    if (!utc_created_at(created_at, sizeof(created_at), "%Y-%m-%dT%H:%M:%SZ")) {
        set_error(error, error_size, "failed to create cache manifest timestamp", NULL);
        return false;
    }
    if (snprintf(vf3d_run_dir,
                 sizeof(vf3d_run_dir),
                 "%s/assets/vf3d/runs/%s",
                 request->project->project_root,
                 request->run_id) >= (int)sizeof(vf3d_run_dir) ||
        snprintf(vf3d_active_dir,
                 sizeof(vf3d_active_dir),
                 "%s/assets/vf3d/active",
                 request->project->project_root) >= (int)sizeof(vf3d_active_dir) ||
        snprintf(physics_run_dir,
                 sizeof(physics_run_dir),
                 "%s/assets/physics/runs/%s",
                 request->project->project_root,
                 request->run_id) >= (int)sizeof(physics_run_dir) ||
        snprintf(physics_active_dir,
                 sizeof(physics_active_dir),
                 "%s/assets/physics/active",
                 request->project->project_root) >= (int)sizeof(physics_active_dir)) {
        set_error(error, error_size, "project cache output path too long", request->project->project_root);
        return false;
    }
    if (request->allow_overwrite) {
        if (!remove_tree(vf3d_run_dir) || !remove_tree(vf3d_active_dir) ||
            !remove_tree(physics_run_dir) || !remove_tree(physics_active_dir)) {
            set_error(error, error_size, "failed to clear previous project cache slot", request->run_id);
            return false;
        }
    } else if (path_exists_kind(vf3d_run_dir, true) || path_exists_kind(physics_run_dir, true)) {
        set_error(error, error_size, "project cache run id already exists", request->run_id);
        return false;
    }
    if (!copy_selected_files(source_dir, vf3d_run_dir, should_copy_to_vf3d) ||
        !copy_selected_files(source_dir, vf3d_active_dir, should_copy_to_vf3d) ||
        !copy_selected_files(source_dir, physics_run_dir, should_copy_to_physics) ||
        !copy_selected_files(source_dir, physics_active_dir, should_copy_to_physics)) {
        set_error(error, error_size, "failed to copy project cache output artifacts", source_dir);
        return false;
    }
    {
        char parent[SCENE_PROJECT_CACHE_OUTPUT_PATH_MAX];
        if (snprintf(parent, sizeof(parent), "%s/physics_sim", request->project->project_root) >=
            (int)sizeof(parent) ||
            !ensure_dir(parent)) {
            set_error(error, error_size, "failed to create physics_sim manifest directory", NULL);
            return false;
        }
    }
    if (snprintf(manifest_path,
                 sizeof(manifest_path),
                 "%s/physics_sim/active_cache_manifest.json",
                 request->project->project_root) >= (int)sizeof(manifest_path) ||
        !write_cache_manifest_file(manifest_path,
                                   request->run_id,
                                   request->source_frame_count,
                                   request->frame_count,
                                   request->export_start_frame,
                                   request->export_stride,
                                   request->export_max_frames,
                                   created_at)) {
        set_error(error, error_size, "failed to write active cache manifest", NULL);
        return false;
    }
    if (snprintf(manifest_path,
                 sizeof(manifest_path),
                 "%s/physics_sim/cache_manifest.json",
                 request->project->project_root) >= (int)sizeof(manifest_path) ||
        !write_cache_manifest_file(manifest_path,
                                   request->run_id,
                                   request->source_frame_count,
                                   request->frame_count,
                                   request->export_start_frame,
                                   request->export_stride,
                                   request->export_max_frames,
                                   created_at)) {
        set_error(error, error_size, "failed to write compatibility cache manifest", NULL);
        return false;
    }
    if (snprintf(manifest_path,
                 sizeof(manifest_path),
                 "%s/physics_sim/runs/%s/cache_manifest.json",
                 request->project->project_root,
                 request->run_id) >= (int)sizeof(manifest_path) ||
        !write_cache_manifest_file(manifest_path,
                                   request->run_id,
                                   request->source_frame_count,
                                   request->frame_count,
                                   request->export_start_frame,
                                   request->export_stride,
                                   request->export_max_frames,
                                   created_at)) {
        set_error(error, error_size, "failed to write run cache manifest", NULL);
        return false;
    }
    return true;
}
