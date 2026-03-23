# =========================
#  Compiler and standard
# =========================
CC        := cc
CSTD      := -std=c11

UNAME_S   := $(shell uname -s)

# =========================
#  Project structure
# =========================
SRC_DIR   := src
INC_DIR   := include
BUILD_DIR := build
TARGET    := physics_sim
VK_RENDERER_DIR := ../shared/vk_renderer
KIT_VIZ_DIR := ../shared/kit/kit_viz
SHIM_MODE ?= off
SYS_SHIMS_DIR := ../shared/sys_shims
SYS_SHIMS_OVERLAY_DIR := $(SYS_SHIMS_DIR)/overlay/include
SYS_SHIMS_INCLUDE_DIR := $(SYS_SHIMS_DIR)/include

# =========================
#  Diagnostics
# =========================
$(info USING MAKEFILE AT: $(abspath $(lastword $(MAKEFILE_LIST))))

# =========================
#  SDL2 via sdl2-config (if available)
# =========================
SDL_CFLAGS := $(shell sdl2-config --cflags 2>/dev/null)
SDL_LIBS   := $(shell sdl2-config --libs 2>/dev/null)
VULKAN_CFLAGS :=
VULKAN_LIBS :=

# =========================
#  Base flags
# =========================
WARN      := -Wall -Wextra -Wpedantic
DEBUG     := -g

CFLAGS    := $(CSTD) $(WARN) $(DEBUG) -I$(INC_DIR) -I$(SRC_DIR) -I$(SRC_DIR)/tools
LDFLAGS   :=
LIBS      :=

ifeq ($(SHIM_MODE),shadow)
	CFLAGS += -I$(SYS_SHIMS_OVERLAY_DIR) -I$(SYS_SHIMS_INCLUDE_DIR) -DSYS_SHIM_MODE_SHADOW=1
endif

# =========================
#  OS-specific flags
# =========================
ifeq ($(UNAME_S),Linux)
    # Needed for clock_gettime + friends
    CFLAGS += -D_POSIX_C_SOURCE=200809L -D_GNU_SOURCE

    ifneq ($(SDL_CFLAGS),)
        # Prefer sdl2-config if present
        CFLAGS += $(SDL_CFLAGS)
        LIBS   += $(SDL_LIBS)
    else
        # Fallback: system headers
        CFLAGS += -I/usr/include/SDL2
        LIBS   += -lSDL2 -lSDL2_ttf
    endif

    # Linux needs librt for clock_gettime
    LIBS += -lrt

    VULKAN_CFLAGS := $(shell pkg-config --cflags vulkan 2>/dev/null)
    VULKAN_LIBS := $(shell pkg-config --libs vulkan 2>/dev/null)
    ifeq ($(strip $(VULKAN_CFLAGS)$(VULKAN_LIBS)),)
        VULKAN_CFLAGS := -I/usr/include
        VULKAN_LIBS := -lvulkan
    endif
endif

ifeq ($(UNAME_S),Darwin)
    # macOS: clock_gettime is in libSystem, no -lrt needed
    # We *do not* use SDL_CFLAGS here, because it adds -I/opt/homebrew/include/SDL2,
    # which conflicts with our #include <SDL2/SDL.h> pattern.
    CFLAGS  += -D_POSIX_C_SOURCE=200809L -I/opt/homebrew/include -D_THREAD_SAFE
    LDFLAGS += -L/opt/homebrew/lib
    LIBS    += -lSDL2 -lSDL2_ttf

    VULKAN_CFLAGS := $(shell pkg-config --cflags vulkan 2>/dev/null)
    VULKAN_LIBS := $(shell pkg-config --libs vulkan 2>/dev/null)
    ifeq ($(strip $(VULKAN_CFLAGS)$(VULKAN_LIBS)),)
        VULKAN_CFLAGS := -I/opt/homebrew/include
        VULKAN_LIBS := -L/opt/homebrew/lib -lvulkan
    endif
    VULKAN_LIBS += -framework Metal -framework QuartzCore -framework Cocoa -framework IOKit -framework CoreVideo
    CFLAGS += -DVK_USE_PLATFORM_METAL_EXT
endif


