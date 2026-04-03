#include "physics_sim/physics_sim_app_main.h"

#include <stdio.h>
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
    bool dispatch_succeeded;
    int last_dispatch_exit_code;
} PhysicsSimDispatchSummary;

typedef struct PhysicsSimDispatchOutput {
    int exit_code;
    PhysicsSimDispatchSummary summary;
} PhysicsSimDispatchOutput;

typedef struct PhysicsSimRuntimeLoopRequest {
    const struct PhysicsSimAppContext *ctx;
} PhysicsSimRuntimeLoopRequest;

typedef struct PhysicsSimRuntimeLoopOutput {
    bool dispatched;
    PhysicsSimDispatchOutput dispatch_output;
} PhysicsSimRuntimeLoopOutput;

typedef struct PhysicsSimRunLoopHandoffRequest {
    struct PhysicsSimAppContext *ctx;
    PhysicsSimRuntimeLoopRequest loop_request;
} PhysicsSimRunLoopHandoffRequest;

typedef struct PhysicsSimRunLoopHandoffOutput {
    bool dispatched;
    int dispatch_exit_code;
    int wrapper_exit_code;
} PhysicsSimRunLoopHandoffOutput;

typedef struct PhysicsSimLifecycleOwnership {
    bool bootstrap_owned;
    bool config_owned;
    bool state_seed_owned;
    bool subsystems_owned;
    bool runtime_owned;
    bool dispatch_owned;
    bool run_loop_handoff_owned;
    bool shutdown_owned;
} PhysicsSimLifecycleOwnership;

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
    PhysicsSimLifecycleOwnership ownership;
    int wrapper_error;
} PhysicsSimAppContext;

typedef enum PhysicsSimWrapperError {
    PHYSICS_SIM_WRAP_OK = 0,
    PHYSICS_SIM_WRAP_BOOTSTRAP_FAILED = 1,
    PHYSICS_SIM_WRAP_CONFIG_LOAD_FAILED = 2,
    PHYSICS_SIM_WRAP_STATE_SEED_FAILED = 3,
    PHYSICS_SIM_WRAP_SUBSYSTEMS_INIT_FAILED = 4,
    PHYSICS_SIM_WRAP_RUNTIME_START_FAILED = 5,
    PHYSICS_SIM_WRAP_DISPATCH_FAILED = 6,
    PHYSICS_SIM_WRAP_STAGE_FINALIZE_FAILED = 7,
    PHYSICS_SIM_WRAP_RUN_LOOP_HANDOFF_FAILED = 8
} PhysicsSimWrapperError;

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
                                             PhysicsSimAppStage next,
                                             const char *fn_name) {
    if (!ctx || ctx->stage != expected) {
        fprintf(stderr,
                "physics_sim: lifecycle stage order violation at %s (expected=%d actual=%d)\n",
                fn_name ? fn_name : "unknown",
                (int)expected,
                ctx ? (int)ctx->stage : -1);
        return false;
    }
    ctx->stage = next;
    return true;
}

static void physics_sim_log_wrapper_error(PhysicsSimWrapperError code,
                                          const char *fn_name,
                                          PhysicsSimAppStage stage,
                                          const char *reason) {
    fprintf(stderr,
            "physics_sim: wrapper error code=%d fn=%s stage=%d reason=%s\n",
            (int)code,
            fn_name ? fn_name : "unknown",
            (int)stage,
            reason ? reason : "unspecified");
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
    ctx->dispatch_summary.last_dispatch_exit_code = 1;
    ctx->wrapper_error = PHYSICS_SIM_WRAP_OK;

    if (!physics_sim_app_transition_stage(ctx,
                                          PHYSICS_SIM_APP_STAGE_INIT,
                                          PHYSICS_SIM_APP_STAGE_BOOTSTRAPPED,
                                          "physics_sim_app_bootstrap_ctx")) {
        return false;
    }
    ctx->bootstrapped = true;
    ctx->ownership.bootstrap_owned = true;
    return true;
}

