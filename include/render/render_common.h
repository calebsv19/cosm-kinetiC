#ifndef RENDER_COMMON_H
#define RENDER_COMMON_H

// Toggle smoothing of the fluid texture before it is upscaled to the window.
// Set to 0 to disable the blur pass when visualizing the grid.
#define RENDERER_ENABLE_SMOOTHING 1

// Toggle bilinear filtering when SDL scales the fluid texture to the window.
// Disabling this reverts to nearest-neighbor pixel doubling.
#define RENDERER_ENABLE_LINEAR_FILTER 1

#if RENDERER_ENABLE_SMOOTHING
static const float RENDERER_SMOOTH_KERNEL_1D[3] = {1.0f, 2.0f, 1.0f};
static const float RENDERER_SMOOTH_KERNEL_1D_SUM = 4.0f;
#endif

#endif // RENDER_COMMON_H
