#include "export/volume_frames_vf3d.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"

static const uint32_t VOLUME_VF3D_MAGIC = ('V' << 24) | ('F' << 16) | ('3' << 8) | ('D');
static const uint32_t VOLUME_VF3D_VERSION_V1 = 1u;

static uint32_t solid_mask_crc32_fnv1a(const uint8_t *solid_mask, size_t count) {
    uint32_t hash = 2166136261u;
    if (!solid_mask) return 0u;
    for (size_t i = 0; i < count; ++i) {
        hash ^= (uint32_t)solid_mask[i];
        hash *= 16777619u;
    }
    return hash;
}

static void json_set_number(cJSON *obj, const char *name, double value) {
    if (!obj || !name) return;
    cJSON_DeleteItemFromObject(obj, name);
    cJSON_AddNumberToObject(obj, name, value);
}

static void json_set_string(cJSON *obj, const char *name, const char *value) {
    if (!obj || !name || !value) return;
    cJSON_DeleteItemFromObject(obj, name);
    cJSON_AddStringToObject(obj, name, value);
}

static const char *path_basename_or_self(const char *path) {
    const char *last_slash = NULL;
    if (!path || !path[0]) return path;
    last_slash = strrchr(path, '/');
    if (last_slash && last_slash[1]) return last_slash + 1;
    return path;
}

static cJSON *manifest_root_open_or_create(const SceneState *scene,
                                           const char *run_name,
                                           const char *manifest_path,
                                           int manifest_version) {
    FILE *f = NULL;
    cJSON *root = NULL;
    if (!manifest_path || !manifest_path[0]) return NULL;

    f = fopen(manifest_path, "rb");
    if (f) {
        long sz = 0;
        fseek(f, 0, SEEK_END);
        sz = ftell(f);
        fseek(f, 0, SEEK_SET);
        if (sz > 0) {
            char *buf = (char *)malloc((size_t)sz + 1u);
            if (buf) {
                size_t n = fread(buf, 1, (size_t)sz, f);
                if (n == (size_t)sz) {
                    buf[sz] = '\0';
                    root = cJSON_Parse(buf);
                }
                free(buf);
            }
        }
        fclose(f);
    }

    if (root) return root;

    root = cJSON_CreateObject();
    if (!root) return NULL;
    cJSON_AddNumberToObject(root, "manifest_version", manifest_version);
    cJSON_AddStringToObject(root, "run_name", run_name ? run_name : "run");
    if (scene && scene->preset && scene->preset->name) {
        cJSON_AddStringToObject(root, "preset", scene->preset->name);
    }
    if (scene && scene->import_shape_count > 0) {
        cJSON *imports = cJSON_AddArrayToObject(root, "imports");
        if (imports) {
            for (size_t i = 0; i < scene->import_shape_count; ++i) {
                const ImportedShape *imp = &scene->import_shapes[i];
                cJSON *obj = NULL;
                if (!imp->path[0]) continue;
                obj = cJSON_CreateObject();
                if (!obj) continue;
                cJSON_AddStringToObject(obj, "path", imp->path);
                cJSON_AddNumberToObject(obj, "pos_x_norm", imp->position_x);
                cJSON_AddNumberToObject(obj, "pos_y_norm", imp->position_y);
                cJSON_AddNumberToObject(obj, "rotation_deg", imp->rotation_deg);
                cJSON_AddNumberToObject(obj, "scale", imp->scale);
                cJSON_AddBoolToObject(obj, "is_static", imp->is_static);
                cJSON_AddItemToArray(imports, obj);
            }
        }
    }
    cJSON_AddArrayToObject(root, "frames");
    return root;
}

bool volume_frames_should_export_vf3d(const SceneState *scene,
                                      SimRuntimeBackendReport *out_report) {
    SimRuntimeBackendReport report = {0};
    bool authoritative_3d = false;

    if (out_report) memset(out_report, 0, sizeof(*out_report));
    if (!scene) return false;
    if (scene->mode_route.requested_space_mode != SPACE_MODE_3D) return false;
    if (!scene_backend_report(scene, &report)) return false;

    authoritative_3d = report.full_3d_solver_live ||
                       report.volumetric_emitters_free_live ||
                       report.volumetric_emitters_attached_live ||
                       report.volumetric_obstacles_live;
    if (!authoritative_3d) return false;
    if (!report.debug_volume_view_3d_available) return false;

    if (out_report) *out_report = report;
    return true;
}

