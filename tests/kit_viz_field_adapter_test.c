#include "render/kit_viz_field_adapter.h"

#include <stdio.h>
#include <stdlib.h>

static void fail(const char *msg) {
    fprintf(stderr, "kit_viz_field_adapter_test: %s\n", msg);
    exit(1);
}

static void expect(int cond, const char *msg) {
    if (!cond) fail(msg);
}

static void test_density_invalid(void) {
    PhysicsKitVizDensityResult r = physics_kit_viz_render_density_ex(NULL);
    expect(r == PHYSICS_KIT_VIZ_DENSITY_INVALID_REQUEST,
           "expected invalid request result for null density request");
}

static void test_density_render(void) {
    float values[4] = {0.0f, 0.25f, 0.5f, 1.0f};
    uint8_t rgba[16] = {0};
    PhysicsKitVizDensityRequest req = {
        .values = values,
        .width = 2,
        .height = 2,
        .base_black_level = 10,
        .out_rgba = rgba,
        .out_rgba_size = sizeof(rgba)
    };
    PhysicsKitVizDensityResult r = physics_kit_viz_render_density_ex(&req);
    expect(r == PHYSICS_KIT_VIZ_DENSITY_RENDERED, "expected density render success");
    expect(rgba[0] >= 10 && rgba[0] <= 255, "expected density remap at or above base black level");
    expect(rgba[0] < rgba[12], "expected monotonic grayscale mapping");
    expect(rgba[3] == 255 && rgba[7] == 255 && rgba[11] == 255 && rgba[15] == 255,
           "expected opaque alpha for all density pixels");
}

static void test_vectors_invalid(void) {
    PhysicsKitVizVectorResult r = physics_kit_viz_build_vectors_ex(NULL);
    expect(r == PHYSICS_KIT_VIZ_VECTOR_INVALID_REQUEST,
           "expected invalid request result for null vector request");
}

static void test_vectors_render(void) {
    enum { W = 4, H = 4 };
    float vx[W * H];
    float vy[W * H];
    for (int i = 0; i < W * H; ++i) {
        vx[i] = 1.0f;
        vy[i] = 0.0f;
    }

    PhysicsKitVizVectorSegment segs[16];
    size_t segment_count = 0;
    PhysicsKitVizVectorRequest req = {
        .vx = vx,
        .vy = vy,
        .width = W,
        .height = H,
        .stride = 2,
        .scale = 1.0f,
        .out_segments = segs,
        .max_segments = 16,
        .out_segment_count = &segment_count
    };
    PhysicsKitVizVectorResult r = physics_kit_viz_build_vectors_ex(&req);
    expect(r == PHYSICS_KIT_VIZ_VECTOR_RENDERED, "expected vector render success");
    expect(segment_count > 0, "expected non-zero segment output");
    expect(segs[0].x1 > segs[0].x0, "expected positive x direction in first segment");
}

static void test_scalar_heatmap_render(void) {
    float values[4] = {-1.0f, -0.5f, 0.5f, 1.0f};
    uint8_t rgba[16] = {0};
    PhysicsKitVizScalarHeatmapRequest req = {
        .values = values,
        .width = 2,
        .height = 2,
        .min_value = -1.0f,
        .max_value = 1.0f,
        .colormap = KIT_VIZ_COLORMAP_HEAT,
        .out_rgba = rgba,
        .out_rgba_size = sizeof(rgba)
    };
    PhysicsKitVizScalarHeatmapResult r = physics_kit_viz_build_scalar_heatmap_ex(&req);
    expect(r == PHYSICS_KIT_VIZ_SCALAR_HEATMAP_RENDERED,
           "expected scalar heatmap render success");
    expect(rgba[3] == 255 && rgba[15] == 255, "expected alpha set");
}

static void test_polyline_render(void) {
    const float xs[4] = {0.0f, 1.0f, 2.0f, 3.0f};
    const float ys[4] = {1.0f, 1.5f, 1.25f, 1.0f};
    PhysicsKitVizVectorSegment segs[4];
    size_t count = 0;
    PhysicsKitVizPolylineRequest req = {
        .xs = xs,
        .ys = ys,
        .point_count = 4,
        .out_segments = segs,
        .max_segments = 4,
        .out_segment_count = &count
    };
    PhysicsKitVizPolylineResult r = physics_kit_viz_build_polyline_ex(&req);
    expect(r == PHYSICS_KIT_VIZ_POLYLINE_RENDERED, "expected polyline render success");
    expect(count == 3, "expected three polyline segments");
    expect(segs[0].x0 == 0.0f && segs[0].x1 == 1.0f, "expected first segment geometry");
}

static void test_polyline_min_points(void) {
    const float xs[1] = {1.0f};
    const float ys[1] = {2.0f};
    PhysicsKitVizVectorSegment segs[1];
    size_t count = 123;
    PhysicsKitVizPolylineRequest req = {
        .xs = xs,
        .ys = ys,
        .point_count = 1,
        .out_segments = segs,
        .max_segments = 1,
        .out_segment_count = &count
    };
    PhysicsKitVizPolylineResult r = physics_kit_viz_build_polyline_ex(&req);
    expect(r == PHYSICS_KIT_VIZ_POLYLINE_RENDERED,
           "expected polyline success for point_count < 2");
    expect(count == 0, "expected zero segments for point_count < 2");
}

static void test_scalar_heatmap_invalid(void) {
    PhysicsKitVizScalarHeatmapResult r = physics_kit_viz_build_scalar_heatmap_ex(NULL);
    expect(r == PHYSICS_KIT_VIZ_SCALAR_HEATMAP_INVALID_REQUEST,
           "expected invalid scalar heatmap request for null");
}

int main(void) {
    test_density_invalid();
    test_density_render();
    test_vectors_invalid();
    test_vectors_render();
    test_scalar_heatmap_render();
    test_scalar_heatmap_invalid();
    test_polyline_render();
    test_polyline_min_points();
    puts("kit_viz_field_adapter_test: success");
    return 0;
}
