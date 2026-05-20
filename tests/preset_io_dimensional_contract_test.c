#include "app/preset_io.h"
#include "app/atmospheric/atmospheric_field.h"
#include "app/scene_presets.h"

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static bool approx_equal(float a, float b, float eps) {
    return fabsf(a - b) <= eps;
}

static bool write_text_file(const char *path, const char *content) {
    FILE *f = fopen(path, "w");
    if (!f) return false;
    fputs(content, f);
    fclose(f);
    return true;
}

static bool test_legacy_omitted_z_fallback(void) {
    static const char *legacy_v11 =
        "11 0 1\n"
        "1 0\n"
        "1.000000 1.000000\n"
        "Legacy Slot\n"
        "\n"
        "1\n"
        "0 0.250000 0.750000 0.100000 2.000000 0.000000 -1.000000 -1 -1\n"
        "FLOW 4\n"
        "0 0 0.000000\n"
        "1 0 0.000000\n"
        "2 0 0.000000\n"
        "3 0 0.000000\n"
        "OBJ 1\n"
        "0 0.400000 0.600000 0.050000 0.050000 0.000000 1 1\n"
        "SHAPE 1\n"
        "legacy/import.asset.json\n"
        "0.300000 0.400000 1.200000 15.000000 1 1.000000 0.200000 1 0 0 0\n";

    char path_template[] = "/tmp/physics_sim_preset_v11_XXXXXX";
    int fd = mkstemp(path_template);
    if (fd < 0) {
        fprintf(stderr, "mkstemp failed for legacy test\n");
        return false;
    }
    close(fd);

    bool ok = false;
    CustomPresetLibrary lib;
    preset_library_init(&lib);

    if (!write_text_file(path_template, legacy_v11)) goto done;
    if (!preset_library_load(path_template, &lib)) goto done;
    if (preset_library_count(&lib) != 1) goto done;

    const CustomPresetSlot *slot = preset_library_get_slot_const(&lib, 0);
    if (!slot) goto done;
    if (slot->preset.dimension_mode != SCENE_DIMENSION_MODE_2D) goto done;
    if (slot->preset.emitter_count != 1) goto done;
    if (slot->preset.object_count != 1) goto done;
    if (slot->preset.import_shape_count != 1) goto done;

    const FluidEmitter *em = &slot->preset.emitters[0];
    const PresetObject *obj = &slot->preset.objects[0];
    const ImportedShape *imp = &slot->preset.import_shapes[0];

    if (!approx_equal(em->position_z, 0.0f, 1e-6f)) goto done;
    if (!approx_equal(em->dir_z, 0.0f, 1e-6f)) goto done;
    if (em->source_mode_3d != EMITTER_3D_SOURCE_MODE_LEGACY_COMPAT) goto done;
    if (em->surface_3d != EMITTER_3D_SURFACE_AUTO) goto done;
    if (em->obstacle_mode_3d != EMITTER_3D_OBSTACLE_MODE_AUTO) goto done;
    if (!approx_equal(em->thermal_buoyancy_3d, 0.0f, 1e-6f)) goto done;
    if (!approx_equal(obj->position_z, 0.0f, 1e-6f)) goto done;
    if (!approx_equal(obj->size_z, obj->size_x, 1e-6f)) goto done;
    if (!approx_equal(imp->position_z, 0.0f, 1e-6f)) goto done;

    ok = true;

done:
    preset_library_shutdown(&lib);
    unlink(path_template);
    return ok;
}