# Always link libm
LIBS += -lm

# =========================
#  Source / object discovery
# =========================
# Find all .c files under src/ excluding CLI tools
TIMER_HUD_DIR := ../shared/timer_hud
TIMER_HUD_INCLUDE := -I$(TIMER_HUD_DIR)/include -I$(TIMER_HUD_DIR)/external

CFLAGS += $(TIMER_HUD_INCLUDE) -I$(VK_RENDERER_DIR)/include $(VULKAN_CFLAGS) \
	-DUSE_VULKAN=1 -DVK_RENDERER_SHADER_ROOT=\"$(abspath $(VK_RENDERER_DIR))\" \
	-include $(VK_RENDERER_DIR)/include/vk_renderer_sdl.h
LIBS += $(VULKAN_LIBS)
CFLAGS += -I$(KIT_VIZ_DIR)/include

SRCS := $(shell find $(SRC_DIR) -name '*.c' \
	! -path '$(SRC_DIR)/tools/cli/*' \
	! -path '$(SRC_DIR)/render/TimerHUD/*' \
	! -path '$(SRC_DIR)/render/TimerHUD_legacy_backup/*')
VK_RENDERER_SRCS := $(shell find $(VK_RENDERER_DIR)/src -name '*.c')
TIMER_HUD_SRCS := $(shell find $(TIMER_HUD_DIR)/src -name '*.c')
TIMER_HUD_EXTERNAL_SRCS := $(TIMER_HUD_DIR)/external/cJSON.c

# Map src/foo/bar.c -> build/foo/bar.o
OBJS := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRCS))
OBJS += $(patsubst $(VK_RENDERER_DIR)/src/%.c,$(BUILD_DIR)/vk_renderer/%.o,$(VK_RENDERER_SRCS))
TIMER_HUD_OBJS := $(patsubst $(TIMER_HUD_DIR)/src/%.c,$(BUILD_DIR)/timer_hud/%.o,$(TIMER_HUD_SRCS))
TIMER_HUD_EXTERNAL_OBJS := $(patsubst $(TIMER_HUD_DIR)/external/%.c,$(BUILD_DIR)/timer_hud_external/%.o,$(TIMER_HUD_EXTERNAL_SRCS))

OBJS := $(OBJS) $(TIMER_HUD_OBJS) $(TIMER_HUD_EXTERNAL_OBJS)
OBJS := $(OBJS) $(CORE_BASE_OBJS) $(CORE_IO_OBJS) $(CORE_PACK_OBJS)
DEPS := $(OBJS:.o=.d)

# CLI tool sources (explicit to avoid multiple mains)
SHAPE_MASK_TOOL_SRC   := $(SRC_DIR)/tools/cli/shape_import_tool.c
SHAPE_MASK_TOOL_OBJ   := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SHAPE_MASK_TOOL_SRC))
SHAPE_ASSET_TOOL_SRC  := $(SRC_DIR)/tools/cli/shape_asset_tool.c
SHAPE_ASSET_TOOL_OBJ  := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SHAPE_ASSET_TOOL_SRC))
SHAPE_SANITY_TOOL_SRC := $(SRC_DIR)/tools/cli/shape_sanity_tool.c
SHAPE_SANITY_TOOL_OBJ := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SHAPE_SANITY_TOOL_SRC))
VF2D_PACK_TOOL_SRC := $(SRC_DIR)/tools/cli/vf2d_pack_tool.c
VF2D_PACK_TOOL_BIN := vf2d_pack_tool
VF2D_DATASET_TOOL_SRC := $(SRC_DIR)/tools/cli/vf2d_dataset_tool.c
VF2D_DATASET_TOOL_BIN := vf2d_dataset_tool
PHYSICS_TRACE_TOOL_SRC := $(SRC_DIR)/tools/cli/physics_trace_tool.c
PHYSICS_TRACE_TOOL_BIN := physics_trace_tool

