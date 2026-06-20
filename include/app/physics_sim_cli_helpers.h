#ifndef PHYSICS_SIM_CLI_HELPERS_H
#define PHYSICS_SIM_CLI_HELPERS_H

#include <stdbool.h>

bool physics_sim_cli_take_value(int argc,
                                char **argv,
                                int *io_index,
                                bool require_non_empty,
                                const char **out_value);
bool physics_sim_cli_parse_int_range(const char *text, int min_value, int max_value, int *out);
bool physics_sim_cli_parse_float(const char *text, float *out);

#endif
