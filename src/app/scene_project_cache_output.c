#define _DARWIN_C_SOURCE
#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700
#include "app/scene_project_cache_output.h"

#include "app/physics_sim_json_helpers.h"
#include "app/physics_sim_job_json.h"
#include <limits.h>
#include <math.h>
#include "export/volume_frame_vf3d_contract.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <time.h>
#include <unistd.h>

static bool cache_run_id_admitted(const char *id) {
    if (!id) return false;
    size_t size = strnlen(id, SCENE_PROJECT_CACHE_OUTPUT_RUN_ID_MAX);
    if (!size || size >= SCENE_PROJECT_CACHE_OUTPUT_RUN_ID_MAX) return false;
    for (size_t i = 0; i < size; ++i) {
        unsigned char c = (unsigned char)id[i];
        bool alnum = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
        if (!alnum && (i == 0 || (c != '-' && c != '_' && c != '.'))) return false;
    }
    return true;
}

static bool cache_path_admitted(const char *path, bool directory, bool missing_ok) {
    char copy[SCENE_PROJECT_CACHE_OUTPUT_PATH_MAX], syntax[SCENE_PROJECT_CACHE_OUTPUT_PATH_MAX];
    if (!path || !path[0] || strnlen(path, sizeof(copy)) >= sizeof(copy)) return false;
    if (strncmp(path, "/tmp/", 5) == 0 || strncmp(path, "/var/", 5) == 0) {
        if (snprintf(copy, sizeof(copy), "/private%s", path) >= (int)sizeof(copy)) return false;
    } else strcpy(copy, path);
    strcpy(syntax, copy); char *state = NULL; int depth = 0;
    for (char *part = strtok_r(syntax, "/", &state); part; part = strtok_r(NULL, "/", &state)) {
        if (++depth > 128 || strcmp(part, "..") == 0 || strcmp(part, ".git") == 0 ||
            strcmp(part, ".ssh") == 0 || strcmp(part, ".aws") == 0) return false;
    }
    int parent = open(copy[0] == '/' ? "/" : ".", O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (parent < 0) return false;
    state = NULL; char *part = strtok_r(copy, "/", &state); bool valid = false;
    while (part) {
        char *next = strtok_r(NULL, "/", &state);
        if (strcmp(part, ".") == 0 && next) { part = next; continue; }
        int descriptor = openat(parent, part, O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC |
                                ((next || directory) ? O_DIRECTORY : 0));
        if (descriptor < 0) { valid = missing_ok && errno == ENOENT; break; }
        close(parent); parent = descriptor;
        if (!next) {
            struct stat st;
            valid = fstat(parent, &st) == 0 && (directory ? S_ISDIR(st.st_mode) : S_ISREG(st.st_mode) && st.st_nlink == 1);
            break;
        }
        part = next;
    }
    close(parent); return valid;
}

static bool cache_within(const char *path, const char *parent) {
    size_t size = strlen(parent);
    return strncmp(path, parent, size) == 0 && (path[size] == 0 || path[size] == '/');
}
static bool cache_project_root_admitted(const char *path) {
    char resolved[SCENE_PROJECT_CACHE_OUTPUT_PATH_MAX];
    if (!cache_path_admitted(path, true, false) || !realpath(path, resolved)) return false;
    if (!strcmp(resolved, "/")) return false;
    const char *protected[] = {"/System", "/usr", "/bin", "/sbin", "/etc", "/private/etc", "/Library", "/Applications", "/dev", "/proc", "/sys"};
    for (size_t i = 0; i < sizeof(protected)/sizeof(protected[0]); ++i) if (cache_within(resolved, protected[i])) return false;
    return true;
}

typedef struct CacheAdmissionBudget { size_t entries; uint64_t bytes; } CacheAdmissionBudget;
static bool cache_tree_admitted(const char *path, unsigned depth, CacheAdmissionBudget *budget) {
    struct stat st;
    if (++budget->entries > 10000 || depth > 16) return false;
    if (lstat(path, &st) != 0) return errno == ENOENT;
    if (S_ISREG(st.st_mode)) {
        uint64_t maximum = UINT64_C(32) * 1024 * 1024 * 1024;
        if (st.st_nlink != 1 || st.st_size < 0 || (uint64_t)st.st_size > UINT64_C(8)*1024*1024*1024 ||
            (uint64_t)st.st_size > maximum - budget->bytes) return false;
        budget->bytes += (uint64_t)st.st_size; return true;
    }
    if (!S_ISDIR(st.st_mode)) return false;
    DIR *dir = opendir(path); if (!dir) return false;
    struct dirent *entry; bool valid = true; errno = 0;
    while ((entry = readdir(dir))) {
        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) continue;
        char child[SCENE_PROJECT_CACHE_OUTPUT_PATH_MAX];
        if (snprintf(child, sizeof(child), "%s/%s", path, entry->d_name) >= (int)sizeof(child) ||
            !cache_tree_admitted(child, depth+1, budget)) { valid = false; break; }
        errno = 0;
    }
    if (valid && errno) valid = false;
    if (closedir(dir) != 0) valid = false;
    return valid;
}

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
    bool ok = fclose(in) == 0;
    if (fclose(out) != 0) ok = false;
    return ok;
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
    if (!path_join(volume_root, sizeof(volume_root), run_output_root, "volume_frames")) return false;
    DIR *dir = opendir(volume_root); if (!dir) return false;
    size_t count = 0, entries = 0; bool valid = true; struct dirent *entry; errno = 0;
    while ((entry = readdir(dir))) {
        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) continue;
        char candidate[SCENE_PROJECT_CACHE_OUTPUT_PATH_MAX];
        if (++entries > 10000 || ++count > 1 || !path_join(candidate, sizeof(candidate), volume_root, entry->d_name) ||
            !cache_path_admitted(candidate, true, false) || snprintf(out, out_size, "%s", candidate) >= (int)out_size) { valid = false; break; }
        errno = 0;
    }
    if (valid && errno) valid = false;
    if (closedir(dir) != 0) valid = false;
    return valid && count == 1;
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

