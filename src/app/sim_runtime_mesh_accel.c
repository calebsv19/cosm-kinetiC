#include "app/sim_runtime_mesh_accel.h"

#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RUNTIME_MESH_ACCEL_LEAF_TRIANGLES 8u

typedef struct RuntimeMeshAccelTriangleEntry {
    size_t triangle_index;
    CoreObjectVec3 min;
    CoreObjectVec3 max;
    CoreObjectVec3 centroid;
} RuntimeMeshAccelTriangleEntry;

typedef struct RuntimeMeshAccelNode {
    CoreObjectVec3 min;
    CoreObjectVec3 max;
    size_t start;
    size_t count;
    int left;
    int right;
    bool leaf;
} RuntimeMeshAccelNode;

static void accel_diag(PhysicsSimRuntimeMeshAccel *accel, const char *message) {
    if (!accel || !message) return;
    snprintf(accel->diagnostics, sizeof(accel->diagnostics), "%s", message);
}

void physics_sim_runtime_mesh_accel_init(PhysicsSimRuntimeMeshAccel *accel) {
    if (!accel) return;
    memset(accel, 0, sizeof(*accel));
    accel_diag(accel, "not built");
}

void physics_sim_runtime_mesh_accel_free(PhysicsSimRuntimeMeshAccel *accel) {
    if (!accel) return;
    free(accel->triangle_entries);
    free(accel->nodes);
    physics_sim_runtime_mesh_accel_init(accel);
}

static double axis_value(CoreObjectVec3 value, int axis) {
    if (axis == 0) return value.x;
    if (axis == 1) return value.y;
    return value.z;
}

static int compare_entries_axis(const RuntimeMeshAccelTriangleEntry *a,
                                const RuntimeMeshAccelTriangleEntry *b,
                                int axis) {
    double av = axis_value(a->centroid, axis);
    double bv = axis_value(b->centroid, axis);
    if (av < bv) return -1;
    if (av > bv) return 1;
    if (a->triangle_index < b->triangle_index) return -1;
    if (a->triangle_index > b->triangle_index) return 1;
    return 0;
}

static void swap_entries(RuntimeMeshAccelTriangleEntry *a,
                         RuntimeMeshAccelTriangleEntry *b) {
    RuntimeMeshAccelTriangleEntry tmp = *a;
    *a = *b;
    *b = tmp;
}

static CoreObjectVec3 vec_min(CoreObjectVec3 a, CoreObjectVec3 b) {
    CoreObjectVec3 r = {
        a.x < b.x ? a.x : b.x,
        a.y < b.y ? a.y : b.y,
        a.z < b.z ? a.z : b.z
    };
    return r;
}

static CoreObjectVec3 vec_max(CoreObjectVec3 a, CoreObjectVec3 b) {
    CoreObjectVec3 r = {
        a.x > b.x ? a.x : b.x,
        a.y > b.y ? a.y : b.y,
        a.z > b.z ? a.z : b.z
    };
    return r;
}

