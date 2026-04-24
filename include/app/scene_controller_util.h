#ifndef SCENE_CONTROLLER_UTIL_H
#define SCENE_CONTROLLER_UTIL_H

#include <stdbool.h>
#include <stdint.h>

typedef struct SceneState SceneState;

bool scene_controller_env_flag_enabled(const char *name);
bool scene_controller_rs1_diag_enabled(void);
bool scene_controller_ir1_diag_enabled(void);
bool scene_controller_runtime_interaction_active(const SceneState *scene);
uint32_t scene_controller_count_bool(bool value);

#endif /* SCENE_CONTROLLER_UTIL_H */
