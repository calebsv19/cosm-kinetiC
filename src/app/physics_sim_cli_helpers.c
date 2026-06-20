#include "app/physics_sim_cli_helpers.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h>

bool physics_sim_cli_take_value(int argc,
                                char **argv,
                                int *io_index,
                                bool require_non_empty,
                                const char **out_value) {
    int next = 0;
    const char *value = NULL;
    if (!argv || !io_index || !out_value) return false;
    next = *io_index + 1;
    if (next >= argc) return false;
    value = argv[next];
    if (!value || (require_non_empty && value[0] == '\0')) return false;
    *io_index = next;
    *out_value = value;
    return true;
}

bool physics_sim_cli_parse_int_range(const char *text, int min_value, int max_value, int *out) {
    char *end = NULL;
    long value = 0;
    if (!text || !text[0] || !out || min_value > max_value) return false;
    errno = 0;
    value = strtol(text, &end, 10);
    if (errno != 0 || end == text || !end || *end != '\0') return false;
    if (value < (long)min_value || value > (long)max_value) return false;
    if (value < (long)INT_MIN || value > (long)INT_MAX) return false;
    *out = (int)value;
    return true;
}

bool physics_sim_cli_parse_float(const char *text, float *out) {
    char *end = NULL;
    double value = 0.0;
    if (!text || !text[0] || !out) return false;
    errno = 0;
    value = strtod(text, &end);
    if (errno != 0 || end == text || !end || *end != '\0') return false;
    *out = (float)value;
    return true;
}
