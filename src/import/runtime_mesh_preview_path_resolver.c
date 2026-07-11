#include "import/runtime_mesh_preview_path_resolver.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#if defined(__linux__) || defined(PHYSICS_SIM_RUNTIME_MESH_PATH_RESOLVER_FORCE_LINUX)
#define PHYSICS_SIM_RUNTIME_MESH_PATH_RESOLVER_LINUX 1
#else
#define PHYSICS_SIM_RUNTIME_MESH_PATH_RESOLVER_LINUX 0
#endif

#define PHYSICS_SIM_RUNTIME_MESH_PREVIEW_PATH_BUFFER 4096u

static bool path_exists(const char *path) {
    struct stat st;
    return path && path[0] && stat(path, &st) == 0;
}

static bool compose_stls_path(const char *root,
                              const char *tail,
                              char *out_path,
                              size_t out_path_size) {
    if (!root || root[0] != '/' || !tail || !tail[0] || !out_path || out_path_size == 0u) {
        return false;
    }
    if (snprintf(out_path, out_path_size, "%s/stls/%s", root, tail) >= (int)out_path_size) {
        out_path[0] = '\0';
        return false;
    }
    if (!path_exists(out_path)) {
        out_path[0] = '\0';
        return false;
    }
    return true;
}

#if PHYSICS_SIM_RUNTIME_MESH_PATH_RESOLVER_LINUX
static bool compose_data_stls_path(const char *data_home,
                                   const char *tail,
                                   char *out_path,
                                   size_t out_path_size) {
    char root[PHYSICS_SIM_RUNTIME_MESH_PREVIEW_PATH_BUFFER] = {0};
    if (!data_home || data_home[0] != '/') return false;
    if (snprintf(root, sizeof(root), "%s/PhysicsSim", data_home) >= (int)sizeof(root)) {
        return false;
    }
    return compose_stls_path(root, tail, out_path, out_path_size);
}

static void trim_trailing_space(char *text) {
    size_t length = 0u;
    if (!text) return;
    length = strlen(text);
    while (length > 0u && (text[length - 1u] == ' ' || text[length - 1u] == '\t' ||
                           text[length - 1u] == '\r' || text[length - 1u] == '\n')) {
        text[--length] = '\0';
    }
}

static bool expand_home_value(const char *value,
                              const char *home,
                              char *out_path,
                              size_t out_path_size) {
    if (!value || !home || home[0] != '/' || !out_path || out_path_size == 0u) return false;
    if (strncmp(value, "$HOME/", 6u) == 0) {
        return snprintf(out_path, out_path_size, "%s/%s", home, value + 6u) < (int)out_path_size;
    }
    if (strcmp(value, "$HOME") == 0) {
        return snprintf(out_path, out_path_size, "%s", home) < (int)out_path_size;
    }
    if (value[0] != '/') return false;
    return snprintf(out_path, out_path_size, "%s", value) < (int)out_path_size;
}

static bool read_xdg_desktop_dir(const char *home,
                                 char *out_desktop_dir,
                                 size_t out_desktop_dir_size) {
    const char *config_home = getenv("XDG_CONFIG_HOME");
    char config_path[PHYSICS_SIM_RUNTIME_MESH_PREVIEW_PATH_BUFFER] = {0};
    FILE *file = NULL;
    char line[PHYSICS_SIM_RUNTIME_MESH_PREVIEW_PATH_BUFFER] = {0};

    if (!home || home[0] != '/' || !out_desktop_dir || out_desktop_dir_size == 0u) return false;
    if (!config_home || config_home[0] != '/') {
        if (snprintf(config_path, sizeof(config_path), "%s/.config/user-dirs.dirs", home) >=
            (int)sizeof(config_path)) {
            return false;
        }
    } else if (snprintf(config_path, sizeof(config_path), "%s/user-dirs.dirs", config_home) >=
               (int)sizeof(config_path)) {
        return false;
    }
    file = fopen(config_path, "r");
    if (!file) return false;
    while (fgets(line, sizeof(line), file)) {
        char *value = NULL;
        trim_trailing_space(line);
        if (strncmp(line, "XDG_DESKTOP_DIR=", 16u) != 0) continue;
        value = line + 16u;
        if (value[0] == '"') {
            size_t length = strlen(value);
            if (length < 2u || value[length - 1u] != '"') continue;
            value[length - 1u] = '\0';
            value += 1;
        }
        (void)fclose(file);
        return expand_home_value(value, home, out_desktop_dir, out_desktop_dir_size);
    }
    (void)fclose(file);
    return false;
}
#endif

bool physics_sim_runtime_mesh_preview_resolve_migrated_path(const char *candidate,
                                                            char *out_path,
                                                            size_t out_path_size) {
    const char *desktop_segment = NULL;
    const char *tail = NULL;
    const char *home = NULL;

    if (!candidate || candidate[0] != '/' || !out_path || out_path_size == 0u) return false;
    out_path[0] = '\0';
    desktop_segment = strstr(candidate, "/Desktop/");
    if (!desktop_segment) return false;
    tail = desktop_segment + strlen("/Desktop/");
    if (!tail[0]) return false;
    home = getenv("HOME");
    if (!home || home[0] != '/') return false;

#if PHYSICS_SIM_RUNTIME_MESH_PATH_RESOLVER_LINUX
    {
        const char *data_home = getenv("XDG_DATA_HOME");
        char desktop_dir[PHYSICS_SIM_RUNTIME_MESH_PREVIEW_PATH_BUFFER] = {0};
        char default_data_home[PHYSICS_SIM_RUNTIME_MESH_PREVIEW_PATH_BUFFER] = {0};
        if (!data_home || data_home[0] != '/') {
            if (snprintf(default_data_home, sizeof(default_data_home), "%s/.local/share", home) <
                (int)sizeof(default_data_home)) {
                data_home = default_data_home;
            }
        }
        if (compose_data_stls_path(data_home, tail, out_path, out_path_size)) return true;
        if (read_xdg_desktop_dir(home, desktop_dir, sizeof(desktop_dir)) &&
            compose_stls_path(desktop_dir, tail, out_path, out_path_size)) {
            return true;
        }
    }
#endif
    {
        char legacy_desktop_dir[PHYSICS_SIM_RUNTIME_MESH_PREVIEW_PATH_BUFFER] = {0};
        if (snprintf(legacy_desktop_dir, sizeof(legacy_desktop_dir), "%s/Desktop", home) >=
            (int)sizeof(legacy_desktop_dir)) {
            return false;
        }
        return compose_stls_path(legacy_desktop_dir, tail, out_path, out_path_size);
    }
}
