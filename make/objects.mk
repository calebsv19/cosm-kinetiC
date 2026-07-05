# =========================
#  Object mapping
# =========================
OBJS := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRCS))
VK_RENDERER_SDL_COMPAT_OBJS := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(VK_RENDERER_SDL_COMPAT_SRCS))
VK_RENDERER_OBJS := $(patsubst $(VK_RENDERER_DIR)/src/%.c,$(BUILD_DIR)/vk_renderer/%.o,$(VK_RENDERER_SRCS))
OBJS += $(VK_RENDERER_OBJS)
OBJS += $(patsubst $(KIT_WORKSPACE_AUTHORING_DIR)/src/%.c,$(BUILD_DIR)/kit_workspace_authoring/%.o,$(KIT_WORKSPACE_AUTHORING_SRCS))
TIMER_HUD_OBJS := $(patsubst $(TIMER_HUD_DIR)/src/%.c,$(BUILD_DIR)/timer_hud/%.o,$(TIMER_HUD_SRCS))
TIMER_HUD_EXTERNAL_OBJS := $(patsubst $(TIMER_HUD_DIR)/external/%.c,$(BUILD_DIR)/timer_hud_external/%.o,$(TIMER_HUD_EXTERNAL_SRCS))

CORE_BASE_OBJS := $(patsubst $(CORE_BASE_DIR)/src/%.c,$(BUILD_DIR)/core_base/%.o,$(CORE_BASE_SRCS))
CORE_IO_OBJS := $(patsubst $(CORE_IO_DIR)/src/%.c,$(BUILD_DIR)/core_io/%.o,$(CORE_IO_SRCS))
CORE_DATA_OBJS := $(patsubst $(CORE_DATA_DIR)/src/%.c,$(BUILD_DIR)/core_data/%.o,$(CORE_DATA_SRCS))
CORE_PACK_OBJS := $(patsubst $(CORE_PACK_DIR)/src/%.c,$(BUILD_DIR)/core_pack/%.o,$(CORE_PACK_SRCS))
CORE_SCENE_OBJS := $(patsubst $(CORE_SCENE_DIR)/src/%.c,$(BUILD_DIR)/core_scene/%.o,$(CORE_SCENE_SRCS))
CORE_SCENE_COMPILE_OBJS := $(patsubst $(CORE_SCENE_COMPILE_DIR)/src/%.c,$(BUILD_DIR)/core_scene_compile/%.o,$(CORE_SCENE_COMPILE_SRCS))
CORE_MESH_ASSET_OBJS := $(patsubst $(CORE_MESH_ASSET_DIR)/src/%.c,$(BUILD_DIR)/core_mesh_asset/%.o,$(CORE_MESH_ASSET_SRCS))
CORE_MESH_PREVIEW_OBJS := $(patsubst $(CORE_MESH_PREVIEW_DIR)/src/%.c,$(BUILD_DIR)/core_mesh_preview/%.o,$(CORE_MESH_PREVIEW_SRCS))
CORE_OBJECT_OBJS := $(patsubst $(CORE_OBJECT_DIR)/src/%.c,$(BUILD_DIR)/core_object/%.o,$(CORE_OBJECT_SRCS))
CORE_UNITS_OBJS := $(patsubst $(CORE_UNITS_DIR)/src/%.c,$(BUILD_DIR)/core_units/%.o,$(CORE_UNITS_SRCS))
CORE_VIEWPORT2D_OBJS := $(patsubst $(CORE_VIEWPORT2D_DIR)/src/%.c,$(BUILD_DIR)/core_viewport2d/%.o,$(CORE_VIEWPORT2D_SRCS))
CORE_PANE_OBJS := $(patsubst $(CORE_PANE_DIR)/src/%.c,$(BUILD_DIR)/core_pane/%.o,$(CORE_PANE_SRCS))
CORE_SIM_OBJS := $(patsubst $(CORE_SIM_DIR)/src/%.c,$(BUILD_DIR)/core_sim/%.o,$(CORE_SIM_SRCS))
CORE_TRACE_OBJS := $(patsubst $(CORE_TRACE_DIR)/src/%.c,$(BUILD_DIR)/core_trace/%.o,$(CORE_TRACE_SRCS))
CORE_THEME_OBJS := $(patsubst $(CORE_THEME_DIR)/src/%.c,$(BUILD_DIR)/core_theme/%.o,$(CORE_THEME_SRCS))
CORE_FONT_OBJS := $(patsubst $(CORE_FONT_DIR)/src/%.c,$(BUILD_DIR)/core_font/%.o,$(CORE_FONT_SRCS))
CORE_HEADLESS_JOB_OBJS := $(patsubst $(CORE_HEADLESS_JOB_DIR)/src/%.c,$(BUILD_DIR)/core_headless_job/%.o,$(CORE_HEADLESS_JOB_SRCS))
KIT_VIZ_OBJS := $(patsubst $(KIT_VIZ_DIR)/src/%.c,$(BUILD_DIR)/kit_viz/%.o,$(KIT_VIZ_SRCS))
KIT_RENDER_OBJS := $(patsubst $(KIT_RENDER_DIR)/src/%.c,$(BUILD_DIR)/kit_render/%.o,$(KIT_RENDER_SRCS))
KIT_PANE_OBJS := $(patsubst $(KIT_PANE_DIR)/src/%.c,$(BUILD_DIR)/kit_pane/%.o,$(KIT_PANE_SRCS))
KIT_UI_OBJS := $(patsubst $(KIT_UI_DIR)/src/%.c,$(BUILD_DIR)/kit_ui/%.o,$(KIT_UI_SRCS))

