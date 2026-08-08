# =========================
#  Shared project structure
# =========================
SHARED_ROOT ?= third_party/codework_shared
SHARED_WORKSPACE_DIR ?= ../shared
VK_RENDERER_DIR := $(SHARED_ROOT)/vk_renderer
VK_RUNTIME_DIR := $(SHARED_ROOT)/vk_runtime
CANONICAL_SHARED_ROOT ?= ../shared
KIT_VIZ_DIR := $(SHARED_ROOT)/kit/kit_viz
KIT_RENDER_DIR := $(SHARED_ROOT)/kit/kit_render
KIT_PANE_DIR := $(SHARED_ROOT)/kit/kit_pane
KIT_UI_DIR := $(SHARED_ROOT)/kit/kit_ui
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
CORE_SCENE_VIEW_DIR := $(SHARED_ROOT)/core/core_scene_view
CORE_MESH_ASSET_DIR := $(SHARED_ROOT)/core/core_mesh_asset
CORE_MESH_PREVIEW_DIR := $(SHARED_ROOT)/core/core_mesh_preview
CORE_OBJECT_DIR := $(SHARED_ROOT)/core/core_object
CORE_UNITS_DIR := $(SHARED_ROOT)/core/core_units
CORE_VIEWPORT2D_DIR := $(SHARED_ROOT)/core/core_viewport2d
CORE_SCREEN_PICK_DIR := $(SHARED_ROOT)/core/core_screen_pick
CORE_PANE_DIR := $(SHARED_ROOT)/core/core_pane
CORE_SIM_DIR := $(SHARED_ROOT)/core/core_sim
CORE_TRACE_DIR := $(SHARED_ROOT)/core/core_trace
CORE_THEME_DIR := $(SHARED_ROOT)/core/core_theme
CORE_FONT_DIR := $(SHARED_ROOT)/core/core_font
CORE_HEADLESS_JOB_DIR := $(SHARED_ROOT)/core/core_headless_job

ifeq ($(wildcard $(CORE_HEADLESS_JOB_DIR)/include/core_headless_job.h),)
CORE_HEADLESS_JOB_DIR := $(SHARED_WORKSPACE_DIR)/core/core_headless_job
endif

ifeq ($(wildcard $(CORE_SCENE_VIEW_DIR)/include/core_scene_view.h),)
CORE_SCENE_VIEW_DIR := $(SHARED_WORKSPACE_DIR)/core/core_scene_view
endif

ifeq ($(wildcard $(CORE_SCREEN_PICK_DIR)/include/core_screen_pick.h),)
CORE_SCREEN_PICK_DIR := $(SHARED_WORKSPACE_DIR)/core/core_screen_pick
endif

ifeq ($(wildcard $(CORE_MESH_ASSET_DIR)/include/core_mesh_asset.h),)
CORE_MESH_ASSET_DIR := $(SHARED_WORKSPACE_DIR)/core/core_mesh_asset
endif

ifeq ($(wildcard $(CORE_MESH_PREVIEW_DIR)/include/core_mesh_preview.h),)
CORE_MESH_PREVIEW_DIR := $(SHARED_WORKSPACE_DIR)/core/core_mesh_preview
endif
