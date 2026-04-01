#ifndef SPACE_MODE_ADAPTER_H
#define SPACE_MODE_ADAPTER_H

#include <stdbool.h>

#include "app/sim_mode.h"

typedef struct SpaceModeViewContext {
    SpaceMode requested_mode;
    SpaceMode projection_mode;
    int canvas_x;
    int canvas_y;
    int canvas_w;
    int canvas_h;
} SpaceModeViewContext;

SpaceMode space_mode_adapter_resolve(SpaceMode mode);
SpaceModeViewContext space_mode_adapter_build_canvas_view_context(SpaceMode mode,
                                                                  int canvas_x,
                                                                  int canvas_y,
                                                                  int canvas_w,
                                                                  int canvas_h);
SpaceModeViewContext space_mode_adapter_build_canvas_view_context_ex(SpaceMode requested_mode,
                                                                     SpaceMode projection_mode,
                                                                     int canvas_x,
                                                                     int canvas_y,
                                                                     int canvas_w,
                                                                     int canvas_h);
SpaceModeViewContext space_mode_adapter_build_canvas_view_context_for_route(const SimModeRoute *route,
                                                                            int canvas_x,
                                                                            int canvas_y,
                                                                            int canvas_w,
                                                                            int canvas_h);
bool space_mode_adapter_is_3d_requested(const SpaceModeViewContext *ctx);
void space_mode_adapter_world_to_screen(const SpaceModeViewContext *ctx,
                                        float world_x,
                                        float world_y,
                                        int *out_x,
                                        int *out_y);
void space_mode_adapter_screen_to_world_clamped(const SpaceModeViewContext *ctx,
                                                int screen_x,
                                                int screen_y,
                                                float *out_x,
                                                float *out_y);
void space_mode_adapter_screen_to_import_world_clamped(const SpaceModeViewContext *ctx,
                                                       int screen_x,
                                                       int screen_y,
                                                       float *out_x,
                                                       float *out_y);

#endif // SPACE_MODE_ADAPTER_H