bool volume_frame_write_vf3d_raw(const SceneState *scene,
                                 uint64_t frame_index,
                                 const char *path,
                                 const SimRuntimeBackendReport *report,
                                 VolumeFrameHeaderVf3dV1 *out_header) {
    SceneFluidVolumeExportView3D volume = {0};
    VolumeFrameHeaderVf3dV1 header = {0};
    FILE *f = NULL;
    float scene_up_x = 0.0f;
    float scene_up_y = 0.0f;
    float scene_up_z = 0.0f;

    if (!scene || !path || !path[0] || !out_header) return false;
    if (!scene_backend_volume_export_view_3d(scene, &volume)) return false;
    if (!volume.density || !volume.velocity_x || !volume.velocity_y ||
        !volume.velocity_z || !volume.pressure || !volume.solid_mask) {
        return false;
    }

    if (volume.scene_up_valid) {
        scene_up_x = volume.scene_up_x;
        scene_up_y = volume.scene_up_y;
        scene_up_z = volume.scene_up_z;
    } else if (report && report->scene_up_valid) {
        scene_up_x = report->scene_up_x;
        scene_up_y = report->scene_up_y;
        scene_up_z = report->scene_up_z;
    } else {
        return false;
    }

    header.magic = VOLUME_VF3D_MAGIC;
    header.version = VOLUME_VF3D_VERSION_V1;
    header.grid_w = (uint32_t)volume.width;
    header.grid_h = (uint32_t)volume.height;
    header.grid_d = (uint32_t)volume.depth;
    header.time_seconds = scene->time;
    header.frame_index = frame_index;
    header.dt_seconds = scene->dt;
    header.origin_x = volume.origin_x;
    header.origin_y = volume.origin_y;
    header.origin_z = volume.origin_z;
    header.voxel_size = volume.voxel_size;
    header.scene_up_x = scene_up_x;
    header.scene_up_y = scene_up_y;
    header.scene_up_z = scene_up_z;
    header.solid_mask_crc32 = solid_mask_crc32_fnv1a(volume.solid_mask, volume.cell_count);
    header.reserved[0] = 0u;
    header.reserved[1] = 0u;
    header.reserved[2] = 0u;

    f = fopen(path, "wb");
    if (!f) return false;

    // Raw vf3d payload order is frozen to the backend-owned linearization:
    // density, velX, velY, velZ, pressure, then solid occupancy bytes.
    if (fwrite(&header, sizeof(header), 1, f) != 1 ||
        fwrite(volume.density, sizeof(float), volume.cell_count, f) != volume.cell_count ||
        fwrite(volume.velocity_x, sizeof(float), volume.cell_count, f) != volume.cell_count ||
        fwrite(volume.velocity_y, sizeof(float), volume.cell_count, f) != volume.cell_count ||
        fwrite(volume.velocity_z, sizeof(float), volume.cell_count, f) != volume.cell_count ||
        fwrite(volume.pressure, sizeof(float), volume.cell_count, f) != volume.cell_count ||
        fwrite(volume.solid_mask, sizeof(uint8_t), volume.cell_count, f) != volume.cell_count) {
        fclose(f);
        return false;
    }

    fclose(f);
    *out_header = header;
    return true;
}