CORE_BASE_DIR := ../shared/core/core_base
CORE_IO_DIR := ../shared/core/core_io
CORE_DATA_DIR := ../shared/core/core_data
CORE_PACK_DIR := ../shared/core/core_pack
CORE_SCENE_DIR := ../shared/core/core_scene
CORE_TRACE_DIR := ../shared/core/core_trace
CORE_THEME_DIR := ../shared/core/core_theme
CORE_FONT_DIR := ../shared/core/core_font
CFLAGS += -I$(CORE_PACK_DIR)/include -I$(CORE_IO_DIR)/include -I$(CORE_BASE_DIR)/include -I$(CORE_SCENE_DIR)/include -I$(CORE_DATA_DIR)/include -I$(CORE_THEME_DIR)/include -I$(CORE_FONT_DIR)/include

CORE_BASE_SRCS := $(CORE_BASE_DIR)/src/core_base.c
CORE_IO_SRCS := $(CORE_IO_DIR)/src/core_io.c
CORE_DATA_SRCS := $(CORE_DATA_DIR)/src/core_data.c
CORE_PACK_SRCS := $(CORE_PACK_DIR)/src/core_pack.c $(CORE_PACK_DIR)/src/core_pack_vf2d.c
CORE_SCENE_SRCS := $(CORE_SCENE_DIR)/src/core_scene.c
CORE_TRACE_SRCS := $(CORE_TRACE_DIR)/src/core_trace.c
CORE_THEME_SRCS := $(CORE_THEME_DIR)/src/core_theme.c
CORE_FONT_SRCS := $(CORE_FONT_DIR)/src/core_font.c
KIT_VIZ_SRCS := $(KIT_VIZ_DIR)/src/kit_viz.c
CORE_BASE_OBJS := $(patsubst $(CORE_BASE_DIR)/src/%.c,$(BUILD_DIR)/core_base/%.o,$(CORE_BASE_SRCS))
CORE_IO_OBJS := $(patsubst $(CORE_IO_DIR)/src/%.c,$(BUILD_DIR)/core_io/%.o,$(CORE_IO_SRCS))
CORE_DATA_OBJS := $(patsubst $(CORE_DATA_DIR)/src/%.c,$(BUILD_DIR)/core_data/%.o,$(CORE_DATA_SRCS))
CORE_PACK_OBJS := $(patsubst $(CORE_PACK_DIR)/src/%.c,$(BUILD_DIR)/core_pack/%.o,$(CORE_PACK_SRCS))
CORE_SCENE_OBJS := $(patsubst $(CORE_SCENE_DIR)/src/%.c,$(BUILD_DIR)/core_scene/%.o,$(CORE_SCENE_SRCS))
CORE_TRACE_OBJS := $(patsubst $(CORE_TRACE_DIR)/src/%.c,$(BUILD_DIR)/core_trace/%.o,$(CORE_TRACE_SRCS))
CORE_THEME_OBJS := $(patsubst $(CORE_THEME_DIR)/src/%.c,$(BUILD_DIR)/core_theme/%.o,$(CORE_THEME_SRCS))
CORE_FONT_OBJS := $(patsubst $(CORE_FONT_DIR)/src/%.c,$(BUILD_DIR)/core_font/%.o,$(CORE_FONT_SRCS))
KIT_VIZ_OBJS := $(patsubst $(KIT_VIZ_DIR)/src/%.c,$(BUILD_DIR)/kit_viz/%.o,$(KIT_VIZ_SRCS))
OBJS += $(CORE_BASE_OBJS) $(CORE_IO_OBJS) $(CORE_DATA_OBJS) $(CORE_PACK_OBJS) $(CORE_SCENE_OBJS) $(CORE_THEME_OBJS) $(CORE_FONT_OBJS) $(KIT_VIZ_OBJS)
DEPS := $(OBJS:.o=.d)
CORE_PACK_TOOL_SRCS := \
	$(VF2D_PACK_TOOL_SRC) \
	$(CORE_PACK_DIR)/src/core_pack.c \
	$(CORE_PACK_DIR)/src/core_pack_vf2d.c \
	$(CORE_IO_DIR)/src/core_io.c \
	$(CORE_BASE_DIR)/src/core_base.c \
	$(CORE_SCENE_DIR)/src/core_scene.c
