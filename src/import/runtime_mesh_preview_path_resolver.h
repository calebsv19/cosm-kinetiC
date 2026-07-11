#ifndef PHYSICS_SIM_RUNTIME_MESH_PREVIEW_PATH_RESOLVER_H
#define PHYSICS_SIM_RUNTIME_MESH_PREVIEW_PATH_RESOLVER_H

#include <stdbool.h>
#include <stddef.h>

/* Recovers legacy absolute paths that used a former user's Desktop/stls root. */
bool physics_sim_runtime_mesh_preview_resolve_migrated_path(const char *candidate,
                                                            char *out_path,
                                                            size_t out_path_size);

#endif
