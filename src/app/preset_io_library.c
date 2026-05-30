#include "app/preset_io_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void preset_library_init(CustomPresetLibrary *lib) {
    if (!lib) return;
    memset(lib, 0, sizeof(*lib));
    lib->slots = NULL;
    lib->slot_capacity = 0;
    lib->slot_count = 0;
    lib->active_slot = 0;
}

void preset_library_shutdown(CustomPresetLibrary *lib) {
    if (!lib) return;
    free(lib->slots);
    lib->slots = NULL;
    lib->slot_capacity = 0;
    lib->slot_count = 0;
    lib->active_slot = 0;
}

int preset_library_count(const CustomPresetLibrary *lib) {
    if (!lib) return 0;
    return lib->slot_count;
}

CustomPresetSlot *preset_library_get_slot(CustomPresetLibrary *lib, int index) {
    if (!lib || index < 0 || index >= lib->slot_count) return NULL;
    return &lib->slots[index];
}

const CustomPresetSlot *preset_library_get_slot_const(const CustomPresetLibrary *lib,
                                                      int index) {
    if (!lib || index < 0 || index >= lib->slot_count) return NULL;
    return &lib->slots[index];
}

CustomPresetSlot *preset_library_add_slot(CustomPresetLibrary *lib,
                                          const char *name,
                                          const FluidScenePreset *preset_copy) {
    if (!lib) return NULL;
    if (!preset_library_reserve(lib, lib->slot_count + 1)) {
        return NULL;
    }

    CustomPresetSlot *slot = &lib->slots[lib->slot_count];
    preset_slot_reset(slot, lib->slot_count);
    if (name && name[0] != '\0') {
        snprintf(slot->name, sizeof(slot->name), "%s", name);
    }
    if (preset_copy) {
        slot->preset = *preset_copy;
        slot->preset.domain = sanitize_domain(preset_copy->domain);
        slot->preset.dimension_mode = sanitize_dimension_mode(preset_copy->dimension_mode);
        slot->preset.domain_width = sanitize_dimension_value(preset_copy->domain_width);
        slot->preset.domain_height = sanitize_dimension_value(preset_copy->domain_height);
        if (slot->preset.emitter_count > MAX_FLUID_EMITTERS) {
            slot->preset.emitter_count = MAX_FLUID_EMITTERS;
        }
        if (slot->preset.object_count > MAX_PRESET_OBJECTS) {
            slot->preset.object_count = MAX_PRESET_OBJECTS;
        }
        if (slot->preset.import_shape_count > MAX_IMPORTED_SHAPES) {
            slot->preset.import_shape_count = MAX_IMPORTED_SHAPES;
        }
        for (size_t e = 0; e < slot->preset.emitter_count; ++e) {
            sanitize_emitter(&slot->preset.emitters[e], slot->preset.dimension_mode);
        }
        for (size_t o = 0; o < slot->preset.object_count; ++o) {
            sanitize_preset_object(&slot->preset.objects[o]);
        }
        for (size_t s = 0; s < slot->preset.import_shape_count; ++s) {
            sanitize_import_shape(&slot->preset.import_shapes[s]);
        }
        sanitize_atmosphere(&slot->preset.atmosphere);
        boundary_flows_assign(slot->preset.boundary_flows,
                              preset_copy->boundary_flows);
    } else {
        memset(&slot->preset, 0, sizeof(slot->preset));
        slot->preset.is_custom = true;
        boundary_flows_reset(slot->preset.boundary_flows);
        slot->preset.domain = SCENE_DOMAIN_BOX;
        slot->preset.dimension_mode = SCENE_DIMENSION_MODE_2D;
        slot->preset.domain_width = 1.0f;
        slot->preset.domain_height = 1.0f;
        slot->preset.structural_scene_path[0] = '\0';
        slot->preset.atmosphere.enabled = false;
    }
    slot->preset.name = slot->name;
    slot->occupied = true;
    lib->slot_count++;
    return slot;
}

bool preset_library_remove_slot(CustomPresetLibrary *lib, int index) {
    if (!lib || index < 0 || index >= lib->slot_count) return false;
    for (int i = index; i + 1 < lib->slot_count; ++i) {
        lib->slots[i] = lib->slots[i + 1];
    }
    lib->slot_count--;
    if (lib->active_slot >= lib->slot_count) {
        lib->active_slot = lib->slot_count - 1;
        if (lib->active_slot < 0) lib->active_slot = 0;
    }
    return true;
}