CORE_PACK_TOOL_INCS := -I$(CORE_PACK_DIR)/include -I$(CORE_IO_DIR)/include -I$(CORE_BASE_DIR)/include -I$(CORE_SCENE_DIR)/include
VF2D_DATASET_TOOL_SRCS := \
	$(VF2D_DATASET_TOOL_SRC) \
	$(SRC_DIR)/export/volume_frames.c \
	$(SRC_DIR)/export/export_paths.c \
	$(CORE_DATA_DIR)/src/core_data.c \
	$(CORE_PACK_DIR)/src/core_pack.c \
	$(CORE_PACK_DIR)/src/core_pack_vf2d.c \
	$(CORE_IO_DIR)/src/core_io.c \
	$(CORE_BASE_DIR)/src/core_base.c \
	$(CORE_SCENE_DIR)/src/core_scene.c \
	$(TIMER_HUD_DIR)/external/cJSON.c
VF2D_DATASET_TOOL_INCS := -I$(INC_DIR) -I$(SRC_DIR) -I$(SRC_DIR)/tools -I$(CORE_DATA_DIR)/include -I$(CORE_PACK_DIR)/include -I$(CORE_IO_DIR)/include -I$(CORE_BASE_DIR)/include -I$(CORE_SCENE_DIR)/include -I$(TIMER_HUD_DIR)/external $(SDL_CFLAGS)
ifeq ($(UNAME_S),Darwin)
VF2D_DATASET_TOOL_INCS += -I/opt/homebrew/include -D_THREAD_SAFE
endif
PHYSICS_TRACE_TOOL_SRCS := \
	$(PHYSICS_TRACE_TOOL_SRC) \
	$(CORE_TRACE_DIR)/src/core_trace.c \
	$(CORE_PACK_DIR)/src/core_pack.c \
	$(CORE_IO_DIR)/src/core_io.c \
	$(CORE_BASE_DIR)/src/core_base.c \
	$(CORE_SCENE_DIR)/src/core_scene.c \
	$(TIMER_HUD_DIR)/external/cJSON.c
PHYSICS_TRACE_TOOL_INCS := -I$(CORE_TRACE_DIR)/include -I$(CORE_PACK_DIR)/include -I$(CORE_IO_DIR)/include -I$(CORE_BASE_DIR)/include -I$(CORE_SCENE_DIR)/include -I$(TIMER_HUD_DIR)/external

# Shared ShapeLib/import pieces reused by the CLI
SHAPE_SHARED_SRCS := \
	$(SRC_DIR)/tools/ShapeLib/shape_core.c \
	$(SRC_DIR)/tools/ShapeLib/shape_flatten.c \
	$(SRC_DIR)/tools/ShapeLib/shape_json.c \
	$(SRC_DIR)/import/shape_import.c \
	$(SRC_DIR)/geo/shape_asset.c \
	$(TIMER_HUD_DIR)/external/cJSON.c
SHAPE_SHARED_OBJS := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SHAPE_SHARED_SRCS))

# =========================
#  Top-level targets
# =========================
.PHONY: all run run-ide-theme run-daw-theme clean video vf2d_pack_tool vf2d_to_pack vf2d_dataset_tool physics_trace_tool manifest_to_trace test-kitviz-field-adapter test-vf2d-dataset-export test-manifest-to-trace-export test-vf2d-pack-dataset-parity shim-parse-smoke shim-parse-parity shim-compile-subset shim-gate test-shared-theme-font-adapter

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)

# Collider test harness (headless)
COLLIDER_TEST_SRC := tests/collider_test.c \
	$(SRC_DIR)/app/app_config.c \
	$(SRC_DIR)/app/shape_lookup.c \
	$(SRC_DIR)/render/import_project.c \
	$(SRC_DIR)/geo/shape_library.c \
	$(SHAPE_SHARED_SRCS) \
	$(SRC_DIR)/physics/rigid/collider_builder.c \
	$(SRC_DIR)/physics/rigid/collider_geom.c \
	$(SRC_DIR)/physics/rigid/collider_legacy.c \
	$(SRC_DIR)/physics/rigid/collider_tagging.c

collider-test: $(COLLIDER_TEST_SRC)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -Isrc -Iinclude -o $(BUILD_DIR)/collider_test $(COLLIDER_TEST_SRC) $(filter-out -lSDL2 -lSDL2_ttf,$(LIBS))

