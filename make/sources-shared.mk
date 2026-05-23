# =========================
#  Shared source declarations
# =========================
CORE_BASE_SRCS := $(CORE_BASE_DIR)/src/core_base.c
CORE_IO_SRCS := $(CORE_IO_DIR)/src/core_io.c
CORE_DATA_SRCS := $(CORE_DATA_DIR)/src/core_data.c
CORE_PACK_SRCS := $(CORE_PACK_DIR)/src/core_pack.c $(CORE_PACK_DIR)/src/core_pack_vf2d.c $(CORE_PACK_DIR)/src/core_pack_vf3d.c
CORE_SCENE_SRCS := $(CORE_SCENE_DIR)/src/core_scene.c
CORE_SCENE_COMPILE_SRCS := $(CORE_SCENE_COMPILE_DIR)/src/core_scene_compile.c
CORE_OBJECT_SRCS := $(CORE_OBJECT_DIR)/src/core_object.c
CORE_UNITS_SRCS := $(CORE_UNITS_DIR)/src/core_units.c
CORE_VIEWPORT2D_SRCS := $(CORE_VIEWPORT2D_DIR)/src/core_viewport2d.c
CORE_PANE_SRCS := $(CORE_PANE_DIR)/src/core_pane.c
CORE_SIM_SRCS := $(CORE_SIM_DIR)/src/core_sim.c
CORE_TRACE_SRCS := $(CORE_TRACE_DIR)/src/core_trace.c
CORE_THEME_SRCS := $(CORE_THEME_DIR)/src/core_theme.c
CORE_FONT_SRCS := $(CORE_FONT_DIR)/src/core_font.c
CORE_HEADLESS_JOB_SRCS := $(CORE_HEADLESS_JOB_DIR)/src/core_headless_job.c
KIT_VIZ_SRCS := $(KIT_VIZ_DIR)/src/kit_viz.c
KIT_RENDER_SRCS := \
	$(KIT_RENDER_DIR)/src/kit_render.c \
	$(KIT_RENDER_DIR)/src/kit_render_external_text.c \
	$(KIT_RENDER_DIR)/src/kit_render_backend_null.c \
	$(KIT_RENDER_DIR)/src/kit_render_backend_vk.c
KIT_PANE_SRCS := $(KIT_PANE_DIR)/src/kit_pane.c
KIT_UI_SRCS := \
	$(KIT_UI_DIR)/src/kit_ui.c \
	$(KIT_UI_DIR)/src/kit_ui_button.c