static bool physics_sim_app_config_load_ctx(PhysicsSimAppContext *ctx) {
    if (!ctx) {
        return false;
    }
    if (!physics_sim_app_transition_stage(ctx,
                                          PHYSICS_SIM_APP_STAGE_BOOTSTRAPPED,
                                          PHYSICS_SIM_APP_STAGE_CONFIG_LOADED,
                                          "physics_sim_app_config_load_ctx")) {
        return false;
    }
    ctx->config_loaded = true;
    ctx->ownership.config_owned = true;
    return true;
}

static bool physics_sim_app_state_seed_ctx(PhysicsSimAppContext *ctx) {
    if (!ctx) {
        return false;
    }
    if (!physics_sim_app_transition_stage(ctx,
                                          PHYSICS_SIM_APP_STAGE_CONFIG_LOADED,
                                          PHYSICS_SIM_APP_STAGE_STATE_SEEDED,
                                          "physics_sim_app_state_seed_ctx")) {
        return false;
    }
    ctx->state_seeded = true;
    ctx->ownership.state_seed_owned = true;
    return true;
}

static bool physics_sim_app_subsystems_init_ctx(PhysicsSimAppContext *ctx) {
    if (!ctx) {
        return false;
    }
    if (!physics_sim_app_transition_stage(ctx,
                                          PHYSICS_SIM_APP_STAGE_STATE_SEEDED,
                                          PHYSICS_SIM_APP_STAGE_SUBSYSTEMS_READY,
                                          "physics_sim_app_subsystems_init_ctx")) {
        return false;
    }
    ctx->subsystems_initialized = true;
    ctx->ownership.subsystems_owned = true;
    return true;
}