collider-tests: collider-test
	$(BUILD_DIR)/collider_test

KIT_VIZ_FIELD_TEST_SRCS := \
	tests/kit_viz_field_adapter_test.c \
	$(SRC_DIR)/render/kit_viz_field_adapter.c \
	$(KIT_VIZ_DIR)/src/kit_viz.c \
	$(CORE_BASE_DIR)/src/core_base.c

test-kitviz-field-adapter: $(KIT_VIZ_FIELD_TEST_SRCS)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CSTD) $(WARN) $(DEBUG) \
		-I$(INC_DIR) -I$(SRC_DIR) \
		-I$(KIT_VIZ_DIR)/include -I$(CORE_BASE_DIR)/include \
		-o $(BUILD_DIR)/kit_viz_field_adapter_test $(KIT_VIZ_FIELD_TEST_SRCS) -lm
	$(BUILD_DIR)/kit_viz_field_adapter_test

shape_sanity_tool: $(SHAPE_SANITY_TOOL_OBJ)
	@mkdir -p $(dir $(SHAPE_SANITY_TOOL_OBJ))
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

SHIM_PARSE_SMOKE_SRC := tests/shim_include_smoke.c
SHIM_PARSE_LOG_DIR := $(BUILD_DIR)/shim
SHIM_PARSE_BASELINE_LOG := $(SHIM_PARSE_LOG_DIR)/parse_headers_baseline.log
SHIM_PARSE_SHADOW_LOG := $(SHIM_PARSE_LOG_DIR)/parse_headers_shadow.log
SHIM_PARSE_FLAGS := $(CSTD) $(WARN) $(DEBUG) -I$(INC_DIR) -I$(SRC_DIR) -I$(SRC_DIR)/tools
SHIM_COMPILE_SUBSET_SRCS := \
	tests/shim_include_smoke.c \
	src/physics/math/math2d.c \
	src/app/app_config.c

shim-parse-smoke:
	@mkdir -p $(SHIM_PARSE_LOG_DIR)
	@echo "Running physics_sim shim parse smoke (shadow include overlay)..."
	@$(CC) $(SHIM_PARSE_FLAGS) -I$(SYS_SHIMS_OVERLAY_DIR) -I$(SYS_SHIMS_INCLUDE_DIR) \
		-fsyntax-only $(SHIM_PARSE_SMOKE_SRC) -H -o /dev/null 2> $(SHIM_PARSE_SHADOW_LOG)
	@grep -E "overlay/include/(stdbool.h|stdint.h|stddef.h|stdarg.h|stdio.h)" $(SHIM_PARSE_SHADOW_LOG) >/dev/null || \
		( echo "shim parse smoke failed: expected overlay header usage not found"; exit 1 )
	@echo "physics_sim shim parse smoke passed."

shim-parse-parity:
	@mkdir -p $(SHIM_PARSE_LOG_DIR)
	@echo "Running physics_sim baseline parse smoke..."
	@$(CC) $(SHIM_PARSE_FLAGS) -fsyntax-only $(SHIM_PARSE_SMOKE_SRC) -H -o /dev/null 2> $(SHIM_PARSE_BASELINE_LOG)
	@echo "Running physics_sim shadow parse smoke..."
	@$(CC) $(SHIM_PARSE_FLAGS) -I$(SYS_SHIMS_OVERLAY_DIR) -I$(SYS_SHIMS_INCLUDE_DIR) \
		-fsyntax-only $(SHIM_PARSE_SMOKE_SRC) -H -o /dev/null 2> $(SHIM_PARSE_SHADOW_LOG)
	@grep -E "overlay/include/(stdbool.h|stdint.h|stddef.h|stdarg.h|stdio.h)" $(SHIM_PARSE_SHADOW_LOG) >/dev/null || \
		( echo "shim parse parity failed: overlay headers not detected"; exit 1 )
	@echo "physics_sim shim parse parity checks passed."

