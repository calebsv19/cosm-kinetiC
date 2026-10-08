#include "app/scene_project_cache_output.h"
#include "export/volume_frame_vf3d_contract.h"

#include "physics_sim_test_support.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

static bool ensure_dir_existing_ok(const char *path) {
    if (mkdir(path, 0755) == 0) return true;
    return errno == EEXIST;
}

static void cleanup_project(const char *root) {
    char path[512];
    snprintf(path, sizeof(path), "%s/assets/physics/active/scene_bundle.json", root);
    physics_sim_test_remove_file_if_exists(path);
    snprintf(path, sizeof(path), "%s/assets/physics/active/manifest.json", root);
    physics_sim_test_remove_file_if_exists(path);
    snprintf(path, sizeof(path), "%s/assets/physics/active", root);
    physics_sim_test_remove_dir_if_exists(path);
    snprintf(path, sizeof(path), "%s/assets/physics", root);
    physics_sim_test_remove_dir_if_exists(path);
    snprintf(path, sizeof(path), "%s/assets/vf3d/active/manifest.json", root);
    physics_sim_test_remove_file_if_exists(path);
    for (int i = 0; i < 3; ++i) {
        snprintf(path, sizeof(path), "%s/assets/vf3d/active/frame_%06d.vf3d", root, 2+i*4);
        physics_sim_test_remove_file_if_exists(path);
    }

    snprintf(path, sizeof(path), "%s/assets/vf3d/active", root);
    physics_sim_test_remove_dir_if_exists(path);
    snprintf(path, sizeof(path), "%s/assets/vf3d", root);
    physics_sim_test_remove_dir_if_exists(path);
    snprintf(path, sizeof(path), "%s/assets", root);
    physics_sim_test_remove_dir_if_exists(path);
    snprintf(path, sizeof(path), "%s/physics_sim/active_cache_manifest.json", root);
    physics_sim_test_remove_file_if_exists(path);
    snprintf(path, sizeof(path), "%s/physics_sim/cache_manifest.json", root);
    physics_sim_test_remove_file_if_exists(path);
    snprintf(path, sizeof(path), "%s/physics_sim", root);
    physics_sim_test_remove_dir_if_exists(path);
    snprintf(path, sizeof(path), "%s/scene_project.json", root);
    physics_sim_test_remove_file_if_exists(path);
    snprintf(path, sizeof(path), "%s/scene_runtime.json", root);
    physics_sim_test_remove_file_if_exists(path);
    snprintf(path, sizeof(path), "%s/scene_authoring.json", root);
    physics_sim_test_remove_file_if_exists(path);
    physics_sim_test_remove_dir_if_exists(root);
}

static bool create_minimal_project(const char *root) {
    char path[512];
    if (!ensure_dir_existing_ok(root)) return false;
    snprintf(path, sizeof(path), "%s/scene_runtime.json", root);
    if (!physics_sim_test_write_text_file(path,
                                          "{\n"
                                          "  \"schema_variant\": \"scene_runtime_v1\",\n"
                                          "  \"scene_id\": \"cache_status_contract\"\n"
                                          "}\n")) {
        return false;
    }
    snprintf(path, sizeof(path), "%s/scene_authoring.json", root);
    if (!physics_sim_test_write_text_file(path,
                                          "{\n"
                                          "  \"scene_name\": \"Cache Status Contract\"\n"
                                          "}\n")) {
        return false;
    }
    snprintf(path, sizeof(path), "%s/scene_project.json", root);
    return physics_sim_test_write_text_file(path,
                                            "{\n"
                                            "  \"schema\": \"scene_project_v1\"\n"
                                            "}\n");
}

static bool create_active_cache_artifact_dirs(const char *root) {
    char path[512];
    const char *dirs[] = {"assets", "assets/vf3d", "assets/vf3d/active", "assets/physics", "assets/physics/active"};
    for (size_t i = 0; i < sizeof(dirs)/sizeof(dirs[0]); ++i) {
        snprintf(path, sizeof(path), "%s/%s", root, dirs[i]); if (!ensure_dir_existing_ok(path)) return false;
    }
    const char *manifest = "{\"manifest_version\":2,\"frame_contract\":\"vf3d\",\"space_mode\":\"3d\",\"grid_w\":1,\"grid_h\":1,\"grid_d\":1,\"frames\":[{\"frame_index\":2,\"path\":\"frame_000002.vf3d\",\"frame_contract\":\"vf3d\"},{\"frame_index\":6,\"path\":\"frame_000006.vf3d\",\"frame_contract\":\"vf3d\"},{\"frame_index\":10,\"path\":\"frame_000010.vf3d\",\"frame_contract\":\"vf3d\"}]}";
    snprintf(path, sizeof(path), "%s/assets/vf3d/active/manifest.json", root);
    if (!physics_sim_test_write_text_file(path, manifest)) return false;
    snprintf(path, sizeof(path), "%s/assets/physics/active/manifest.json", root);
    if (!physics_sim_test_write_text_file(path, manifest)) return false;
    snprintf(path, sizeof(path), "%s/assets/physics/active/scene_bundle.json", root);
    if (!physics_sim_test_write_text_file(path, "{\"bundle_type\":\"physics_scene_bundle_v1\",\"bundle_version\":1,\"fluid_source\":{\"kind\":\"manifest\",\"path\":\"manifest.json\",\"contract\":\"vf3d\"}}")) return false;
    for (int i = 0; i < 3; ++i) {
        VolumeFrameHeaderVf3dV1 header = {0}; header.magic = 0x56463344; header.version = 1;
        header.grid_w = header.grid_h = header.grid_d = 1; header.frame_index = (uint64_t)(2+i*4);
        header.solid_mask_crc32 = UINT32_C(2166136261)*UINT32_C(16777619);
        header.dt_seconds = 1.0/60.0; header.voxel_size = header.scene_up_z = 1;
        unsigned char payload[21] = {0};
        snprintf(path, sizeof(path), "%s/assets/vf3d/active/frame_%06d.vf3d", root, 2+i*4);
        FILE *file = fopen(path, "wb"); if (!file) return false;
        bool ok = fwrite(&header, sizeof(header), 1, file) == 1 && fwrite(payload, sizeof(payload), 1, file) == 1;
        if (fclose(file) != 0) ok = false;
        if (!ok) return false;
    }
    return true;
}

