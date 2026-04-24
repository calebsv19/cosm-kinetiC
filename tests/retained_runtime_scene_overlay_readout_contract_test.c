#include "render/retained_runtime_scene_overlay_readout.h"

#include <stdbool.h>
#include <stdio.h>

void vk_renderer_set_draw_color(VkRenderer *renderer, float r, float g, float b, float a) {
    (void)renderer;
    (void)r;
    (void)g;
    (void)b;
    (void)a;
}

void vk_renderer_draw_line(VkRenderer *renderer, float x0, float y0, float x1, float y1) {
    (void)renderer;
    (void)x0;
    (void)y0;
    (void)x1;
    (void)y1;
}

void vk_renderer_fill_rect(VkRenderer *renderer, const SDL_Rect *rect) {
    (void)renderer;
    (void)rect;
}

static bool nearly_equal(double a, double b) {
    double diff = a - b;
    if (diff < 0.0) diff = -diff;
    return diff < 0.0001;
}

static bool test_stride_scales_with_cell_budget(void) {
    return retained_runtime_overlay_readout_stride_for_cell_count(1024) == 1 &&
           retained_runtime_overlay_readout_stride_for_cell_count(8192) == 2 &&
           retained_runtime_overlay_readout_stride_for_cell_count(50000) == 3 &&
           retained_runtime_overlay_readout_stride_for_cell_count(120000) == 4 &&
           retained_runtime_overlay_readout_stride_for_cell_count(300000) == 6;
}

static bool test_density_threshold_tracks_peak_density_with_floor(void) {
    return nearly_equal(retained_runtime_overlay_readout_density_threshold(0.0f), 0.015) &&
           nearly_equal(retained_runtime_overlay_readout_density_threshold(0.05f), 0.015) &&
           nearly_equal(retained_runtime_overlay_readout_density_threshold(1.0f), 0.12);
}

static bool test_voxel_center_uses_world_min_and_voxel_size(void) {
    SceneDebugVolumeView3D view = {
        .world_min_x = -1.0f,
        .world_min_y = -2.0f,
        .world_min_z = -3.0f,
        .voxel_size = 0.25f,
    };
    CoreObjectVec3 center = retained_runtime_overlay_readout_voxel_center(&view, 0, 1, 2);
    return nearly_equal(center.x, -0.875) &&
           nearly_equal(center.y, -1.625) &&
           nearly_equal(center.z, -2.375);
}

int main(void) {
    if (!test_stride_scales_with_cell_budget()) {
        fprintf(stderr,
                "retained_runtime_scene_overlay_readout_contract_test: stride mapping failed\n");
        return 1;
    }
    if (!test_density_threshold_tracks_peak_density_with_floor()) {
        fprintf(stderr,
                "retained_runtime_scene_overlay_readout_contract_test: density threshold failed\n");
        return 1;
    }
    if (!test_voxel_center_uses_world_min_and_voxel_size()) {
        fprintf(stderr,
                "retained_runtime_scene_overlay_readout_contract_test: voxel center failed\n");
        return 1;
    }
    fprintf(stdout, "retained_runtime_scene_overlay_readout_contract_test: success\n");
    return 0;
}