static double vec_dot(CoreObjectVec3 a, CoreObjectVec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static CoreObjectVec3 vec_cross(CoreObjectVec3 a, CoreObjectVec3 b) {
    CoreObjectVec3 r = {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
    return r;
}

static CoreObjectVec3 vec_sub(CoreObjectVec3 a, CoreObjectVec3 b) {
    CoreObjectVec3 r = {a.x - b.x, a.y - b.y, a.z - b.z};
    return r;
}

static RuntimeMeshAccelTriangleEntry make_entry(
    const CoreMeshAssetRuntimeDocument *document,
    const CoreObjectVec3 *world_vertices,
    size_t triangle_index) {
    const CoreMeshAssetRuntimeTriangle *tri = &document->triangles[triangle_index];
    CoreObjectVec3 a = world_vertices[tri->a];
    CoreObjectVec3 b = world_vertices[tri->b];
    CoreObjectVec3 c = world_vertices[tri->c];
    RuntimeMeshAccelTriangleEntry entry;
    entry.triangle_index = triangle_index;
    entry.min = vec_min(a, vec_min(b, c));
    entry.max = vec_max(a, vec_max(b, c));
    entry.centroid = (CoreObjectVec3){
        (a.x + b.x + c.x) / 3.0,
        (a.y + b.y + c.y) / 3.0,
        (a.z + b.z + c.z) / 3.0
    };
    return entry;
}

static int largest_axis(CoreObjectVec3 min, CoreObjectVec3 max) {
    double sx = max.x - min.x;
    double sy = max.y - min.y;
    double sz = max.z - min.z;
    if (sx >= sy && sx >= sz) return 0;
    if (sy >= sx && sy >= sz) return 1;
    return 2;
}

static void sort_entries_by_axis(RuntimeMeshAccelTriangleEntry *entries,
                                 size_t start,
                                 size_t count,
                                 int axis) {
    size_t lo = start;
    size_t hi = start + count - 1u;
    RuntimeMeshAccelTriangleEntry pivot;
    size_t i = lo;
    size_t j = hi;
    if (!entries || count < 2u) return;
    pivot = entries[start + count / 2u];
    while (i <= j) {
        while (compare_entries_axis(&entries[i], &pivot, axis) < 0) i++;
        while (compare_entries_axis(&entries[j], &pivot, axis) > 0) {
            if (j == 0u) break;
            j--;
        }
        if (i <= j) {
            swap_entries(&entries[i], &entries[j]);
            i++;
            if (j == 0u) break;
            j--;
        }
    }
    if (lo < j) sort_entries_by_axis(entries, lo, j - lo + 1u, axis);
    if (i < hi) sort_entries_by_axis(entries, i, hi - i + 1u, axis);
}

static bool compute_range_bounds(RuntimeMeshAccelTriangleEntry *entries,
                                 size_t start,
                                 size_t count,
                                 CoreObjectVec3 *out_min,
                                 CoreObjectVec3 *out_max,
                                 CoreObjectVec3 *out_centroid_min,
                                 CoreObjectVec3 *out_centroid_max) {
    if (!entries || count == 0u || !out_min || !out_max) return false;
    *out_min = entries[start].min;
    *out_max = entries[start].max;
    if (out_centroid_min) *out_centroid_min = entries[start].centroid;
    if (out_centroid_max) *out_centroid_max = entries[start].centroid;
    for (size_t i = start + 1u; i < start + count; ++i) {
        *out_min = vec_min(*out_min, entries[i].min);
        *out_max = vec_max(*out_max, entries[i].max);
        if (out_centroid_min) *out_centroid_min = vec_min(*out_centroid_min, entries[i].centroid);
        if (out_centroid_max) *out_centroid_max = vec_max(*out_centroid_max, entries[i].centroid);
    }
    return true;
}

static int build_node(PhysicsSimRuntimeMeshAccel *accel,
                      RuntimeMeshAccelTriangleEntry *entries,
                      RuntimeMeshAccelNode *nodes,
                      size_t capacity,
                      size_t start,
                      size_t count,
                      size_t depth) {
    CoreObjectVec3 centroid_min = {0};
    CoreObjectVec3 centroid_max = {0};
    RuntimeMeshAccelNode *node = NULL;
    int node_index = 0;
    if (!accel || !entries || !nodes || count == 0u ||
        accel->node_count >= capacity) {
        return -1;
    }
    node_index = (int)accel->node_count++;
    node = &nodes[node_index];
    memset(node, 0, sizeof(*node));
    node->left = -1;
    node->right = -1;
    node->start = start;
    node->count = count;
    compute_range_bounds(entries,
                         start,
                         count,
                         &node->min,
                         &node->max,
                         &centroid_min,
                         &centroid_max);
    if (depth > accel->max_depth) accel->max_depth = depth;
    if (count <= RUNTIME_MESH_ACCEL_LEAF_TRIANGLES ||
        accel->node_count + 2u > capacity) {
        node->leaf = true;
        accel->leaf_count++;
        return node_index;
    }

    sort_entries_by_axis(entries, start, count, largest_axis(centroid_min, centroid_max));
    node->left = build_node(accel,
                            entries,
                            nodes,
                            capacity,
                            start,
                            count / 2u,
                            depth + 1u);
    node->right = build_node(accel,
                             entries,
                             nodes,
                             capacity,
                             start + count / 2u,
                             count - count / 2u,
                             depth + 1u);
    if (node->left < 0 || node->right < 0) {
        node->leaf = true;
        node->left = -1;
        node->right = -1;
        accel->leaf_count++;
    }
    return node_index;
}

bool physics_sim_runtime_mesh_accel_build(
    PhysicsSimRuntimeMeshAccel *accel,
    const CoreMeshAssetRuntimeDocument *document,
    const CoreObjectVec3 *world_vertices) {
    RuntimeMeshAccelTriangleEntry *entries = NULL;
    RuntimeMeshAccelNode *nodes = NULL;
    size_t valid_triangles = 0u;
    size_t node_capacity = 0u;

    if (!accel) return false;
    physics_sim_runtime_mesh_accel_free(accel);
    accel->document = document;
    accel->world_vertices = world_vertices;
    if (!document || !world_vertices || !document->triangles ||
        document->triangle_count == 0u || document->vertex_count == 0u) {
        accel_diag(accel, "invalid mesh for acceleration");
        return false;
    }
    entries = (RuntimeMeshAccelTriangleEntry *)calloc(document->triangle_count, sizeof(*entries));
    if (!entries) {
        accel_diag(accel, "failed to allocate triangle entries");
        return false;
    }
    for (size_t i = 0u; i < document->triangle_count; ++i) {
        const CoreMeshAssetRuntimeTriangle *tri = &document->triangles[i];
        if (tri->a >= document->vertex_count ||
            tri->b >= document->vertex_count ||
            tri->c >= document->vertex_count) {
            continue;
        }
        entries[valid_triangles++] = make_entry(document, world_vertices, i);
    }
    if (valid_triangles == 0u) {
        free(entries);
        accel_diag(accel, "no valid triangles for acceleration");
        return false;
    }
    node_capacity = valid_triangles * 2u + 1u;
    nodes = (RuntimeMeshAccelNode *)calloc(node_capacity, sizeof(*nodes));
    if (!nodes) {
        free(entries);
        accel_diag(accel, "failed to allocate acceleration nodes");
        return false;
    }
    accel->triangle_entries = entries;
    accel->nodes = nodes;
    accel->triangle_count = valid_triangles;
    accel->node_count = 0u;
    accel->leaf_count = 0u;
    accel->max_depth = 0u;
    if (build_node(accel, entries, nodes, node_capacity, 0u, valid_triangles, 1u) < 0) {
        physics_sim_runtime_mesh_accel_free(accel);
        accel_diag(accel, "failed to build acceleration tree");
        return false;
    }
    accel_diag(accel, "ok");
    return true;
}

static bool slab_axis(double origin,
                      double direction,
                      double min_value,
                      double max_value,
                      double *tmin,
                      double *tmax) {
    const double eps = 1.0e-12;
    double t1 = 0.0;
    double t2 = 0.0;
    if (fabs(direction) < eps) {
        return origin >= min_value && origin <= max_value;
    }
    t1 = (min_value - origin) / direction;
    t2 = (max_value - origin) / direction;
    if (t1 > t2) {
        double tmp = t1;
        t1 = t2;
        t2 = tmp;
    }
    if (t1 > *tmin) *tmin = t1;
    if (t2 < *tmax) *tmax = t2;
    return *tmin <= *tmax;
}

static bool ray_intersects_bounds(CoreObjectVec3 origin,
                                  CoreObjectVec3 direction,
                                  CoreObjectVec3 min,
                                  CoreObjectVec3 max) {
    double tmin = 0.0;
    double tmax = DBL_MAX;
    return slab_axis(origin.x, direction.x, min.x, max.x, &tmin, &tmax) &&
           slab_axis(origin.y, direction.y, min.y, max.y, &tmin, &tmax) &&
           slab_axis(origin.z, direction.z, min.z, max.z, &tmin, &tmax);
}

static bool ray_intersects_triangle(CoreObjectVec3 origin,
                                    CoreObjectVec3 direction,
                                    CoreObjectVec3 a,
                                    CoreObjectVec3 b,
                                    CoreObjectVec3 c) {
    const double eps = 1.0e-9;
    CoreObjectVec3 edge1 = vec_sub(b, a);
    CoreObjectVec3 edge2 = vec_sub(c, a);
    CoreObjectVec3 h = vec_cross(direction, edge2);
    double det = vec_dot(edge1, h);
    double inv_det = 0.0;
    CoreObjectVec3 s = {0};
    double u = 0.0;
    CoreObjectVec3 q = {0};
    double v = 0.0;
    double t = 0.0;
    if (det > -eps && det < eps) return false;
    inv_det = 1.0 / det;
    s = vec_sub(origin, a);
    u = inv_det * vec_dot(s, h);
    if (u < -eps || u > 1.0 + eps) return false;
    q = vec_cross(s, edge1);
    v = inv_det * vec_dot(direction, q);
    if (v < -eps || u + v > 1.0 + eps) return false;
    t = inv_det * vec_dot(edge2, q);
    return t > eps;
}

static int count_node_intersections(const PhysicsSimRuntimeMeshAccel *accel,
                                    int node_index,
                                    CoreObjectVec3 origin,
                                    CoreObjectVec3 direction) {
    const RuntimeMeshAccelTriangleEntry *entries =
        (const RuntimeMeshAccelTriangleEntry *)accel->triangle_entries;
    const RuntimeMeshAccelNode *nodes = (const RuntimeMeshAccelNode *)accel->nodes;
    const RuntimeMeshAccelNode *node = NULL;
    int hits = 0;
    if (!entries || !nodes || node_index < 0 ||
        (size_t)node_index >= accel->node_count) {
        return 0;
    }
    node = &nodes[node_index];
    if (!ray_intersects_bounds(origin, direction, node->min, node->max)) return 0;
    if (!node->leaf) {
        hits += count_node_intersections(accel, node->left, origin, direction);
        hits += count_node_intersections(accel, node->right, origin, direction);
        return hits;
    }
    for (size_t i = node->start; i < node->start + node->count; ++i) {
        const RuntimeMeshAccelTriangleEntry *entry = &entries[i];
        const CoreMeshAssetRuntimeTriangle *tri = &accel->document->triangles[entry->triangle_index];
        if (ray_intersects_triangle(origin,
                                    direction,
                                    accel->world_vertices[tri->a],
                                    accel->world_vertices[tri->b],
                                    accel->world_vertices[tri->c])) {
            if (hits < INT_MAX) hits++;
        }
    }
    return hits;
}

int physics_sim_runtime_mesh_accel_count_ray_intersections(
    const PhysicsSimRuntimeMeshAccel *accel,
    CoreObjectVec3 origin,
    CoreObjectVec3 direction) {
    if (!accel || !accel->document || !accel->world_vertices ||
        !accel->triangle_entries || !accel->nodes || accel->node_count == 0u) {
        return 0;
    }
    return count_node_intersections(accel, 0, origin, direction);
}

void physics_sim_runtime_mesh_accel_stats(
    const PhysicsSimRuntimeMeshAccel *accel,
    PhysicsSimRuntimeMeshAccelStats *out_stats) {
    if (!out_stats) return;
    memset(out_stats, 0, sizeof(*out_stats));
    if (!accel) {
        snprintf(out_stats->diagnostics, sizeof(out_stats->diagnostics), "%s", "not built");
        return;
    }
    out_stats->built = accel->triangle_entries != NULL && accel->nodes != NULL &&
                       accel->node_count > 0u;
    out_stats->triangle_count = accel->triangle_count;
    out_stats->node_count = accel->node_count;
    out_stats->leaf_count = accel->leaf_count;
    out_stats->max_depth = accel->max_depth;
    snprintf(out_stats->diagnostics,
             sizeof(out_stats->diagnostics),
             "%s",
             accel->diagnostics[0] ? accel->diagnostics : "ok");
}