OBJS := $(OBJS) $(TIMER_HUD_OBJS) $(TIMER_HUD_EXTERNAL_OBJS)
OBJS += $(CORE_BASE_OBJS) $(CORE_IO_OBJS) $(CORE_DATA_OBJS) $(CORE_PACK_OBJS) $(CORE_SCENE_OBJS) $(CORE_SCENE_COMPILE_OBJS) $(CORE_MESH_ASSET_OBJS) $(CORE_MESH_PREVIEW_OBJS) $(CORE_OBJECT_OBJS) $(CORE_UNITS_OBJS) $(CORE_VIEWPORT2D_OBJS) $(CORE_PANE_OBJS) $(CORE_SIM_OBJS) $(CORE_THEME_OBJS) $(CORE_FONT_OBJS) $(CORE_HEADLESS_JOB_OBJS) $(KIT_VIZ_OBJS) $(KIT_RENDER_OBJS) $(KIT_PANE_OBJS) $(KIT_UI_OBJS)
DEPS := $(OBJS:.o=.d)
PHYSICS_SIM_APP_OBJS_NO_MAIN := $(filter-out $(BUILD_DIR)/main.o,$(OBJS))
PHYSICS_SIM_HEADLESS_WORKER_EXCLUDED_SRCS := \
	$(SRC_DIR)/main.c \
	$(SRC_DIR)/app/scene_menu.c \
	$(SRC_DIR)/app/scene_menu_layout_helpers.c \
	$(SRC_DIR)/app/menu/menu_input.c \
	$(SRC_DIR)/app/menu/menu_render.c \
	$(SRC_DIR)/app/menu/menu_settings_render.c \
	$(SRC_DIR)/app/menu/menu_settings_draft.c \
	$(SRC_DIR)/app/menu/menu_settings_input.c \
	$(SRC_DIR)/app/menu/menu_settings_layout.c \
	$(SRC_DIR)/app/menu/menu_settings_provider_2d.c \
	$(SRC_DIR)/app/menu/menu_settings_provider_3d.c \
	$(SRC_DIR)/app/menu/menu_settings_provider_atmospheric.c \
	$(SRC_DIR)/app/menu/menu_settings_provider_common.c \
	$(SRC_DIR)/app/menu/menu_settings_provider_structural.c \
	$(SRC_DIR)/app/menu/menu_settings_provider_wind.c \
	$(SRC_DIR)/app/menu/menu_settings_schema.c \
	$(SRC_DIR)/app/menu/menu_state.c \
	$(SRC_DIR)/app/menu/menu_state_presets.c \
	$(SRC_DIR)/app/menu/menu_state_text_edits.c \
	$(SRC_DIR)/app/menu/shared_theme_font_adapter.c \
	$(SRC_DIR)/app/menu/menu_window.c \
	$(SRC_DIR)/app/menu/workspace_authoring/physics_sim_workspace_authoring_host.c \
	$(SRC_DIR)/app/menu/workspace_authoring/physics_sim_workspace_authoring_overlay.c \
	$(SRC_DIR)/app/menu/workspace_authoring/physics_sim_workspace_authoring_overlay_model.c \
	$(SRC_DIR)/app/structural/structural_controller.c \
	$(SRC_DIR)/app/structural/structural_controller_render.c \
	$(SRC_DIR)/app/structural/structural_editor.c \
	$(SRC_DIR)/app/structural/structural_controller_runtime.c \
	$(SRC_DIR)/app/structural/structural_preset_editor.c \
	$(SRC_DIR)/app/structural/structural_preset_editor_render_helpers.c \
	$(SRC_DIR)/app/structural/structural_render.c \
	$(SRC_DIR)/app/ui/physics_sim_ui_button.c \
	$(SRC_DIR)/app/editor/editor_scroll.c \
	$(SRC_DIR)/app/editor/scene_editor.c \
	$(SRC_DIR)/app/editor/scene_editor_canvas.c \
	$(SRC_DIR)/app/editor/scene_editor_canvas_geom.c \
	$(SRC_DIR)/app/editor/scene_editor_canvas_hit.c \
	$(SRC_DIR)/app/editor/scene_editor_canvas_retained.c \
	$(SRC_DIR)/app/editor/scene_editor_import.c \
	$(SRC_DIR)/app/editor/scene_editor_input.c \
	$(SRC_DIR)/app/editor/scene_editor_input_text_keys.c \
	$(SRC_DIR)/app/editor/scene_editor_input_button_actions.c \
	$(SRC_DIR)/app/editor/scene_editor_input_common.c \
	$(SRC_DIR)/app/editor/scene_editor_input_hit_helpers.c \
	$(SRC_DIR)/app/editor/scene_editor_input_import_helpers.c \
	$(SRC_DIR)/app/editor/scene_editor_model.c \
	$(SRC_DIR)/app/editor/scene_editor_pane_host.c \
	$(SRC_DIR)/app/editor/scene_editor_panel.c \
	$(SRC_DIR)/app/editor/scene_editor_panel_inspector.c \
	$(SRC_DIR)/app/editor/scene_editor_panel_lists.c \
	$(SRC_DIR)/app/editor/scene_editor_panel_summary.c \
	$(SRC_DIR)/app/editor/scene_editor_precision.c \
	$(SRC_DIR)/app/editor/scene_editor_precision_helpers.c \
	$(SRC_DIR)/app/editor/scene_editor_retained_document.c \
	$(SRC_DIR)/app/editor/scene_editor_scene_library.c \
	$(SRC_DIR)/app/editor/scene_editor_scroll.c \
	$(SRC_DIR)/app/editor/scene_editor_session.c \
	$(SRC_DIR)/app/editor/scene_editor_session_overlay_edit.c \
	$(SRC_DIR)/app/editor/scene_editor_session_labels.c \
	$(SRC_DIR)/app/editor/scene_editor_state.c \
	$(SRC_DIR)/app/editor/scene_editor_widgets.c \
	$(SRC_DIR)/render/debug_draw_objects.c \
	$(SRC_DIR)/render/font_bridge.c \
	$(SRC_DIR)/render/hud_overlay.c \
	$(SRC_DIR)/render/retained_runtime_scene_overlay.c \
	$(SRC_DIR)/render/retained_runtime_scene_overlay_geom.c \
	$(SRC_DIR)/render/retained_runtime_scene_overlay_readout.c \
	$(SRC_DIR)/render/retained_runtime_scene_overlay_space.c \
	$(SRC_DIR)/render/text_draw.c \
	$(SRC_DIR)/render/text_upload_policy.c \
	$(SRC_DIR)/render/particle_overlay.c \
	$(SRC_DIR)/render/renderer_sdl.c \
	$(SRC_DIR)/render/timer_hud_adapter.c \
	$(SRC_DIR)/render/velocity_overlay.c \
	$(SRC_DIR)/render/vk_shared_device.c \
	$(SRC_DIR)/ui/scrollbar.c