static bool physics_sim_runtime_start_ctx(PhysicsSimAppContext *ctx) {
    if (!ctx) {
        return false;
    }
    if (!physics_sim_app_transition_stage(ctx,
                                          PHYSICS_SIM_APP_STAGE_SUBSYSTEMS_READY,
                                          PHYSICS_SIM_APP_STAGE_RUNTIME_STARTED,
                                          "physics_sim_runtime_start_ctx")) {
        return false;
    }
    ctx->runtime_started = true;
    ctx->ownership.runtime_owned = true;
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

static bool physics_sim_app_runtime_loop_adapter(const PhysicsSimRuntimeLoopRequest *request,
                                                 PhysicsSimRuntimeLoopOutput *out) {
    if (!request || !out) {
        return false;
    }
    memset(out, 0, sizeof(*out));
    if (!request->ctx || !request->ctx->legacy_entry) {
        return false;
    }
    out->dispatch_output = physics_sim_app_dispatch_runtime(request->ctx);
    out->dispatched = true;
    return true;
}

static bool physics_sim_app_run_loop_handoff_ctx(const PhysicsSimRunLoopHandoffRequest *request,
                                                 PhysicsSimRunLoopHandoffOutput *out) {
    PhysicsSimRuntimeLoopOutput loop_output = {0};
    if (!request || !out || !request->ctx || !request->loop_request.ctx) {
        if (out) {
            memset(out, 0, sizeof(*out));
            out->wrapper_exit_code = PHYSICS_SIM_WRAP_RUN_LOOP_HANDOFF_FAILED;
        }
        if (request && request->ctx) {
            request->ctx->wrapper_error = PHYSICS_SIM_WRAP_RUN_LOOP_HANDOFF_FAILED;
            physics_sim_log_wrapper_error(PHYSICS_SIM_WRAP_RUN_LOOP_HANDOFF_FAILED,
                                          "physics_sim_app_run_loop_handoff_ctx",
                                          request->ctx->stage,
                                          "invalid handoff request");
        }
        return false;
    }
    memset(out, 0, sizeof(*out));
    out->wrapper_exit_code = PHYSICS_SIM_WRAP_DISPATCH_FAILED;
    request->ctx->ownership.run_loop_handoff_owned = true;

    if (!physics_sim_app_runtime_loop_adapter(&request->loop_request, &loop_output)) {
        request->ctx->dispatch_summary.dispatch_succeeded = false;
        request->ctx->dispatch_summary.last_dispatch_exit_code = PHYSICS_SIM_WRAP_DISPATCH_FAILED;
        request->ctx->wrapper_error = PHYSICS_SIM_WRAP_DISPATCH_FAILED;
        physics_sim_log_wrapper_error(PHYSICS_SIM_WRAP_DISPATCH_FAILED,
                                      "physics_sim_app_run_loop_handoff_ctx",
                                      request->ctx->stage,
                                      "runtime loop adapter failed");
        return false;
    }

    request->ctx->dispatch_summary = loop_output.dispatch_output.summary;
    request->ctx->dispatch_summary.dispatch_succeeded = true;
    request->ctx->dispatch_summary.last_dispatch_exit_code = loop_output.dispatch_output.exit_code;
    request->ctx->ownership.dispatch_owned = true;
    request->ctx->exit_code = loop_output.dispatch_output.exit_code;
    out->dispatch_exit_code = loop_output.dispatch_output.exit_code;

    if (!physics_sim_app_transition_stage(request->ctx,
                                          PHYSICS_SIM_APP_STAGE_RUNTIME_STARTED,
                                          PHYSICS_SIM_APP_STAGE_LOOP_COMPLETED,
                                          "physics_sim_app_run_loop_handoff_ctx")) {
        physics_sim_log_wrapper_error(PHYSICS_SIM_WRAP_STAGE_FINALIZE_FAILED,
                                      "physics_sim_app_run_loop_handoff_ctx",
                                      request->ctx->stage,
                                      "failed to finalize loop stage transition");
        out->wrapper_exit_code = PHYSICS_SIM_WRAP_STAGE_FINALIZE_FAILED;
        request->ctx->wrapper_error = PHYSICS_SIM_WRAP_STAGE_FINALIZE_FAILED;
        return false;
    }

    request->ctx->run_loop_completed = true;
    request->ctx->wrapper_error = PHYSICS_SIM_WRAP_OK;
    out->dispatched = true;
    out->wrapper_exit_code = request->ctx->exit_code;
    return true;
}

static int physics_sim_app_run_loop_ctx(PhysicsSimAppContext *ctx) {
    PhysicsSimRunLoopHandoffRequest handoff_request = {0};
    PhysicsSimRunLoopHandoffOutput handoff_output = {0};

    if (!ctx || !ctx->legacy_entry) {
        if (ctx) {
            ctx->wrapper_error = PHYSICS_SIM_WRAP_DISPATCH_FAILED;
        }
        physics_sim_log_wrapper_error(PHYSICS_SIM_WRAP_DISPATCH_FAILED,
                                      "physics_sim_app_run_loop_ctx",
                                      ctx ? ctx->stage : PHYSICS_SIM_APP_STAGE_INIT,
                                      "invalid wrapper context");
        return PHYSICS_SIM_WRAP_DISPATCH_FAILED;
    }
    if (ctx->stage != PHYSICS_SIM_APP_STAGE_RUNTIME_STARTED) {
        ctx->wrapper_error = PHYSICS_SIM_WRAP_DISPATCH_FAILED;
        physics_sim_log_wrapper_error(PHYSICS_SIM_WRAP_DISPATCH_FAILED,
                                      "physics_sim_app_run_loop_ctx",
                                      ctx->stage,
                                      "run loop invoked before runtime start");
        return PHYSICS_SIM_WRAP_DISPATCH_FAILED;
    }

    handoff_request.ctx = ctx;
    handoff_request.loop_request.ctx = ctx;
    if (!physics_sim_app_run_loop_handoff_ctx(&handoff_request, &handoff_output)) {
        return handoff_output.wrapper_exit_code;
    }
    return handoff_output.wrapper_exit_code;
}

static void physics_sim_app_release_ownership_ctx(PhysicsSimAppContext *ctx) {
    if (!ctx) {
        return;
    }
    ctx->ownership.dispatch_owned = false;
    ctx->ownership.run_loop_handoff_owned = false;
    ctx->ownership.runtime_owned = false;
    ctx->ownership.subsystems_owned = false;
    ctx->ownership.state_seed_owned = false;
    ctx->ownership.config_owned = false;
    ctx->ownership.bootstrap_owned = false;
}

static void physics_sim_app_shutdown_ctx(PhysicsSimAppContext *ctx) {
    if (!ctx) {
        return;
    }
    if (ctx->stage == PHYSICS_SIM_APP_STAGE_SHUTDOWN_COMPLETED) {
        return;
    }
    physics_sim_app_release_ownership_ctx(ctx);
    ctx->ownership.shutdown_owned = true;
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
    int exit_code = PHYSICS_SIM_WRAP_BOOTSTRAP_FAILED;

    g_physics_sim_app_ctx.launch_args.argc = argc;
    g_physics_sim_app_ctx.launch_args.argv = argv;

    if (!physics_sim_app_bootstrap_ctx(&g_physics_sim_app_ctx)) {
        physics_sim_log_wrapper_error(PHYSICS_SIM_WRAP_BOOTSTRAP_FAILED,
                                      "physics_sim_app_main",
                                      g_physics_sim_app_ctx.stage,
                                      "bootstrap failed");
        return exit_code;
    }
    if (!physics_sim_app_config_load_ctx(&g_physics_sim_app_ctx)) {
        exit_code = PHYSICS_SIM_WRAP_CONFIG_LOAD_FAILED;
        physics_sim_log_wrapper_error(PHYSICS_SIM_WRAP_CONFIG_LOAD_FAILED,
                                      "physics_sim_app_main",
                                      g_physics_sim_app_ctx.stage,
                                      "config load failed");
        goto shutdown;
    }
    if (!physics_sim_app_state_seed_ctx(&g_physics_sim_app_ctx)) {
        exit_code = PHYSICS_SIM_WRAP_STATE_SEED_FAILED;
        physics_sim_log_wrapper_error(PHYSICS_SIM_WRAP_STATE_SEED_FAILED,
                                      "physics_sim_app_main",
                                      g_physics_sim_app_ctx.stage,
                                      "state seed failed");
        goto shutdown;
    }
    if (!physics_sim_app_subsystems_init_ctx(&g_physics_sim_app_ctx)) {
        exit_code = PHYSICS_SIM_WRAP_SUBSYSTEMS_INIT_FAILED;
        physics_sim_log_wrapper_error(PHYSICS_SIM_WRAP_SUBSYSTEMS_INIT_FAILED,
                                      "physics_sim_app_main",
                                      g_physics_sim_app_ctx.stage,
                                      "subsystems init failed");
        goto shutdown;
    }
    if (!physics_sim_runtime_start_ctx(&g_physics_sim_app_ctx)) {
        exit_code = PHYSICS_SIM_WRAP_RUNTIME_START_FAILED;
        physics_sim_log_wrapper_error(PHYSICS_SIM_WRAP_RUNTIME_START_FAILED,
                                      "physics_sim_app_main",
                                      g_physics_sim_app_ctx.stage,
                                      "runtime start failed");
        goto shutdown;
    }

    exit_code = physics_sim_app_run_loop_ctx(&g_physics_sim_app_ctx);
shutdown:
    physics_sim_app_shutdown_ctx(&g_physics_sim_app_ctx);
    fprintf(stderr,
            "physics_sim: wrapper exit stage=%d exit_code=%d dispatch_count=%u dispatch_ok=%d wrapper_error=%d\n",
            (int)g_physics_sim_app_ctx.stage,
            exit_code,
            (unsigned)g_physics_sim_app_ctx.dispatch_summary.dispatch_count,
            g_physics_sim_app_ctx.dispatch_summary.dispatch_succeeded ? 1 : 0,
            g_physics_sim_app_ctx.wrapper_error);
    return exit_code;
}
