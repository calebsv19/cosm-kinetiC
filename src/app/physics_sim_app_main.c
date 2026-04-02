#include "physics_sim/physics_sim_app_main.h"

#include <stdint.h>
#include <string.h>

typedef enum PhysicsSimAppStage {
    PHYSICS_SIM_APP_STAGE_INIT = 0,
    PHYSICS_SIM_APP_STAGE_BOOTSTRAPPED,
    PHYSICS_SIM_APP_STAGE_CONFIG_LOADED,
    PHYSICS_SIM_APP_STAGE_STATE_SEEDED,
    PHYSICS_SIM_APP_STAGE_SUBSYSTEMS_READY,
    PHYSICS_SIM_APP_STAGE_RUNTIME_STARTED,
    PHYSICS_SIM_APP_STAGE_LOOP_COMPLETED,
    PHYSICS_SIM_APP_STAGE_SHUTDOWN_COMPLETED
} PhysicsSimAppStage;

typedef struct PhysicsSimLaunchArgs {
    int argc;
    char **argv;
} PhysicsSimLaunchArgs;

typedef struct PhysicsSimDispatchSummary {
    uint32_t dispatch_count;
    bool used_legacy_entry;
} PhysicsSimDispatchSummary;

typedef struct PhysicsSimDispatchOutput {
    int exit_code;
    PhysicsSimDispatchSummary summary;
} PhysicsSimDispatchOutput;

typedef struct PhysicsSimAppContext {
    PhysicsSimAppStage stage;
    bool bootstrapped;
    bool config_loaded;
    bool state_seeded;
    bool subsystems_initialized;
    bool runtime_started;
    bool run_loop_completed;
    bool shutdown_completed;
    int exit_code;
    int (*legacy_entry)(int argc, char **argv);
    PhysicsSimLaunchArgs launch_args;
    PhysicsSimDispatchSummary dispatch_summary;
} PhysicsSimAppContext;

static int physics_sim_default_legacy_entry(int argc, char **argv) {
    (void)argc;
    (void)argv;
    return 1;
}

static PhysicsSimAppContext g_physics_sim_app_ctx = {
    .stage = PHYSICS_SIM_APP_STAGE_INIT,
    .exit_code = 1,
    .legacy_entry = physics_sim_default_legacy_entry
};

// Guards stage progression to keep wrapper lifecycle deterministic.
static bool physics_sim_app_transition_stage(PhysicsSimAppContext *ctx,
                                             PhysicsSimAppStage expected,
                                             PhysicsSimAppStage next) {
    if (!ctx || ctx->stage != expected) {
        return false;
    }
    ctx->stage = next;
    return true;
}

static bool physics_sim_app_bootstrap_ctx(PhysicsSimAppContext *ctx) {
    int (*legacy_entry)(int argc, char **argv) = physics_sim_default_legacy_entry;
    PhysicsSimLaunchArgs launch_args = {0};

    if (!ctx) {
        return false;
    }
    if (ctx->legacy_entry) {
        legacy_entry = ctx->legacy_entry;
    }
    launch_args = ctx->launch_args;

    memset(ctx, 0, sizeof(*ctx));
    ctx->legacy_entry = legacy_entry;
    ctx->launch_args = launch_args;
    ctx->exit_code = 1;

    if (!physics_sim_app_transition_stage(ctx,
                                          PHYSICS_SIM_APP_STAGE_INIT,
                                          PHYSICS_SIM_APP_STAGE_BOOTSTRAPPED)) {
        return false;
    }
    ctx->bootstrapped = true;
    return true;
}

static bool physics_sim_app_config_load_ctx(PhysicsSimAppContext *ctx) {
    if (!ctx) {
        return false;
    }
    if (!physics_sim_app_transition_stage(ctx,
                                          PHYSICS_SIM_APP_STAGE_BOOTSTRAPPED,
                                          PHYSICS_SIM_APP_STAGE_CONFIG_LOADED)) {
        return false;
    }
    ctx->config_loaded = true;
    return true;
}

static bool physics_sim_app_state_seed_ctx(PhysicsSimAppContext *ctx) {
    if (!ctx) {
        return false;
    }
    if (!physics_sim_app_transition_stage(ctx,
                                          PHYSICS_SIM_APP_STAGE_CONFIG_LOADED,
                                          PHYSICS_SIM_APP_STAGE_STATE_SEEDED)) {
        return false;
    }
    ctx->state_seeded = true;
    return true;
}

static bool physics_sim_app_subsystems_init_ctx(PhysicsSimAppContext *ctx) {
    if (!ctx) {
        return false;
    }
    if (!physics_sim_app_transition_stage(ctx,
                                          PHYSICS_SIM_APP_STAGE_STATE_SEEDED,
                                          PHYSICS_SIM_APP_STAGE_SUBSYSTEMS_READY)) {
        return false;
    }
    ctx->subsystems_initialized = true;
    return true;
}

