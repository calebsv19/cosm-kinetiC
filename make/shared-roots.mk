# =========================
#  Shared project structure
# =========================
SHARED_ROOT ?= third_party/codework_shared
VK_RENDERER_DIR := $(SHARED_ROOT)/vk_renderer
KIT_VIZ_DIR := $(SHARED_ROOT)/kit/kit_viz
KIT_RENDER_DIR := $(SHARED_ROOT)/kit/kit_render
KIT_PANE_DIR := $(SHARED_ROOT)/kit/kit_pane
KIT_WORKSPACE_AUTHORING_DIR := $(SHARED_ROOT)/kit/kit_workspace_authoring
SHARED_ASSETS_DIR := $(SHARED_ROOT)/assets
SHIM_MODE ?= off
SYS_SHIMS_DIR := $(SHARED_ROOT)/sys_shims
SYS_SHIMS_OVERLAY_DIR := $(SYS_SHIMS_DIR)/overlay/include
SYS_SHIMS_INCLUDE_DIR := $(SYS_SHIMS_DIR)/include

TIMER_HUD_DIR := $(SHARED_ROOT)/timer_hud

CORE_BASE_DIR := $(SHARED_ROOT)/core/core_base
CORE_IO_DIR := $(SHARED_ROOT)/core/core_io
CORE_DATA_DIR := $(SHARED_ROOT)/core/core_data
CORE_PACK_DIR := $(SHARED_ROOT)/core/core_pack
CORE_SCENE_DIR := $(SHARED_ROOT)/core/core_scene
CORE_SCENE_COMPILE_DIR := $(SHARED_ROOT)/core/core_scene_compile
CORE_OBJECT_DIR := $(SHARED_ROOT)/core/core_object
CORE_UNITS_DIR := $(SHARED_ROOT)/core/core_units
CORE_VIEWPORT2D_DIR := $(SHARED_ROOT)/core/core_viewport2d
CORE_PANE_DIR := $(SHARED_ROOT)/core/core_pane
CORE_SIM_DIR := $(SHARED_ROOT)/core/core_sim
CORE_TRACE_DIR := $(SHARED_ROOT)/core/core_trace
CORE_THEME_DIR := $(SHARED_ROOT)/core/core_theme
CORE_FONT_DIR := $(SHARED_ROOT)/core/core_font
