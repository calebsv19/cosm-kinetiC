#include "render/text_upload_policy.h"

#include <math.h>

static const float k_physics_sim_max_text_raster_scale = 2.5f;

float physics_sim_text_raster_scale(SDL_Renderer *renderer) {
    const VkRenderer *vk = NULL;
    float logical_w = 0.0f;
    float logical_h = 0.0f;
    float scale_x = 1.0f;
    float scale_y = 1.0f;
    float raster_scale = 1.0f;

    if (!renderer) {
        return 1.0f;
    }

    vk = (const VkRenderer *)renderer;
    logical_w = vk->draw_state.logical_size[0];
    logical_h = vk->draw_state.logical_size[1];
    if (logical_w > 0.0f) {
        scale_x = (float)vk->context.swapchain.extent.width / logical_w;
    }
    if (logical_h > 0.0f) {
        scale_y = (float)vk->context.swapchain.extent.height / logical_h;
    }

    raster_scale = (scale_x < scale_y) ? scale_x : scale_y;
    if (!isfinite(raster_scale) || raster_scale < 1.0f) {
        raster_scale = 1.0f;
    }
    if (raster_scale > k_physics_sim_max_text_raster_scale) {
        raster_scale = k_physics_sim_max_text_raster_scale;
    }
    return raster_scale;
}

VkFilter physics_sim_text_upload_filter(SDL_Renderer *renderer) {
    if (physics_sim_text_raster_scale(renderer) > 1.0f) {
        return VK_FILTER_NEAREST;
    }
    return VK_FILTER_LINEAR;
}

int physics_sim_text_raster_point_size(SDL_Renderer *renderer,
                                       int base_point_size,
                                       int min_point_size) {
    int raster_size = 0;
    int min_size = 0;
    float raster_scale = physics_sim_text_raster_scale(renderer);

    if (min_point_size < 1) {
        min_point_size = 1;
    }
    if (base_point_size < min_point_size) {
        base_point_size = min_point_size;
    }

    raster_size = (int)lroundf((float)base_point_size * raster_scale);
    min_size = (int)lroundf((float)min_point_size * raster_scale);
    if (min_size < min_point_size) {
        min_size = min_point_size;
    }
    if (raster_size < min_size) {
        raster_size = min_size;
    }
    if (raster_size < 1) {
        raster_size = 1;
    }
    return raster_size;
}

int physics_sim_text_logical_pixels(SDL_Renderer *renderer, int raster_pixels) {
    int logical_pixels = 0;
    float raster_scale = 0.0f;

    if (raster_pixels <= 0) {
        return 0;
    }
    raster_scale = physics_sim_text_raster_scale(renderer);
    logical_pixels = (int)lroundf((float)raster_pixels / raster_scale);
    if (logical_pixels < 1) {
        logical_pixels = 1;
    }
    return logical_pixels;
}
