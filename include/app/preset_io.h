#ifndef PRESET_IO_H
#define PRESET_IO_H

#include <stdbool.h>

#include "app/scene_presets.h"

#define CUSTOM_PRESET_SLOT_COUNT 4
#define CUSTOM_PRESET_NAME_MAX 64

typedef struct CustomPresetSlot {
    bool occupied;
    char name[CUSTOM_PRESET_NAME_MAX];
    FluidScenePreset preset;
} CustomPresetSlot;

typedef struct CustomPresetLibrary {
    CustomPresetSlot slots[CUSTOM_PRESET_SLOT_COUNT];
    int active_slot;
} CustomPresetLibrary;

void preset_library_init(CustomPresetLibrary *lib);
bool preset_library_load(const char *path, CustomPresetLibrary *lib);
bool preset_library_save(const char *path, const CustomPresetLibrary *lib);
CustomPresetSlot *preset_library_get_slot(CustomPresetLibrary *lib, int index);
const CustomPresetSlot *preset_library_get_slot_const(const CustomPresetLibrary *lib,
                                                      int index);

#endif // PRESET_IO_H