static bool test_project_without_cache_reports_command(void) {
    const char *root = "tmp/scene_project_cache_status_no_cache";
    SceneProjectCacheOutputStatus status = {0};
    char error[160];
    char command[SCENE_PROJECT_CACHE_OUTPUT_COMMAND_MAX];
    cleanup_project(root);
    if (!create_minimal_project(root)) {
        cleanup_project(root);
        return false;
    }
    if (!scene_project_cache_output_status_from_project(root, &status, error, sizeof(error))) {
        cleanup_project(root);
        return false;
    }
    if (!scene_project_cache_output_make_update_command(root,
                                                        12,
                                                        32,
                                                        16,
                                                        8,
                                                        command,
                                                        sizeof(command))) {
        cleanup_project(root);
        return false;
    }
    cleanup_project(root);
    if (!status.is_scene_project) return false;
    if (status.has_active_manifest || status.has_compat_manifest) return false;
    if (strcmp(status.summary, "Active Run: none yet") != 0) return false;
    if (strcmp(status.cache_target_summary, "Cache Target: no active cache yet") != 0) return false;
    if (status.active_cache_ready) return false;
    return strstr(command, "--scene-project \"tmp/scene_project_cache_status_no_cache\"") != NULL &&
           strstr(command, "--frames 12") != NULL &&
           strstr(command, "--grid 32x16x8") != NULL &&
           strstr(command, "--save-volume-frames --overwrite") != NULL;
}

static bool test_project_active_cache_manifest_reports_run(void) {
    const char *root = "tmp/scene_project_cache_status_active";
    SceneProjectCacheOutputStatus status = {0};
    char error[160];
    char runtime_path[512];
    char physics_dir[512];
    char manifest_path[512];
    cleanup_project(root);
    if (!create_minimal_project(root)) {
        cleanup_project(root);
        return false;
    }
    snprintf(physics_dir, sizeof(physics_dir), "%s/physics_sim", root);
    if (!ensure_dir_existing_ok(physics_dir)) {
        cleanup_project(root);
        return false;
    }
    snprintf(manifest_path, sizeof(manifest_path), "%s/active_cache_manifest.json", physics_dir);
    if (!physics_sim_test_write_text_file(manifest_path,
                                          "{\n"
                                          "  \"schema\": \"physics_sim_active_cache_manifest_v1\",\n"
                                          "  \"vf3d_active_dir\": \"assets/vf3d/active\",\n"
                                          "  \"physics_active_dir\": \"assets/physics/active\",\n"
                                          "  \"scene_bundle\": \"assets/physics/active/scene_bundle.json\",\n"
                                          "  \"active_run_id\": \"physics-run-contract-0001\",\n"
                                          "  \"frame_count\": 3,\n"
                                          "  \"export_start_frame\": 2,\n"
                                          "  \"export_stride\": 4,\n"
                                          "  \"export_max_frames\": 3\n"
                                          "}\n")) {
        cleanup_project(root);
        return false;
    }
    if (!create_active_cache_artifact_dirs(root)) {
        cleanup_project(root);
        return false;
    }
    snprintf(runtime_path, sizeof(runtime_path), "%s/scene_runtime.json", root);
    if (!scene_project_cache_output_status_from_runtime_scene(runtime_path,
                                                              &status,
                                                              error,
                                                              sizeof(error))) {
        cleanup_project(root);
        return false;
    }
    cleanup_project(root);
    if (!status.is_scene_project || !status.has_active_manifest) return false;
    if (strcmp(status.active_run_id, "physics-run-contract-0001") != 0) return false;
    if (status.frame_count != 3) return false;
    if (status.export_start_frame != 2 || status.export_stride != 4 || status.export_max_frames != 3) {
        return false;
    }
    if (!status.has_vf3d_active_dir || !status.has_physics_active_dir ||
        !status.has_scene_bundle || !status.active_cache_ready) {
        return false;
    }
    return strcmp(status.cache_target_summary,
                  "Cache Target: VF3D active + physics bundle ready") == 0 &&
           strcmp(status.summary,
                  "Active Run: physics-run-contract-0001 (3 frames, start 2, stride 4, max 3)") == 0;
}

int main(void) {
    if (!ensure_dir_existing_ok("tmp")) {
        fprintf(stderr, "scene_project_cache_output_status_contract_test: tmp setup failed\n");
        return 1;
    }
    if (!test_project_without_cache_reports_command()) {
        fprintf(stderr, "scene_project_cache_output_status_contract_test: no-cache status failed\n");
        return 1;
    }
    if (!test_project_active_cache_manifest_reports_run()) {
        fprintf(stderr, "scene_project_cache_output_status_contract_test: active-cache status failed\n");
        return 1;
    }
    fprintf(stdout, "scene_project_cache_output_status_contract_test: success\n");
    return 0;
}