shim-compile-subset:
	@mkdir -p $(SHIM_PARSE_LOG_DIR)
	@echo "Running physics_sim shim compile subset (baseline)..."
	@for src in $(SHIM_COMPILE_SUBSET_SRCS); do \
		obj="$(SHIM_PARSE_LOG_DIR)/$$(basename "$$src" .c)_baseline.o"; \
		$(CC) $(CFLAGS) -c "$$src" -o "$$obj"; \
	done
	@echo "Running physics_sim shim compile subset (shadow)..."
	@for src in $(SHIM_COMPILE_SUBSET_SRCS); do \
		obj="$(SHIM_PARSE_LOG_DIR)/$$(basename "$$src" .c)_shadow.o"; \
		$(CC) $(CFLAGS) -I$(SYS_SHIMS_OVERLAY_DIR) -I$(SYS_SHIMS_INCLUDE_DIR) -DSYS_SHIM_MODE_SHADOW=1 -c "$$src" -o "$$obj"; \
	done
	@echo "physics_sim shim compile subset passed."

shim-gate: shim-parse-parity shim-compile-subset
	@echo "physics_sim shim gate passed."

# =========================
#  Compile rule (with depgen)
# =========================
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/timer_hud/%.o: $(TIMER_HUD_DIR)/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/timer_hud_external/%.o: $(TIMER_HUD_DIR)/external/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/vk_renderer/%.o: $(VK_RENDERER_DIR)/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/core_base/%.o: $(CORE_BASE_DIR)/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/core_io/%.o: $(CORE_IO_DIR)/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/core_data/%.o: $(CORE_DATA_DIR)/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/core_pack/%.o: $(CORE_PACK_DIR)/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/core_scene/%.o: $(CORE_SCENE_DIR)/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/core_theme/%.o: $(CORE_THEME_DIR)/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/core_font/%.o: $(CORE_FONT_DIR)/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/kit_viz/%.o: $(KIT_VIZ_DIR)/src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

# =========================
#  Convenience targets
# =========================
run: $(TARGET)
	./$(TARGET)

run-ide-theme: $(TARGET)
	PHYSICS_SIM_USE_SHARED_THEME_FONT=1 PHYSICS_SIM_USE_SHARED_THEME=1 PHYSICS_SIM_USE_SHARED_FONT=1 PHYSICS_SIM_THEME_PRESET=ide_gray PHYSICS_SIM_FONT_PRESET=ide ./$(TARGET)

run-daw-theme: $(TARGET)
	PHYSICS_SIM_USE_SHARED_THEME_FONT=1 PHYSICS_SIM_USE_SHARED_THEME=1 PHYSICS_SIM_USE_SHARED_FONT=1 PHYSICS_SIM_THEME_PRESET=daw_default PHYSICS_SIM_FONT_PRESET=daw_default ./$(TARGET)

VIDEO_FPS ?= 30
video:
	ffmpeg -y -framerate $(VIDEO_FPS) -i export/render_frames/frame_%06d.bmp -pix_fmt yuv420p export/render_vid/output.mp4

clean:
	rm -rf $(BUILD_DIR) $(TARGET) $(VF2D_PACK_TOOL_BIN) $(VF2D_DATASET_TOOL_BIN) $(PHYSICS_TRACE_TOOL_BIN)

shape_mask_tool: $(SHAPE_MASK_TOOL_OBJ) $(SHAPE_SHARED_OBJS)
	$(CC) $(LDFLAGS) -o $@ $(SHAPE_MASK_TOOL_OBJ) $(SHAPE_SHARED_OBJS) -lm

shape_asset_tool: $(SHAPE_ASSET_TOOL_OBJ) $(SHAPE_SHARED_OBJS)
	$(CC) $(LDFLAGS) -o $@ $(SHAPE_ASSET_TOOL_OBJ) $(SHAPE_SHARED_OBJS) -lm

# legacy alias
shape_import_tool: shape_mask_tool

vf2d_pack_tool: $(CORE_PACK_TOOL_SRCS)
	$(CC) $(CSTD) $(WARN) $(DEBUG) $(CORE_PACK_TOOL_INCS) -o $(VF2D_PACK_TOOL_BIN) $(CORE_PACK_TOOL_SRCS)

