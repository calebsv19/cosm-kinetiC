#include "physics/rigid/collider_debug.h"

#include <stdio.h>

void collider_debug_log_path(bool enabled,
                             const char *path,
                             size_t path_index,
                             int verts,
                             int corners,
                             int spans_kept,
                             int spans_total,
                             int prims) {
    if (!enabled) return;
    fprintf(stderr,
            "[collider] import %s path=%zu verts=%d corners=%d spans=%d/%d prims=%d\n",
            path ? path : "(null)",
            path_index,
            verts,
            corners,
            spans_kept,
            spans_total,
            prims);
}