static bool test_v15_additive_roundtrip(void) {
    char path_template[] = "/tmp/physics_sim_preset_v15_XXXXXX";
    int fd = mkstemp(path_template);
    if (fd < 0) {
        fprintf(stderr, "mkstemp failed for roundtrip test\n");
        return false;
    }
    close(fd);

    bool ok = false;
    CustomPresetLibrary lib;
    CustomPresetLibrary reloaded;
    preset_library_init(&lib);
    preset_library_init(&reloaded);

    const FluidScenePreset *base = scene_presets_get_default();
    CustomPresetSlot *slot = preset_library_add_slot(&lib, "Dimensional Slot", base);
    if (!slot) goto done;
    slot->preset.dimension_mode = SCENE_DIMENSION_MODE_3D;

    slot->preset.emitter_count = 1;
    slot->preset.emitters[0] = (FluidEmitter){
        .type = EMITTER_VELOCITY_JET,
        .position_x = 0.12f,
        .position_y = 0.34f,
        .position_z = 0.56f,
        .radius = 0.08f,
        .strength = 11.0f,
        .dir_x = 0.3f,
        .dir_y = -0.8f,
        .dir_z = 0.5f,
        .attached_object = -1,
        .attached_import = -1,
        .source_mode_3d = EMITTER_3D_SOURCE_MODE_SURFACE_PATCH,
        .surface_3d = EMITTER_3D_SURFACE_TOP,
        .obstacle_mode_3d = EMITTER_3D_OBSTACLE_MODE_RETAIN_ATTACHED,
        .thermal_buoyancy_3d = 6.5f
    };

    slot->preset.object_count = 1;
    slot->preset.objects[0] = (PresetObject){
        .type = PRESET_OBJECT_BOX,
        .position_x = 0.2f,
        .position_y = 0.3f,
        .position_z = 0.4f,
        .size_x = 0.1f,
        .size_y = 0.2f,
        .size_z = 0.3f,
        .angle = 0.9f,
        .is_static = true,
        .gravity_enabled = false
    };

    slot->preset.import_shape_count = 1;
    memset(&slot->preset.import_shapes[0], 0, sizeof(slot->preset.import_shapes[0]));
    snprintf(slot->preset.import_shapes[0].path,
             sizeof(slot->preset.import_shapes[0].path),
             "config/objects/sample.asset.json");
    slot->preset.import_shapes[0].position_x = 0.21f;
    slot->preset.import_shapes[0].position_y = 0.31f;
    slot->preset.import_shapes[0].position_z = 0.41f;
    slot->preset.import_shapes[0].scale = 1.25f;
    slot->preset.import_shapes[0].rotation_deg = 23.0f;
    slot->preset.import_shapes[0].enabled = true;
    slot->preset.import_shapes[0].density = 1.1f;
    slot->preset.import_shapes[0].friction = 0.22f;
    slot->preset.import_shapes[0].is_static = true;
    slot->preset.import_shapes[0].gravity_enabled = false;

    if (!preset_library_save(path_template, &lib)) goto done;
    if (!preset_library_load(path_template, &reloaded)) goto done;
    if (preset_library_count(&reloaded) != 1) goto done;

    const CustomPresetSlot *loaded = preset_library_get_slot_const(&reloaded, 0);
    if (!loaded) goto done;
    if (loaded->preset.dimension_mode != SCENE_DIMENSION_MODE_3D) goto done;
    if (loaded->preset.emitter_count != 1) goto done;
    if (loaded->preset.object_count != 1) goto done;
    if (loaded->preset.import_shape_count != 1) goto done;

    const FluidEmitter *em = &loaded->preset.emitters[0];
    const PresetObject *obj = &loaded->preset.objects[0];
    const ImportedShape *imp = &loaded->preset.import_shapes[0];
    float dir_len = sqrtf(0.3f * 0.3f + (-0.8f) * (-0.8f) + 0.5f * 0.5f);
    if (!approx_equal(em->position_z, 0.56f, 1e-4f)) goto done;
    if (!approx_equal(em->dir_x, 0.3f / dir_len, 1e-4f)) goto done;
    if (!approx_equal(em->dir_y, -0.8f / dir_len, 1e-4f)) goto done;
    if (!approx_equal(em->dir_z, 0.5f / dir_len, 1e-4f)) goto done;
    if (em->source_mode_3d != EMITTER_3D_SOURCE_MODE_SURFACE_PATCH) goto done;
    if (em->surface_3d != EMITTER_3D_SURFACE_TOP) goto done;
    if (em->obstacle_mode_3d != EMITTER_3D_OBSTACLE_MODE_RETAIN_ATTACHED) goto done;
    if (!approx_equal(em->thermal_buoyancy_3d, 6.5f, 1e-4f)) goto done;
    if (!approx_equal(obj->position_z, 0.4f, 1e-4f)) goto done;
    if (!approx_equal(obj->size_z, 0.3f, 1e-4f)) goto done;
    if (!approx_equal(imp->position_z, 0.41f, 1e-4f)) goto done;

    ok = true;

done:
    preset_library_shutdown(&lib);
    preset_library_shutdown(&reloaded);
    unlink(path_template);
    return ok;
}

