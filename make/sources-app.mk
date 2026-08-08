# =========================
#  App and renderer source discovery
# =========================
SRCS := $(shell find $(SRC_DIR) -name '*.c' \
	! -path '$(SRC_DIR)/tools/cli/*' \
	! -path '$(SRC_DIR)/render/renderer_sdl_headless_stub.c' \
	! -path '$(SRC_DIR)/render/kit_render_backend_vk_headless_stub.c' \
	! -path '$(SRC_DIR)/render/retained_runtime_scene_overlay_headless_stub.c' \
	! -path '$(SRC_DIR)/render/TimerHUD/*' \
	! -path '$(SRC_DIR)/render/TimerHUD_legacy_backup/*')
VK_RENDERER_SRCS := $(shell find $(VK_RENDERER_DIR)/src -name '*.c')
VK_RUNTIME_SRCS := $(shell find $(VK_RUNTIME_DIR)/src -name '*.c')
VK_RENDERER_SDL_COMPAT_SRCS := \
	$(SRC_DIR)/app/editor/editor_scroll.c \
	$(SRC_DIR)/app/editor/scene_editor_canvas.c \
	$(SRC_DIR)/app/editor/scene_editor_canvas_retained.c \
	$(SRC_DIR)/app/editor/scene_editor_panel.c \
	$(SRC_DIR)/app/editor/scene_editor_panel_inspector.c \
	$(SRC_DIR)/app/editor/scene_editor_panel_lists.c \
	$(SRC_DIR)/app/editor/scene_editor_panel_summary.c \
	$(SRC_DIR)/app/editor/scene_editor_precision.c \
	$(SRC_DIR)/app/editor/scene_editor_precision_helpers.c \
	$(SRC_DIR)/app/editor/scene_editor_widgets.c \
	$(SRC_DIR)/app/menu/menu_render.c \
	$(SRC_DIR)/app/menu/menu_settings_render.c \
	$(SRC_DIR)/app/menu/workspace_authoring/physics_sim_workspace_authoring_overlay.c \
	$(SRC_DIR)/app/scene_menu.c \
	$(SRC_DIR)/app/structural/structural_controller_render.c \
	$(SRC_DIR)/app/structural/structural_editor.c \
	$(SRC_DIR)/app/structural/structural_preset_editor_render_helpers.c \
	$(SRC_DIR)/app/structural/structural_render.c \
	$(SRC_DIR)/render/debug_draw_objects.c \
	$(SRC_DIR)/render/hud_overlay.c \
	$(SRC_DIR)/render/particle_overlay.c \
	$(SRC_DIR)/render/retained_runtime_scene_overlay.c \
	$(SRC_DIR)/render/retained_runtime_scene_overlay_readout.c \
	$(SRC_DIR)/render/timer_hud_adapter.c \
	$(SRC_DIR)/render/velocity_overlay.c \
	$(SRC_DIR)/ui/scrollbar.c
KIT_WORKSPACE_AUTHORING_SRCS := \
	$(KIT_WORKSPACE_AUTHORING_DIR)/src/kit_workspace_authoring.c \
	$(KIT_WORKSPACE_AUTHORING_DIR)/src/ui/kit_workspace_authoring_ui_overlay.c \
	$(KIT_WORKSPACE_AUTHORING_DIR)/src/ui/kit_workspace_authoring_ui_font_theme.c
TIMER_HUD_SRCS := $(shell find $(TIMER_HUD_DIR)/src -name '*.c')
TIMER_HUD_EXTERNAL_SRCS := $(TIMER_HUD_DIR)/external/cJSON.c
