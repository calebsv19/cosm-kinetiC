#ifndef PHYSICS_SIM_SCENE_EDITOR_SCENE_LIBRARY_H
#define PHYSICS_SIM_SCENE_EDITOR_SCENE_LIBRARY_H

#include <stdbool.h>

#include "app/scene_presets.h"
#include "app/editor/scene_editor_session.h"

#define PHYSICS_SIM_SCENE_LIBRARY_MAX_ENTRIES 64

typedef enum PhysicsSimSceneLibraryMode {
    PHYSICS_SIM_SCENE_LIBRARY_MODE_2D = 0,
    PHYSICS_SIM_SCENE_LIBRARY_MODE_3D
} PhysicsSimSceneLibraryMode;

typedef enum PhysicsSimSceneLibraryEntryKind {
    PHYSICS_SIM_SCENE_LIBRARY_ENTRY_LEGACY_PRESET = 0,
    PHYSICS_SIM_SCENE_LIBRARY_ENTRY_RUNTIME_SCENE
} PhysicsSimSceneLibraryEntryKind;

typedef struct PhysicsSimSceneLibraryEntry {
    PhysicsSimSceneLibraryEntryKind kind;
    bool active;
    bool user_scene;
    char display_name[128];
    char source_path[512];
    char scene_id[128];
    int object_count;
    long long modified_unix;
} PhysicsSimSceneLibraryEntry;

typedef struct PhysicsSimSceneLibraryCatalog {
    int count;
    int selected_index;
    PhysicsSimSceneLibraryEntry entries[PHYSICS_SIM_SCENE_LIBRARY_MAX_ENTRIES];
} PhysicsSimSceneLibraryCatalog;

typedef struct PhysicsSimEditorSceneLibrary {
    PhysicsSimSceneLibraryMode mode;
    PhysicsSimSceneLibraryCatalog legacy_presets;
    PhysicsSimSceneLibraryCatalog retained_scenes;
} PhysicsSimEditorSceneLibrary;

void physics_sim_editor_scene_library_refresh(PhysicsSimEditorSceneLibrary *library,
                                              const FluidScenePreset *working_preset,
                                              const PhysicsSimEditorSession *session,
                                              const char *runtime_scene_dir,
                                              const char *current_runtime_scene_path);
const PhysicsSimSceneLibraryEntry *physics_sim_editor_scene_library_selected_legacy(const PhysicsSimEditorSceneLibrary *library);
const PhysicsSimSceneLibraryEntry *physics_sim_editor_scene_library_selected_retained(const PhysicsSimEditorSceneLibrary *library);
bool physics_sim_editor_scene_library_has_retained_entries(const PhysicsSimEditorSceneLibrary *library);
int physics_sim_editor_scene_library_find_retained_index_by_path(const PhysicsSimEditorSceneLibrary *library,
                                                                 const char *source_path);
const char *physics_sim_editor_scene_library_mode_label(PhysicsSimSceneLibraryMode mode);

#endif