static bool test_3d_slot_sanitize_defaults_invalid_direction_up(void) {
    CustomPresetLibrary lib;
    FluidScenePreset preset = {0};
    CustomPresetSlot *slot = NULL;
    bool ok = false;

    preset_library_init(&lib);
    preset.dimension_mode = SCENE_DIMENSION_MODE_3D;
    preset.emitter_count = 1;
    preset.emitters[0] = (FluidEmitter){
        .type = EMITTER_VELOCITY_JET,
        .position_x = 0.5f,
        .position_y = 0.5f,
        .position_z = 0.25f,
        .radius = 0.08f,
        .strength = 10.0f,
        .dir_x = 0.0f,
        .dir_y = 0.0f,
        .dir_z = 0.0f,
        .attached_object = -1,
        .attached_import = -1,
    };

    slot = preset_library_add_slot(&lib, "3D Direction Defaults", &preset);
    if (!slot) goto done;
    if (!approx_equal(slot->preset.emitters[0].dir_x, 0.0f, 1e-6f)) goto done;
    if (!approx_equal(slot->preset.emitters[0].dir_y, 0.0f, 1e-6f)) goto done;
    if (!approx_equal(slot->preset.emitters[0].dir_z, 1.0f, 1e-6f)) goto done;
    ok = true;

done:
    preset_library_shutdown(&lib);
    return ok;
}

