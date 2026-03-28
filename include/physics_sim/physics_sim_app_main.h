#ifndef PHYSICS_SIM_PHYSICS_SIM_APP_MAIN_H
#define PHYSICS_SIM_PHYSICS_SIM_APP_MAIN_H

#include <stdbool.h>

bool physics_sim_app_bootstrap(void);
bool physics_sim_app_config_load(void);
bool physics_sim_app_state_seed(void);
bool physics_sim_app_subsystems_init(void);
bool physics_sim_runtime_start(void);
void physics_sim_app_set_legacy_entry(int (*legacy_entry)(int argc, char **argv));
int physics_sim_app_run_loop(void);
void physics_sim_app_shutdown(void);

int physics_sim_app_main(int argc, char **argv);

#endif
