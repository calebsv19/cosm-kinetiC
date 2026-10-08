#define _DARWIN_C_SOURCE
#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700
#include "app/data_paths.h"

#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

static const char *k_default_config_path = "config/app.json";
static const char *k_runtime_config_path = "data/runtime/app_state.json";
static const char *k_default_input_root = "config";
static const char *k_default_preset_path = "config/custom_preset.txt";
static const char *k_runtime_preset_path = "data/runtime/custom_preset.txt";
static const char *k_default_shape_asset_dir = "config/objects";
static const char *k_default_import_dir = "import";
static const char *k_default_snapshot_dir = "data/snapshots";
#ifdef PHYSICS_SIM_REPO_ROOT
static const char *k_default_runtime_scene_sample_dir =
    PHYSICS_SIM_REPO_ROOT "/config/samples";
#else
static const char *k_default_runtime_scene_sample_dir =
    "config/samples";
#endif
#ifdef PHYSICS_SIM_REPO_ROOT
static const char *k_default_runtime_scene_user_dir =
    PHYSICS_SIM_REPO_ROOT "/data/runtime/scenes";
#else
static const char *k_default_runtime_scene_user_dir =
    "data/runtime/scenes";
#endif
#ifdef PHYSICS_SIM_REPO_ROOT
static const char *k_default_runtime_scene_visual_test_path =
    PHYSICS_SIM_REPO_ROOT "/config/samples/ps4d_runtime_scene_visual_test.json";
#else
static const char *k_default_runtime_scene_visual_test_path =
    "config/samples/ps4d_runtime_scene_visual_test.json";
#endif

static bool physics_sim_file_exists(const char *path) {
    FILE *f = NULL;
    if (!path || !path[0]) return false;
    f = fopen(path, "rb");
    if (!f) return false;
    fclose(f);
    return true;
}

const char *physics_sim_default_config_path(void) {
    return k_default_config_path;
}

const char *physics_sim_runtime_config_path(void) {
    return k_runtime_config_path;
}

const char *physics_sim_default_input_root(void) {
    return k_default_input_root;
}

const char *physics_sim_default_preset_path(void) {
    return k_default_preset_path;
}

const char *physics_sim_runtime_preset_path(void) {
    return k_runtime_preset_path;
}

const char *physics_sim_default_shape_asset_dir(void) {
    return k_default_shape_asset_dir;
}

const char *physics_sim_default_import_dir(void) {
    return k_default_import_dir;
}

const char *physics_sim_default_snapshot_dir(void) {
    return k_default_snapshot_dir;
}

const char *physics_sim_default_runtime_scene_sample_dir(void) {
    return k_default_runtime_scene_sample_dir;
}

const char *physics_sim_default_runtime_scene_user_dir(void) {
    return k_default_runtime_scene_user_dir;
}

const char *physics_sim_default_runtime_scene_visual_test_path(void) {
    return k_default_runtime_scene_visual_test_path;
}

const char *physics_sim_resolve_config_load_path(void) {
    return physics_sim_file_exists(k_runtime_config_path)
               ? k_runtime_config_path
               : k_default_config_path;
}

const char *physics_sim_resolve_preset_load_path(void) {
    return physics_sim_file_exists(k_runtime_preset_path)
               ? k_runtime_preset_path
               : k_default_preset_path;
}

const char *physics_sim_resolve_input_root(const char *configured_root) {
    if (configured_root && configured_root[0]) {
        return configured_root;
    }
    return k_default_input_root;
}

bool physics_sim_compose_root_path(const char *root,
                                   const char *leaf,
                                   char *out,
                                   size_t out_size) {
    size_t root_len = 0;
    if (!root || !root[0] || !leaf || !leaf[0] || !out || out_size == 0) {
        return false;
    }
    root_len = strlen(root);
    if (root_len + 1 + strlen(leaf) + 1 > out_size) {
        return false;
    }
    if (root[root_len - 1] == '/') {
        snprintf(out, out_size, "%s%s", root, leaf);
    } else {
        snprintf(out, out_size, "%s/%s", root, leaf);
    }
    return true;
}

