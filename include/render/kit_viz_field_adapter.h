#ifndef KIT_VIZ_FIELD_ADAPTER_H
#define KIT_VIZ_FIELD_ADAPTER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "kit_viz.h"

typedef KitVizVecSegment PhysicsKitVizVectorSegment;

typedef struct PhysicsKitVizDensityRequest {
    const float *values;
    uint32_t width;
    uint32_t height;
    uint8_t base_black_level;
    uint8_t *out_rgba;
    size_t out_rgba_size;
} PhysicsKitVizDensityRequest;

typedef enum PhysicsKitVizDensityResult {
    PHYSICS_KIT_VIZ_DENSITY_RENDERED = 0,
    PHYSICS_KIT_VIZ_DENSITY_INVALID_REQUEST,
    PHYSICS_KIT_VIZ_DENSITY_RENDER_FAILED
} PhysicsKitVizDensityResult;

// Converts normalized field values [0, 1] into display-ready RGBA using kit_viz.
PhysicsKitVizDensityResult physics_kit_viz_render_density_ex(
    const PhysicsKitVizDensityRequest *request);

// Boolean convenience wrapper for call sites that do not need result details.
bool physics_kit_viz_render_density(const PhysicsKitVizDensityRequest *request);

typedef struct PhysicsKitVizVectorRequest {
    const float *vx;
    const float *vy;
    uint32_t width;
    uint32_t height;
    uint32_t stride;
    float scale;
    PhysicsKitVizVectorSegment *out_segments;
    size_t max_segments;
    size_t *out_segment_count;
} PhysicsKitVizVectorRequest;

typedef enum PhysicsKitVizVectorResult {
    PHYSICS_KIT_VIZ_VECTOR_RENDERED = 0,
    PHYSICS_KIT_VIZ_VECTOR_INVALID_REQUEST,
    PHYSICS_KIT_VIZ_VECTOR_RENDER_FAILED
} PhysicsKitVizVectorResult;

PhysicsKitVizVectorResult physics_kit_viz_build_vectors_ex(
    const PhysicsKitVizVectorRequest *request);
bool physics_kit_viz_build_vectors(const PhysicsKitVizVectorRequest *request);

typedef struct PhysicsKitVizScalarHeatmapRequest {
    const float *values;
    uint32_t width;
    uint32_t height;
    float min_value;
    float max_value;
    KitVizColormap colormap;
    uint8_t *out_rgba;
    size_t out_rgba_size;
} PhysicsKitVizScalarHeatmapRequest;

typedef enum PhysicsKitVizScalarHeatmapResult {
    PHYSICS_KIT_VIZ_SCALAR_HEATMAP_RENDERED = 0,
    PHYSICS_KIT_VIZ_SCALAR_HEATMAP_INVALID_REQUEST,
    PHYSICS_KIT_VIZ_SCALAR_HEATMAP_RENDER_FAILED
} PhysicsKitVizScalarHeatmapResult;

PhysicsKitVizScalarHeatmapResult physics_kit_viz_build_scalar_heatmap_ex(
    const PhysicsKitVizScalarHeatmapRequest *request);
bool physics_kit_viz_build_scalar_heatmap(
    const PhysicsKitVizScalarHeatmapRequest *request);

typedef struct PhysicsKitVizPolylineRequest {
    const float *xs;
    const float *ys;
    uint32_t point_count;
    PhysicsKitVizVectorSegment *out_segments;
    size_t max_segments;
    size_t *out_segment_count;
} PhysicsKitVizPolylineRequest;

typedef enum PhysicsKitVizPolylineResult {
    PHYSICS_KIT_VIZ_POLYLINE_RENDERED = 0,
    PHYSICS_KIT_VIZ_POLYLINE_INVALID_REQUEST,
    PHYSICS_KIT_VIZ_POLYLINE_RENDER_FAILED
} PhysicsKitVizPolylineResult;

PhysicsKitVizPolylineResult physics_kit_viz_build_polyline_ex(
    const PhysicsKitVizPolylineRequest *request);
bool physics_kit_viz_build_polyline(const PhysicsKitVizPolylineRequest *request);

#endif // KIT_VIZ_FIELD_ADAPTER_H
