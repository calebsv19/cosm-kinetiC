#ifndef PRESET_IO_H
#define PRESET_IO_H

#include <stdbool.h>

#include "app/scene_presets.h"

#define CUSTOM_PRESET_LIBRARY_INITIAL_CAPACITY 8
#define CUSTOM_PRESET_NAME_MAX 64

typedef struct CustomPresetSlot {
    bool occupied;
    char name[CUSTOM_PRESET_NAME_MAX];
    FluidScenePreset preset;
} CustomPresetSlot;

typedef struct CustomPresetLibrary {
    CustomPresetSlot *slots;
    int slot_count;
    int slot_capacity;
    int active_slot;
} CustomPresetLibrary;

void preset_library_init(CustomPresetLibrary *lib);
void preset_library_shutdown(CustomPresetLibrary *lib);
bool preset_library_load(const char *path, CustomPresetLibrary *lib);
bool preset_library_save(const char *path, const CustomPresetLibrary *lib);
CustomPresetSlot *preset_library_get_slot(CustomPresetLibrary *lib, int index);
const CustomPresetSlot *preset_library_get_slot_const(const CustomPresetLibrary *lib,
                                                      int index);
int  preset_library_count(const CustomPresetLibrary *lib);
CustomPresetSlot *preset_library_add_slot(CustomPresetLibrary *lib,
                                          const char *name,
                                          const FluidScenePreset *preset_copy);
bool preset_library_remove_slot(CustomPresetLibrary *lib, int index);

#endif // PRESET_IO_H