static bool cache_same_entry(const char *path, bool existed, const struct stat *before);
static json_object *cache_read_manifest(const char *path) {
    const size_t capacity = 1024 * 1024;
    char *bytes = NULL; struct stat before, after;
    if (!cache_path_admitted(path, false, false)) return NULL;
    int fd = open(path, O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) return NULL;
    bool ok = fstat(fd, &before) == 0 && S_ISREG(before.st_mode) && before.st_nlink == 1 &&
        before.st_size > 0 && before.st_size > 0 && before.st_size <= (off_t)capacity;
    if (ok) { bytes = malloc((size_t)before.st_size); if (!bytes) ok = false; }
    size_t offset = 0;
    while (ok && offset < (size_t)before.st_size) {
        ssize_t count = read(fd, bytes + offset, (size_t)before.st_size - offset);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) { ok = false; break; }
        offset += (size_t)count;
    }
    char extra;
    if (ok) ok = read(fd, &extra, 1) == 0 && fstat(fd, &after) == 0 &&
        cache_same_entry(path, true, &before) && before.st_dev == after.st_dev && before.st_ino == after.st_ino &&
        before.st_mode == after.st_mode && before.st_size == after.st_size && before.st_nlink == after.st_nlink;
    if (close(fd) != 0) ok = false;
    json_object *object = ok ? physics_sim_job_json_parse(bytes, offset) : NULL;
    free(bytes); return object;
}
static bool cache_manifest_string(json_object *object, const char *key, const char *expected) {
    json_object *value = NULL;
    return json_object_object_get_ex(object, key, &value) && json_object_is_type(value, json_type_string) &&
        (size_t)json_object_get_string_len(value) == strlen(expected) && !strcmp(json_object_get_string(value), expected);
}
static bool cache_manifest_contract(json_object *object, SceneProjectCacheOutputStatus *out) {
    json_object *id = NULL;
    if (!cache_manifest_string(object, "schema", "physics_sim_active_cache_manifest_v1") ||
        !cache_manifest_string(object, "vf3d_active_dir", "assets/vf3d/active") ||
        !cache_manifest_string(object, "physics_active_dir", "assets/physics/active") ||
        !cache_manifest_string(object, "scene_bundle", "assets/physics/active/scene_bundle.json") ||
        !json_object_object_get_ex(object, "active_run_id", &id) || !json_object_is_type(id, json_type_string) ||
        (size_t)json_object_get_string_len(id) != strlen(json_object_get_string(id)) ||
        !cache_run_id_admitted(json_object_get_string(id))) return false;
    snprintf(out->active_run_id, sizeof(out->active_run_id), "%s", json_object_get_string(id));
    const char *keys[] = {"frame_count", "export_start_frame", "export_stride", "export_max_frames"};
    int *values[] = {&out->frame_count, &out->export_start_frame, &out->export_stride, &out->export_max_frames};
    for (size_t i = 0; i < 4; ++i) {
        if (!physics_sim_job_json_integer(object, keys[i], values[i]) || *values[i] < (i == 2 ? 1 : 0)) return false;
    }
    if (out->export_max_frames > 0 && out->frame_count > out->export_max_frames) return false;
    json_object *optional = NULL;
    if (json_object_object_get_ex(object, "project_root", &optional) && !cache_manifest_string(object, "project_root", ".")) return false;
    if (json_object_object_get_ex(object, "runtime_scene", &optional) && !cache_manifest_string(object, "runtime_scene", "scene_runtime.json")) return false;
    if (json_object_object_get_ex(object, "retained_frame_indices", &optional)) {
        if (!json_object_is_type(optional, json_type_array) || json_object_array_length(optional) != (size_t)out->frame_count) return false;
        int64_t previous = -1;
        for (size_t i = 0; i < json_object_array_length(optional); ++i) {
            json_object *index = json_object_array_get_idx(optional, i);
            if (!json_object_is_type(index, json_type_int)) return false;
            int64_t frame = json_object_get_int64(index);
            if (frame != (int64_t)out->export_start_frame + (int64_t)i * out->export_stride ||
                frame > INT_MAX || frame <= previous) return false;
            previous = frame;
        }
        if (out->export_max_frames > 0 && out->frame_count > out->export_max_frames) return false;
    }
    return true;
}
typedef struct CachePayloadBudget {
    uint64_t bytes;
    struct timespec started;
} CachePayloadBudget;
static bool cache_payload_time(const CachePayloadBudget *budget) {
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) return false;
    double elapsed = (double)(now.tv_sec - budget->started.tv_sec) +
        (double)(now.tv_nsec - budget->started.tv_nsec) / 1000000000.0;
    return elapsed >= 0 && elapsed <= 120.0;
}
static bool cache_payload_read(int fd, void *buffer, size_t bytes, off_t *offset, const CachePayloadBudget *budget) {
    size_t read_bytes = 0;
    while (read_bytes < bytes) {
        if (!cache_payload_time(budget)) return false;
        ssize_t count = pread(fd, (char *)buffer + read_bytes, bytes - read_bytes, *offset);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) return false;
        read_bytes += (size_t)count; *offset += count;
    }
    return true;
}
static bool cache_payload_validate(int fd, const VolumeFrameHeaderVf3dV1 *header, uint64_t cells, const CachePayloadBudget *budget) {
    _Static_assert(sizeof(float) == 4, "VF3D v1 requires four-byte native float");
    float values[4096]; unsigned char mask[4096]; off_t offset = (off_t)sizeof(*header);
    for (unsigned field = 0; field < 5; ++field) {
        uint64_t remaining = cells;
        while (remaining) {
            size_t count = remaining < 4096 ? (size_t)remaining : 4096;
            if (!cache_payload_read(fd, values, count * sizeof(float), &offset, budget)) return false;
            for (size_t i = 0; i < count; ++i) if (!isfinite(values[i])) return false;
            remaining -= count;
        }
    }
    /* The historical crc32-named producer field is FNV-1a over all mask bytes.
     * Nonzero mask values denote solid cells; do not invent a binary-only rule. */
    uint32_t hash = VOLUME_FRAME_VF3D_MASK_HASH_INITIAL; uint64_t remaining = cells;
    while (remaining) {
        size_t count = remaining < sizeof(mask) ? (size_t)remaining : sizeof(mask);
        if (!cache_payload_read(fd, mask, count, &offset, budget)) return false;
        hash = volume_frame_vf3d_mask_hash_update(hash, mask, count);
        remaining -= count;
    }
    unsigned char extra;
    return hash == header->solid_mask_crc32 && cache_payload_time(budget) && pread(fd, &extra, 1, offset) == 0;
}
static bool cache_frame_file(const char *path, int index, const int grids[3], CachePayloadBudget *budget) {
    if (!cache_path_admitted(path, false, false)) return false;
    int fd = open(path, O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) return false;
    struct stat before = {0}; VolumeFrameHeaderVf3dV1 h;
    bool ok = fstat(fd, &before) == 0 && S_ISREG(before.st_mode) && before.st_nlink == 1 &&
        pread(fd, &h, sizeof(h), 0) == (ssize_t)sizeof(h);
    if (ok) {
        uint64_t cells = h.grid_w;
        ok = h.magic == UINT32_C(0x56463344) && h.version == 1 &&
            h.grid_w == (uint32_t)grids[0] && h.grid_h == (uint32_t)grids[1] && h.grid_d == (uint32_t)grids[2] &&
            h.frame_index == (uint64_t)index && isfinite(h.time_seconds) && isfinite(h.dt_seconds) && h.dt_seconds >= 0 &&
            isfinite(h.origin_x) && isfinite(h.origin_y) && isfinite(h.origin_z) && isfinite(h.voxel_size) && h.voxel_size > 0 &&
            isfinite(h.scene_up_x) && isfinite(h.scene_up_y) && isfinite(h.scene_up_z) &&
            (h.scene_up_x != 0 || h.scene_up_y != 0 || h.scene_up_z != 0) &&
            !h.reserved[0] && !h.reserved[1] && !h.reserved[2];
        const uint64_t cap = UINT64_C(8)*1024*1024*1024;
        if (!h.grid_h || cells > cap / h.grid_h) ok = false; else cells *= h.grid_h;
        if (!h.grid_d || cells > cap / h.grid_d) ok = false; else cells *= h.grid_d;
        if (cells > (cap - sizeof(h)) / 21 || before.st_size < 0 ||
            (uint64_t)before.st_size != sizeof(h) + cells*21) ok = false;
        const uint64_t aggregate = UINT64_C(32)*1024*1024*1024;
        if (ok && (uint64_t)before.st_size > aggregate - budget->bytes) ok = false;
        if (ok) { budget->bytes += (uint64_t)before.st_size; ok = cache_payload_validate(fd, &h, cells, budget); }
    }
    if (!cache_same_entry(path, true, &before)) ok = false;
    if (close(fd) != 0) ok = false;
    return ok;
}
static bool cache_inventory(const char *vf3d, const char *physics, int count, int start, int stride) {
    if (count <= 0 || count > 10000 || start < 0 || stride <= 0) return false;
    CachePayloadBudget budget = {0};
    if (clock_gettime(CLOCK_MONOTONIC, &budget.started) != 0) return false;
    char path[SCENE_PROJECT_CACHE_OUTPUT_PATH_MAX];
    if (!path_join(path, sizeof(path), vf3d, "manifest.json")) return false;
    json_object *manifest = cache_read_manifest(path), *physics_manifest = NULL, *bundle = NULL, *frames = NULL;
    int version = 0, grids[3] = {0}; bool ok = manifest &&
        physics_sim_job_json_integer(manifest, "manifest_version", &version) && version == 2 &&
        cache_manifest_string(manifest, "frame_contract", "vf3d") && cache_manifest_string(manifest, "space_mode", "3d") &&
        json_object_object_get_ex(manifest, "frames", &frames) && json_object_is_type(frames, json_type_array) &&
        json_object_array_length(frames) == (size_t)count;
    const char *dimensions[] = {"grid_w", "grid_h", "grid_d"};
    for (size_t i = 0; ok && i < 3; ++i) ok = physics_sim_job_json_integer(manifest, dimensions[i], &grids[i]) && grids[i] > 0;
    if (ok && path_join(path, sizeof(path), physics, "manifest.json")) physics_manifest = cache_read_manifest(path);
    ok = ok && physics_manifest && json_object_equal(manifest, physics_manifest);
    if (ok && path_join(path, sizeof(path), physics, "scene_bundle.json")) bundle = cache_read_manifest(path);
    json_object *fluid = NULL;
    ok = ok && bundle && cache_manifest_string(bundle, "bundle_type", "physics_scene_bundle_v1") &&
        physics_sim_job_json_integer(bundle, "bundle_version", &version) && version == 1 &&
        json_object_object_get_ex(bundle, "fluid_source", &fluid) && json_object_is_type(fluid, json_type_object) &&
        cache_manifest_string(fluid, "kind", "manifest") && cache_manifest_string(fluid, "path", "manifest.json") &&
        cache_manifest_string(fluid, "contract", "vf3d");
    for (int i = 0; ok && i < count; ++i) {
        int64_t expected = (int64_t)start + (int64_t)i*stride; int index;
        json_object *entry = json_object_array_get_idx(frames, (size_t)i);
        char name[64];
        if (expected > INT_MAX || !json_object_is_type(entry, json_type_object) ||
            !physics_sim_job_json_integer(entry, "frame_index", &index) || index != expected ||
            snprintf(name, sizeof(name), "frame_%06d.vf3d", index) >= (int)sizeof(name) ||
            !cache_manifest_string(entry, "path", name) || !cache_manifest_string(entry, "frame_contract", "vf3d") ||
            !path_join(path, sizeof(path), vf3d, name) || !cache_frame_file(path, index, grids, &budget)) ok = false;
    }
    /* Existing tree admission bounds enumeration. Only declared VF3D/pack stems
     * are allowed; pack content qualification remains a separate contract. */
    DIR *dir = ok ? opendir(vf3d) : NULL;
    if (ok && !dir) ok = false;
    if (dir) {
        struct dirent *entry; size_t seen = 0; errno = 0;
        while (ok && (entry = readdir(dir))) {
            if (++seen > 10000) { ok = false; break; }
            if (!has_suffix(entry->d_name, ".vf3d") && !has_suffix(entry->d_name, ".pack")) continue;
            uint64_t frame = 0; char canonical[64]; const char *cursor = entry->d_name + 6;
            const char *suffix = has_suffix(entry->d_name, ".vf3d") ? ".vf3d" : ".pack";
            if (strncmp(entry->d_name, "frame_", 6) || *cursor < '0' || *cursor > '9') { ok = false; break; }
            while (*cursor >= '0' && *cursor <= '9') {
                unsigned digit = (unsigned)(*cursor - '0');
                if (frame > ((uint64_t)INT_MAX - digit) / 10) { ok = false; break; }
                frame = frame*10 + digit; ++cursor;
            }
            if (!ok || strcmp(cursor, suffix) || frame < (unsigned)start ||
                (frame - (unsigned)start) % (unsigned)stride || (frame - (unsigned)start) / (unsigned)stride >= (unsigned)count ||
                snprintf(canonical, sizeof(canonical), "frame_%06llu%s", (unsigned long long)frame, suffix) >= (int)sizeof(canonical) || strcmp(canonical, entry->d_name)) ok = false;
            errno = 0;
        }
        if (ok && errno) ok = false;
        if (closedir(dir) != 0) ok = false;
    }
    if (manifest) json_object_put(manifest);
    if (physics_manifest) json_object_put(physics_manifest);
    if (bundle) json_object_put(bundle);
    return ok;
}