static bool physics_sim_runtime_start_ctx(PhysicsSimAppContext *ctx) {
    if (!ctx) {
        return false;
    }
    if (!physics_sim_app_transition_stage(ctx,
                                          PHYSICS_SIM_APP_STAGE_SUBSYSTEMS_READY,
                                          PHYSICS_SIM_APP_STAGE_RUNTIME_STARTED)) {
        return false;
    }
    ctx->runtime_started = true;
    return true;
}

// CP2 dispatch seam: route runtime dispatch through one explicit helper.
static PhysicsSimDispatchOutput physics_sim_app_dispatch_runtime(const PhysicsSimAppContext *ctx) {
    PhysicsSimDispatchOutput out = {
        .exit_code = 1,
        .summary = {0}
    };
    if (!ctx || !ctx->legacy_entry) {
        return out;
    }

    out.summary.dispatch_count = 1u;
    out.summary.used_legacy_entry = true;
    out.exit_code = ctx->legacy_entry(ctx->launch_args.argc, ctx->launch_args.argv);
    return out;
}

static int physics_sim_app_run_loop_ctx(PhysicsSimAppContext *ctx) {
    PhysicsSimDispatchOutput dispatch_output = {0};

    if (!ctx || !ctx->legacy_entry) {
        return 1;
    }
    if (ctx->stage != PHYSICS_SIM_APP_STAGE_RUNTIME_STARTED) {
        return 1;
    }

    dispatch_output = physics_sim_app_dispatch_runtime(ctx);
    ctx->dispatch_summary = dispatch_output.summary;
    ctx->exit_code = dispatch_output.exit_code;
    if (!physics_sim_app_transition_stage(ctx,
                                          PHYSICS_SIM_APP_STAGE_RUNTIME_STARTED,
                                          PHYSICS_SIM_APP_STAGE_LOOP_COMPLETED)) {
        return 1;
    }
    ctx->run_loop_completed = true;
    return ctx->exit_code;
}

static void physics_sim_app_shutdown_ctx(PhysicsSimAppContext *ctx) {
    if (!ctx) {
        return;
    }
    if (ctx->stage == PHYSICS_SIM_APP_STAGE_SHUTDOWN_COMPLETED) {
        return;
    }
    ctx->stage = PHYSICS_SIM_APP_STAGE_SHUTDOWN_COMPLETED;
    ctx->shutdown_completed = true;
}

bool physics_sim_app_bootstrap(void) {
    return physics_sim_app_bootstrap_ctx(&g_physics_sim_app_ctx);
}

bool physics_sim_app_config_load(void) {
    return physics_sim_app_config_load_ctx(&g_physics_sim_app_ctx);
}

bool physics_sim_app_state_seed(void) {
    return physics_sim_app_state_seed_ctx(&g_physics_sim_app_ctx);
}

bool physics_sim_app_subsystems_init(void) {
    return physics_sim_app_subsystems_init_ctx(&g_physics_sim_app_ctx);
}

bool physics_sim_runtime_start(void) {
    return physics_sim_runtime_start_ctx(&g_physics_sim_app_ctx);
}

void physics_sim_app_set_legacy_entry(int (*legacy_entry)(int argc, char **argv)) {
    if (legacy_entry) {
        g_physics_sim_app_ctx.legacy_entry = legacy_entry;
    }
}

int physics_sim_app_run_loop(void) {
    return physics_sim_app_run_loop_ctx(&g_physics_sim_app_ctx);
}

void physics_sim_app_shutdown(void) {
    physics_sim_app_shutdown_ctx(&g_physics_sim_app_ctx);
}

int physics_sim_app_main(int argc, char **argv) {
    int exit_code = 1;

    g_physics_sim_app_ctx.launch_args.argc = argc;
    g_physics_sim_app_ctx.launch_args.argv = argv;

    if (!physics_sim_app_bootstrap_ctx(&g_physics_sim_app_ctx)) {
        return exit_code;
    }
    if (!physics_sim_app_config_load_ctx(&g_physics_sim_app_ctx)) {
        goto shutdown;
    }
    if (!physics_sim_app_state_seed_ctx(&g_physics_sim_app_ctx)) {
        goto shutdown;
    }
    if (!physics_sim_app_subsystems_init_ctx(&g_physics_sim_app_ctx)) {
        goto shutdown;
    }
    if (!physics_sim_runtime_start_ctx(&g_physics_sim_app_ctx)) {
        goto shutdown;
    }

    exit_code = physics_sim_app_run_loop_ctx(&g_physics_sim_app_ctx);
shutdown:
    physics_sim_app_shutdown_ctx(&g_physics_sim_app_ctx);
    return exit_code;
}
