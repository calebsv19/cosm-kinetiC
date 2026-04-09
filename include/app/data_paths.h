#ifndef PHYSICS_SIM_APP_DATA_PATHS_H
#define PHYSICS_SIM_APP_DATA_PATHS_H

#include <stddef.h>
#include <stdbool.h>

const char *physics_sim_default_config_path(void);
const char *physics_sim_runtime_config_path(void);
const char *physics_sim_default_input_root(void);
const char *physics_sim_default_preset_path(void);
const char *physics_sim_runtime_preset_path(void);
const char *physics_sim_default_shape_asset_dir(void);
const char *physics_sim_default_import_dir(void);
const char *physics_sim_default_snapshot_dir(void);

const char *physics_sim_resolve_config_load_path(void);
const char *physics_sim_resolve_preset_load_path(void);
const char *physics_sim_resolve_input_root(const char *configured_root);
bool physics_sim_compose_root_path(const char *root,
                                   const char *leaf,
                                   char *out,
                                   size_t out_size);
const char *physics_sim_resolve_preset_load_path_for_root(const char *configured_root,
                                                          char *buffer,
                                                          size_t buffer_size);
const char *physics_sim_resolve_shape_asset_dir_for_root(const char *configured_root,
                                                         char *buffer,
                                                         size_t buffer_size);
const char *physics_sim_resolve_import_dir_for_root(const char *configured_root,
                                                    char *buffer,
                                                    size_t buffer_size);
const char *physics_sim_resolve_snapshot_output_dir(const char *configured_dir);

bool physics_sim_ensure_runtime_dirs(void);

#endif // PHYSICS_SIM_APP_DATA_PATHS_H
