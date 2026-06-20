#include "app/physics_sim_file_helpers.h"

#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

bool physics_sim_copy_string(char *dst, size_t dst_size, const char *src) {
    if (!dst || dst_size == 0u || !src) return false;
    if (snprintf(dst, dst_size, "%s", src) >= (int)dst_size) {
        dst[0] = '\0';
        return false;
    }
    return true;
}

bool physics_sim_file_exists(const char *path) {
    return path && path[0] && access(path, F_OK) == 0;
}

bool physics_sim_dir_is_empty(const char *path) {
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

bool physics_sim_parent_dir_of(const char *path, char *out_dir, size_t out_dir_size) {
    const char *slash = NULL;
    size_t len = 0u;
    if (!path || !path[0] || !out_dir || out_dir_size == 0u) return false;
    slash = strrchr(path, '/');
    if (!slash) return physics_sim_copy_string(out_dir, out_dir_size, ".");
    len = (size_t)(slash - path);
    if (len == 0u) return physics_sim_copy_string(out_dir, out_dir_size, "/");
    if (len >= out_dir_size) len = out_dir_size - 1u;
    memcpy(out_dir, path, len);
    out_dir[len] = '\0';
    return true;
}

bool physics_sim_ensure_directory_exists(const char *path) {
    char tmp[PATH_MAX];
    size_t len = 0u;
    if (!path || !path[0]) return false;
    len = strlen(path);
    if (len >= sizeof(tmp)) return false;
    memcpy(tmp, path, len + 1u);
    for (size_t i = 1u; i < len; ++i) {
        if (tmp[i] != '/') continue;
        tmp[i] = '\0';
        if (tmp[0] != '\0' && mkdir(tmp, 0700) != 0 && errno != EEXIST) return false;
        tmp[i] = '/';
    }
    if (mkdir(tmp, 0700) != 0 && errno != EEXIST) return false;
    return true;
}

bool physics_sim_ensure_parent_directory_exists(const char *path) {
    char dir[PATH_MAX];
    if (!physics_sim_parent_dir_of(path, dir, sizeof(dir))) return false;
    return physics_sim_ensure_directory_exists(dir);
}

bool physics_sim_read_text_file(const char *path, char **out_text) {
    FILE *file = NULL;
    long size = 0;
    char *text = NULL;
    size_t read_count = 0u;
    if (!path || !path[0] || !out_text) return false;
    *out_text = NULL;
    file = fopen(path, "rb");
    if (!file) return false;
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return false;
    }
    size = ftell(file);
    if (size < 0) {
        fclose(file);
        return false;
    }
    if (fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return false;
    }
    text = (char *)malloc((size_t)size + 1u);
    if (!text) {
        fclose(file);
        return false;
    }
    read_count = fread(text, 1u, (size_t)size, file);
    fclose(file);
    if (read_count != (size_t)size) {
        free(text);
        return false;
    }
    text[size] = '\0';
    *out_text = text;
    return true;
}

bool physics_sim_write_text_file(const char *path, const char *text) {
    FILE *file = NULL;
    if (!path || !path[0] || !text) return false;
    if (!physics_sim_ensure_parent_directory_exists(path)) return false;
    file = fopen(path, "wb");
    if (!file) return false;
    if (fputs(text, file) < 0) {
        fclose(file);
        return false;
    }
    fclose(file);
    return true;
}

bool physics_sim_resolve_real_path(const char *path, char *out, size_t out_size) {
    char resolved[PATH_MAX];
    if (!path || !path[0] || !out || out_size == 0u) return false;
    if (!realpath(path, resolved)) return false;
    return physics_sim_copy_string(out, out_size, resolved);
}
