#ifndef FIELD_OVERLAY_H
#define FIELD_OVERLAY_H

#include <stdbool.h>
#include <stdint.h>

#include "app/scene_state.h"

typedef struct FieldOverlayConfig {
    bool draw_vorticity;
    bool draw_pressure;
} FieldOverlayConfig;

// Currently no heavy init, but kept for symmetry / future.
bool field_overlay_init(void);

// Frees any internal buffers used for overlay computation.
void field_overlay_shutdown(void);

// Applies vorticity / pressure overlays directly onto the fluid texture.
// Safe to call every frame; no-op if config disables both overlays.
void field_overlay_apply(const SceneState *scene,
                         uint8_t *pixels,
                         int pitch,
                         const FieldOverlayConfig *cfg);

#endif // FIELD_OVERLAY_H
