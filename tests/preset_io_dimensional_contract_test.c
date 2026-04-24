#include "app/preset_io.h"
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
    if (!approx_equal(obj->position_z, 0.0f, 1e-6f)) goto done;
    if (!approx_equal(obj->size_z, obj->size_x, 1e-6f)) goto done;
    if (!approx_equal(imp->position_z, 0.0f, 1e-6f)) goto done;

    ok = true;

done:
    preset_library_shutdown(&lib);
    unlink(path_template);
    return ok;
}

static bool test_v12_additive_roundtrip(void) {
    char path_template[] = "/tmp/physics_sim_preset_v12_XXXXXX";
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
        .attached_import = -1
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

int main(void) {
    if (!test_legacy_omitted_z_fallback()) {
        fprintf(stderr, "preset_io_dimensional_contract_test: legacy fallback failed\n");
        return 1;
    }
    if (!test_v12_additive_roundtrip()) {
        fprintf(stderr, "preset_io_dimensional_contract_test: v12 roundtrip failed\n");
        return 1;
    }
    if (!test_3d_slot_sanitize_defaults_invalid_direction_up()) {
        fprintf(stderr, "preset_io_dimensional_contract_test: 3d default direction failed\n");
        return 1;
    }
    fprintf(stdout, "preset_io_dimensional_contract_test: success\n");
    return 0;
}
