# =========================
#  Object mapping
# =========================
OBJS := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRCS))
VK_RENDERER_SDL_COMPAT_OBJS := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(VK_RENDERER_SDL_COMPAT_SRCS))
OBJS += $(patsubst $(VK_RENDERER_DIR)/src/%.c,$(BUILD_DIR)/vk_renderer/%.o,$(VK_RENDERER_SRCS))
OBJS += $(patsubst $(KIT_WORKSPACE_AUTHORING_DIR)/src/%.c,$(BUILD_DIR)/kit_workspace_authoring/%.o,$(KIT_WORKSPACE_AUTHORING_SRCS))
TIMER_HUD_OBJS := $(patsubst $(TIMER_HUD_DIR)/src/%.c,$(BUILD_DIR)/timer_hud/%.o,$(TIMER_HUD_SRCS))
TIMER_HUD_EXTERNAL_OBJS := $(patsubst $(TIMER_HUD_DIR)/external/%.c,$(BUILD_DIR)/timer_hud_external/%.o,$(TIMER_HUD_EXTERNAL_SRCS))

CORE_BASE_OBJS := $(patsubst $(CORE_BASE_DIR)/src/%.c,$(BUILD_DIR)/core_base/%.o,$(CORE_BASE_SRCS))
CORE_IO_OBJS := $(patsubst $(CORE_IO_DIR)/src/%.c,$(BUILD_DIR)/core_io/%.o,$(CORE_IO_SRCS))
CORE_DATA_OBJS := $(patsubst $(CORE_DATA_DIR)/src/%.c,$(BUILD_DIR)/core_data/%.o,$(CORE_DATA_SRCS))
CORE_PACK_OBJS := $(patsubst $(CORE_PACK_DIR)/src/%.c,$(BUILD_DIR)/core_pack/%.o,$(CORE_PACK_SRCS))
CORE_SCENE_OBJS := $(patsubst $(CORE_SCENE_DIR)/src/%.c,$(BUILD_DIR)/core_scene/%.o,$(CORE_SCENE_SRCS))
CORE_SCENE_COMPILE_OBJS := $(patsubst $(CORE_SCENE_COMPILE_DIR)/src/%.c,$(BUILD_DIR)/core_scene_compile/%.o,$(CORE_SCENE_COMPILE_SRCS))
CORE_OBJECT_OBJS := $(patsubst $(CORE_OBJECT_DIR)/src/%.c,$(BUILD_DIR)/core_object/%.o,$(CORE_OBJECT_SRCS))
CORE_UNITS_OBJS := $(patsubst $(CORE_UNITS_DIR)/src/%.c,$(BUILD_DIR)/core_units/%.o,$(CORE_UNITS_SRCS))
CORE_VIEWPORT2D_OBJS := $(patsubst $(CORE_VIEWPORT2D_DIR)/src/%.c,$(BUILD_DIR)/core_viewport2d/%.o,$(CORE_VIEWPORT2D_SRCS))
CORE_PANE_OBJS := $(patsubst $(CORE_PANE_DIR)/src/%.c,$(BUILD_DIR)/core_pane/%.o,$(CORE_PANE_SRCS))
CORE_SIM_OBJS := $(patsubst $(CORE_SIM_DIR)/src/%.c,$(BUILD_DIR)/core_sim/%.o,$(CORE_SIM_SRCS))
CORE_TRACE_OBJS := $(patsubst $(CORE_TRACE_DIR)/src/%.c,$(BUILD_DIR)/core_trace/%.o,$(CORE_TRACE_SRCS))
CORE_THEME_OBJS := $(patsubst $(CORE_THEME_DIR)/src/%.c,$(BUILD_DIR)/core_theme/%.o,$(CORE_THEME_SRCS))
CORE_FONT_OBJS := $(patsubst $(CORE_FONT_DIR)/src/%.c,$(BUILD_DIR)/core_font/%.o,$(CORE_FONT_SRCS))
KIT_VIZ_OBJS := $(patsubst $(KIT_VIZ_DIR)/src/%.c,$(BUILD_DIR)/kit_viz/%.o,$(KIT_VIZ_SRCS))
KIT_RENDER_OBJS := $(patsubst $(KIT_RENDER_DIR)/src/%.c,$(BUILD_DIR)/kit_render/%.o,$(KIT_RENDER_SRCS))
KIT_PANE_OBJS := $(patsubst $(KIT_PANE_DIR)/src/%.c,$(BUILD_DIR)/kit_pane/%.o,$(KIT_PANE_SRCS))

OBJS := $(OBJS) $(TIMER_HUD_OBJS) $(TIMER_HUD_EXTERNAL_OBJS)
OBJS += $(CORE_BASE_OBJS) $(CORE_IO_OBJS) $(CORE_DATA_OBJS) $(CORE_PACK_OBJS) $(CORE_SCENE_OBJS) $(CORE_SCENE_COMPILE_OBJS) $(CORE_OBJECT_OBJS) $(CORE_UNITS_OBJS) $(CORE_VIEWPORT2D_OBJS) $(CORE_PANE_OBJS) $(CORE_SIM_OBJS) $(CORE_THEME_OBJS) $(CORE_FONT_OBJS) $(KIT_VIZ_OBJS) $(KIT_RENDER_OBJS) $(KIT_PANE_OBJS)
DEPS := $(OBJS:.o=.d)

SHAPE_MASK_TOOL_OBJ   := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SHAPE_MASK_TOOL_SRC))
SHAPE_ASSET_TOOL_OBJ  := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SHAPE_ASSET_TOOL_SRC))
SHAPE_SANITY_TOOL_OBJ := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SHAPE_SANITY_TOOL_SRC))
SHAPE_SHARED_OBJS := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SHAPE_SHARED_SRCS))

$(VK_RENDERER_SDL_COMPAT_OBJS): CFLAGS += $(VK_RENDERER_SDL_COMPAT_CFLAGS)
