#ifndef PHYSICS_SIM_FILE_PICKER_H
#define PHYSICS_SIM_FILE_PICKER_H

#include <stddef.h>

typedef enum {
    PHYSICS_SIM_FILE_PICKER_SELECTED = 0,
    PHYSICS_SIM_FILE_PICKER_CANCELLED,
    PHYSICS_SIM_FILE_PICKER_UNAVAILABLE,
    PHYSICS_SIM_FILE_PICKER_FAILED
} PhysicsSimFilePickerResult;

/* Opens a host chooser without routing prompt or path text through a shell. */
PhysicsSimFilePickerResult PhysicsSim_FilePicker_SelectFolder(const char *prompt,
                                                              const char *initial_directory,
                                                              char *out_path,
                                                              size_t out_path_size);

PhysicsSimFilePickerResult PhysicsSim_FilePicker_SelectFile(const char *prompt,
                                                            const char *initial_directory,
                                                            char *out_path,
                                                            size_t out_path_size);

#endif
