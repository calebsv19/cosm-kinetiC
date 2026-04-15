#ifndef PHYSICS_SIM_RENDER_TEXT_UPLOAD_POLICY_H
#define PHYSICS_SIM_RENDER_TEXT_UPLOAD_POLICY_H

#include <SDL2/SDL.h>

#include "vk_renderer.h"

float physics_sim_text_raster_scale(SDL_Renderer *renderer);
VkFilter physics_sim_text_upload_filter(SDL_Renderer *renderer);
int physics_sim_text_raster_point_size(SDL_Renderer *renderer,
                                       int base_point_size,
                                       int min_point_size);
int physics_sim_text_logical_pixels(SDL_Renderer *renderer, int raster_pixels);

#endif
