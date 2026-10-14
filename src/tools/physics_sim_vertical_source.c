#include "physics_sim_vertical_source.h"
#include <math.h>
#include <stddef.h>

bool physics_sim_vertical_source(const double *support, int horizontal,
                                int vertical, double *full) {
    if (!support || !full || support == full || horizontal < 8 ||
        horizontal > 128 || horizontal % 8 ||
        (vertical != horizontal && vertical != 2 * horizontal)) return false;
    int plane = horizontal * horizontal;
    int supported = plane * (horizontal / 8), count = plane * vertical;
    int ratio = vertical / horizontal;
    for (int q = 0; q < 2 * supported; q++)
        if (!isfinite(support[q]) || support[q] < 0) return false;
    for (int a = 0; a < 2; a++)
        for (int k = 0; k < vertical; k++)
            for (int p = 0; p < plane; p++) {
                size_t q = (size_t)a * count + k * plane + p;
                full[q] = k < vertical / 8 ?
                    support[a * supported + (k / ratio) * plane + p] / ratio : 0;
            }
    return true;
}
