#ifndef PHYSICS_SIM_HEADLESS_OUTPUT_H
#define PHYSICS_SIM_HEADLESS_OUTPUT_H
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/stat.h>
typedef struct PhysicsSimHeadlessOutputOwner {
    char root[1024];
    int descriptor;
    struct stat running_marker;
} PhysicsSimHeadlessOutputOwner;
bool physics_sim_headless_storage_directory(const char *path, char *normalized_path, size_t size);
bool physics_sim_headless_output_prepare(const char *root, const char *input,
    bool overwrite, PhysicsSimHeadlessOutputOwner *owner, char *error, size_t error_size);
/* Read-only current producer receipt admission; not process authentication. */
bool physics_sim_headless_output_completed(const char *root);
bool physics_sim_headless_output_finish(PhysicsSimHeadlessOutputOwner *owner);
void physics_sim_headless_output_close(PhysicsSimHeadlessOutputOwner *owner);
typedef struct PhysicsSimHeadlessSidecar {
    char path[1024];
    char parent[1024];
    int descriptor;
    int parent_descriptor;
    int pending_descriptor;
    int pending_stream_descriptor;
    unsigned long sequence;
    char pending_name[128];
    bool held;
    bool missing;
    bool track_predecessor;
    struct stat predecessor;
} PhysicsSimHeadlessSidecar;
bool physics_sim_headless_sidecar_plan(const char *output, const char *input,
    const char *selected, const char *fallback, bool overwrite,
    char *path, size_t size, char *error, size_t error_size);
/* Begin an admitted replacement, or an absent destination without empty reservation. */
bool physics_sim_headless_sidecar_replace(const char *path, PhysicsSimHeadlessSidecar *sidecar);
bool physics_sim_headless_sidecar_claim(const char *path, PhysicsSimHeadlessSidecar *sidecar);
bool physics_sim_headless_sidecar_check(const PhysicsSimHeadlessSidecar *sidecar);
FILE *physics_sim_headless_sidecar_stream(PhysicsSimHeadlessSidecar *sidecar);
bool physics_sim_headless_sidecar_publish(PhysicsSimHeadlessSidecar *sidecar, FILE *stream);
void physics_sim_headless_sidecar_close(PhysicsSimHeadlessSidecar *sidecar);
#endif
