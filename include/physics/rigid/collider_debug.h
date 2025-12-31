#ifndef COLLIDER_DEBUG_H
#define COLLIDER_DEBUG_H

#include <stddef.h>
#include <stdbool.h>

void collider_debug_log_path(bool enabled,
                             const char *path,
                             size_t path_index,
                             int verts,
                             int corners,
                             int spans_kept,
                             int spans_total,
                             int prims);

#endif // COLLIDER_DEBUG_H
