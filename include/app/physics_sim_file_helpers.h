#ifndef PHYSICS_SIM_FILE_HELPERS_H
#define PHYSICS_SIM_FILE_HELPERS_H

#include <stdbool.h>
#include <stddef.h>

bool physics_sim_copy_string(char *dst, size_t dst_size, const char *src);
bool physics_sim_file_exists(const char *path);
bool physics_sim_dir_is_empty(const char *path);
bool physics_sim_parent_dir_of(const char *path, char *out_dir, size_t out_dir_size);
bool physics_sim_ensure_directory_exists(const char *path);
bool physics_sim_ensure_parent_directory_exists(const char *path);
bool physics_sim_read_text_file(const char *path, char **out_text);
bool physics_sim_write_text_file(const char *path, const char *text);
bool physics_sim_resolve_real_path(const char *path, char *out, size_t out_size);

#endif
