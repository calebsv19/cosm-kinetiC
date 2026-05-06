#include "app/sim_runtime_3d_brick_store.h"

#include <stdlib.h>
#include <string.h>

enum {
    SIM_RUNTIME_3D_BRICK_FIELD_COUNT = 5,
};

typedef struct SimRuntime3DBrick {
    float *density;
    float *velocity_x;
    float *velocity_y;
    float *velocity_z;
    float *pressure;
} SimRuntime3DBrick;

static int clamp_int(int value, int min_value, int max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static bool compute_brick_grid(int grid_w,
                               int grid_h,
                               int grid_d,
                               int brick_size,
                               int *out_bricks_w,
                               int *out_bricks_h,
                               int *out_bricks_d,
                               size_t *out_brick_count) {
    size_t plane_count = 0;
    size_t brick_count = 0;
    int bricks_w = 0;
    int bricks_h = 0;
    int bricks_d = 0;
    if (!out_bricks_w || !out_bricks_h || !out_bricks_d || !out_brick_count) return false;
    if (grid_w <= 0 || grid_h <= 0 || grid_d <= 0 || brick_size <= 0) return false;
    bricks_w = (grid_w + brick_size - 1) / brick_size;
    bricks_h = (grid_h + brick_size - 1) / brick_size;
    bricks_d = (grid_d + brick_size - 1) / brick_size;
    if ((size_t)bricks_w > SIZE_MAX / (size_t)bricks_h) return false;
    plane_count = (size_t)bricks_w * (size_t)bricks_h;
    if (plane_count > SIZE_MAX / (size_t)bricks_d) return false;
    brick_count = plane_count * (size_t)bricks_d;
    *out_bricks_w = bricks_w;
    *out_bricks_h = bricks_h;
    *out_bricks_d = bricks_d;
    *out_brick_count = brick_count;
    return true;
}

static size_t brick_linear_index(const SimRuntime3DBrickStore *store,
                                 int brick_x,
                                 int brick_y,
                                 int brick_z) {
    return ((size_t)brick_z * (size_t)store->bricks_h + (size_t)brick_y) *
               (size_t)store->bricks_w +
           (size_t)brick_x;
}

static size_t brick_cell_index(const SimRuntime3DBrickStore *store,
                               int x,
                               int y,
                               int z) {
    size_t local_x = (size_t)(x % store->brick_size);
    size_t local_y = (size_t)(y % store->brick_size);
    size_t local_z = (size_t)(z % store->brick_size);
    return (local_z * (size_t)store->brick_size + local_y) * (size_t)store->brick_size + local_x;
}

static SimRuntime3DBrick *brick_at(const SimRuntime3DBrickStore *store,
                                   int brick_x,
                                   int brick_y,
                                   int brick_z) {
    if (!store || !store->bricks) return NULL;
    if (brick_x < 0 || brick_x >= store->bricks_w ||
        brick_y < 0 || brick_y >= store->bricks_h ||
        brick_z < 0 || brick_z >= store->bricks_d) {
        return NULL;
    }
    return (SimRuntime3DBrick *)store->bricks[brick_linear_index(store, brick_x, brick_y, brick_z)];
}

static SimRuntime3DBrick *ensure_brick(SimRuntime3DBrickStore *store,
                                       int brick_x,
                                       int brick_y,
                                       int brick_z) {
    SimRuntime3DBrick *brick = NULL;
    size_t float_bytes = 0;
    if (!store || !store->bricks) return NULL;
    brick = brick_at(store, brick_x, brick_y, brick_z);
    if (brick) return brick;

    brick = (SimRuntime3DBrick *)calloc(1, sizeof(*brick));
    if (!brick) return NULL;
    if (store->brick_cell_count > SIZE_MAX / sizeof(float)) {
        free(brick);
        return NULL;
    }
    float_bytes = store->brick_cell_count * sizeof(float);
    brick->density = (float *)calloc(1, float_bytes);
    brick->velocity_x = (float *)calloc(1, float_bytes);
    brick->velocity_y = (float *)calloc(1, float_bytes);
    brick->velocity_z = (float *)calloc(1, float_bytes);
    brick->pressure = (float *)calloc(1, float_bytes);
    if (!brick->density ||
        !brick->velocity_x ||
        !brick->velocity_y ||
        !brick->velocity_z ||
        !brick->pressure) {
        free(brick->pressure);
        free(brick->velocity_z);
        free(brick->velocity_y);
        free(brick->velocity_x);
        free(brick->density);
        free(brick);
        return NULL;
    }

    store->bricks[brick_linear_index(store, brick_x, brick_y, brick_z)] = brick;
    store->allocated_brick_count++;
    return brick;
}

static bool brick_is_empty(const SimRuntime3DBrickStore *store, const SimRuntime3DBrick *brick) {
    if (!store || !brick) return true;
    for (size_t i = 0; i < store->brick_cell_count; ++i) {
        if (brick->density[i] != 0.0f ||
            brick->velocity_x[i] != 0.0f ||
            brick->velocity_y[i] != 0.0f ||
            brick->velocity_z[i] != 0.0f ||
            brick->pressure[i] != 0.0f) {
            return false;
        }
    }
    return true;
}

static void destroy_brick(SimRuntime3DBrickStore *store,
                          int brick_x,
                          int brick_y,
                          int brick_z) {
    size_t index = 0;
    SimRuntime3DBrick *brick = NULL;
    if (!store || !store->bricks) return;
    index = brick_linear_index(store, brick_x, brick_y, brick_z);
    brick = (SimRuntime3DBrick *)store->bricks[index];
    if (!brick) return;
    free(brick->pressure);
    free(brick->velocity_z);
    free(brick->velocity_y);
    free(brick->velocity_x);
    free(brick->density);
    free(brick);
    store->bricks[index] = NULL;
    if (store->allocated_brick_count > 0) {
        store->allocated_brick_count--;
    }
}

static bool cell_coordinates_valid(const SimRuntime3DBrickStore *store, int x, int y, int z) {
    return store &&
           x >= 0 && x < store->desc.grid_w &&
           y >= 0 && y < store->desc.grid_h &&
           z >= 0 && z < store->desc.grid_d;
}

static SimRuntime3DBrick *brick_for_cell(SimRuntime3DBrickStore *store,
                                         int x,
                                         int y,
                                         int z,
                                         bool create_if_missing) {
    int brick_x = 0;
    int brick_y = 0;
    int brick_z = 0;
    if (!cell_coordinates_valid(store, x, y, z)) return NULL;
    brick_x = x / store->brick_size;
    brick_y = y / store->brick_size;
    brick_z = z / store->brick_size;
    return create_if_missing ? ensure_brick(store, brick_x, brick_y, brick_z)
                             : brick_at(store, brick_x, brick_y, brick_z);
}

static const SimRuntime3DBrick *brick_for_cell_const(const SimRuntime3DBrickStore *store,
                                                     int x,
                                                     int y,
                                                     int z) {
    int brick_x = 0;
    int brick_y = 0;
    int brick_z = 0;
    if (!cell_coordinates_valid(store, x, y, z)) return NULL;
    brick_x = x / store->brick_size;
    brick_y = y / store->brick_size;
    brick_z = z / store->brick_size;
    return brick_at(store, brick_x, brick_y, brick_z);
}

bool sim_runtime_3d_brick_store_init(SimRuntime3DBrickStore *store,
                                     const SimRuntime3DDomainDesc *desc,
                                     int brick_size) {
    SimRuntime3DBrickStore next = {0};
    if (!store || !desc) return false;
    brick_size = clamp_int(brick_size, 2, 32);
    if (!compute_brick_grid(desc->grid_w,
                            desc->grid_h,
                            desc->grid_d,
                            brick_size,
                            &next.bricks_w,
                            &next.bricks_h,
                            &next.bricks_d,
                            &next.brick_count)) {
        return false;
    }
    if ((size_t)brick_size > SIZE_MAX / (size_t)brick_size) return false;
    next.brick_cell_count = (size_t)brick_size * (size_t)brick_size * (size_t)brick_size;
    next.desc = *desc;
    next.brick_size = brick_size;
    next.bricks = (void **)calloc(next.brick_count, sizeof(void *));
    if (!next.bricks) return false;
    *store = next;
    return true;
}

void sim_runtime_3d_brick_store_destroy(SimRuntime3DBrickStore *store) {
    if (!store) return;
    if (store->bricks) {
        for (int z = 0; z < store->bricks_d; ++z) {
            for (int y = 0; y < store->bricks_h; ++y) {
                for (int x = 0; x < store->bricks_w; ++x) {
                    destroy_brick(store, x, y, z);
                }
            }
        }
        free(store->bricks);
    }
    memset(store, 0, sizeof(*store));
}

void sim_runtime_3d_brick_store_clear(SimRuntime3DBrickStore *store) {
    if (!store || !store->bricks) return;
    for (int z = 0; z < store->bricks_d; ++z) {
        for (int y = 0; y < store->bricks_h; ++y) {
            for (int x = 0; x < store->bricks_w; ++x) {
                destroy_brick(store, x, y, z);
            }
        }
    }
}

bool sim_runtime_3d_brick_store_add_cell(SimRuntime3DBrickStore *store,
                                         int x,
                                         int y,
                                         int z,
                                         float density_delta,
                                         float velocity_x_delta,
                                         float velocity_y_delta,
                                         float velocity_z_delta,
                                         float pressure_delta) {
    SimRuntime3DBrick *brick = NULL;
    size_t cell_index = 0;
    if (!cell_coordinates_valid(store, x, y, z)) return false;
    if (density_delta == 0.0f &&
        velocity_x_delta == 0.0f &&
        velocity_y_delta == 0.0f &&
        velocity_z_delta == 0.0f &&
        pressure_delta == 0.0f) {
        return true;
    }
    brick = brick_for_cell(store, x, y, z, true);
    if (!brick) return false;
    cell_index = brick_cell_index(store, x, y, z);
    brick->density[cell_index] += density_delta;
    brick->velocity_x[cell_index] += velocity_x_delta;
    brick->velocity_y[cell_index] += velocity_y_delta;
    brick->velocity_z[cell_index] += velocity_z_delta;
    brick->pressure[cell_index] += pressure_delta;
    return true;
}

bool sim_runtime_3d_brick_store_set_cell(SimRuntime3DBrickStore *store,
                                         int x,
                                         int y,
                                         int z,
                                         float density,
                                         float velocity_x,
                                         float velocity_y,
                                         float velocity_z,
                                         float pressure) {
    SimRuntime3DBrick *brick = NULL;
    size_t cell_index = 0;
    int brick_x = 0;
    int brick_y = 0;
    int brick_z = 0;
    if (!cell_coordinates_valid(store, x, y, z)) return false;
    brick_x = x / store->brick_size;
    brick_y = y / store->brick_size;
    brick_z = z / store->brick_size;
    if (density == 0.0f &&
        velocity_x == 0.0f &&
        velocity_y == 0.0f &&
        velocity_z == 0.0f &&
        pressure == 0.0f) {
        brick = brick_for_cell(store, x, y, z, false);
        if (!brick) return true;
        cell_index = brick_cell_index(store, x, y, z);
        brick->density[cell_index] = 0.0f;
        brick->velocity_x[cell_index] = 0.0f;
        brick->velocity_y[cell_index] = 0.0f;
        brick->velocity_z[cell_index] = 0.0f;
        brick->pressure[cell_index] = 0.0f;
        if (brick_is_empty(store, brick)) {
            destroy_brick(store, brick_x, brick_y, brick_z);
        }
        return true;
    }

    brick = brick_for_cell(store, x, y, z, true);
    if (!brick) return false;
    cell_index = brick_cell_index(store, x, y, z);
    brick->density[cell_index] = density;
    brick->velocity_x[cell_index] = velocity_x;
    brick->velocity_y[cell_index] = velocity_y;
    brick->velocity_z[cell_index] = velocity_z;
    brick->pressure[cell_index] = pressure;
    return true;
}

bool sim_runtime_3d_brick_store_get_cell(const SimRuntime3DBrickStore *store,
                                         int x,
                                         int y,
                                         int z,
                                         float *out_density,
                                         float *out_velocity_x,
                                         float *out_velocity_y,
                                         float *out_velocity_z,
                                         float *out_pressure) {
    const SimRuntime3DBrick *brick = NULL;
    size_t cell_index = 0;
    if (!cell_coordinates_valid(store, x, y, z)) return false;
    brick = brick_for_cell_const(store, x, y, z);
    if (!brick) {
        if (out_density) *out_density = 0.0f;
        if (out_velocity_x) *out_velocity_x = 0.0f;
        if (out_velocity_y) *out_velocity_y = 0.0f;
        if (out_velocity_z) *out_velocity_z = 0.0f;
        if (out_pressure) *out_pressure = 0.0f;
        return true;
    }
    cell_index = brick_cell_index(store, x, y, z);
    if (out_density) *out_density = brick->density[cell_index];
    if (out_velocity_x) *out_velocity_x = brick->velocity_x[cell_index];
    if (out_velocity_y) *out_velocity_y = brick->velocity_y[cell_index];
    if (out_velocity_z) *out_velocity_z = brick->velocity_z[cell_index];
    if (out_pressure) *out_pressure = brick->pressure[cell_index];
    return true;
}

bool sim_runtime_3d_brick_store_zero_cell(SimRuntime3DBrickStore *store,
                                          int x,
                                          int y,
                                          int z) {
    return sim_runtime_3d_brick_store_set_cell(store, x, y, z, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
}

bool sim_runtime_3d_brick_store_has_active_cells(const SimRuntime3DBrickStore *store) {
    return store && store->allocated_brick_count > 0;
}

bool sim_runtime_3d_brick_store_active_region(const SimRuntime3DBrickStore *store,
                                              SimRuntime3DBrickRegion *out_region) {
    SimRuntime3DBrickRegion region = {0};
    bool have_region = false;
    if (!store || !out_region || !store->bricks) return false;
    for (int brick_z = 0; brick_z < store->bricks_d; ++brick_z) {
        for (int brick_y = 0; brick_y < store->bricks_h; ++brick_y) {
            for (int brick_x = 0; brick_x < store->bricks_w; ++brick_x) {
                const SimRuntime3DBrick *brick = brick_at(store, brick_x, brick_y, brick_z);
                int min_x = 0;
                int min_y = 0;
                int min_z = 0;
                int max_x = 0;
                int max_y = 0;
                int max_z = 0;
                if (!brick) continue;
                min_x = brick_x * store->brick_size;
                min_y = brick_y * store->brick_size;
                min_z = brick_z * store->brick_size;
                max_x = min_x + store->brick_size - 1;
                max_y = min_y + store->brick_size - 1;
                max_z = min_z + store->brick_size - 1;
                max_x = clamp_int(max_x, 0, store->desc.grid_w - 1);
                max_y = clamp_int(max_y, 0, store->desc.grid_h - 1);
                max_z = clamp_int(max_z, 0, store->desc.grid_d - 1);
                if (!have_region) {
                    region = (SimRuntime3DBrickRegion){
                        .min_x = min_x,
                        .min_y = min_y,
                        .min_z = min_z,
                        .max_x = max_x,
                        .max_y = max_y,
                        .max_z = max_z,
                    };
                    have_region = true;
                } else {
                    if (min_x < region.min_x) region.min_x = min_x;
                    if (min_y < region.min_y) region.min_y = min_y;
                    if (min_z < region.min_z) region.min_z = min_z;
                    if (max_x > region.max_x) region.max_x = max_x;
                    if (max_y > region.max_y) region.max_y = max_y;
                    if (max_z > region.max_z) region.max_z = max_z;
                }
            }
        }
    }
    if (!have_region) return false;
    *out_region = region;
    return true;
}

bool sim_runtime_3d_brick_store_collect_active_clusters(const SimRuntime3DBrickStore *store,
                                                        SimRuntime3DBrickRegion *out_regions,
                                                        size_t max_regions,
                                                        size_t *out_region_count,
                                                        bool *out_overflow_merged) {
    uint8_t *visited = NULL;
    size_t *queue = NULL;
    size_t region_count = 0u;
    bool overflow_merged = false;
    if (!store || !out_regions || max_regions == 0u || !out_region_count || !store->bricks) {
        return false;
    }
    visited = (uint8_t *)calloc(store->brick_count, sizeof(uint8_t));
    queue = (size_t *)calloc(store->brick_count, sizeof(size_t));
    if (!visited || !queue) {
        free(queue);
        free(visited);
        return false;
    }

    for (int brick_z = 0; brick_z < store->bricks_d; ++brick_z) {
        for (int brick_y = 0; brick_y < store->bricks_h; ++brick_y) {
            for (int brick_x = 0; brick_x < store->bricks_w; ++brick_x) {
                int cluster_min_x = 0;
                int cluster_min_y = 0;
                int cluster_min_z = 0;
                int cluster_max_x = 0;
                int cluster_max_y = 0;
                int cluster_max_z = 0;
                bool have_cluster = false;
                size_t head = 0u;
                size_t tail = 0u;
                size_t cluster_slot = 0u;
                size_t root_index = brick_linear_index(store, brick_x, brick_y, brick_z);
                if (!store->bricks[root_index] || visited[root_index]) continue;
                visited[root_index] = 1u;
                queue[tail++] = root_index;

                while (head < tail) {
                    size_t current = queue[head++];
                    int current_brick_x = (int)(current % (size_t)store->bricks_w);
                    size_t yz_index = current / (size_t)store->bricks_w;
                    int current_brick_y = (int)(yz_index % (size_t)store->bricks_h);
                    int current_brick_z = (int)(yz_index / (size_t)store->bricks_h);
                    int brick_min_x = current_brick_x * store->brick_size;
                    int brick_min_y = current_brick_y * store->brick_size;
                    int brick_min_z = current_brick_z * store->brick_size;
                    int brick_max_x = clamp_int(brick_min_x + store->brick_size - 1, 0, store->desc.grid_w - 1);
                    int brick_max_y = clamp_int(brick_min_y + store->brick_size - 1, 0, store->desc.grid_h - 1);
                    int brick_max_z = clamp_int(brick_min_z + store->brick_size - 1, 0, store->desc.grid_d - 1);
                    static const int neighbor_offsets[6][3] = {
                        {-1, 0, 0}, {1, 0, 0}, {0, -1, 0},
                        {0, 1, 0},  {0, 0, -1}, {0, 0, 1},
                    };
                    if (!have_cluster) {
                        cluster_min_x = brick_min_x;
                        cluster_min_y = brick_min_y;
                        cluster_min_z = brick_min_z;
                        cluster_max_x = brick_max_x;
                        cluster_max_y = brick_max_y;
                        cluster_max_z = brick_max_z;
                        have_cluster = true;
                    } else {
                        if (brick_min_x < cluster_min_x) cluster_min_x = brick_min_x;
                        if (brick_min_y < cluster_min_y) cluster_min_y = brick_min_y;
                        if (brick_min_z < cluster_min_z) cluster_min_z = brick_min_z;
                        if (brick_max_x > cluster_max_x) cluster_max_x = brick_max_x;
                        if (brick_max_y > cluster_max_y) cluster_max_y = brick_max_y;
                        if (brick_max_z > cluster_max_z) cluster_max_z = brick_max_z;
                    }
                    for (size_t neighbor_i = 0u; neighbor_i < 6u; ++neighbor_i) {
                        int neighbor_x = current_brick_x + neighbor_offsets[neighbor_i][0];
                        int neighbor_y = current_brick_y + neighbor_offsets[neighbor_i][1];
                        int neighbor_z = current_brick_z + neighbor_offsets[neighbor_i][2];
                        size_t neighbor_index = 0u;
                        if (neighbor_x < 0 || neighbor_x >= store->bricks_w ||
                            neighbor_y < 0 || neighbor_y >= store->bricks_h ||
                            neighbor_z < 0 || neighbor_z >= store->bricks_d) {
                            continue;
                        }
                        neighbor_index = brick_linear_index(store, neighbor_x, neighbor_y, neighbor_z);
                        if (!store->bricks[neighbor_index] || visited[neighbor_index]) continue;
                        visited[neighbor_index] = 1u;
                        queue[tail++] = neighbor_index;
                    }
                }

                if (!have_cluster) continue;
                cluster_slot = region_count < max_regions ? region_count : (max_regions - 1u);
                if (region_count >= max_regions) {
                    overflow_merged = true;
                    if (cluster_min_x < out_regions[cluster_slot].min_x) {
                        out_regions[cluster_slot].min_x = cluster_min_x;
                    }
                    if (cluster_min_y < out_regions[cluster_slot].min_y) {
                        out_regions[cluster_slot].min_y = cluster_min_y;
                    }
                    if (cluster_min_z < out_regions[cluster_slot].min_z) {
                        out_regions[cluster_slot].min_z = cluster_min_z;
                    }
                    if (cluster_max_x > out_regions[cluster_slot].max_x) {
                        out_regions[cluster_slot].max_x = cluster_max_x;
                    }
                    if (cluster_max_y > out_regions[cluster_slot].max_y) {
                        out_regions[cluster_slot].max_y = cluster_max_y;
                    }
                    if (cluster_max_z > out_regions[cluster_slot].max_z) {
                        out_regions[cluster_slot].max_z = cluster_max_z;
                    }
                } else {
                    out_regions[cluster_slot] = (SimRuntime3DBrickRegion){
                        .min_x = cluster_min_x,
                        .min_y = cluster_min_y,
                        .min_z = cluster_min_z,
                        .max_x = cluster_max_x,
                        .max_y = cluster_max_y,
                        .max_z = cluster_max_z,
                    };
                    region_count++;
                }
            }
        }
    }

    free(queue);
    free(visited);
    *out_region_count = region_count;
    if (out_overflow_merged) *out_overflow_merged = overflow_merged;
    return region_count > 0u;
}

void sim_runtime_3d_brick_region_expand_clamped(const SimRuntime3DDomainDesc *desc,
                                                SimRuntime3DBrickRegion *region,
                                                int padding_cells) {
    if (!desc || !region || padding_cells <= 0) return;
    region->min_x = clamp_int(region->min_x - padding_cells, 0, desc->grid_w - 1);
    region->min_y = clamp_int(region->min_y - padding_cells, 0, desc->grid_h - 1);
    region->min_z = clamp_int(region->min_z - padding_cells, 0, desc->grid_d - 1);
    region->max_x = clamp_int(region->max_x + padding_cells, 0, desc->grid_w - 1);
    region->max_y = clamp_int(region->max_y + padding_cells, 0, desc->grid_h - 1);
    region->max_z = clamp_int(region->max_z + padding_cells, 0, desc->grid_d - 1);
}

bool sim_runtime_3d_brick_region_desc(const SimRuntime3DDomainDesc *domain_desc,
                                      const SimRuntime3DBrickRegion *region,
                                      SimRuntime3DDomainDesc *out_desc) {
    SimRuntime3DDomainDesc desc = {0};
    int region_w = 0;
    int region_h = 0;
    int region_d = 0;
    if (!domain_desc || !region || !out_desc) return false;
    if (region->min_x < 0 || region->min_y < 0 || region->min_z < 0 ||
        region->max_x < region->min_x ||
        region->max_y < region->min_y ||
        region->max_z < region->min_z ||
        region->max_x >= domain_desc->grid_w ||
        region->max_y >= domain_desc->grid_h ||
        region->max_z >= domain_desc->grid_d) {
        return false;
    }
    desc = *domain_desc;
    region_w = region->max_x - region->min_x + 1;
    region_h = region->max_y - region->min_y + 1;
    region_d = region->max_z - region->min_z + 1;
    desc.grid_w = region_w;
    desc.grid_h = region_h;
    desc.grid_d = region_d;
    desc.applied_major_axis_cells = region_w > region_h ? region_w : region_h;
    desc.applied_depth_cells = region_d;
    desc.requested_major_axis_cells = desc.applied_major_axis_cells;
    desc.requested_depth_cells = desc.applied_depth_cells;
    desc.slice_cell_count = (size_t)region_w * (size_t)region_h;
    desc.cell_count = desc.slice_cell_count * (size_t)region_d;
    desc.world_min_x = domain_desc->world_min_x + (float)region->min_x * domain_desc->voxel_size;
    desc.world_min_y = domain_desc->world_min_y + (float)region->min_y * domain_desc->voxel_size;
    desc.world_min_z = domain_desc->world_min_z + (float)region->min_z * domain_desc->voxel_size;
    desc.world_max_x = desc.world_min_x + (float)region_w * domain_desc->voxel_size;
    desc.world_max_y = desc.world_min_y + (float)region_h * domain_desc->voxel_size;
    desc.world_max_z = desc.world_min_z + (float)region_d * domain_desc->voxel_size;
    *out_desc = desc;
    return true;
}

bool sim_runtime_3d_brick_store_materialize_region(const SimRuntime3DBrickStore *store,
                                                   const SimRuntime3DBrickRegion *region,
                                                   SimRuntime3DVolume *volume) {
    if (!store || !region || !volume) return false;
    if (volume->desc.grid_w != (region->max_x - region->min_x + 1) ||
        volume->desc.grid_h != (region->max_y - region->min_y + 1) ||
        volume->desc.grid_d != (region->max_z - region->min_z + 1)) {
        return false;
    }
    sim_runtime_3d_volume_clear(volume);
    for (int z = region->min_z; z <= region->max_z; ++z) {
        for (int y = region->min_y; y <= region->max_y; ++y) {
            for (int x = region->min_x; x <= region->max_x; ++x) {
                const SimRuntime3DBrick *brick = brick_for_cell_const(store, x, y, z);
                size_t dst_idx = 0;
                size_t src_idx = 0;
                if (!brick) continue;
                dst_idx = sim_runtime_3d_volume_index(&volume->desc,
                                                      x - region->min_x,
                                                      y - region->min_y,
                                                      z - region->min_z);
                src_idx = brick_cell_index(store, x, y, z);
                volume->density[dst_idx] = brick->density[src_idx];
                volume->velocity_x[dst_idx] = brick->velocity_x[src_idx];
                volume->velocity_y[dst_idx] = brick->velocity_y[src_idx];
                volume->velocity_z[dst_idx] = brick->velocity_z[src_idx];
                volume->pressure[dst_idx] = brick->pressure[src_idx];
            }
        }
    }
    return true;
}

bool sim_runtime_3d_brick_store_commit_region(SimRuntime3DBrickStore *store,
                                              const SimRuntime3DBrickRegion *region,
                                              const SimRuntime3DVolume *volume) {
    if (!store || !region || !volume) return false;
    if (volume->desc.grid_w != (region->max_x - region->min_x + 1) ||
        volume->desc.grid_h != (region->max_y - region->min_y + 1) ||
        volume->desc.grid_d != (region->max_z - region->min_z + 1)) {
        return false;
    }
    for (int z = region->min_z; z <= region->max_z; ++z) {
        for (int y = region->min_y; y <= region->max_y; ++y) {
            for (int x = region->min_x; x <= region->max_x; ++x) {
                size_t src_idx = sim_runtime_3d_volume_index(&volume->desc,
                                                             x - region->min_x,
                                                             y - region->min_y,
                                                             z - region->min_z);
                if (!sim_runtime_3d_brick_store_set_cell(store,
                                                         x,
                                                         y,
                                                         z,
                                                         volume->density[src_idx],
                                                         volume->velocity_x[src_idx],
                                                         volume->velocity_y[src_idx],
                                                         volume->velocity_z[src_idx],
                                                         volume->pressure[src_idx])) {
                    return false;
                }
            }
        }
    }
    return true;
}

bool sim_runtime_3d_brick_store_fill_slice_xy(const SimRuntime3DBrickStore *store,
                                              int z,
                                              float *density,
                                              float *velocity_x,
                                              float *velocity_y,
                                              float *pressure) {
    int brick_z = 0;
    if (!store || z < 0 || z >= store->desc.grid_d) return false;
    if ((density && !velocity_x) || (density && !velocity_y) || (density && !pressure)) {
        return false;
    }
    if (density) memset(density, 0, store->desc.slice_cell_count * sizeof(float));
    if (velocity_x) memset(velocity_x, 0, store->desc.slice_cell_count * sizeof(float));
    if (velocity_y) memset(velocity_y, 0, store->desc.slice_cell_count * sizeof(float));
    if (pressure) memset(pressure, 0, store->desc.slice_cell_count * sizeof(float));

    brick_z = z / store->brick_size;
    for (int brick_y = 0; brick_y < store->bricks_h; ++brick_y) {
        for (int brick_x = 0; brick_x < store->bricks_w; ++brick_x) {
            const SimRuntime3DBrick *brick = brick_at(store, brick_x, brick_y, brick_z);
            int min_x = 0;
            int min_y = 0;
            int max_x = 0;
            int max_y = 0;
            if (!brick) continue;
            min_x = brick_x * store->brick_size;
            min_y = brick_y * store->brick_size;
            max_x = clamp_int(min_x + store->brick_size - 1, 0, store->desc.grid_w - 1);
            max_y = clamp_int(min_y + store->brick_size - 1, 0, store->desc.grid_h - 1);
            for (int y = min_y; y <= max_y; ++y) {
                for (int x = min_x; x <= max_x; ++x) {
                    size_t slice_idx = (size_t)y * (size_t)store->desc.grid_w + (size_t)x;
                    size_t cell_index = brick_cell_index(store, x, y, z);
                    if (density) density[slice_idx] = brick->density[cell_index];
                    if (velocity_x) velocity_x[slice_idx] = brick->velocity_x[cell_index];
                    if (velocity_y) velocity_y[slice_idx] = brick->velocity_y[cell_index];
                    if (pressure) pressure[slice_idx] = brick->pressure[cell_index];
                }
            }
        }
    }
    return true;
}

bool sim_runtime_3d_brick_store_materialize_full(const SimRuntime3DBrickStore *store,
                                                 SimRuntime3DVolume *volume) {
    if (!store || !volume) return false;
    if (volume->desc.grid_w != store->desc.grid_w ||
        volume->desc.grid_h != store->desc.grid_h ||
        volume->desc.grid_d != store->desc.grid_d) {
        return false;
    }
    sim_runtime_3d_volume_clear(volume);
    for (int brick_z = 0; brick_z < store->bricks_d; ++brick_z) {
        for (int brick_y = 0; brick_y < store->bricks_h; ++brick_y) {
            for (int brick_x = 0; brick_x < store->bricks_w; ++brick_x) {
                const SimRuntime3DBrick *brick = brick_at(store, brick_x, brick_y, brick_z);
                int min_x = 0;
                int min_y = 0;
                int min_z = 0;
                int max_x = 0;
                int max_y = 0;
                int max_z = 0;
                if (!brick) continue;
                min_x = brick_x * store->brick_size;
                min_y = brick_y * store->brick_size;
                min_z = brick_z * store->brick_size;
                max_x = clamp_int(min_x + store->brick_size - 1, 0, store->desc.grid_w - 1);
                max_y = clamp_int(min_y + store->brick_size - 1, 0, store->desc.grid_h - 1);
                max_z = clamp_int(min_z + store->brick_size - 1, 0, store->desc.grid_d - 1);
                for (int z = min_z; z <= max_z; ++z) {
                    for (int y = min_y; y <= max_y; ++y) {
                        for (int x = min_x; x <= max_x; ++x) {
                            size_t dst_idx = sim_runtime_3d_volume_index(&volume->desc, x, y, z);
                            size_t src_idx = brick_cell_index(store, x, y, z);
                            volume->density[dst_idx] = brick->density[src_idx];
                            volume->velocity_x[dst_idx] = brick->velocity_x[src_idx];
                            volume->velocity_y[dst_idx] = brick->velocity_y[src_idx];
                            volume->velocity_z[dst_idx] = brick->velocity_z[src_idx];
                            volume->pressure[dst_idx] = brick->pressure[src_idx];
                        }
                    }
                }
            }
        }
    }
    return true;
}

bool sim_runtime_3d_brick_store_visit_active_cells(
    const SimRuntime3DBrickStore *store,
    SimRuntime3DBrickStoreActiveCellVisitor visitor,
    void *user_data) {
    if (!store || !visitor || !store->bricks) return false;
    for (int brick_z = 0; brick_z < store->bricks_d; ++brick_z) {
        for (int brick_y = 0; brick_y < store->bricks_h; ++brick_y) {
            for (int brick_x = 0; brick_x < store->bricks_w; ++brick_x) {
                const SimRuntime3DBrick *brick = brick_at(store, brick_x, brick_y, brick_z);
                int min_x = 0;
                int min_y = 0;
                int min_z = 0;
                int max_x = 0;
                int max_y = 0;
                int max_z = 0;
                if (!brick) continue;
                min_x = brick_x * store->brick_size;
                min_y = brick_y * store->brick_size;
                min_z = brick_z * store->brick_size;
                max_x = clamp_int(min_x + store->brick_size - 1, 0, store->desc.grid_w - 1);
                max_y = clamp_int(min_y + store->brick_size - 1, 0, store->desc.grid_h - 1);
                max_z = clamp_int(min_z + store->brick_size - 1, 0, store->desc.grid_d - 1);
                for (int z = min_z; z <= max_z; ++z) {
                    for (int y = min_y; y <= max_y; ++y) {
                        for (int x = min_x; x <= max_x; ++x) {
                            size_t cell_index = brick_cell_index(store, x, y, z);
                            if (!visitor(x,
                                         y,
                                         z,
                                         brick->density[cell_index],
                                         brick->velocity_x[cell_index],
                                         brick->velocity_y[cell_index],
                                         brick->velocity_z[cell_index],
                                         brick->pressure[cell_index],
                                         user_data)) {
                                return false;
                            }
                        }
                    }
                }
            }
        }
    }
    return true;
}