static bool physics_sim_dir_exists(const char *path) {
    DIR *d = NULL;
    if (!path || !path[0]) return false;
    d = opendir(path);
    if (!d) return false;
    closedir(d);
    return true;
}

static void physics_sim_append_unique_root(const char **roots,
                                           size_t *count,
                                           size_t cap,
                                           const char *candidate) {
    if (!roots || !count || !candidate || !candidate[0]) return;
    for (size_t i = 0; i < *count; ++i) {
        if (strcmp(roots[i], candidate) == 0) {
            return;
        }
    }
    if (*count >= cap) return;
    roots[*count] = candidate;
    (*count)++;
}

const char *physics_sim_resolve_preset_load_path_for_root(const char *configured_root,
                                                          char *buffer,
                                                          size_t buffer_size) {
    const char *root = physics_sim_resolve_input_root(configured_root);
    if (physics_sim_file_exists(k_runtime_preset_path)) {
        return k_runtime_preset_path;
    }
    if (physics_sim_compose_root_path(root, "custom_preset.txt", buffer, buffer_size) &&
        physics_sim_file_exists(buffer)) {
        return buffer;
    }
    return k_default_preset_path;
}

const char *physics_sim_resolve_shape_asset_dir_for_root(const char *configured_root,
                                                         char *buffer,
                                                         size_t buffer_size) {
    const char *root = physics_sim_resolve_input_root(configured_root);
    if (physics_sim_compose_root_path(root, "objects", buffer, buffer_size) &&
        physics_sim_dir_exists(buffer)) {
        return buffer;
    }
    return k_default_shape_asset_dir;
}

const char *physics_sim_resolve_import_dir_for_root(const char *configured_root,
                                                    char *buffer,
                                                    size_t buffer_size) {
    const char *root = physics_sim_resolve_input_root(configured_root);
    if (physics_sim_compose_root_path(root, "import", buffer, buffer_size) &&
        physics_sim_dir_exists(buffer)) {
        return buffer;
    }
    return k_default_import_dir;
}

const char *physics_sim_resolve_snapshot_output_dir(const char *configured_dir) {
    if (configured_dir && configured_dir[0]) {
        return configured_dir;
    }
    return k_default_snapshot_dir;
}

size_t physics_sim_runtime_scene_catalog_roots(const char *configured_input_root,
                                               const char ***out_roots) {
    static char input_root_direct[PATH_MAX];
    static const char *roots[2];
    size_t count = 0;
    const char *input_root = physics_sim_resolve_input_root(configured_input_root);

    if (snprintf(input_root_direct, sizeof(input_root_direct), "%s", input_root) < (int)sizeof(input_root_direct)) {
        physics_sim_append_unique_root(roots, &count, sizeof(roots) / sizeof(roots[0]), input_root_direct);
    }
    if (out_roots) {
        *out_roots = roots;
    }
    return count;
}

/* Fixed app-owned directory graph. Validate every existing slot before mkdir. */
static bool runtime_same_directory(int parent, const char *name, int descriptor) {
    struct stat named, opened;
    return descriptor >= 0 && fstat(descriptor, &opened) == 0 &&
        fstatat(parent, name, &named, AT_SYMLINK_NOFOLLOW) == 0 &&
        S_ISDIR(named.st_mode) && named.st_dev == opened.st_dev && named.st_ino == opened.st_ino;
}

static bool runtime_root_current(const char *path, int descriptor) {
    struct stat named, opened;
    return lstat(path, &named) == 0 && S_ISDIR(named.st_mode) &&
        fstat(descriptor, &opened) == 0 && named.st_dev == opened.st_dev && named.st_ino == opened.st_ino;
}