PHYSICS_SIM_HEADLESS_WORKER_EXCLUDED_OBJS := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(PHYSICS_SIM_HEADLESS_WORKER_EXCLUDED_SRCS))
PHYSICS_SIM_HEADLESS_RENDERER_STUB_SRC := $(SRC_DIR)/render/renderer_sdl_headless_stub.c
PHYSICS_SIM_HEADLESS_RENDERER_STUB_OBJ := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(PHYSICS_SIM_HEADLESS_RENDERER_STUB_SRC))
PHYSICS_SIM_HEADLESS_OVERLAY_STUB_SRC := $(SRC_DIR)/render/retained_runtime_scene_overlay_headless_stub.c
PHYSICS_SIM_HEADLESS_OVERLAY_STUB_OBJ := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(PHYSICS_SIM_HEADLESS_OVERLAY_STUB_SRC))
PHYSICS_SIM_HEADLESS_KIT_RENDER_STUB_SRC := $(SRC_DIR)/render/kit_render_backend_vk_headless_stub.c
PHYSICS_SIM_HEADLESS_KIT_RENDER_STUB_OBJ := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(PHYSICS_SIM_HEADLESS_KIT_RENDER_STUB_SRC))
PHYSICS_SIM_HEADLESS_KIT_RENDER_EXCLUDED_OBJS := \
	$(BUILD_DIR)/kit_render/kit_render_backend_vk.o \
	$(BUILD_DIR)/kit_render/kit_render_external_text.o
