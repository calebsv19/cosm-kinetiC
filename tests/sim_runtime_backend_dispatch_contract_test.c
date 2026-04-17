#include "app/sim_runtime_backend.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

static int g_create_2d_calls = 0;
static int g_create_3d_calls = 0;
static int g_destroy_calls = 0;

static void test_backend_destroy(SimRuntimeBackend *backend) {
    ++g_destroy_calls;
    free(backend);
}

static bool test_backend_valid(const SimRuntimeBackend *backend) {
    return backend && backend->impl != NULL;
}

static const SimRuntimeBackendOps g_test_ops = {
    .destroy = test_backend_destroy,
    .valid = test_backend_valid,
};

SimRuntimeBackend *sim_runtime_backend_2d_create(const AppConfig *cfg,
                                                 const FluidScenePreset *preset,
                                                 const SimModeRoute *mode_route,
                                                 const PhysicsSimRuntimeVisualBootstrap *runtime_visual) {
    SimRuntimeBackend *backend = NULL;
    (void)cfg;
    (void)preset;
    (void)mode_route;
    (void)runtime_visual;
    ++g_create_2d_calls;
    backend = (SimRuntimeBackend *)calloc(1, sizeof(*backend));
    if (!backend) return NULL;
    backend->kind = SIM_RUNTIME_BACKEND_KIND_FLUID_2D;
    backend->impl = backend;
    backend->ops = &g_test_ops;
    return backend;
}

SimRuntimeBackend *sim_runtime_backend_3d_scaffold_create(const AppConfig *cfg,
                                                          const FluidScenePreset *preset,
                                                          const SimModeRoute *mode_route,
                                                          const PhysicsSimRuntimeVisualBootstrap *runtime_visual) {
    SimRuntimeBackend *backend = NULL;
    (void)cfg;
    (void)preset;
    (void)mode_route;
    (void)runtime_visual;
    ++g_create_3d_calls;
    backend = (SimRuntimeBackend *)calloc(1, sizeof(*backend));
    if (!backend) return NULL;
    backend->kind = SIM_RUNTIME_BACKEND_KIND_FLUID_3D_SCAFFOLD;
    backend->impl = backend;
    backend->ops = &g_test_ops;
    return backend;
}

static bool test_dispatches_to_2d_backend(void) {
    AppConfig cfg = {0};
    SimModeRoute route = {
        .backend_lane = SIM_BACKEND_CANONICAL_2D,
    };
    SimRuntimeBackend *backend = NULL;
    cfg.grid_w = 32;
    cfg.grid_h = 24;

    g_create_2d_calls = 0;
    g_create_3d_calls = 0;
    g_destroy_calls = 0;

    backend = sim_runtime_backend_create(&cfg, NULL, &route, NULL);
    if (!backend) return false;
    if (g_create_2d_calls != 1) return false;
    if (g_create_3d_calls != 0) return false;
    if (sim_runtime_backend_kind(backend) != SIM_RUNTIME_BACKEND_KIND_FLUID_2D) return false;
    if (!sim_runtime_backend_valid(backend)) return false;
    sim_runtime_backend_destroy(backend);
    if (g_destroy_calls != 1) return false;
    return true;
}

static bool test_dispatches_to_3d_scaffold_backend(void) {
    AppConfig cfg = {0};
    SimModeRoute route = {
        .backend_lane = SIM_BACKEND_CONTROLLED_3D,
    };
    SimRuntimeBackend *backend = NULL;
    cfg.grid_w = 32;
    cfg.grid_h = 24;

    g_create_2d_calls = 0;
    g_create_3d_calls = 0;
    g_destroy_calls = 0;

    backend = sim_runtime_backend_create(&cfg, NULL, &route, NULL);
    if (!backend) return false;
    if (g_create_2d_calls != 0) return false;
    if (g_create_3d_calls != 1) return false;
    if (sim_runtime_backend_kind(backend) != SIM_RUNTIME_BACKEND_KIND_FLUID_3D_SCAFFOLD) return false;
    if (!sim_runtime_backend_valid(backend)) return false;
    sim_runtime_backend_destroy(backend);
    if (g_destroy_calls != 1) return false;
    return true;
}

static bool test_null_config_returns_null(void) {
    SimModeRoute route = {
        .backend_lane = SIM_BACKEND_CANONICAL_2D,
    };
    g_create_2d_calls = 0;
    g_create_3d_calls = 0;
    if (sim_runtime_backend_create(NULL, NULL, &route, NULL) != NULL) return false;
    if (g_create_2d_calls != 0) return false;
    if (g_create_3d_calls != 0) return false;
    return true;
}

int main(void) {
    if (!test_dispatches_to_2d_backend()) {
        fprintf(stderr, "sim_runtime_backend_dispatch_contract_test: 2D dispatch failed\n");
        return 1;
    }
    if (!test_dispatches_to_3d_scaffold_backend()) {
        fprintf(stderr, "sim_runtime_backend_dispatch_contract_test: 3D dispatch failed\n");
        return 1;
    }
    if (!test_null_config_returns_null()) {
        fprintf(stderr, "sim_runtime_backend_dispatch_contract_test: null-config handling failed\n");
        return 1;
    }
    fprintf(stdout, "sim_runtime_backend_dispatch_contract_test: success\n");
    return 0;
}