static bool runtime_root_admitted(char *path, size_t size) {
    if (!getcwd(path, size)) return false;
    const char *protected[] = {"/System", "/usr", "/bin", "/sbin", "/etc", "/private/etc",
        "/Library", "/Applications", "/dev", "/proc", "/sys"};
    if (strcmp(path, "/") == 0) return false;
    const char *home = getenv("HOME");
    if (home) {
        char resolved_home[PATH_MAX];
        if (!realpath(home, resolved_home) || strcmp(path, resolved_home) == 0) return false;
    }
    for (size_t i = 0; i < sizeof(protected) / sizeof(protected[0]); ++i) {
        size_t n = strlen(protected[i]);
        if (strncmp(path, protected[i], n) == 0 && (path[n] == 0 || path[n] == '/')) return false;
    }
    char ancestor[PATH_MAX];
    if (snprintf(ancestor, sizeof(ancestor), "%s", path) >= (int)sizeof(ancestor)) return false;
    for (;;) {
        char marker[PATH_MAX]; struct stat st;
        if (snprintf(marker, sizeof(marker), "%s/.git", ancestor) >= (int)sizeof(marker)) return false;
        int marker_result = lstat(marker, &st);
        if (marker_result == 0 && strcmp(ancestor, path) != 0) return false;
        if (marker_result != 0 && errno != ENOENT && errno != ENOTDIR) return false;
        char *slash = strrchr(ancestor, '/');
        if (!slash || slash == ancestor) break;
        *slash = 0;
    }
    return true;
}

bool physics_sim_ensure_runtime_dirs(void) {
    static const char *names[] = {"data", "runtime", "scenes", "snapshots"};
    static const int parents[] = {-1, 0, 1, 0};
    int descriptors[] = {-1, -1, -1, -1};
    char path[PATH_MAX];
    if (!runtime_root_admitted(path, sizeof(path))) return false;
    int root = open(".", O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    bool valid = root >= 0 && runtime_root_current(path, root);
    /* Missing ancestors imply missing descendants. Existing invalid leaves hold
       the whole operation before any directory is allocated. */
    for (size_t i = 0; valid && i < 4; ++i) {
        int parent = parents[i] < 0 ? root : descriptors[parents[i]];
        if (parent < 0) continue;
        struct stat st;
        if (fstatat(parent, names[i], &st, AT_SYMLINK_NOFOLLOW) != 0) {
            valid = errno == ENOENT;
            continue;
        }
        if (!S_ISDIR(st.st_mode)) { valid = false; break; }
        descriptors[i] = openat(parent, names[i], O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
        valid = runtime_same_directory(parent, names[i], descriptors[i]);
    }
    for (size_t i = 0; valid && i < 4; ++i) {
        valid = runtime_root_current(path, root);
        for (size_t j = 0; valid && j < 4; ++j) {
            if (descriptors[j] < 0) continue;
            int parent = parents[j] < 0 ? root : descriptors[parents[j]];
            valid = runtime_same_directory(parent, names[j], descriptors[j]);
        }
        if (!valid) break;
        int parent = parents[i] < 0 ? root : descriptors[parents[i]];
        if (descriptors[i] < 0) {
            valid = mkdirat(parent, names[i], 0700) == 0 || errno == EEXIST;
            if (valid) descriptors[i] = openat(parent, names[i], O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
        }
        valid = valid && runtime_same_directory(parent, names[i], descriptors[i]) &&
            fsync(descriptors[i]) == 0 && fsync(parent) == 0;
    }
    valid = valid && runtime_root_current(path, root);
    for (size_t i = 0; valid && i < 4; ++i) {
        int parent = parents[i] < 0 ? root : descriptors[parents[i]];
        valid = runtime_same_directory(parent, names[i], descriptors[i]);
    }
    for (size_t i = 0; i < 4; ++i) if (descriptors[i] >= 0) close(descriptors[i]);
    if (root >= 0) close(root);
    return valid;
}
