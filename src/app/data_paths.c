#include "app/data_paths.h"

#include <dirent.h>
#include <errno.h>
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

bool physics_sim_ensure_runtime_dirs(void) {
    if (mkdir("data", 0755) != 0 && errno != EEXIST) {
        return false;
    }
    if (mkdir("data/runtime", 0755) != 0 && errno != EEXIST) {
        return false;
    }
    if (mkdir("data/runtime/scenes", 0755) != 0 && errno != EEXIST) {
        return false;
    }
    if (mkdir("data/snapshots", 0755) != 0 && errno != EEXIST) {
        return false;
    }
    return true;
}
