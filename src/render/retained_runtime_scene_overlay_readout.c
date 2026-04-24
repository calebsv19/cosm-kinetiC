#include "render/retained_runtime_scene_overlay_readout.h"

#include "app/scene_state.h"

static SDL_Color COLOR_READOUT_OBSTACLE = {138, 164, 184, 84};
static SDL_Color COLOR_READOUT_FLUID_LOW = {94, 198, 255, 92};
static SDL_Color COLOR_READOUT_FLUID_HIGH = {232, 246, 255, 196};

static float clampf_local(float value, float min_value, float max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static SDL_Color lerp_color_local(SDL_Color a, SDL_Color b, float t) {
    SDL_Color result;
    t = clampf_local(t, 0.0f, 1.0f);
    result.r = (Uint8)(a.r + (Uint8)((float)(b.r - a.r) * t));
    result.g = (Uint8)(a.g + (Uint8)((float)(b.g - a.g) * t));
    result.b = (Uint8)(a.b + (Uint8)((float)(b.b - a.b) * t));
    result.a = (Uint8)(a.a + (Uint8)((float)(b.a - a.a) * t));
    return result;
}

static void draw_point_square(SDL_Renderer *renderer, int x, int y, int radius, SDL_Color color) {
    SDL_Rect rect = {x - radius, y - radius, radius * 2 + 1, radius * 2 + 1};
    if (!renderer || radius < 0) return;
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_RenderFillRect(renderer, &rect);
}

static void draw_cross(SDL_Renderer *renderer, int x, int y, int radius, SDL_Color color) {
    if (!renderer || radius <= 0) return;
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_RenderDrawLine(renderer, x - radius, y, x + radius, y);
    SDL_RenderDrawLine(renderer, x, y - radius, x, y + radius);
}

static void project_point_local(const SceneEditorViewportState *viewport,
                                int window_w,
                                int window_h,
                                CoreObjectVec3 point,
                                int *out_x,
                                int *out_y) {
    scene_editor_viewport_project_point3(viewport,
                                         0,
                                         0,
                                         window_w,
                                         window_h,
                                         (float)point.x,
                                         (float)point.y,
                                         (float)point.z,
                                         out_x,
                                         out_y);
}

int retained_runtime_overlay_readout_stride_for_cell_count(size_t cell_count) {
    if (cell_count <= 4096u) return 1;
    if (cell_count <= 32768u) return 2;
    if (cell_count <= 65536u) return 3;
    if (cell_count <= 196608u) return 4;
    return 6;
}

float retained_runtime_overlay_readout_density_threshold(float max_density) {
    if (max_density <= 0.0f) return 0.015f;
    {
        float threshold = max_density * 0.12f;
        if (threshold < 0.015f) threshold = 0.015f;
        return threshold;
    }
}

CoreObjectVec3 retained_runtime_overlay_readout_voxel_center(const SceneDebugVolumeView3D *view,
                                                             int x,
                                                             int y,
                                                             int z) {
    CoreObjectVec3 point = {0};
    if (!view) return point;
    point.x = (double)view->world_min_x + ((double)x + 0.5) * (double)view->voxel_size;
    point.y = (double)view->world_min_y + ((double)y + 0.5) * (double)view->voxel_size;
    point.z = (double)view->world_min_z + ((double)z + 0.5) * (double)view->voxel_size;
    return point;
}

static float max_density_for_view(const SceneDebugVolumeView3D *view) {
    float max_density = 0.0f;
    if (!view || !view->density) return 0.0f;
    for (size_t i = 0; i < view->cell_count; ++i) {
        if (view->density[i] > max_density) {
            max_density = view->density[i];
        }
    }
    return max_density;
}

void retained_runtime_overlay_draw_volume_readout(SDL_Renderer *renderer,
                                                  const SceneEditorViewportState *viewport,
                                                  int window_w,
                                                  int window_h,
                                                  const SceneState *scene) {
    SceneDebugVolumeView3D view = {0};
    int stride = 1;
    float max_density = 0.0f;
    float density_threshold = 0.0f;
    if (!renderer || !viewport || !scene) return;
    if (!scene_backend_debug_volume_view_3d(scene, &view)) return;
    if (view.width <= 0 || view.height <= 0 || view.depth <= 1 ||
        view.cell_count == 0 || view.voxel_size <= 0.0f ||
        !view.density || !view.solid_mask) {
        return;
    }

    stride = retained_runtime_overlay_readout_stride_for_cell_count(view.cell_count);
    max_density = max_density_for_view(&view);
    density_threshold = retained_runtime_overlay_readout_density_threshold(max_density);

    for (int z = 0; z < view.depth; z += stride) {
        for (int y = 0; y < view.height; y += stride) {
            for (int x = 0; x < view.width; x += stride) {
                size_t idx = ((size_t)z * (size_t)view.width * (size_t)view.height) +
                             ((size_t)y * (size_t)view.width) +
                             (size_t)x;
                float density = view.density[idx];
                bool solid = view.solid_mask[idx] != 0;
                CoreObjectVec3 point;
                int screen_x = 0;
                int screen_y = 0;
                if (!solid && density < density_threshold) continue;

                point = retained_runtime_overlay_readout_voxel_center(&view, x, y, z);
                project_point_local(viewport, window_w, window_h, point, &screen_x, &screen_y);

                if (solid) {
                    draw_cross(renderer, screen_x, screen_y, stride <= 2 ? 2 : 1, COLOR_READOUT_OBSTACLE);
                }
                if (density >= density_threshold) {
                    float normalized = (max_density > 0.0f) ? (density / max_density) : 0.0f;
                    SDL_Color color = lerp_color_local(COLOR_READOUT_FLUID_LOW,
                                                       COLOR_READOUT_FLUID_HIGH,
                                                       normalized);
                    int radius = normalized > 0.55f ? 2 : 1;
                    draw_point_square(renderer, screen_x, screen_y, radius, color);
                }
            }
        }
    }
}
