#include <stdio.h>
#include <string.h>
#include <math.h>

#include "app/app_config.h"
#include "app/shape_lookup.h"
#include "app/scene_presets.h"
#include "geo/shape_library.h"
#include "physics/rigid/collider_builder.h"
#include "physics/rigid/collider_geom.h"

typedef struct {
    const char *name;
    const char *path;
} ShapeCase;

static float part_area(const ImportedShape *imp, int part_index) {
    if (!imp || part_index < 0 || part_index >= imp->collider_part_count) return 0.0f;
    int off = imp->collider_part_offsets[part_index];
    int count = imp->collider_part_counts[part_index];
    if (count < 3) return 0.0f;
    float area = 0.0f;
    for (int i = 0; i < count; ++i) {
        Vec2 a = imp->collider_parts_verts[off + i];
        Vec2 b = imp->collider_parts_verts[off + ((i + 1) % count)];
        area += (a.x * b.y - b.x * a.y);
    }
    return 0.5f * fabsf(area);
}

static void run_case(const ShapeCase *tc, const AppConfig *cfg, const ShapeAssetLibrary *lib) {
    ImportedShape imp = {0};
    strncpy(imp.path, tc->path, sizeof(imp.path) - 1);
    imp.position_x = 0.5f;
    imp.position_y = 0.5f;
    imp.rotation_deg = 0.0f;
    imp.scale = 1.0f;
    imp.enabled = true;
    imp.gravity_enabled = false;
    imp.is_static = true;

    bool ok = collider_build_import(cfg, lib, &imp);
    printf("\n=== %s (%s) ===\n", tc->name, tc->path);
    if (!ok) {
        printf("build: FAILED\n");
        return;
    }
    float total_area = 0.0f;
    for (int i = 0; i < imp.collider_part_count; ++i) {
        float a = part_area(&imp, i);
        printf(" part %d: verts=%d area=%.2f\n", i, imp.collider_part_counts[i], a);
        total_area += a;
    }
    printf(" parts=%d pooled_verts=%d total_area=%.2f\n",
           imp.collider_part_count, imp.collider_part_offsets[imp.collider_part_count - 1] +
           imp.collider_part_counts[imp.collider_part_count - 1],
           total_area);
}

int main(void) {
    AppConfig cfg = app_config_default();
    cfg.collider_debug_logs = true;
    cfg.window_w = 800;
    cfg.window_h = 800;
    cfg.grid_w = 128;
    cfg.grid_h = 128;

    ShapeAssetLibrary lib = {0};
    if (!shape_library_load_dir("config/objects", &lib)) {
        fprintf(stderr, "Failed to load shape library from config/objects\n");
        return 1;
    }

    const ShapeCase cases[] = {
        {"hexagon", "config/objects/Hexagon.asset.json"},
        {"u_shape", "config/objects/u_shape.asset.json"},
        {"small_waist", "config/objects/small_waist.asset.json"},
        {"gap", "config/objects/gap.asset.json"},
        {"bubble_m", "config/objects/bubble_m.asset.json"},
    };
    size_t case_count = sizeof(cases) / sizeof(cases[0]);
    for (size_t i = 0; i < case_count; ++i) {
        run_case(&cases[i], &cfg, &lib);
    }

    shape_library_free(&lib);
    return 0;
}
