#include "import/runtime_mesh_preview_bridge.h"
#include "import/runtime_mesh_preview_path_resolver.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int g_failures = 0;

static void expect_true(const char *name, bool condition) {
    if (!condition) {
        fprintf(stderr, "FAIL %-56s condition=false\n", name);
        g_failures += 1;
    }
}

static bool write_text_file(const char *path, const char *text) {
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

static void restore_environment(const char *name, const char *value) {
    if (value && value[0]) {
        (void)setenv(name, value, 1);
    } else {
        (void)unsetenv(name);
    }
}

static char *read_text_file_alloc(const char *path) {
    FILE *f = NULL;
    long size = 0;
    char *text = NULL;
    size_t read_count = 0u;
    if (!path) return NULL;
    f = fopen(path, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }
    size = ftell(f);
    if (size < 0) {
        fclose(f);
        return NULL;
    }
    if (fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return NULL;
    }
    text = (char *)malloc((size_t)size + 1u);
    if (!text) {
        fclose(f);
        return NULL;
    }
    read_count = fread(text, 1u, (size_t)size, f);
    fclose(f);
    if (read_count != (size_t)size) {
        free(text);
        return NULL;
    }
    text[size] = '\0';
    return text;
}

static bool setup_runtime_scene_fixture(const char *root_dir,
                                        const char *asset_path,
                                        const char *scene_path) {
    const char *assets_dir = "/private/tmp/physics_sim_mesh_preview_bridge/assets";
    const char *mesh_dir = "/private/tmp/physics_sim_mesh_preview_bridge/assets/mesh_assets";
    const char *source_asset_path =
        "../ray_tracing/tests/fixtures/mesh_asset_runtime_spheres/assets/mesh_assets/asset_sphere_8x4.runtime.json";
    const char *scene_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_physics_mesh_preview_bridge\","
        "\"space_mode_default\":\"3d\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"objects\":[{"
        "\"object_id\":\"obj_mesh_preview\","
        "\"object_type\":\"mesh_asset_instance\","
        "\"geometry_ref\":{\"kind\":\"mesh_asset\",\"id\":\"asset_sphere_8x4\"},"
        "\"transform\":{"
          "\"position\":{\"x\":2.0,\"y\":-3.0,\"z\":0.5},"
          "\"rotation\":{\"x\":0.0,\"y\":0.0,\"z\":0.0},"
          "\"scale\":{\"x\":2.0,\"y\":3.0,\"z\":0.5}"
        "}"
        "}],"
        "\"materials\":[],"
        "\"lights\":[],"
        "\"cameras\":[],"
        "\"constraints\":[],"
        "\"extensions\":{}"
        "}";
    char *asset_text = NULL;

    mkdir(root_dir, 0777);
    mkdir(assets_dir, 0777);
    mkdir(mesh_dir, 0777);
    asset_text = read_text_file_alloc(source_asset_path);
    expect_true("mesh_preview_bridge_read_source_asset", asset_text != NULL);
    if (!asset_text) return false;
    expect_true("mesh_preview_bridge_write_asset", write_text_file(asset_path, asset_text));
    free(asset_text);
    expect_true("mesh_preview_bridge_write_scene", write_text_file(scene_path, scene_json));
    return true;
}

static void cleanup_runtime_scene_fixture(const char *asset_path,
                                          const char *preview_path,
                                          const char *scene_path) {
    if (preview_path && preview_path[0]) remove(preview_path);
    remove(asset_path);
    remove(scene_path);
    rmdir("/private/tmp/physics_sim_mesh_preview_bridge/assets/mesh_assets");
    rmdir("/private/tmp/physics_sim_mesh_preview_bridge/assets");
    rmdir("/private/tmp/physics_sim_mesh_preview_bridge");
}

static void test_scan_scene_attaches_preview_metadata(void) {
    const char *root_dir = "/private/tmp/physics_sim_mesh_preview_bridge";
    const char *scene_path = "/private/tmp/physics_sim_mesh_preview_bridge/scene_runtime.json";
    const char *asset_path =
        "/private/tmp/physics_sim_mesh_preview_bridge/assets/mesh_assets/asset_sphere_8x4.runtime.json";
    PhysicsSimRuntimeMeshPreviewSet set;
    CoreResult preview_result = core_result_ok();
    char diagnostics[256] = {0};
    char preview_path[PHYSICS_SIM_RUNTIME_MESH_PREVIEW_PATH_MAX] = {0};
    bool ok = false;

    if (!setup_runtime_scene_fixture(root_dir, asset_path, scene_path)) {
        cleanup_runtime_scene_fixture(asset_path, preview_path, scene_path);
        return;
    }

    preview_result = core_mesh_preview_save_for_runtime_file(asset_path,
                                                            CORE_MESH_PREVIEW_MODE_BOUNDS_PROXY_V1,
                                                            0u,
                                                            preview_path,
                                                            sizeof(preview_path));
    expect_true("mesh_preview_bridge_sidecar_write", preview_result.code == CORE_OK);

    physics_sim_runtime_mesh_preview_set_init(&set);
    ok = physics_sim_runtime_mesh_preview_scan_scene_file(scene_path,
                                                          &set,
                                                          diagnostics,
                                                          sizeof(diagnostics));
    expect_true("mesh_preview_bridge_scan_ok", ok);
    expect_true("mesh_preview_bridge_contract_valid", set.valid_contract);
    expect_true("mesh_preview_bridge_one_instance", set.instance_count == 1);
    if (set.instance_count == 1) {
        const PhysicsSimRuntimeMeshPreviewInstance *instance = &set.instances[0];
        expect_true("mesh_preview_bridge_runtime_path", instance->runtime_path_resolved);
        expect_true("mesh_preview_bridge_preview_path", instance->preview_path_resolved);
        expect_true("mesh_preview_bridge_preview_exists", instance->preview_file_exists);
        expect_true("mesh_preview_bridge_metadata_valid", instance->preview_metadata_valid);
        expect_true("mesh_preview_bridge_mode",
                    instance->metadata.mode == CORE_MESH_PREVIEW_MODE_BOUNDS_PROXY_V1);
        expect_true("mesh_preview_bridge_source_triangles",
                    instance->metadata.source_triangle_count == 48u);
        expect_true("mesh_preview_bridge_radius",
                    instance->metadata.bounding_sphere_radius > 0.0);
        expect_true("mesh_preview_bridge_world_bounds", instance->has_world_bounds);
        expect_true("mesh_preview_bridge_world_min_x",
                    instance->world_bounds_min.x > -0.0001 &&
                    instance->world_bounds_min.x < 0.0001);
        expect_true("mesh_preview_bridge_world_max_x",
                    instance->world_bounds_max.x > 3.9999 &&
                    instance->world_bounds_max.x < 4.0001);
        expect_true("mesh_preview_bridge_world_min_y",
                    instance->world_bounds_min.y > -6.0001 &&
                    instance->world_bounds_min.y < -5.9999);
        expect_true("mesh_preview_bridge_world_max_z",
                    instance->world_bounds_max.z > 0.9999 &&
                    instance->world_bounds_max.z < 1.0001);
        expect_true("mesh_preview_bridge_default_solid",
                    instance->fluid_obstacle_enabled &&
                    strcmp(instance->fluid_behavior, "solid_obstacle") == 0);
        expect_true("mesh_preview_bridge_default_not_emitter",
                    !instance->fluid_emitter_enabled);
    }

    cleanup_runtime_scene_fixture(asset_path, preview_path, scene_path);
}

static void test_scan_scene_keeps_missing_preview_advisory(void) {
    const char *scene_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_physics_mesh_preview_missing\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"objects\":[{"
        "\"object_id\":\"obj_mesh_missing\","
        "\"object_type\":\"mesh_asset_instance\","
        "\"geometry_ref\":{\"kind\":\"mesh_asset\",\"id\":\"asset_missing\"}"
        "}],"
        "\"materials\":[],"
        "\"lights\":[],"
        "\"cameras\":[]"
        "}";
    PhysicsSimRuntimeMeshPreviewSet set;
    char diagnostics[256] = {0};
    bool ok = physics_sim_runtime_mesh_preview_scan_scene_json(scene_json,
                                                               NULL,
                                                               &set,
                                                               diagnostics,
                                                               sizeof(diagnostics));
    expect_true("mesh_preview_missing_scan_ok", ok);
    expect_true("mesh_preview_missing_contract_valid", set.valid_contract);
    expect_true("mesh_preview_missing_one_instance", set.instance_count == 1);
    if (set.instance_count == 1) {
        expect_true("mesh_preview_missing_runtime_unresolved",
                    !set.instances[0].runtime_path_resolved);
        expect_true("mesh_preview_missing_metadata_invalid",
                    !set.instances[0].preview_metadata_valid);
    }
}

static void test_scan_scene_recovers_migrated_desktop_stls_path(void) {
    const char *root_dir = "/private/tmp/physics_sim_mesh_preview_migrated";
    const char *home_dir = "/private/tmp/physics_sim_mesh_preview_migrated/home";
    const char *desktop_dir = "/private/tmp/physics_sim_mesh_preview_migrated/home/Desktop";
    const char *stls_dir = "/private/tmp/physics_sim_mesh_preview_migrated/home/Desktop/stls";
    const char *library_dir = "/private/tmp/physics_sim_mesh_preview_migrated/home/Desktop/stls/migrated_meshes";
    const char *asset_path =
        "/private/tmp/physics_sim_mesh_preview_migrated/home/Desktop/stls/migrated_meshes/asset_sphere_8x4.runtime.json";
    const char *legacy_path =
        "/legacy-home/Desktop/migrated_meshes/asset_sphere_8x4.runtime.json";
    const char *source_asset_path =
        "../ray_tracing/tests/fixtures/mesh_asset_runtime_spheres/assets/mesh_assets/asset_sphere_8x4.runtime.json";
    const char *old_home = getenv("HOME");
    const char *old_xdg_data_home = getenv("XDG_DATA_HOME");
    const char *old_xdg_config_home = getenv("XDG_CONFIG_HOME");
    char old_home_copy[PHYSICS_SIM_RUNTIME_MESH_PREVIEW_PATH_MAX] = {0};
    char old_xdg_data_home_copy[PHYSICS_SIM_RUNTIME_MESH_PREVIEW_PATH_MAX] = {0};
    char old_xdg_config_home_copy[PHYSICS_SIM_RUNTIME_MESH_PREVIEW_PATH_MAX] = {0};
    char scene_json[4096] = {0};
    char *asset_text = NULL;
    PhysicsSimRuntimeMeshPreviewSet set;
    char diagnostics[256] = {0};
    bool ok = false;

    if (old_home && old_home[0]) {
        snprintf(old_home_copy, sizeof(old_home_copy), "%s", old_home);
    }
    if (old_xdg_data_home && old_xdg_data_home[0]) {
        snprintf(old_xdg_data_home_copy, sizeof(old_xdg_data_home_copy), "%s", old_xdg_data_home);
    }
    if (old_xdg_config_home && old_xdg_config_home[0]) {
        snprintf(old_xdg_config_home_copy, sizeof(old_xdg_config_home_copy), "%s", old_xdg_config_home);
    }
    mkdir(root_dir, 0777);
    mkdir(home_dir, 0777);
    mkdir(desktop_dir, 0777);
    mkdir(stls_dir, 0777);
    mkdir(library_dir, 0777);

    asset_text = read_text_file_alloc(source_asset_path);
    expect_true("mesh_preview_migrated_read_source_asset", asset_text != NULL);
    if (asset_text) {
        expect_true("mesh_preview_migrated_write_asset",
                    write_text_file(asset_path, asset_text));
        free(asset_text);
    }

    snprintf(scene_json,
             sizeof(scene_json),
             "{"
             "\"schema_family\":\"codework_scene\","
             "\"schema_variant\":\"scene_runtime_v1\","
             "\"schema_version\":1,"
             "\"scene_id\":\"scene_physics_mesh_preview_migrated\","
             "\"unit_system\":\"meters\","
             "\"world_scale\":1.0,"
             "\"objects\":[{"
             "\"object_id\":\"obj_mesh_migrated\","
             "\"object_type\":\"mesh_asset_instance\","
             "\"geometry_ref\":{\"kind\":\"mesh_asset\",\"id\":\"asset_sphere_8x4\"},"
             "\"extensions\":{\"line_drawing\":{\"runtime_mesh_path\":\"%s\"}}"
             "}],"
             "\"materials\":[],"
             "\"lights\":[],"
             "\"cameras\":[]"
             "}",
             legacy_path);

    expect_true("mesh_preview_migrated_set_home", setenv("HOME", home_dir, 1) == 0);
    (void)unsetenv("XDG_DATA_HOME");
    (void)unsetenv("XDG_CONFIG_HOME");
    ok = physics_sim_runtime_mesh_preview_scan_scene_json(scene_json,
                                                          NULL,
                                                          &set,
                                                          diagnostics,
                                                          sizeof(diagnostics));
    restore_environment("HOME", old_home_copy);
    restore_environment("XDG_DATA_HOME", old_xdg_data_home_copy);
    restore_environment("XDG_CONFIG_HOME", old_xdg_config_home_copy);

    expect_true("mesh_preview_migrated_scan_ok", ok);
    expect_true("mesh_preview_migrated_one_instance", set.instance_count == 1);
    if (set.instance_count == 1) {
        const PhysicsSimRuntimeMeshPreviewInstance *instance = &set.instances[0];
        expect_true("mesh_preview_migrated_runtime_resolved", instance->runtime_path_resolved);
        expect_true("mesh_preview_migrated_runtime_recovered", instance->runtime_path_recovered);
        expect_true("mesh_preview_migrated_path_matches",
                    strcmp(instance->runtime_mesh_path, asset_path) == 0);
        expect_true("mesh_preview_migrated_hint_kept",
                    strcmp(instance->runtime_mesh_path_hint, legacy_path) == 0);
    }

    remove(asset_path);
    rmdir(library_dir);
    rmdir(stls_dir);
    rmdir(desktop_dir);
    rmdir(home_dir);
    rmdir(root_dir);
}

static void test_migrated_path_prefers_xdg_data_home(void) {
    const char *root_dir = "/private/tmp/physics_sim_mesh_preview_xdg_data";
    const char *home_dir = "/private/tmp/physics_sim_mesh_preview_xdg_data/home";
    const char *data_dir = "/private/tmp/physics_sim_mesh_preview_xdg_data/data";
    const char *physics_sim_dir = "/private/tmp/physics_sim_mesh_preview_xdg_data/data/PhysicsSim";
    const char *stls_dir = "/private/tmp/physics_sim_mesh_preview_xdg_data/data/PhysicsSim/stls";
    const char *library_dir = "/private/tmp/physics_sim_mesh_preview_xdg_data/data/PhysicsSim/stls/library";
    const char *asset_path = "/private/tmp/physics_sim_mesh_preview_xdg_data/data/PhysicsSim/stls/library/mesh.runtime.json";
    const char *old_home = getenv("HOME");
    const char *old_xdg_data_home = getenv("XDG_DATA_HOME");
    char old_home_copy[PHYSICS_SIM_RUNTIME_MESH_PREVIEW_PATH_MAX] = {0};
    char old_xdg_data_home_copy[PHYSICS_SIM_RUNTIME_MESH_PREVIEW_PATH_MAX] = {0};
    char resolved[PHYSICS_SIM_RUNTIME_MESH_PREVIEW_PATH_MAX] = {0};

    if (old_home) snprintf(old_home_copy, sizeof(old_home_copy), "%s", old_home);
    if (old_xdg_data_home) snprintf(old_xdg_data_home_copy, sizeof(old_xdg_data_home_copy), "%s", old_xdg_data_home);
    mkdir(root_dir, 0777); mkdir(home_dir, 0777); mkdir(data_dir, 0777); mkdir(physics_sim_dir, 0777);
    mkdir(stls_dir, 0777); mkdir(library_dir, 0777);
    expect_true("mesh_preview_xdg_data_write_asset", write_text_file(asset_path, "{}"));
    expect_true("mesh_preview_xdg_data_set_home", setenv("HOME", home_dir, 1) == 0);
    expect_true("mesh_preview_xdg_data_set_root", setenv("XDG_DATA_HOME", data_dir, 1) == 0);
    expect_true("mesh_preview_xdg_data_resolved",
                physics_sim_runtime_mesh_preview_resolve_migrated_path(
                    "/former-home/Desktop/library/mesh.runtime.json", resolved, sizeof(resolved)));
    expect_true("mesh_preview_xdg_data_path", strcmp(resolved, asset_path) == 0);
    restore_environment("HOME", old_home_copy);
    restore_environment("XDG_DATA_HOME", old_xdg_data_home_copy);
    remove(asset_path); rmdir(library_dir); rmdir(stls_dir); rmdir(physics_sim_dir); rmdir(data_dir); rmdir(home_dir); rmdir(root_dir);
}

static void test_migrated_path_uses_xdg_desktop_dir(void) {
    const char *root_dir = "/private/tmp/physics_sim_mesh_preview_xdg_desktop";
    const char *home_dir = "/private/tmp/physics_sim_mesh_preview_xdg_desktop/home";
    const char *config_dir = "/private/tmp/physics_sim_mesh_preview_xdg_desktop/config";
    const char *desktop_dir = "/private/tmp/physics_sim_mesh_preview_xdg_desktop/home/Workspace";
    const char *stls_dir = "/private/tmp/physics_sim_mesh_preview_xdg_desktop/home/Workspace/stls";
    const char *library_dir = "/private/tmp/physics_sim_mesh_preview_xdg_desktop/home/Workspace/stls/library";
    const char *asset_path = "/private/tmp/physics_sim_mesh_preview_xdg_desktop/home/Workspace/stls/library/mesh.runtime.json";
    const char *config_path = "/private/tmp/physics_sim_mesh_preview_xdg_desktop/config/user-dirs.dirs";
    const char *old_home = getenv("HOME");
    const char *old_xdg_data_home = getenv("XDG_DATA_HOME");
    const char *old_xdg_config_home = getenv("XDG_CONFIG_HOME");
    char old_home_copy[PHYSICS_SIM_RUNTIME_MESH_PREVIEW_PATH_MAX] = {0};
    char old_xdg_data_home_copy[PHYSICS_SIM_RUNTIME_MESH_PREVIEW_PATH_MAX] = {0};
    char old_xdg_config_home_copy[PHYSICS_SIM_RUNTIME_MESH_PREVIEW_PATH_MAX] = {0};
    char resolved[PHYSICS_SIM_RUNTIME_MESH_PREVIEW_PATH_MAX] = {0};

    if (old_home) snprintf(old_home_copy, sizeof(old_home_copy), "%s", old_home);
    if (old_xdg_data_home) snprintf(old_xdg_data_home_copy, sizeof(old_xdg_data_home_copy), "%s", old_xdg_data_home);
    if (old_xdg_config_home) snprintf(old_xdg_config_home_copy, sizeof(old_xdg_config_home_copy), "%s", old_xdg_config_home);
    mkdir(root_dir, 0777); mkdir(home_dir, 0777); mkdir(config_dir, 0777); mkdir(desktop_dir, 0777);
    mkdir(stls_dir, 0777); mkdir(library_dir, 0777);
    expect_true("mesh_preview_xdg_desktop_write_asset", write_text_file(asset_path, "{}"));
    expect_true("mesh_preview_xdg_desktop_write_config", write_text_file(config_path, "XDG_DESKTOP_DIR=\"$HOME/Workspace\"\n"));
    expect_true("mesh_preview_xdg_desktop_set_home", setenv("HOME", home_dir, 1) == 0);
    expect_true("mesh_preview_xdg_desktop_set_config", setenv("XDG_CONFIG_HOME", config_dir, 1) == 0);
    (void)unsetenv("XDG_DATA_HOME");
    expect_true("mesh_preview_xdg_desktop_resolved",
                physics_sim_runtime_mesh_preview_resolve_migrated_path(
                    "/former-home/Desktop/library/mesh.runtime.json", resolved, sizeof(resolved)));
    expect_true("mesh_preview_xdg_desktop_path", strcmp(resolved, asset_path) == 0);
    restore_environment("HOME", old_home_copy);
    restore_environment("XDG_DATA_HOME", old_xdg_data_home_copy);
    restore_environment("XDG_CONFIG_HOME", old_xdg_config_home_copy);
    remove(asset_path); remove(config_path); rmdir(library_dir); rmdir(stls_dir); rmdir(desktop_dir);
    rmdir(config_dir); rmdir(home_dir); rmdir(root_dir);
}

static void test_scan_scene_mesh_emitter_disables_default_obstacle(void) {
    const char *scene_json =
        "{"
        "\"schema_family\":\"codework_scene\","
        "\"schema_variant\":\"scene_runtime_v1\","
        "\"schema_version\":1,"
        "\"scene_id\":\"scene_physics_mesh_emitter\","
        "\"unit_system\":\"meters\","
        "\"world_scale\":1.0,"
        "\"objects\":[{"
        "\"object_id\":\"obj_mesh_emitter\","
        "\"object_type\":\"mesh_asset_instance\","
        "\"geometry_ref\":{\"kind\":\"mesh_asset\",\"id\":\"asset_missing\"},"
        "\"extensions\":{\"physics_sim\":{"
        "\"fluid_behavior\":\"surface_heat_emitter\","
        "\"emitter\":{\"active\":true,\"type\":\"Jet\",\"mode_3d\":\"SurfaceShell\","
        "\"surface_3d\":\"AllFaces\",\"strength\":12.5,"
        "\"direction\":{\"x\":1.0,\"y\":0.0,\"z\":0.0}}"
        "}}"
        "}],"
        "\"materials\":[],"
        "\"lights\":[],"
        "\"cameras\":[]"
        "}";
    PhysicsSimRuntimeMeshPreviewSet set;
    char diagnostics[256] = {0};
    bool ok = physics_sim_runtime_mesh_preview_scan_scene_json(scene_json,
                                                               NULL,
                                                               &set,
                                                               diagnostics,
                                                               sizeof(diagnostics));
    expect_true("mesh_preview_emitter_scan_ok", ok);
    expect_true("mesh_preview_emitter_one_instance", set.instance_count == 1);
    if (set.instance_count == 1) {
        const PhysicsSimRuntimeMeshPreviewInstance *instance = &set.instances[0];
        expect_true("mesh_preview_emitter_enabled", instance->fluid_emitter_enabled);
        expect_true("mesh_preview_emitter_not_solid", !instance->fluid_obstacle_enabled);
        expect_true("mesh_preview_emitter_type", instance->emitter_type == EMITTER_VELOCITY_JET);
        expect_true("mesh_preview_emitter_mode",
                    instance->emitter_source_mode_3d == EMITTER_3D_SOURCE_MODE_SURFACE_SHELL);
        expect_true("mesh_preview_emitter_strength",
                    instance->emitter_strength > 12.49f && instance->emitter_strength < 12.51f);
        expect_true("mesh_preview_emitter_direction",
                    instance->emitter_direction.x > 0.99 && instance->emitter_direction.x < 1.01);
    }
}

int main(void) {
    test_scan_scene_attaches_preview_metadata();
    test_scan_scene_keeps_missing_preview_advisory();
    test_scan_scene_recovers_migrated_desktop_stls_path();
    test_migrated_path_prefers_xdg_data_home();
    test_migrated_path_uses_xdg_desktop_dir();
    test_scan_scene_mesh_emitter_disables_default_obstacle();
    if (g_failures != 0) {
        fprintf(stderr, "runtime_mesh_preview_bridge_contract_test: %d failure(s)\n", g_failures);
        return 1;
    }
    fprintf(stdout, "runtime_mesh_preview_bridge_contract_test: success\n");
    return 0;
}