bool volume_frame_manifest_append_vf3d(const SceneState *scene,
                                       const VolumeFrameHeaderVf3dV1 *header,
                                       const char *frame_path,
                                       const char *run_dir) {
    char manifest_path[512];
    cJSON *root = NULL;
    cJSON *space_contract = NULL;
    cJSON *frames = NULL;
    cJSON *entry = NULL;
    char *text = NULL;
    FILE *f = NULL;
    int author_w = 0;
    int author_h = 0;

    if (!scene || !header || !frame_path || !run_dir || !run_dir[0]) return false;

    snprintf(manifest_path, sizeof(manifest_path), "%s/manifest.json", run_dir);
    root = manifest_root_open_or_create(scene, run_dir, manifest_path, 2);
    if (!root) return false;

    author_w = (scene->config && scene->config->window_w > 0) ? scene->config->window_w : (int)header->grid_w;
    author_h = (scene->config && scene->config->window_h > 0) ? scene->config->window_h : (int)header->grid_h;

    json_set_number(root, "manifest_version", 2);
    json_set_string(root, "run_name", run_dir);
    json_set_string(root, "frame_contract", "vf3d");
    json_set_string(root, "space_mode", "3d");
    json_set_number(root, "grid_w", header->grid_w);
    json_set_number(root, "grid_h", header->grid_h);
    json_set_number(root, "grid_d", header->grid_d);
    json_set_number(root, "origin_x", header->origin_x);
    json_set_number(root, "origin_y", header->origin_y);
    json_set_number(root, "origin_z", header->origin_z);
    json_set_number(root, "voxel_size", header->voxel_size);
    json_set_number(root, "scene_up_x", header->scene_up_x);
    json_set_number(root, "scene_up_y", header->scene_up_y);
    json_set_number(root, "scene_up_z", header->scene_up_z);
    json_set_number(root, "solid_mask_crc32", header->solid_mask_crc32);
    cJSON_DeleteItemFromObject(root, "cell_size");
    cJSON_DeleteItemFromObject(root, "obstacle_mask_crc32");

    space_contract = cJSON_GetObjectItem(root, "space_contract");
    if (!cJSON_IsObject(space_contract)) {
        cJSON_DeleteItemFromObject(root, "space_contract");
        space_contract = cJSON_CreateObject();
        if (space_contract) cJSON_AddItemToObject(root, "space_contract", space_contract);
    }
    if (!cJSON_IsObject(space_contract)) {
        cJSON_Delete(root);
        return false;
    }

    json_set_number(space_contract, "version", 2);
    json_set_string(space_contract, "space_mode", "3d");
    json_set_string(space_contract, "axis_authority", "xyz");
    json_set_number(space_contract, "grid_w", header->grid_w);
    json_set_number(space_contract, "grid_h", header->grid_h);
    json_set_number(space_contract, "grid_d", header->grid_d);
    json_set_number(space_contract, "origin_x", header->origin_x);
    json_set_number(space_contract, "origin_y", header->origin_y);
    json_set_number(space_contract, "origin_z", header->origin_z);
    json_set_number(space_contract, "voxel_size", header->voxel_size);
    json_set_number(space_contract, "scene_up_x", header->scene_up_x);
    json_set_number(space_contract, "scene_up_y", header->scene_up_y);
    json_set_number(space_contract, "scene_up_z", header->scene_up_z);
    json_set_number(space_contract, "author_window_w", author_w);
    json_set_number(space_contract, "author_window_h", author_h);
    json_set_number(space_contract, "import_fit", 0.25f);
    cJSON_DeleteItemFromObject(space_contract, "cell_size");

    frames = cJSON_GetObjectItem(root, "frames");
    if (!cJSON_IsArray(frames)) {
        cJSON_DeleteItemFromObject(root, "frames");
        frames = cJSON_AddArrayToObject(root, "frames");
    }
    if (!cJSON_IsArray(frames)) {
        cJSON_Delete(root);
        return false;
    }

    entry = cJSON_CreateObject();
    if (!entry) {
        cJSON_Delete(root);
        return false;
    }
    cJSON_AddNumberToObject(entry, "frame_index", (double)header->frame_index);
    cJSON_AddNumberToObject(entry, "time_seconds", header->time_seconds);
    cJSON_AddNumberToObject(entry, "dt_seconds", header->dt_seconds);
    cJSON_AddStringToObject(entry, "path", path_basename_or_self(frame_path));
    cJSON_AddStringToObject(entry, "frame_contract", "vf3d");
    cJSON_AddItemToArray(frames, entry);

    text = cJSON_Print(root);
    cJSON_Delete(root);
    if (!text) return false;

    f = fopen(manifest_path, "wb");
    if (!f) {
        free(text);
        return false;
    }
    if (fwrite(text, 1, strlen(text), f) != strlen(text)) {
        fclose(f);
        free(text);
        return false;
    }
    fclose(f);
    free(text);
    return true;
}

