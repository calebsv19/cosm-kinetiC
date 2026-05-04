#ifndef PHYSICS_SIM_WORKSPACE_AUTHORING_HOST_H
#define PHYSICS_SIM_WORKSPACE_AUTHORING_HOST_H

#include <SDL2/SDL.h>
#include <stdint.h>

#include "core_base.h"
#include "kit_workspace_authoring.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum PhysicsSimWorkspaceAuthoringOverlayMode {
    PHYSICS_SIM_WORKSPACE_AUTHORING_OVERLAY_PANE = 0,
    PHYSICS_SIM_WORKSPACE_AUTHORING_OVERLAY_FONT_THEME = 1
} PhysicsSimWorkspaceAuthoringOverlayMode;

typedef struct PhysicsSimWorkspaceAuthoringHostState {
    uint8_t active;
    uint8_t key_c_down;
    uint8_t key_v_down;
    uint8_t entry_chord_armed_key;
    PhysicsSimWorkspaceAuthoringOverlayMode overlay_mode;
    uint32_t enter_count;
    uint32_t apply_count;
    uint32_t cancel_count;
    uint32_t overlay_cycle_count;
    uint32_t consumed_event_count;
    uint32_t last_event_consumed;
    uint32_t last_event_entered;
    uint32_t last_event_exited;
    uint32_t captured_runtime_event_count;
    uint32_t viewport_width;
    uint32_t viewport_height;
    uint32_t last_pointer_x;
    uint32_t last_pointer_y;
    uint32_t last_pointer_ready;
    uint32_t overlay_button_click_count;
    uint32_t add_stub_count;
    uint32_t last_overlay_button_id;
} PhysicsSimWorkspaceAuthoringHostState;

void physics_sim_workspace_authoring_host_reset(
    PhysicsSimWorkspaceAuthoringHostState *host);
void physics_sim_workspace_authoring_host_set_viewport(
    PhysicsSimWorkspaceAuthoringHostState *host,
    int width,
    int height);
int physics_sim_workspace_authoring_host_active(
    const PhysicsSimWorkspaceAuthoringHostState *host);
int physics_sim_workspace_authoring_host_pane_overlay_active(
    const PhysicsSimWorkspaceAuthoringHostState *host);
int physics_sim_workspace_authoring_host_font_theme_overlay_active(
    const PhysicsSimWorkspaceAuthoringHostState *host);
CoreResult physics_sim_workspace_authoring_host_enter(
    PhysicsSimWorkspaceAuthoringHostState *host);
CoreResult physics_sim_workspace_authoring_host_apply(
    PhysicsSimWorkspaceAuthoringHostState *host);
CoreResult physics_sim_workspace_authoring_host_cancel(
    PhysicsSimWorkspaceAuthoringHostState *host);
CoreResult physics_sim_workspace_authoring_host_cycle_overlay(
    PhysicsSimWorkspaceAuthoringHostState *host);
int physics_sim_workspace_authoring_host_handle_sdl_event(
    PhysicsSimWorkspaceAuthoringHostState *host,
    const SDL_Event *event,
    int text_entry_active);

#ifdef __cplusplus
}
#endif

#endif