static bool test_v13_atmospheric_roundtrip(void) {
    char path_template[] = "/tmp/physics_sim_preset_v13_atmos_XXXXXX";
    int fd = mkstemp(path_template);
    if (fd < 0) {
        fprintf(stderr, "mkstemp failed for atmospheric roundtrip test\n");
        return false;
    }
    close(fd);

    bool ok = false;
    CustomPresetLibrary lib;
    CustomPresetLibrary reloaded;
    preset_library_init(&lib);
    preset_library_init(&reloaded);

    FluidScenePreset preset = {0};
    preset.domain = SCENE_DOMAIN_ATMOSPHERIC;
    preset.dimension_mode = SCENE_DIMENSION_MODE_3D;
    preset.domain_width = 2.0f;
    preset.domain_height = 1.0f;
    preset.atmosphere.enabled = true;
    preset.atmosphere.seed = 4242u;
    preset.atmosphere.base_density = 0.15f;
    preset.atmosphere.density_scale = 5.5f;
    preset.atmosphere.density_threshold = 0.42f;
    preset.atmosphere.base_wind_x = 3.0f;
    preset.atmosphere.base_wind_y = -0.25f;
    preset.atmosphere.base_wind_z = 1.5f;
    preset.atmosphere.turbulence_strength = 2.25f;
    preset.atmosphere.noise_scale = 4.0f;
    preset.atmosphere.detail_scale = 12.0f;
    preset.atmosphere.band_min_y = 0.2f;
    preset.atmosphere.band_max_y = 0.7f;
    preset.atmosphere.band_edge_falloff = 0.12f;
    preset.atmosphere.region_count = 1;
    preset.atmosphere.regions[0] = (AtmosphericDensityRegion){
        .enabled = true,
        .shape = ATMOSPHERIC_REGION_ELLIPSE,
        .center_x = 0.45f,
        .center_y = 0.55f,
        .center_z = 0.35f,
        .size_x = 0.2f,
        .size_y = 0.1f,
        .size_z = 0.3f,
        .density = 1.75f,
        .falloff = 0.4f,
    };

    if (!preset_library_add_slot(&lib, "Atmos Slot", &preset)) goto done;
    if (!preset_library_save(path_template, &lib)) goto done;
    if (!preset_library_load(path_template, &reloaded)) goto done;
    const CustomPresetSlot *loaded = preset_library_get_slot_const(&reloaded, 0);
    if (!loaded) goto done;
    if (loaded->preset.domain != SCENE_DOMAIN_ATMOSPHERIC) goto done;
    if (loaded->preset.dimension_mode != SCENE_DIMENSION_MODE_3D) goto done;
    if (!loaded->preset.atmosphere.enabled) goto done;
    if (loaded->preset.atmosphere.seed != 4242u) goto done;
    if (!approx_equal(loaded->preset.atmosphere.base_density, 0.15f, 1e-4f)) goto done;
    if (!approx_equal(loaded->preset.atmosphere.density_scale, 5.5f, 1e-4f)) goto done;
    if (!approx_equal(loaded->preset.atmosphere.base_wind_z, 1.5f, 1e-4f)) goto done;
    if (loaded->preset.atmosphere.region_count != 1) goto done;
    if (loaded->preset.atmosphere.regions[0].shape != ATMOSPHERIC_REGION_ELLIPSE) goto done;
    if (!approx_equal(loaded->preset.atmosphere.regions[0].density, 1.75f, 1e-4f)) goto done;
    ok = true;

done:
    preset_library_shutdown(&lib);
    preset_library_shutdown(&reloaded);
    unlink(path_template);
    return ok;
}

static bool test_v13_optional_layer_defaults_off(void) {
    static const char *legacy_v13 =
        "13 0 1\n"
        "1 0\n"
        "1.000000 1.000000\n"
        "1\n"
        "Legacy Atmos Layer\n"
        "\n"
        "ATMOS 1 7777 0.100000 6.000000 0.500000 2.000000 0.000000 1.000000 1.500000 3.000000 9.000000 0.200000 0.800000 0.100000 0\n"
        "0\n";

    char path_template[] = "/tmp/physics_sim_preset_v13_atmos_layer_XXXXXX";
    int fd = mkstemp(path_template);
    if (fd < 0) {
        fprintf(stderr, "mkstemp failed for v13 optional layer fallback test\n");
        return false;
    }
    close(fd);

    bool ok = false;
    CustomPresetLibrary lib;
    preset_library_init(&lib);

    if (!write_text_file(path_template, legacy_v13)) goto done;
    if (!preset_library_load(path_template, &lib)) goto done;
    if (preset_library_count(&lib) != 1) goto done;

    const CustomPresetSlot *loaded = preset_library_get_slot_const(&lib, 0);
    if (!loaded) goto done;
    if (loaded->preset.domain != SCENE_DOMAIN_BOX) goto done;
    if (loaded->preset.dimension_mode != SCENE_DIMENSION_MODE_3D) goto done;
    if (!loaded->preset.atmosphere.enabled) goto done;
    if (loaded->preset.atmospheric_initial_state_enabled) goto done;
    if (atmospheric_initial_state_source(&loaded->preset) !=
        ATMOSPHERIC_INITIAL_STATE_NONE) {
        goto done;
    }
    ok = true;

done:
    preset_library_shutdown(&lib);
    unlink(path_template);
    return ok;
}

