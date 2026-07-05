#ifndef SCENE_PROJECT_CACHE_OUTPUT_H
#define SCENE_PROJECT_CACHE_OUTPUT_H

#include <stdbool.h>
#include <stddef.h>

#define SCENE_PROJECT_CACHE_OUTPUT_PATH_MAX 1024
#define SCENE_PROJECT_CACHE_OUTPUT_RUN_ID_MAX 64
#define SCENE_PROJECT_CACHE_OUTPUT_STATUS_MAX 160
#define SCENE_PROJECT_CACHE_OUTPUT_COMMAND_MAX 1536

typedef struct SceneProjectCacheOutputResolved {
    char project_root[SCENE_PROJECT_CACHE_OUTPUT_PATH_MAX];
    char scene_project_path[SCENE_PROJECT_CACHE_OUTPUT_PATH_MAX];
    char scene_authoring_path[SCENE_PROJECT_CACHE_OUTPUT_PATH_MAX];
    char scene_runtime_path[SCENE_PROJECT_CACHE_OUTPUT_PATH_MAX];
    bool has_scene_project;
} SceneProjectCacheOutputResolved;

typedef struct SceneProjectCacheOutputPublishRequest {
    const SceneProjectCacheOutputResolved *project;
    const char *run_id;
    const char *run_output_root;
    bool allow_overwrite;
    int source_frame_count;
    int frame_count;
    int export_start_frame;
    int export_stride;
    int export_max_frames;
} SceneProjectCacheOutputPublishRequest;

typedef struct SceneProjectCacheOutputStatus {
    bool is_scene_project;
    bool has_active_manifest;
    bool has_compat_manifest;
    bool has_vf3d_active_dir;
    bool has_physics_active_dir;
    bool has_scene_bundle;
    bool active_cache_ready;
    char project_root[SCENE_PROJECT_CACHE_OUTPUT_PATH_MAX];
    char manifest_path[SCENE_PROJECT_CACHE_OUTPUT_PATH_MAX];
    char active_run_id[SCENE_PROJECT_CACHE_OUTPUT_RUN_ID_MAX];
    int frame_count;
    int export_start_frame;
    int export_stride;
    int export_max_frames;
    char cache_target_summary[SCENE_PROJECT_CACHE_OUTPUT_STATUS_MAX];
    char summary[SCENE_PROJECT_CACHE_OUTPUT_STATUS_MAX];
    char update_command[SCENE_PROJECT_CACHE_OUTPUT_COMMAND_MAX];
} SceneProjectCacheOutputStatus;

bool scene_project_cache_output_resolve(const char *project_root,
                                        SceneProjectCacheOutputResolved *out,
                                        char *error,
                                        size_t error_size);

bool scene_project_cache_output_status_from_project(const char *project_root,
                                                    SceneProjectCacheOutputStatus *out,
                                                    char *error,
                                                    size_t error_size);

bool scene_project_cache_output_status_from_runtime_scene(const char *runtime_scene_path,
                                                          SceneProjectCacheOutputStatus *out,
                                                          char *error,
                                                          size_t error_size);

bool scene_project_cache_output_make_update_command(const char *project_root,
                                                    int frames,
                                                    int grid_w,
                                                    int grid_h,
                                                    int grid_d,
                                                    char *out,
                                                    size_t out_size);

bool scene_project_cache_output_make_run_id(char *out,
                                            size_t out_size,
                                            char *created_at,
                                            size_t created_at_size);

bool scene_project_cache_output_default_run_root(const char *project_root,
                                                 const char *run_id,
                                                 char *out,
                                                 size_t out_size);

bool scene_project_cache_output_publish(const SceneProjectCacheOutputPublishRequest *request,
                                        char *error,
                                        size_t error_size);

#endif
