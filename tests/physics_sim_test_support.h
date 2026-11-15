#ifndef PHYSICS_SIM_TEST_SUPPORT_H
#define PHYSICS_SIM_TEST_SUPPORT_H

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#if defined(__GNUC__) || defined(__clang__)
#define PHYSICS_SIM_TEST_SUPPORT_UNUSED __attribute__((unused))
#else
#define PHYSICS_SIM_TEST_SUPPORT_UNUSED
#endif

static PHYSICS_SIM_TEST_SUPPORT_UNUSED void physics_sim_test_temp_path(char *out,
                                                                       size_t capacity,
                                                                       const char *leaf) {
    const char *root = getenv("TMPDIR");
    if (!root || !root[0]) root = "/tmp";
    if (!out || !capacity) return;
    int length = snprintf(out, capacity, "%s/%s", root, leaf);
    if (root[0] != '/' || length < 0 || (size_t)length >= capacity) out[0] = '\0';
}

static PHYSICS_SIM_TEST_SUPPORT_UNUSED bool physics_sim_test_write_text_file(const char *path,
                                                                             const char *text) {
    FILE *f = NULL;
    size_t len = 0u;
    size_t written = 0u;
    if (!path || !text) return false;
    f = fopen(path, "wb");
    if (!f) return false;
    len = strlen(text);
    written = fwrite(text, 1u, len, f);
    fclose(f);
    return written == len;
}

static PHYSICS_SIM_TEST_SUPPORT_UNUSED bool physics_sim_test_read_text_file(const char *path,
                                                                            char *out,
                                                                            size_t out_size) {
    FILE *f = NULL;
    size_t size = 0u;
    if (!path || !out || out_size == 0u) return false;
    f = fopen(path, "rb");
    if (!f) return false;
    size = fread(out, 1u, out_size - 1u, f);
    fclose(f);
    out[size] = '\0';
    return true;
}

static PHYSICS_SIM_TEST_SUPPORT_UNUSED bool physics_sim_test_make_dir(const char *path, mode_t mode) {
    if (!path || !path[0]) return false;
    return mkdir(path, mode) == 0;
}

static PHYSICS_SIM_TEST_SUPPORT_UNUSED void physics_sim_test_remove_file_if_exists(const char *path) {
    if (!path || !path[0]) return;
    (void)remove(path);
}

static PHYSICS_SIM_TEST_SUPPORT_UNUSED void physics_sim_test_remove_dir_if_exists(const char *path) {
    if (!path || !path[0]) return;
    (void)rmdir(path);
}

#endif