PHYSICS_SIM_HEADLESS_WORKER_OBJS := \
	$(filter-out $(PHYSICS_SIM_HEADLESS_WORKER_EXCLUDED_OBJS) $(VK_RENDERER_OBJS) $(PHYSICS_SIM_HEADLESS_KIT_RENDER_EXCLUDED_OBJS),$(PHYSICS_SIM_APP_OBJS_NO_MAIN)) \
	$(PHYSICS_SIM_HEADLESS_RENDERER_STUB_OBJ) \
	$(PHYSICS_SIM_HEADLESS_OVERLAY_STUB_OBJ) \
	$(PHYSICS_SIM_HEADLESS_KIT_RENDER_STUB_OBJ)

SHAPE_MASK_TOOL_OBJ   := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SHAPE_MASK_TOOL_SRC))
SHAPE_ASSET_TOOL_OBJ  := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SHAPE_ASSET_TOOL_SRC))
SHAPE_SANITY_TOOL_OBJ := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SHAPE_SANITY_TOOL_SRC))
PHYSICS_SIM_HEADLESS_TOOL_OBJ := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(PHYSICS_SIM_HEADLESS_TOOL_SRC))
PHYSICS_SIM_JOB_RUNNER_TOOL_OBJ := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(PHYSICS_SIM_JOB_RUNNER_TOOL_SRC))
SHAPE_SHARED_OBJS := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SHAPE_SHARED_SRCS))

$(VK_RENDERER_SDL_COMPAT_OBJS): CFLAGS += $(VK_RENDERER_SDL_COMPAT_CFLAGS)

FISICS_OBJS := $(patsubst $(BUILD_DIR)/%,$(FISICS_BUILD_DIR)/%,$(OBJS))
FISICS_VK_RENDERER_SDL_COMPAT_OBJS := $(patsubst $(BUILD_DIR)/%,$(FISICS_BUILD_DIR)/%,$(VK_RENDERER_SDL_COMPAT_OBJS))
$(FISICS_VK_RENDERER_SDL_COMPAT_OBJS): FISICS_COMPILE_FLAGS += $(VK_RENDERER_SDL_COMPAT_CFLAGS)