bool volume_frame_write_scene_bundle_vf3d(const SceneState *scene,
                                          const VolumeFrameHeaderVf3dV1 *header,
                                          const char *run_dir) {
    char bundle_path[512];
    const char *camera_path = getenv("PHYSICS_SCENE_CAMERA_PATH");
    const char *light_path = getenv("PHYSICS_SCENE_LIGHT_PATH");
    const char *mapping_profile = getenv("PHYSICS_SCENE_ASSET_MAPPING_PROFILE");
    FILE *f = NULL;
    int author_w = 0;
    int author_h = 0;

    if (!scene || !header || !run_dir || !run_dir[0]) return false;
    if (!mapping_profile || !mapping_profile[0]) mapping_profile = "physics_to_ray_v1";

    snprintf(bundle_path, sizeof(bundle_path), "%s/scene_bundle.json", run_dir);
    f = fopen(bundle_path, "wb");
    if (!f) return false;

    author_w = (scene->config && scene->config->window_w > 0) ? scene->config->window_w : (int)header->grid_w;
    author_h = (scene->config && scene->config->window_h > 0) ? scene->config->window_h : (int)header->grid_h;

    fprintf(f, "{\n");
    fprintf(f, "  \"bundle_type\": \"physics_scene_bundle_v1\",\n");
    fprintf(f, "  \"bundle_version\": 1,\n");
    fprintf(f, "  \"profile\": \"physics\",\n");
    fprintf(f, "  \"fluid_source\": {\n");
    fprintf(f, "    \"kind\": \"manifest\",\n");
    fprintf(f, "    \"path\": \"manifest.json\",\n");
    fprintf(f, "    \"contract\": \"vf3d\"\n");
    fprintf(f, "  }");
    if (scene->mode_route.water_mode_active) {
        fprintf(f, ",\n");
        fprintf(f, "  \"water_source\": {\n");
        fprintf(f, "    \"kind\": \"water_manifest\",\n");
        fprintf(f, "    \"path\": \"water_manifest_v1.json\",\n");
        fprintf(f, "    \"contract\": \"water_manifest_v1\",\n");
        fprintf(f, "    \"surface_representation\": \"heightfield\"\n");
        fprintf(f, "  }");
    }
    fprintf(f, ",\n");
    fprintf(f, "  \"scene_metadata\": {\n");
    fprintf(f, "    \"asset_mapping_profile\": \"%s\",\n", mapping_profile);
    fprintf(f, "    \"space_contract\": {\n");
    fprintf(f, "      \"version\": 2,\n");
    fprintf(f, "      \"space_mode\": \"3d\",\n");
    fprintf(f, "      \"axis_authority\": \"xyz\",\n");
    fprintf(f, "      \"grid_w\": %u,\n", header->grid_w);
    fprintf(f, "      \"grid_h\": %u,\n", header->grid_h);
    fprintf(f, "      \"grid_d\": %u,\n", header->grid_d);
    fprintf(f, "      \"origin_x\": %.6f,\n", (double)header->origin_x);
    fprintf(f, "      \"origin_y\": %.6f,\n", (double)header->origin_y);
    fprintf(f, "      \"origin_z\": %.6f,\n", (double)header->origin_z);
    fprintf(f, "      \"voxel_size\": %.6f,\n", (double)header->voxel_size);
    fprintf(f, "      \"scene_up_x\": %.6f,\n", (double)header->scene_up_x);
    fprintf(f, "      \"scene_up_y\": %.6f,\n", (double)header->scene_up_y);
    fprintf(f, "      \"scene_up_z\": %.6f,\n", (double)header->scene_up_z);
    fprintf(f, "      \"author_window_w\": %d,\n", author_w);
    fprintf(f, "      \"author_window_h\": %d,\n", author_h);
    fprintf(f, "      \"import_fit\": 0.25\n");
    fprintf(f, "    }");
    if (camera_path && camera_path[0]) {
        fprintf(f, ",\n    \"camera_path\": \"%s\"", camera_path);
    }
    if (light_path && light_path[0]) {
        fprintf(f, ",\n    \"light_path\": \"%s\"", light_path);
    }
    fprintf(f, "\n  },\n");
    fprintf(f, "  \"notes\": \"Auto-generated by physics_sim export\"\n");
    fprintf(f, "}\n");
    fclose(f);
    return true;
}