static bool cache_optional_entry(const char *path, bool directory, bool *present) {
    struct stat info; *present = false;
    if (!cache_path_admitted(path, directory, true)) return false;
    if (lstat(path, &info) == 0) { *present = true; return true; }
    return errno == ENOENT;
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
    out->active_cache_ready = out->frame_count > 0 && out->has_vf3d_active_dir &&
                              out->has_physics_active_dir &&
                              out->has_scene_bundle;
    if (out->active_cache_ready) {
        snprintf(out->cache_target_summary,
                 sizeof(out->cache_target_summary),
                 "Cache Target: VF3D active + physics bundle ready");
        return;
    }
    missing[0] = '\0';
    if (out->frame_count <= 0) append_missing_cache_piece(missing, sizeof(missing), "frames");
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
    for (int64_t frame = start; frame < frames; frame += stride) {
        if (max_frames > 0 && written >= max_frames) break;
        fprintf(f, "%s%d", written > 0 ? ", " : "", (int)frame);
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
    bool ok = !ferror(f) && fflush(f) == 0;
    if (fclose(f) != 0) ok = false;
    return ok;
}


static bool cache_generated_manifest_matches(const char *path,
                                             const SceneProjectCacheOutputPublishRequest *request,
                                             const char *created_at) {
    json_object *object = cache_read_manifest(path), *indices = NULL;
    SceneProjectCacheOutputStatus status = {0};
    bool ok = object && json_object_object_length(object) == 13 && cache_manifest_contract(object, &status) &&
        cache_manifest_string(object, "project_root", ".") &&
        cache_manifest_string(object, "runtime_scene", "scene_runtime.json") &&
        cache_manifest_string(object, "created_at", created_at) &&
        json_object_object_get_ex(object, "retained_frame_indices", &indices) &&
        !strcmp(status.active_run_id, request->run_id) && status.frame_count == request->frame_count &&
        status.export_start_frame == request->export_start_frame && status.export_stride == request->export_stride &&
        status.export_max_frames == request->export_max_frames;
    if (object) json_object_put(object);
    return ok;
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

static bool cache_status_unlocked(const char *project_root,
                                                    SceneProjectCacheOutputStatus *out,
                                                    char *error,
                                                    size_t error_size) {
    SceneProjectCacheOutputResolved resolved = {0};
    char active_manifest[SCENE_PROJECT_CACHE_OUTPUT_PATH_MAX];
    char compat_manifest[SCENE_PROJECT_CACHE_OUTPUT_PATH_MAX];
    const char *manifest_to_read = NULL;
    if (!out) {
        set_error(error, error_size, "missing scene project cache status output", NULL);
        return false;
    }
    memset(out, 0, sizeof(*out));
    if (!scene_project_cache_output_resolve(project_root, &resolved, error, error_size)) {
        return false;
    }
    char pending[SCENE_PROJECT_CACHE_OUTPUT_PATH_MAX]; struct stat held;
    if (snprintf(pending, sizeof(pending), "%s/physics_sim/.cache-publication.pending", resolved.project_root) >= (int)sizeof(pending) ||
        lstat(pending, &held) == 0 || errno != ENOENT) {
        set_error(error, error_size, "cache publication incomplete; inspection/recovery required", NULL);
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
    if (!cache_path_admitted(resolved.scene_runtime_path, false, false) ||
        !cache_path_admitted(resolved.scene_authoring_path, false, false) ||
        !cache_path_admitted(resolved.scene_project_path, false, true) ||
        !cache_optional_entry(active_manifest, false, &out->has_active_manifest) ||
        !cache_optional_entry(compat_manifest, false, &out->has_compat_manifest)) {
        set_error(error, error_size, "cache status manifest/input path admission held", NULL); return false;
    }
    manifest_to_read = out->has_active_manifest ? active_manifest : (out->has_compat_manifest ? compat_manifest : NULL);
    if (manifest_to_read) {
        json_object *object = cache_read_manifest(manifest_to_read);
        bool valid = object && cache_manifest_contract(object, out);
        if (valid && out->has_active_manifest && out->has_compat_manifest) {
            json_object *compat = cache_read_manifest(compat_manifest);
            valid = compat && json_object_equal(object, compat);
            if (compat) json_object_put(compat);
        }
        if (object) json_object_put(object);
        if (!valid) { set_error(error, error_size, "cache manifest schema/content held", NULL); return false; }
        snprintf(out->manifest_path, sizeof(out->manifest_path), "%s", manifest_to_read);
        const char *names[] = {"assets/vf3d/active", "assets/physics/active", "assets/physics/active/scene_bundle.json"};
        bool *present[] = {&out->has_vf3d_active_dir, &out->has_physics_active_dir, &out->has_scene_bundle};
        for (size_t i = 0; i < 3; ++i) {
            char selected[SCENE_PROJECT_CACHE_OUTPUT_PATH_MAX];
            if (!path_join(selected, sizeof(selected), resolved.project_root, names[i]) ||
                !cache_optional_entry(selected, i < 2, present[i])) {
                set_error(error, error_size, "cache active artifact path admission held", NULL); return false;
            }
        }
        CacheAdmissionBudget budget = {0};
        for (size_t i = 0; i < 2; ++i) {
            char selected[SCENE_PROJECT_CACHE_OUTPUT_PATH_MAX];
            if (!path_join(selected, sizeof(selected), resolved.project_root, names[i]) || !cache_tree_admitted(selected, 0, &budget)) {
                set_error(error, error_size, "cache active tree admission held", NULL); return false;
            }
        }
    }
    if (manifest_to_read && out->frame_count > 0) {
        char vf3d[SCENE_PROJECT_CACHE_OUTPUT_PATH_MAX], physics[SCENE_PROJECT_CACHE_OUTPUT_PATH_MAX];
        if (!path_join(vf3d, sizeof(vf3d), resolved.project_root, "assets/vf3d/active") ||
            !path_join(physics, sizeof(physics), resolved.project_root, "assets/physics/active") ||
            !cache_inventory(vf3d, physics, out->frame_count, out->export_start_frame, out->export_stride)) {
            set_error(error, error_size, "cache declared frame inventory held", NULL); return false;
        }
    }
    set_cache_target_summary(out);
    set_cache_run_summary(out);
    if (!scene_project_cache_output_make_update_command(resolved.project_root,
                                                         0,
                                                         0,
                                                         0,
                                                         0,
                                                         out->update_command,
                                                         sizeof(out->update_command))) return false;
    return true;
}

bool scene_project_cache_output_status_from_project(const char *project_root,
                                                    SceneProjectCacheOutputStatus *out,
                                                    char *error, size_t error_size) {
    char lock_path[SCENE_PROJECT_CACHE_OUTPUT_PATH_MAX];
    if (out) memset(out, 0, sizeof(*out));
    if (!cache_project_root_admitted(project_root) ||
        snprintf(lock_path, sizeof(lock_path), "%s/physics_sim/.cache-publication.lock", project_root) >= (int)sizeof(lock_path) ||
        !cache_path_admitted(lock_path, false, true)) {
        set_error(error, error_size, "cache status path admission held", NULL); return false;
    }
    int owner = open(lock_path, O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC);
    if (owner < 0 && errno != ENOENT) return false;
    struct stat owner_identity;
    if (owner >= 0) {
        struct stat st;
        if (fstat(owner, &st) != 0 || !S_ISREG(st.st_mode) || st.st_nlink != 1 || st.st_size != 0 ||
            flock(owner, LOCK_SH | LOCK_NB) != 0) {
            close(owner); set_error(error, error_size, "cache publication owner is active or invalid", NULL); return false;
        }
        owner_identity = st;
    }
    bool ok = cache_status_unlocked(project_root, out, error, error_size);
    if (owner >= 0) {
        if (!cache_same_entry(lock_path, true, &owner_identity)) ok = false;
        if (close(owner) != 0) ok = false;
    } else {
        bool appeared = false;
        if (!cache_optional_entry(lock_path, false, &appeared) || appeared) ok = false;
    }
    if (!ok && out) memset(out, 0, sizeof(*out));
    return ok;
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
        return cache_run_id_admitted(override) && snprintf(out, out_size, "%s", override) < (int)out_size;
    }
    if (!utc_created_at(compact_time, sizeof(compact_time), "%Y%m%dT%H%M%SZ")) return false;
    return snprintf(out, out_size, "physics-run-%s", compact_time) < (int)out_size;
}

bool scene_project_cache_output_default_run_root(const char *project_root,
                                                 const char *run_id,
                                                 char *out,
                                                 size_t out_size) {
    if (!project_root || !project_root[0] || !cache_run_id_admitted(run_id) || !out || out_size == 0u || !cache_project_root_admitted(project_root)) {
        return false;
    }
    return snprintf(out, out_size, "%s/physics_sim/runs/%s", project_root, run_id) < (int)out_size;
}


/* Fixed seven-slot publication. Incomplete attempts retain every staged or
 * displaced entry and leave a durable hold; recovery is an explicit operation. */
static bool cache_sync_path(const char *path, bool directory) {
    int fd = open(path, O_RDONLY | O_NOFOLLOW | O_CLOEXEC | (directory ? O_DIRECTORY : 0));
    if (fd < 0) return false;
    bool ok = fsync(fd) == 0;
    if (close(fd) != 0) ok = false;
    return ok;
}
static bool cache_sync_flat(const char *path) {
    DIR *dir = opendir(path); if (!dir) return false;
    bool ok = true; struct dirent *entry; errno = 0;
    while ((entry = readdir(dir))) {
        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) continue;
        char child[SCENE_PROJECT_CACHE_OUTPUT_PATH_MAX];
        if (!path_join(child, sizeof(child), path, entry->d_name) ||
            !cache_path_admitted(child, false, false) || !cache_sync_path(child, false)) { ok = false; break; }
        errno = 0;
    }
    if (ok && errno) ok = false;
    if (closedir(dir) != 0) ok = false;
    return ok && cache_sync_path(path, true);
}
static bool cache_same_entry(const char *path, bool existed, const struct stat *before) {
    struct stat now;
    if (lstat(path, &now) != 0) return !existed && errno == ENOENT;
    return existed && now.st_dev == before->st_dev && now.st_ino == before->st_ino &&
        now.st_mode == before->st_mode && now.st_nlink == before->st_nlink &&
        now.st_size == before->st_size &&
#ifdef __APPLE__
        now.st_mtimespec.tv_sec == before->st_mtimespec.tv_sec && now.st_mtimespec.tv_nsec == before->st_mtimespec.tv_nsec &&
        now.st_ctimespec.tv_sec == before->st_ctimespec.tv_sec && now.st_ctimespec.tv_nsec == before->st_ctimespec.tv_nsec;
#else
        now.st_mtim.tv_sec == before->st_mtim.tv_sec && now.st_mtim.tv_nsec == before->st_mtim.tv_nsec &&
        now.st_ctim.tv_sec == before->st_ctim.tv_sec && now.st_ctim.tv_nsec == before->st_ctim.tv_nsec;
#endif
}

/* Compare complete staged bytes to their current admitted source. This closes
 * copy-corruption gaps, including finite VF3D changes and opaque pack/sidecars;
 * it is not a durable source digest or proof of historical authenticity. */
static bool cache_copy_file_equal(const char *source, const char *staged, CachePayloadBudget *budget) {
    if (!cache_path_admitted(source, false, false) || !cache_path_admitted(staged, false, false)) return false;
    int a = open(source, O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC);
    int b = open(staged, O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC);
    struct stat first = {0}, second = {0};
    const uint64_t file_cap = UINT64_C(8)*1024*1024*1024;
    const uint64_t pass_cap = UINT64_C(32)*1024*1024*1024;
    bool ok = a >= 0 && b >= 0 && fstat(a, &first) == 0 && fstat(b, &second) == 0 &&
        S_ISREG(first.st_mode) && S_ISREG(second.st_mode) && first.st_nlink == 1 && second.st_nlink == 1 &&
        first.st_size >= 0 && first.st_size == second.st_size && (uint64_t)first.st_size <= file_cap &&
        (uint64_t)first.st_size <= pass_cap - budget->bytes;
    unsigned char left[8192], right[8192]; off_t left_offset = 0, right_offset = 0;
    uint64_t remaining = ok ? (uint64_t)first.st_size : 0;
    if (ok) budget->bytes += remaining;
    while (ok && remaining) {
        size_t count = remaining < sizeof(left) ? (size_t)remaining : sizeof(left);
        ok = cache_payload_read(a, left, count, &left_offset, budget) &&
            cache_payload_read(b, right, count, &right_offset, budget) && memcmp(left, right, count) == 0;
        remaining -= count;
    }
    unsigned char extra;
    if (ok) ok = cache_payload_time(budget) && pread(a, &extra, 1, left_offset) == 0 &&
        pread(b, &extra, 1, right_offset) == 0 && cache_same_entry(source, true, &first) &&
        cache_same_entry(staged, true, &second);
    if (a >= 0 && close(a) != 0) ok = false;
    if (b >= 0 && close(b) != 0) ok = false;
    return ok;
}
static bool cache_copy_tree_equal(const char *source, const char *staged,
                                  bool (*predicate)(const char *), CachePayloadBudget *budget) {
    struct stat source_before, staged_before;
    if (!cache_path_admitted(source, true, false) || !cache_path_admitted(staged, true, false) ||
        lstat(source, &source_before) != 0 || lstat(staged, &staged_before) != 0) return false;
    DIR *dir = opendir(source); if (!dir) return false;
    bool ok = true; size_t entries = 0, expected = 0; struct dirent *entry; errno = 0;
    while (ok && (entry = readdir(dir))) {
        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) continue;
        if (++entries > 10000) { ok = false; break; }
        if (predicate(entry->d_name)) {
            char a[SCENE_PROJECT_CACHE_OUTPUT_PATH_MAX], b[SCENE_PROJECT_CACHE_OUTPUT_PATH_MAX];
            ++expected;
            ok = path_join(a, sizeof(a), source, entry->d_name) && path_join(b, sizeof(b), staged, entry->d_name) &&
                cache_copy_file_equal(a, b, budget);
        }
        errno = 0;
    }
    if (ok && errno) ok = false;
    if (closedir(dir) != 0) ok = false;
    dir = ok ? opendir(staged) : NULL;
    if (ok && !dir) ok = false;
    size_t observed = 0; errno = 0;
    while (ok && (entry = readdir(dir))) {
        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) continue;
        if (++observed > 10000 || !predicate(entry->d_name)) { ok = false; break; }
        errno = 0;
    }
    if (ok && errno) ok = false;
    if (dir && closedir(dir) != 0) ok = false;
    return ok && observed == expected && cache_same_entry(source, true, &source_before) &&
        cache_same_entry(staged, true, &staged_before);
}


