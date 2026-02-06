#include "render/kit_viz_field_adapter.h"

#include "kit_viz.h"

#include <stddef.h>

static uint8_t remap_black_level(uint8_t c, uint8_t base_black_level) {
    const uint16_t span = (uint16_t)(255u - base_black_level);
    const uint16_t scaled = (uint16_t)((uint16_t)c * span) / 255u;
    return (uint8_t)(base_black_level + scaled);
}

PhysicsKitVizDensityResult physics_kit_viz_render_density_ex(
    const PhysicsKitVizDensityRequest *request) {
    if (!request || !request->values || !request->out_rgba ||
        request->width == 0 || request->height == 0) {
        return PHYSICS_KIT_VIZ_DENSITY_INVALID_REQUEST;
    }

    size_t needed = (size_t)request->width * (size_t)request->height * 4u;
    if (request->out_rgba_size < needed) {
        return PHYSICS_KIT_VIZ_DENSITY_INVALID_REQUEST;
    }

    CoreResult r = kit_viz_build_heatmap_rgba(request->values,
                                              request->width,
                                              request->height,
                                              0.0f,
                                              1.0f,
                                              KIT_VIZ_COLORMAP_GRAYSCALE,
                                              request->out_rgba,
                                              request->out_rgba_size);
    if (r.code != CORE_OK) {
        return PHYSICS_KIT_VIZ_DENSITY_RENDER_FAILED;
    }

    if (request->base_black_level != 0) {
        for (size_t i = 0; i < needed; i += 4u) {
            request->out_rgba[i + 0] = remap_black_level(request->out_rgba[i + 0],
                                                         request->base_black_level);
            request->out_rgba[i + 1] = remap_black_level(request->out_rgba[i + 1],
                                                         request->base_black_level);
            request->out_rgba[i + 2] = remap_black_level(request->out_rgba[i + 2],
                                                         request->base_black_level);
            request->out_rgba[i + 3] = 255;
        }
    }

    return PHYSICS_KIT_VIZ_DENSITY_RENDERED;
}

bool physics_kit_viz_render_density(const PhysicsKitVizDensityRequest *request) {
    return physics_kit_viz_render_density_ex(request) == PHYSICS_KIT_VIZ_DENSITY_RENDERED;
}

PhysicsKitVizVectorResult physics_kit_viz_build_vectors_ex(
    const PhysicsKitVizVectorRequest *request) {
    if (!request || !request->vx || !request->vy || !request->out_segments ||
        !request->out_segment_count || request->width == 0 || request->height == 0 ||
        request->max_segments == 0 || request->scale <= 0.0f) {
        return PHYSICS_KIT_VIZ_VECTOR_INVALID_REQUEST;
    }

    CoreResult r = kit_viz_build_vector_segments(request->vx,
                                                 request->vy,
                                                 request->width,
                                                 request->height,
                                                 request->stride,
                                                 request->scale,
                                                 request->out_segments,
                                                 request->max_segments,
                                                 request->out_segment_count);
    if (r.code != CORE_OK) {
        return PHYSICS_KIT_VIZ_VECTOR_RENDER_FAILED;
    }
    return PHYSICS_KIT_VIZ_VECTOR_RENDERED;
}

bool physics_kit_viz_build_vectors(const PhysicsKitVizVectorRequest *request) {
    return physics_kit_viz_build_vectors_ex(request) == PHYSICS_KIT_VIZ_VECTOR_RENDERED;
}

PhysicsKitVizScalarHeatmapResult physics_kit_viz_build_scalar_heatmap_ex(
    const PhysicsKitVizScalarHeatmapRequest *request) {
    if (!request || !request->values || !request->out_rgba ||
        request->width == 0 || request->height == 0) {
        return PHYSICS_KIT_VIZ_SCALAR_HEATMAP_INVALID_REQUEST;
    }

    size_t needed = (size_t)request->width * (size_t)request->height * 4u;
    if (request->out_rgba_size < needed) {
        return PHYSICS_KIT_VIZ_SCALAR_HEATMAP_INVALID_REQUEST;
    }

    CoreResult r = kit_viz_build_heatmap_rgba(request->values,
                                              request->width,
                                              request->height,
                                              request->min_value,
                                              request->max_value,
                                              request->colormap,
                                              request->out_rgba,
                                              request->out_rgba_size);
    if (r.code != CORE_OK) {
        return PHYSICS_KIT_VIZ_SCALAR_HEATMAP_RENDER_FAILED;
    }
    return PHYSICS_KIT_VIZ_SCALAR_HEATMAP_RENDERED;
}

bool physics_kit_viz_build_scalar_heatmap(
    const PhysicsKitVizScalarHeatmapRequest *request) {
    return physics_kit_viz_build_scalar_heatmap_ex(request) ==
           PHYSICS_KIT_VIZ_SCALAR_HEATMAP_RENDERED;
}

PhysicsKitVizPolylineResult physics_kit_viz_build_polyline_ex(
    const PhysicsKitVizPolylineRequest *request) {
    if (!request || !request->xs || !request->ys || !request->out_segments ||
        !request->out_segment_count || request->max_segments == 0) {
        return PHYSICS_KIT_VIZ_POLYLINE_INVALID_REQUEST;
    }

    CoreResult r = kit_viz_build_polyline_segments(request->xs,
                                                   request->ys,
                                                   request->point_count,
                                                   request->out_segments,
                                                   request->max_segments,
                                                   request->out_segment_count);
    if (r.code != CORE_OK) {
        return PHYSICS_KIT_VIZ_POLYLINE_RENDER_FAILED;
    }
    return PHYSICS_KIT_VIZ_POLYLINE_RENDERED;
}

bool physics_kit_viz_build_polyline(const PhysicsKitVizPolylineRequest *request) {
    return physics_kit_viz_build_polyline_ex(request) ==
           PHYSICS_KIT_VIZ_POLYLINE_RENDERED;
}
