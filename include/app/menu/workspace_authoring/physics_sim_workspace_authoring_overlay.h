#ifndef PHYSICS_SIM_WORKSPACE_AUTHORING_OVERLAY_H
#define PHYSICS_SIM_WORKSPACE_AUTHORING_OVERLAY_H

#include "app/menu/menu_types.h"

#ifdef __cplusplus
extern "C" {
#endif

void physics_sim_workspace_authoring_overlay_draw(SceneMenuInteraction *ctx,
                                                  int width,
                                                  int height);

#ifdef __cplusplus
}
#endif

#endif