static bool should_copy_to_cache(const char *name) {
    return should_copy_to_vf3d(name) || should_copy_to_physics(name);
}
typedef struct CacheWitnessEntry {
    char name[NAME_MAX + 1];
    struct stat identity;
} CacheWitnessEntry;
typedef struct CacheTreeWitness {
    struct stat directory;
    CacheWitnessEntry *entries;
    size_t count;
} CacheTreeWitness;
static bool cache_tree_unchanged(const char *source, const CacheTreeWitness *witness) {
    if (!cache_same_entry(source, true, &witness->directory)) return false;
    for (size_t i = 0; i < witness->count; ++i) {
        char path[SCENE_PROJECT_CACHE_OUTPUT_PATH_MAX];
        if (!path_join(path, sizeof(path), source, witness->entries[i].name) ||
            !cache_path_admitted(path, false, false) ||
            !cache_same_entry(path, true, &witness->entries[i].identity)) return false;
    }
    return cache_same_entry(source, true, &witness->directory);
}
static bool cache_tree_capture(const char *source, CacheTreeWitness *witness, bool (*predicate)(const char *)) {
    if (!cache_path_admitted(source, true, false) || lstat(source, &witness->directory) != 0) return false;
    /* Fixed ceiling; allocation is bounded independently of attacker filenames. */
    witness->entries = calloc(10000, sizeof(*witness->entries));
    if (!witness->entries) return false;
    DIR *dir = opendir(source); if (!dir) return false;
    bool ok = true; size_t seen = 0; struct dirent *entry; errno = 0;
    while (ok && (entry = readdir(dir))) {
        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) continue;
        if (++seen > 10000) { ok = false; break; }
        if (!predicate || predicate(entry->d_name)) {
            CacheWitnessEntry *selected = &witness->entries[witness->count];
            char path[SCENE_PROJECT_CACHE_OUTPUT_PATH_MAX];
            ok = snprintf(selected->name, sizeof(selected->name), "%s", entry->d_name) < (int)sizeof(selected->name) &&
                path_join(path, sizeof(path), source, selected->name) && cache_path_admitted(path, false, false) &&
                lstat(path, &selected->identity) == 0 && S_ISREG(selected->identity.st_mode) && selected->identity.st_nlink == 1;
            if (ok) ++witness->count;
        }
        errno = 0;
    }
    if (ok && errno) ok = false;
    if (closedir(dir) != 0) ok = false;
    return ok && witness->count > 0 && cache_tree_unchanged(source, witness);
}

