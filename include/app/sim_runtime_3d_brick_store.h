#ifndef SIM_RUNTIME_3D_BRICK_STORE_H
#define SIM_RUNTIME_3D_BRICK_STORE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "app/sim_runtime_3d_domain.h"

typedef struct SimRuntime3DBrickRegion {
    int min_x;
    int min_y;
    int min_z;
    int max_x;
    int max_y;
    int max_z;
} SimRuntime3DBrickRegion;

typedef struct SimRuntime3DBrickStore {
    SimRuntime3DDomainDesc desc;
    int brick_size;
    int bricks_w;
    int bricks_h;
    int bricks_d;
    size_t brick_cell_count;
    size_t brick_count;
    size_t allocated_brick_count;
    void **bricks;
} SimRuntime3DBrickStore;

typedef bool (*SimRuntime3DBrickStoreActiveCellVisitor)(int x,
                                                        int y,
                                                        int z,
                                                        float density,
                                                        float velocity_x,
                                                        float velocity_y,
                                                        float velocity_z,
                                                        float pressure,
                                                        void *user_data);

bool sim_runtime_3d_brick_store_init(SimRuntime3DBrickStore *store,
                                     const SimRuntime3DDomainDesc *desc,
                                     int brick_size);
void sim_runtime_3d_brick_store_destroy(SimRuntime3DBrickStore *store);
void sim_runtime_3d_brick_store_clear(SimRuntime3DBrickStore *store);

bool sim_runtime_3d_brick_store_add_cell(SimRuntime3DBrickStore *store,
                                         int x,
                                         int y,
                                         int z,
                                         float density_delta,
                                         float velocity_x_delta,
                                         float velocity_y_delta,
                                         float velocity_z_delta,
                                         float pressure_delta);
bool sim_runtime_3d_brick_store_set_cell(SimRuntime3DBrickStore *store,
                                         int x,
                                         int y,
                                         int z,
                                         float density,
                                         float velocity_x,
                                         float velocity_y,
                                         float velocity_z,
                                         float pressure);
bool sim_runtime_3d_brick_store_get_cell(const SimRuntime3DBrickStore *store,
                                         int x,
                                         int y,
                                         int z,
                                         float *out_density,
                                         float *out_velocity_x,
                                         float *out_velocity_y,
                                         float *out_velocity_z,
                                         float *out_pressure);
bool sim_runtime_3d_brick_store_zero_cell(SimRuntime3DBrickStore *store,
                                          int x,
                                          int y,
                                          int z);

bool sim_runtime_3d_brick_store_has_active_cells(const SimRuntime3DBrickStore *store);
bool sim_runtime_3d_brick_store_active_region(const SimRuntime3DBrickStore *store,
                                              SimRuntime3DBrickRegion *out_region);
bool sim_runtime_3d_brick_store_collect_active_clusters(const SimRuntime3DBrickStore *store,
                                                        SimRuntime3DBrickRegion *out_regions,
                                                        size_t max_regions,
                                                        size_t *out_region_count,
                                                        bool *out_overflow_merged);
void sim_runtime_3d_brick_region_expand_clamped(const SimRuntime3DDomainDesc *desc,
                                                SimRuntime3DBrickRegion *region,
                                                int padding_cells);
bool sim_runtime_3d_brick_region_desc(const SimRuntime3DDomainDesc *domain_desc,
                                      const SimRuntime3DBrickRegion *region,
                                      SimRuntime3DDomainDesc *out_desc);

bool sim_runtime_3d_brick_store_materialize_region(const SimRuntime3DBrickStore *store,
                                                   const SimRuntime3DBrickRegion *region,
                                                   SimRuntime3DVolume *volume);
bool sim_runtime_3d_brick_store_commit_region(SimRuntime3DBrickStore *store,
                                              const SimRuntime3DBrickRegion *region,
                                              const SimRuntime3DVolume *volume);
bool sim_runtime_3d_brick_store_fill_slice_xy(const SimRuntime3DBrickStore *store,
                                              int z,
                                              float *density,
                                              float *velocity_x,
                                              float *velocity_y,
                                              float *pressure);
bool sim_runtime_3d_brick_store_materialize_full(const SimRuntime3DBrickStore *store,
                                                 SimRuntime3DVolume *volume);
bool sim_runtime_3d_brick_store_visit_active_cells(
    const SimRuntime3DBrickStore *store,
    SimRuntime3DBrickStoreActiveCellVisitor visitor,
    void *user_data);

#endif
