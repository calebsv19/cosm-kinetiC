#include "physics_sim/physics_sim_app_main.h"

#include <string.h>

typedef struct PhysicsSimLifecycleState {
    bool bootstrapped;
    bool config_loaded;
    bool state_seeded;
    bool subsystems_initialized;
    bool runtime_started;
    bool run_loop_completed;
    bool shutdown_completed;
    int exit_code;
} PhysicsSimLifecycleState;

static PhysicsSimLifecycleState g_physics_sim_lifecycle = {0};

static int g_physics_sim_launch_argc = 0;
static char **g_physics_sim_launch_argv = NULL;

static int physics_sim_default_legacy_entry(int argc, char **argv) {
    (void)argc;
    (void)argv;
    return 1;
}

static int (*g_physics_sim_legacy_entry)(int argc, char **argv) =
    physics_sim_default_legacy_entry;

bool physics_sim_app_bootstrap(void) {
    memset(&g_physics_sim_lifecycle, 0, sizeof(g_physics_sim_lifecycle));
    g_physics_sim_lifecycle.bootstrapped = true;
    return true;
}

bool physics_sim_app_config_load(void) {
    if (!g_physics_sim_lifecycle.bootstrapped) {
        return false;
    }
    g_physics_sim_lifecycle.config_loaded = true;
    return true;
}

bool physics_sim_app_state_seed(void) {
    if (!g_physics_sim_lifecycle.config_loaded) {
        return false;
    }
    g_physics_sim_lifecycle.state_seeded = true;
    return true;
}

bool physics_sim_app_subsystems_init(void) {
    if (!g_physics_sim_lifecycle.state_seeded) {
        return false;
    }
    g_physics_sim_lifecycle.subsystems_initialized = true;
    return true;
}

bool physics_sim_runtime_start(void) {
    if (!g_physics_sim_lifecycle.subsystems_initialized) {
        return false;
    }
    g_physics_sim_lifecycle.runtime_started = true;
    return true;
}

void physics_sim_app_set_legacy_entry(int (*legacy_entry)(int argc, char **argv)) {
    if (legacy_entry) {
        g_physics_sim_legacy_entry = legacy_entry;
    }
}

int physics_sim_app_run_loop(void) {
    if (!g_physics_sim_lifecycle.runtime_started) {
        return 1;
    }
    g_physics_sim_lifecycle.exit_code =
        g_physics_sim_legacy_entry(g_physics_sim_launch_argc, g_physics_sim_launch_argv);
    g_physics_sim_lifecycle.run_loop_completed = true;
    return g_physics_sim_lifecycle.exit_code;
}

void physics_sim_app_shutdown(void) {
    if (!g_physics_sim_lifecycle.bootstrapped) {
        return;
    }
    g_physics_sim_lifecycle.shutdown_completed = true;
}

int physics_sim_app_main(int argc, char **argv) {
    int exit_code = 1;

    g_physics_sim_launch_argc = argc;
    g_physics_sim_launch_argv = argv;

    if (!physics_sim_app_bootstrap()) {
        return exit_code;
    }
    if (!physics_sim_app_config_load()) {
        physics_sim_app_shutdown();
        return exit_code;
    }
    if (!physics_sim_app_state_seed()) {
        physics_sim_app_shutdown();
        return exit_code;
    }
    if (!physics_sim_app_subsystems_init()) {
        physics_sim_app_shutdown();
        return exit_code;
    }
    if (!physics_sim_runtime_start()) {
        physics_sim_app_shutdown();
        return exit_code;
    }

    exit_code = physics_sim_app_run_loop();
    physics_sim_app_shutdown();
    return exit_code;
}
