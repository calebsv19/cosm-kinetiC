#ifndef SIM_RUNTIME_BACKEND_3D_TEST_SUPPORT_H
#define SIM_RUNTIME_BACKEND_3D_TEST_SUPPORT_H

#include "app/sim_runtime_backend.h"

#include <stdbool.h>
#include <string.h>

#if defined(__GNUC__) || defined(__clang__)
#define SIM_RUNTIME_TEST_SUPPORT_UNUSED __attribute__((unused))
#else
#define SIM_RUNTIME_TEST_SUPPORT_UNUSED
#endif

typedef struct SimRuntimeBackend3DScaffoldTestView {
    SimRuntime3DVolume volume;
    const uint8_t *obstacle_occupancy;
    int compatibility_slice_z;
    bool fluid_slice_dirty;
} SimRuntimeBackend3DScaffoldTestView;

static SIM_RUNTIME_TEST_SUPPORT_UNUSED bool sim_runtime_backend_3d_test_view_refresh(
    const SimRuntimeBackend *backend,
    SimRuntimeBackend3DScaffoldTestView *out_view) {
    SimRuntime3DDomainDesc desc = {0};
    SimRuntimeBackendReport report = {0};
    SceneFluidVolumeExportView3D export_view = {0};
    if (!backend || !out_view) return false;
    if (!sim_runtime_backend_get_domain_desc_3d(backend, &desc)) return false;
    if (!sim_runtime_backend_get_report(backend, &report)) return false;
    if (!sim_runtime_backend_get_volume_export_view_3d(backend, &export_view)) return false;
    memset(out_view, 0, sizeof(*out_view));
    out_view->volume.desc = desc;
    out_view->volume.density = (float *)export_view.density;
    out_view->volume.velocity_x = (float *)export_view.velocity_x;
    out_view->volume.velocity_y = (float *)export_view.velocity_y;
    out_view->volume.velocity_z = (float *)export_view.velocity_z;
    out_view->volume.pressure = (float *)export_view.pressure;
    out_view->obstacle_occupancy = export_view.solid_mask;
    out_view->compatibility_slice_z = report.compatibility_slice_z;
    out_view->fluid_slice_dirty = report.compatibility_view_2d_derived;
    return true;
}

static SIM_RUNTIME_TEST_SUPPORT_UNUSED bool sim_runtime_backend_3d_test_write_cell(
    SimRuntimeBackend *backend,
    int x,
    int y,
    int z,
    float density,
    float velocity_x,
    float velocity_y,
    float velocity_z,
    float pressure,
    uint8_t solid) {
    return sim_runtime_backend_debug_write_volume_cell_3d(backend,
                                                          x,
                                                          y,
                                                          z,
                                                          density,
                                                          velocity_x,
                                                          velocity_y,
                                                          velocity_z,
                                                          pressure,
                                                          solid);
}

static SIM_RUNTIME_TEST_SUPPORT_UNUSED bool sim_runtime_backend_3d_test_zero_dense_mirror(
    SimRuntimeBackend *backend) {
    return sim_runtime_backend_debug_zero_dense_mirror_3d(backend);
}

static SIM_RUNTIME_TEST_SUPPORT_UNUSED bool sim_runtime_backend_3d_test_zero_obstacle_dense_cache(
    SimRuntimeBackend *backend) {
    return sim_runtime_backend_debug_zero_obstacle_dense_cache_3d(backend);
}

static SIM_RUNTIME_TEST_SUPPORT_UNUSED bool sim_runtime_backend_3d_test_reset_truth(
    SimRuntimeBackend *backend) {
    return sim_runtime_backend_debug_reset_volume_truth_3d(backend);
}

#undef SIM_RUNTIME_TEST_SUPPORT_UNUSED

#endif