static bool test_v14_optional_layer_roundtrip(void) {
    char path_template[] = "/tmp/physics_sim_preset_v14_atmos_layer_XXXXXX";
    int fd = mkstemp(path_template);
    if (fd < 0) {
        fprintf(stderr, "mkstemp failed for v14 optional layer roundtrip test\n");
        return false;
    }
    close(fd);

    bool ok = false;
    CustomPresetLibrary lib;
    CustomPresetLibrary reloaded;
    preset_library_init(&lib);
    preset_library_init(&reloaded);

    FluidScenePreset preset = {0};
    preset.domain = SCENE_DOMAIN_BOX;
    preset.dimension_mode = SCENE_DIMENSION_MODE_3D;
    preset.domain_width = 1.5f;
    preset.domain_height = 1.0f;
    preset.atmospheric_initial_state_enabled = true;
    preset.atmosphere.enabled = true;
    preset.atmosphere.seed = 9001u;
    preset.atmosphere.base_density = 0.05f;
    preset.atmosphere.density_scale = 7.25f;
    preset.atmosphere.density_threshold = 0.48f;
    preset.atmosphere.base_wind_x = 4.0f;
    preset.atmosphere.base_wind_y = 0.1f;
    preset.atmosphere.base_wind_z = 1.25f;
    preset.atmosphere.turbulence_strength = 2.75f;
    preset.atmosphere.noise_scale = 3.5f;
    preset.atmosphere.detail_scale = 11.0f;
    preset.atmosphere.band_min_y = 0.25f;
    preset.atmosphere.band_max_y = 0.75f;
    preset.atmosphere.band_edge_falloff = 0.08f;

    if (!preset_library_add_slot(&lib, "3D Optional Atmos Layer", &preset)) goto done;
    if (!preset_library_save(path_template, &lib)) goto done;
    if (!preset_library_load(path_template, &reloaded)) goto done;

    const CustomPresetSlot *loaded = preset_library_get_slot_const(&reloaded, 0);
    if (!loaded) goto done;
    if (loaded->preset.domain != SCENE_DOMAIN_BOX) goto done;
    if (loaded->preset.dimension_mode != SCENE_DIMENSION_MODE_3D) goto done;
    if (!loaded->preset.atmospheric_initial_state_enabled) goto done;
    if (!loaded->preset.atmosphere.enabled) goto done;
    if (loaded->preset.atmosphere.seed != 9001u) goto done;
    if (!approx_equal(loaded->preset.atmosphere.density_scale, 7.25f, 1e-4f)) goto done;
    if (!approx_equal(loaded->preset.atmosphere.base_wind_z, 1.25f, 1e-4f)) goto done;
    if (atmospheric_initial_state_source(&loaded->preset) !=
        ATMOSPHERIC_INITIAL_STATE_OPTIONAL_LAYER) {
        goto done;
    }
    ok = true;

done:
    preset_library_shutdown(&lib);
    preset_library_shutdown(&reloaded);
    unlink(path_template);
    return ok;
}

int main(void) {
    if (!test_legacy_omitted_z_fallback()) {
        fprintf(stderr, "preset_io_dimensional_contract_test: legacy fallback failed\n");
        return 1;
    }
    if (!test_v15_additive_roundtrip()) {
        fprintf(stderr, "preset_io_dimensional_contract_test: v15 roundtrip failed\n");
        return 1;
    }
    if (!test_3d_slot_sanitize_defaults_invalid_direction_up()) {
        fprintf(stderr, "preset_io_dimensional_contract_test: 3d default direction failed\n");
        return 1;
    }
    if (!test_v13_atmospheric_roundtrip()) {
        fprintf(stderr, "preset_io_dimensional_contract_test: v13 atmospheric roundtrip failed\n");
        return 1;
    }
    if (!test_v13_optional_layer_defaults_off()) {
        fprintf(stderr, "preset_io_dimensional_contract_test: v13 optional layer fallback failed\n");
        return 1;
    }
    if (!test_v14_optional_layer_roundtrip()) {
        fprintf(stderr, "preset_io_dimensional_contract_test: v14 optional layer roundtrip failed\n");
        return 1;
    }
    fprintf(stdout, "preset_io_dimensional_contract_test: success\n");
    return 0;
}
