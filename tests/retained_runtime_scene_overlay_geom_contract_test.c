#include "render/retained_runtime_scene_overlay_geom.h"

#include <stdio.h>

static bool nearly_equal(double a, double b) {
    double diff = a - b;
    if (diff < 0.0) diff = -diff;
    return diff < 0.0001;
}

static bool test_prism_corner_order_matches_rectangular_box_edges(void) {
    CoreSceneRectPrismPrimitive prism = {0};
    CoreObjectVec3 corners[8];
    prism.frame.origin = (CoreObjectVec3){0.0, 0.0, 0.0};
    prism.frame.axis_u = (CoreObjectVec3){1.0, 0.0, 0.0};
    prism.frame.axis_v = (CoreObjectVec3){0.0, 1.0, 0.0};
    prism.frame.normal = (CoreObjectVec3){0.0, 0.0, 1.0};
    prism.width = 2.0;
    prism.height = 4.0;
    prism.depth = 6.0;

    retained_runtime_overlay_fill_prism_corners(&prism, corners);

    return nearly_equal(corners[0].x, -1.0) && nearly_equal(corners[0].y, -2.0) &&
           nearly_equal(corners[0].z, -3.0) && nearly_equal(corners[1].x, 1.0) &&
           nearly_equal(corners[1].y, -2.0) && nearly_equal(corners[1].z, -3.0) &&
           nearly_equal(corners[2].x, 1.0) && nearly_equal(corners[2].y, 2.0) &&
           nearly_equal(corners[2].z, -3.0) && nearly_equal(corners[3].x, -1.0) &&
           nearly_equal(corners[3].y, 2.0) && nearly_equal(corners[3].z, -3.0) &&
           nearly_equal(corners[4].x, -1.0) && nearly_equal(corners[4].y, -2.0) &&
           nearly_equal(corners[4].z, 3.0) && nearly_equal(corners[5].x, 1.0) &&
           nearly_equal(corners[5].y, -2.0) && nearly_equal(corners[5].z, 3.0) &&
           nearly_equal(corners[6].x, 1.0) && nearly_equal(corners[6].y, 2.0) &&
           nearly_equal(corners[6].z, 3.0) && nearly_equal(corners[7].x, -1.0) &&
           nearly_equal(corners[7].y, 2.0) && nearly_equal(corners[7].z, 3.0);
}

static bool test_aabb_corner_order_matches_prism_edges(void) {
    CoreObjectVec3 corners[8];
    retained_runtime_overlay_fill_aabb_corners((CoreObjectVec3){-2.0, -3.0, -4.0},
                                               (CoreObjectVec3){2.0, 3.0, 4.0},
                                               corners);
    return nearly_equal(corners[0].x, -2.0) && nearly_equal(corners[0].y, -3.0) &&
           nearly_equal(corners[0].z, -4.0) && nearly_equal(corners[1].x, 2.0) &&
           nearly_equal(corners[1].y, -3.0) && nearly_equal(corners[1].z, -4.0) &&
           nearly_equal(corners[2].x, 2.0) && nearly_equal(corners[2].y, 3.0) &&
           nearly_equal(corners[2].z, -4.0) && nearly_equal(corners[3].x, -2.0) &&
           nearly_equal(corners[3].y, 3.0) && nearly_equal(corners[3].z, -4.0) &&
           nearly_equal(corners[4].x, -2.0) && nearly_equal(corners[4].y, -3.0) &&
           nearly_equal(corners[4].z, 4.0) && nearly_equal(corners[5].x, 2.0) &&
           nearly_equal(corners[5].y, -3.0) && nearly_equal(corners[5].z, 4.0) &&
           nearly_equal(corners[6].x, 2.0) && nearly_equal(corners[6].y, 3.0) &&
           nearly_equal(corners[6].z, 4.0) && nearly_equal(corners[7].x, -2.0) &&
           nearly_equal(corners[7].y, 3.0) && nearly_equal(corners[7].z, 4.0);
}

int main(void) {
    if (!test_prism_corner_order_matches_rectangular_box_edges()) {
        fprintf(stderr, "retained_runtime_scene_overlay_geom_contract_test: prism ordering failed\n");
        return 1;
    }
    if (!test_aabb_corner_order_matches_prism_edges()) {
        fprintf(stderr, "retained_runtime_scene_overlay_geom_contract_test: aabb ordering failed\n");
        return 1;
    }
    fprintf(stdout, "retained_runtime_scene_overlay_geom_contract_test: success\n");
    return 0;
}
