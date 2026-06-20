#include "app/physics_sim_diagnostic_helpers.h"

#include <stdio.h>

void physics_sim_diag_set(char *out, size_t out_size, const char *message) {
    if (!out || out_size == 0u || !message) return;
    snprintf(out, out_size, "%s", message);
}