static bool cache_publish_transaction(const SceneProjectCacheOutputPublishRequest *request,
                                     const char *source, const char *const targets[7],
                                     const char *created_at, char *error, size_t error_size) {
    char parent[SCENE_PROJECT_CACHE_OUTPUT_PATH_MAX], lock_path[SCENE_PROJECT_CACHE_OUTPUT_PATH_MAX];
    char pending[SCENE_PROJECT_CACHE_OUTPUT_PATH_MAX], attempt[SCENE_PROJECT_CACHE_OUTPUT_PATH_MAX];
    char staged[7][SCENE_PROJECT_CACHE_OUTPUT_PATH_MAX], previous[7][SCENE_PROJECT_CACHE_OUTPUT_PATH_MAX];
    struct stat before[7]; bool existed[7]; int owner = -1; bool ok = false;
    CacheTreeWitness source_witness = {0}, staged_witness[4] = {{0}};
    struct stat manifest_witness[3];
    if (!cache_tree_capture(source, &source_witness, should_copy_to_cache) ||
        !cache_inventory(source, source, request->frame_count, request->export_start_frame, request->export_stride) ||
        !cache_tree_unchanged(source, &source_witness)) goto done;
    if (!path_join(parent, sizeof(parent), request->project->project_root, "physics_sim") ||
        !cache_path_admitted(parent, true, true) || !ensure_dir(parent) ||
        !cache_path_admitted(parent, true, false) ||
        !path_join(lock_path, sizeof(lock_path), parent, ".cache-publication.lock") ||
        !path_join(pending, sizeof(pending), parent, ".cache-publication.pending") ||
        !cache_path_admitted(lock_path, false, true)) goto done;
    owner = open(lock_path, O_RDWR | O_CREAT | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC, 0600);
    struct stat lock;
    if (owner < 0 || fstat(owner, &lock) != 0 || !S_ISREG(lock.st_mode) || lock.st_nlink != 1 ||
        lock.st_size != 0 || flock(owner, LOCK_EX | LOCK_NB) != 0) goto done;
    struct stat held;
    if (lstat(pending, &held) == 0 || errno != ENOENT) goto done;
    for (size_t i = 0; i < 7; ++i) {
        if (!cache_path_admitted(targets[i], i < 4, true)) goto done;
        existed[i] = lstat(targets[i], &before[i]) == 0;
        if (!existed[i] && errno != ENOENT) goto done;
    }
    if (!request->allow_overwrite && (existed[0] || existed[2])) goto done;
    if (snprintf(attempt, sizeof(attempt), "%s/.cache-publication-attempt-XXXXXX", parent) >= (int)sizeof(attempt) ||
        !mkdtemp(attempt)) goto done;
    for (size_t i = 0; i < 7; ++i) {
        if (snprintf(staged[i], sizeof(staged[i]), "%s/new-%zu", attempt, i) >= (int)sizeof(staged[i]) ||
            snprintf(previous[i], sizeof(previous[i]), "%s/prior-%zu", attempt, i) >= (int)sizeof(previous[i])) goto done;
    }
    /* The durable request lists exact targets and original presence, including
     * absence. It exists before the pending hold and before any slot mutation. */
    char plan[SCENE_PROJECT_CACHE_OUTPUT_PATH_MAX];
    if (!path_join(plan, sizeof(plan), attempt, "plan.json")) goto done;
    FILE *record = fopen(plan, "wx"); if (!record) goto done;
    fputs("{\"schema\":\"physics-cache-publication-v1\",\"targets\":[", record);
    for (size_t i = 0; i < 7; ++i) {
        if (i) fputc(',', record);
        fputs("{\"path\":", record); physics_sim_json_write_string(record, targets[i]);
        fprintf(record, ",\"existed\":%s}", existed[i] ? "true" : "false");
    }
    fputs("]}\n", record);
    bool recorded = !ferror(record) && fflush(record) == 0 && fsync(fileno(record)) == 0;
    if (fclose(record) != 0) recorded = false;
    if (!recorded || !cache_sync_path(attempt, true)) goto done;
    int hold = open(pending, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
    if (hold < 0) goto done;
    size_t n = strlen(attempt);
    bool held_ok = write(hold, attempt, n) == (ssize_t)n && fsync(hold) == 0;
    struct stat pending_identity;
    if (fstat(hold, &pending_identity) != 0) held_ok = false;
    if (close(hold) != 0) held_ok = false;
    if (!held_ok || !cache_sync_path(parent, true)) goto done;
    for (size_t i = 0; i < 7; ++i) {
        bool prepared = i < 4 ? copy_selected_files(source, staged[i], i < 2 ? should_copy_to_vf3d : should_copy_to_physics) :
            write_cache_manifest_file(staged[i], request->run_id, request->source_frame_count,
                request->frame_count, request->export_start_frame, request->export_stride,
                request->export_max_frames, created_at);
        if (!prepared || !(i < 4 ? cache_sync_flat(staged[i]) : cache_sync_path(staged[i], false))) goto done;
    }
    for (size_t i = 0; i < 4; ++i)
        if (!cache_tree_capture(staged[i], &staged_witness[i], NULL)) goto done;
    for (size_t i = 4; i < 7; ++i)
        if (!cache_path_admitted(staged[i], false, false) || lstat(staged[i], &manifest_witness[i-4]) != 0 ||
            !S_ISREG(manifest_witness[i-4].st_mode) || manifest_witness[i-4].st_nlink != 1) goto done;
    for (size_t i = 4; i < 7; ++i)
        if (!cache_generated_manifest_matches(staged[i], request, created_at)) goto done;
    if (!cache_inventory(staged[0], staged[2], request->frame_count, request->export_start_frame, request->export_stride) ||
        !cache_inventory(staged[1], staged[3], request->frame_count, request->export_start_frame, request->export_stride) ||
        !cache_sync_path(attempt, true)) goto done;
    CachePayloadBudget copy_budget = {0};
    if (clock_gettime(CLOCK_MONOTONIC, &copy_budget.started) != 0) goto done;
    for (size_t i = 0; i < 4; ++i)
        if (!cache_copy_tree_equal(source, staged[i], i < 2 ? should_copy_to_vf3d : should_copy_to_physics, &copy_budget)) goto done;
    if (!cache_tree_unchanged(source, &source_witness)) goto done;
    for (size_t i = 0; i < 4; ++i)
        if (!cache_tree_unchanged(staged[i], &staged_witness[i])) goto done;
    for (size_t i = 4; i < 7; ++i)
        if (!cache_same_entry(staged[i], true, &manifest_witness[i-4])) goto done;
    /* Whole-plan predecessor check precedes the first rename. Each entry is
     * checked again immediately before displacement. No predecessor is deleted. */
    if (!cache_same_entry(lock_path, true, &lock) || !cache_same_entry(pending, true, &pending_identity)) goto done;
    for (size_t i = 0; i < 7; ++i) if (!cache_same_entry(targets[i], existed[i], &before[i])) goto done;
    for (size_t i = 0; i < 7; ++i) {
        if (!(i < 4 ? cache_tree_unchanged(staged[i], &staged_witness[i]) :
              cache_same_entry(staged[i], true, &manifest_witness[i-4]))) goto done;
        char target_parent[SCENE_PROJECT_CACHE_OUTPUT_PATH_MAX];
        if (!path_dirname(targets[i], target_parent, sizeof(target_parent)) ||
            !cache_path_admitted(target_parent, true, true) || !ensure_dir(target_parent) ||
            !cache_path_admitted(target_parent, true, false) ||
            !cache_same_entry(targets[i], existed[i], &before[i])) goto done;
        if (existed[i] && rename(targets[i], previous[i]) != 0) goto done;
        if (!cache_sync_path(attempt, true) || !cache_sync_path(target_parent, true) ||
            rename(staged[i], targets[i]) != 0 || !cache_sync_path(target_parent, true) ||
            !cache_sync_path(attempt, true)) goto done;
    }
    char complete[SCENE_PROJECT_CACHE_OUTPUT_PATH_MAX];
    if (!path_join(complete, sizeof(complete), attempt, "complete")) goto done;
    int terminal = open(complete, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
    if (terminal < 0) goto done;
    bool terminal_ok = fsync(terminal) == 0;
    if (close(terminal) != 0) terminal_ok = false;
    if (!terminal_ok || !cache_sync_path(attempt, true) ||
        !cache_same_entry(lock_path, true, &lock) || !cache_same_entry(pending, true, &pending_identity) || unlink(pending) != 0 ||
        !cache_sync_path(parent, true)) goto done;
    ok = true;
done:
    free(source_witness.entries);
    for (size_t i = 0; i < 4; ++i) free(staged_witness[i].entries);
    if (owner >= 0) close(owner);
    if (!ok) set_error(error, error_size, "cache publication held; retained attempt requires inspection/recovery", NULL);
    return ok;
}

bool scene_project_cache_output_publish(const SceneProjectCacheOutputPublishRequest *request,
                                        char *error,
                                        size_t error_size) {
    char source_dir[SCENE_PROJECT_CACHE_OUTPUT_PATH_MAX];
    char vf3d_run_dir[SCENE_PROJECT_CACHE_OUTPUT_PATH_MAX];
    char vf3d_active_dir[SCENE_PROJECT_CACHE_OUTPUT_PATH_MAX];
    char physics_run_dir[SCENE_PROJECT_CACHE_OUTPUT_PATH_MAX];
    char physics_active_dir[SCENE_PROJECT_CACHE_OUTPUT_PATH_MAX];
    char created_at[32];
    if (!request || !request->project || !request->run_id || !request->run_output_root) {
        set_error(error, error_size, "invalid project cache publish request", NULL);
        return false;
    }
    if (request->source_frame_count < 0 || request->frame_count < 0 ||
        request->export_start_frame < 0 || request->export_stride <= 0 || request->export_max_frames < 0) {
        set_error(error, error_size, "invalid project cache frame selection", NULL); return false;
    }
    int64_t selected_count = request->source_frame_count > request->export_start_frame ?
        1 + ((int64_t)request->source_frame_count - 1 - request->export_start_frame) / request->export_stride : 0;
    if (request->export_max_frames > 0 && selected_count > request->export_max_frames)
        selected_count = request->export_max_frames;
    if (request->frame_count <= 0 || request->frame_count > 10000 || selected_count != request->frame_count) {
        set_error(error, error_size, "inconsistent or unbounded project cache frame selection", NULL); return false;
    }
    if (!cache_run_id_admitted(request->run_id) ||
        !memchr(request->project->project_root, 0, sizeof(request->project->project_root)) ||
        !cache_project_root_admitted(request->project->project_root) ||
        !cache_path_admitted(request->run_output_root, true, false)) {
        set_error(error, error_size, "project cache identifier or root admission held", NULL);
        return false;
    }
    char volume_parent[SCENE_PROJECT_CACHE_OUTPUT_PATH_MAX];
    if (!path_join(volume_parent, sizeof(volume_parent), request->run_output_root, "volume_frames") ||
        !cache_path_admitted(volume_parent, true, false)) {
        set_error(error, error_size, "volume frame parent admission held", NULL);
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
    char resolved_project[SCENE_PROJECT_CACHE_OUTPUT_PATH_MAX], resolved_source[SCENE_PROJECT_CACHE_OUTPUT_PATH_MAX];
    if (!cache_path_admitted(source_dir, true, false) || !realpath(request->project->project_root, resolved_project) || !realpath(source_dir, resolved_source)) {
        set_error(error, error_size, "cache source identity admission held", NULL);
        return false;
    }
    const char *suffixes[] = {"assets/vf3d/runs", "assets/vf3d/active", "assets/physics/runs", "assets/physics/active"};
    for (size_t i = 0; i < 4; ++i) {
        char selected[SCENE_PROJECT_CACHE_OUTPUT_PATH_MAX];
        int size = (i == 0 || i == 2) ? snprintf(selected, sizeof(selected), "%s/%s/%s", resolved_project, suffixes[i], request->run_id) :
            snprintf(selected, sizeof(selected), "%s/%s", resolved_project, suffixes[i]);
        if (size >= (int)sizeof(selected) || cache_within(resolved_source, selected) || cache_within(selected, resolved_source)) {
            set_error(error, error_size, "cache source overlaps a selected replacement slot", NULL);
            return false;
        }
    }
    CacheAdmissionBudget budget = {0};
    const char *trees[] = {source_dir, vf3d_run_dir, vf3d_active_dir, physics_run_dir, physics_active_dir};
    for (size_t i = 0; i < sizeof(trees)/sizeof(trees[0]); ++i) {
        if (!cache_path_admitted(trees[i], true, i != 0) || !cache_tree_admitted(trees[i], 0, &budget)) {
            set_error(error, error_size, "project cache tree admission held", trees[i]);
            return false;
        }
    }
    const char *manifests[] = {"active_cache_manifest.json", "cache_manifest.json"};
    for (size_t i = 0; i < 2; ++i) {
        char selected[SCENE_PROJECT_CACHE_OUTPUT_PATH_MAX];
        if (snprintf(selected, sizeof(selected), "%s/physics_sim/%s", request->project->project_root, manifests[i]) >= (int)sizeof(selected) ||
            !cache_path_admitted(selected, false, true)) {
            set_error(error, error_size, "project cache manifest path admission held", NULL);
            return false;
        }
    }
    char run_manifest[SCENE_PROJECT_CACHE_OUTPUT_PATH_MAX];
    if (snprintf(run_manifest, sizeof(run_manifest), "%s/physics_sim/runs/%s/cache_manifest.json", request->project->project_root, request->run_id) >= (int)sizeof(run_manifest) ||
        !cache_path_admitted(run_manifest, false, true)) {
        set_error(error, error_size, "run cache manifest path admission held", NULL);
        return false;
    }
    char active_manifest[SCENE_PROJECT_CACHE_OUTPUT_PATH_MAX], compat_manifest[SCENE_PROJECT_CACHE_OUTPUT_PATH_MAX];
    if (snprintf(active_manifest, sizeof(active_manifest), "%s/physics_sim/active_cache_manifest.json", request->project->project_root) >= (int)sizeof(active_manifest) ||
        snprintf(compat_manifest, sizeof(compat_manifest), "%s/physics_sim/cache_manifest.json", request->project->project_root) >= (int)sizeof(compat_manifest)) return false;
    const char *targets[7] = {vf3d_run_dir, vf3d_active_dir, physics_run_dir, physics_active_dir,
                             active_manifest, compat_manifest, run_manifest};
    /* Journal destinations and pending attempt identity must not depend on the
     * caller's future working directory. Preserve the external request layout. */
    SceneProjectCacheOutputResolved canonical_project = *request->project;
    snprintf(canonical_project.project_root, sizeof(canonical_project.project_root), "%s", resolved_project);
    SceneProjectCacheOutputPublishRequest canonical_request = *request;
    canonical_request.project = &canonical_project;
    char canonical_targets[7][SCENE_PROJECT_CACHE_OUTPUT_PATH_MAX]; const char *selected[7];
    size_t prefix = strlen(request->project->project_root);
    for (size_t i = 0; i < 7; ++i) {
        if (snprintf(canonical_targets[i], sizeof(canonical_targets[i]), "%s%s", resolved_project, targets[i] + prefix) >= (int)sizeof(canonical_targets[i])) return false;
        selected[i] = canonical_targets[i];
    }
    return cache_publish_transaction(&canonical_request, source_dir, selected, created_at, error, error_size);
}