vf2d_dataset_tool: $(VF2D_DATASET_TOOL_SRCS)
	$(CC) $(CSTD) $(WARN) $(DEBUG) $(VF2D_DATASET_TOOL_INCS) -o $(VF2D_DATASET_TOOL_BIN) $(VF2D_DATASET_TOOL_SRCS) $(filter-out -lSDL2 -lSDL2_ttf,$(LIBS))

physics_trace_tool: $(PHYSICS_TRACE_TOOL_SRCS)
	$(CC) $(CSTD) $(WARN) $(DEBUG) $(PHYSICS_TRACE_TOOL_INCS) -o $(PHYSICS_TRACE_TOOL_BIN) $(PHYSICS_TRACE_TOOL_SRCS)

test-vf2d-dataset-export: vf2d_dataset_tool
	tests/integration/run_vf2d_dataset_export.sh

test-manifest-to-trace-export: physics_trace_tool
	tests/integration/run_manifest_to_trace_export.sh

test-vf2d-pack-dataset-parity: vf2d_pack_tool vf2d_dataset_tool
	tests/integration/run_vf2d_pack_dataset_parity.sh

SHARED_THEME_FONT_ADAPTER_TEST_SRCS := \
	tests/shared_theme_font_adapter_test.c \
	src/app/menu/shared_theme_font_adapter.c \
	$(CORE_THEME_DIR)/src/core_theme.c \
	$(CORE_FONT_DIR)/src/core_font.c \
	$(CORE_BASE_DIR)/src/core_base.c

test-shared-theme-font-adapter: $(SHARED_THEME_FONT_ADAPTER_TEST_SRCS)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) \
		-I$(INC_DIR) -I$(SRC_DIR) \
		-I$(CORE_THEME_DIR)/include -I$(CORE_FONT_DIR)/include -I$(CORE_BASE_DIR)/include \
		-o $(BUILD_DIR)/shared_theme_font_adapter_test $(SHARED_THEME_FONT_ADAPTER_TEST_SRCS) $(filter-out -lSDL2 -lSDL2_ttf,$(LIBS))
	$(BUILD_DIR)/shared_theme_font_adapter_test

vf2d_to_pack: vf2d_pack_tool
	@if [ -z "$(VF2D)" ] || [ -z "$(PACK)" ]; then \
		echo "usage: make vf2d_to_pack VF2D=/path/frame.vf2d PACK=/path/out.pack [MANIFEST=/path/manifest.json]"; \
		exit 1; \
	fi
	@if [ ! -f "$(VF2D)" ]; then \
		echo "vf2d_to_pack: VF2D file not found: $(VF2D)"; \
		exit 1; \
	fi
	@if [ -n "$(MANIFEST)" ] && [ ! -f "$(MANIFEST)" ]; then \
		echo "vf2d_to_pack: MANIFEST file not found: $(MANIFEST)"; \
		exit 1; \
	fi
	@mkdir -p "$$(dirname "$(PACK)")"
	@if [ -n "$(MANIFEST)" ]; then \
		./$(VF2D_PACK_TOOL_BIN) "$(VF2D)" "$(PACK)" "$(MANIFEST)"; \
	else \
		./$(VF2D_PACK_TOOL_BIN) "$(VF2D)" "$(PACK)"; \
	fi

manifest_to_trace: physics_trace_tool
	@if [ -z "$(MANIFEST)" ] || [ -z "$(TRACE)" ]; then \
		echo "usage: make manifest_to_trace MANIFEST=/path/manifest.json TRACE=/path/output.trace.pack [ITER=20]"; \
		exit 1; \
	fi
	@if [ ! -f "$(MANIFEST)" ]; then \
		echo "manifest_to_trace: MANIFEST file not found: $(MANIFEST)"; \
		exit 1; \
	fi
	@mkdir -p "$$(dirname "$(TRACE)")"
	@if [ -n "$(ITER)" ]; then \
		./$(PHYSICS_TRACE_TOOL_BIN) "$(MANIFEST)" "$(TRACE)" "$(ITER)"; \
	else \
		./$(PHYSICS_TRACE_TOOL_BIN) "$(MANIFEST)" "$(TRACE)"; \
	fi
		
# =========================
#  Auto-generated deps
# =========================
-include $(DEPS)
