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
SHARED_ASSETS_DIR := ../shared/assets
CORE_SCENE_COMPILE_DIR := ../shared/core/core_scene_compile
SHIM_MODE ?= off
SYS_SHIMS_DIR := ../shared/sys_shims
SYS_SHIMS_OVERLAY_DIR := $(SYS_SHIMS_DIR)/overlay/include
SYS_SHIMS_INCLUDE_DIR := $(SYS_SHIMS_DIR)/include
DIST_DIR := dist
PACKAGE_APP_NAME := kinetiC.app
PACKAGE_APP_DIR := $(DIST_DIR)/$(PACKAGE_APP_NAME)
PACKAGE_CONTENTS_DIR := $(PACKAGE_APP_DIR)/Contents
PACKAGE_MACOS_DIR := $(PACKAGE_CONTENTS_DIR)/MacOS
PACKAGE_RESOURCES_DIR := $(PACKAGE_CONTENTS_DIR)/Resources
PACKAGE_FRAMEWORKS_DIR := $(PACKAGE_CONTENTS_DIR)/Frameworks
PACKAGE_INFO_PLIST_SRC := tools/packaging/macos/Info.plist
PACKAGE_LAUNCHER_SRC := tools/packaging/macos/physics-sim-launcher
PACKAGE_DYLIB_BUNDLER := tools/packaging/macos/bundle-dylibs.sh
DESKTOP_APP_DIR ?= $(HOME)/Desktop/$(PACKAGE_APP_NAME)
PACKAGE_ADHOC_SIGN_IDENTITY ?= -

# RL0 release contract.
RELEASE_VERSION_FILE ?= VERSION
RELEASE_VERSION ?= $(strip $(shell cat "$(RELEASE_VERSION_FILE)" 2>/dev/null))
ifeq ($(RELEASE_VERSION),)
RELEASE_VERSION := 0.1.0
endif
RELEASE_CHANNEL ?= stable
RELEASE_PRODUCT_NAME := kinetiC
RELEASE_PROGRAM_KEY := physics_sim
RELEASE_BUNDLE_ID := com.cosm.kinetic
RELEASE_ARTIFACT_BASENAME := $(RELEASE_PRODUCT_NAME)-$(RELEASE_VERSION)-macOS-$(RELEASE_CHANNEL)
RELEASE_DIR := build/release
RELEASE_APP_ZIP := $(RELEASE_DIR)/$(RELEASE_ARTIFACT_BASENAME).zip
RELEASE_MANIFEST := $(RELEASE_DIR)/$(RELEASE_ARTIFACT_BASENAME).manifest.txt
RELEASE_CODESIGN_IDENTITY ?= $(if $(strip $(APPLE_SIGN_IDENTITY)),$(APPLE_SIGN_IDENTITY),$(PACKAGE_ADHOC_SIGN_IDENTITY))
APPLE_SIGN_IDENTITY ?=
APPLE_NOTARY_PROFILE ?=
APPLE_TEAM_ID ?=
STAPLE_MAX_ATTEMPTS ?= 6
STAPLE_RETRY_DELAY_SEC ?= 15

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
JSON_CFLAGS := $(shell pkg-config --cflags json-c 2>/dev/null)
JSON_LIBS := $(shell pkg-config --libs json-c 2>/dev/null)

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

ifeq ($(strip $(JSON_LIBS)),)
JSON_LIBS := -ljson-c
endif
ifeq ($(UNAME_S),Darwin)
ifeq ($(strip $(JSON_CFLAGS)),)
JSON_CFLAGS := -I/opt/homebrew/include
endif
endif

# Always link libm
LIBS += -lm
LIBS += $(JSON_LIBS)

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
CFLAGS += -I$(CORE_SCENE_COMPILE_DIR)/include

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
CORE_SCENE_COMPILE_DIR := ../shared/core/core_scene_compile
CORE_TRACE_DIR := ../shared/core/core_trace
CORE_THEME_DIR := ../shared/core/core_theme
CORE_FONT_DIR := ../shared/core/core_font
CFLAGS += -I$(CORE_PACK_DIR)/include -I$(CORE_IO_DIR)/include -I$(CORE_BASE_DIR)/include -I$(CORE_SCENE_DIR)/include -I$(CORE_SCENE_COMPILE_DIR)/include -I$(CORE_DATA_DIR)/include -I$(CORE_THEME_DIR)/include -I$(CORE_FONT_DIR)/include

CORE_BASE_SRCS := $(CORE_BASE_DIR)/src/core_base.c
CORE_IO_SRCS := $(CORE_IO_DIR)/src/core_io.c
CORE_DATA_SRCS := $(CORE_DATA_DIR)/src/core_data.c
CORE_PACK_SRCS := $(CORE_PACK_DIR)/src/core_pack.c $(CORE_PACK_DIR)/src/core_pack_vf2d.c
CORE_SCENE_SRCS := $(CORE_SCENE_DIR)/src/core_scene.c
CORE_SCENE_COMPILE_SRCS := $(CORE_SCENE_COMPILE_DIR)/src/core_scene_compile.c
CORE_TRACE_SRCS := $(CORE_TRACE_DIR)/src/core_trace.c
CORE_THEME_SRCS := $(CORE_THEME_DIR)/src/core_theme.c
CORE_FONT_SRCS := $(CORE_FONT_DIR)/src/core_font.c
KIT_VIZ_SRCS := $(KIT_VIZ_DIR)/src/kit_viz.c
CORE_BASE_OBJS := $(patsubst $(CORE_BASE_DIR)/src/%.c,$(BUILD_DIR)/core_base/%.o,$(CORE_BASE_SRCS))
CORE_IO_OBJS := $(patsubst $(CORE_IO_DIR)/src/%.c,$(BUILD_DIR)/core_io/%.o,$(CORE_IO_SRCS))
CORE_DATA_OBJS := $(patsubst $(CORE_DATA_DIR)/src/%.c,$(BUILD_DIR)/core_data/%.o,$(CORE_DATA_SRCS))
CORE_PACK_OBJS := $(patsubst $(CORE_PACK_DIR)/src/%.c,$(BUILD_DIR)/core_pack/%.o,$(CORE_PACK_SRCS))
CORE_SCENE_OBJS := $(patsubst $(CORE_SCENE_DIR)/src/%.c,$(BUILD_DIR)/core_scene/%.o,$(CORE_SCENE_SRCS))
CORE_SCENE_COMPILE_OBJS := $(patsubst $(CORE_SCENE_COMPILE_DIR)/src/%.c,$(BUILD_DIR)/core_scene_compile/%.o,$(CORE_SCENE_COMPILE_SRCS))
CORE_TRACE_OBJS := $(patsubst $(CORE_TRACE_DIR)/src/%.c,$(BUILD_DIR)/core_trace/%.o,$(CORE_TRACE_SRCS))
CORE_THEME_OBJS := $(patsubst $(CORE_THEME_DIR)/src/%.c,$(BUILD_DIR)/core_theme/%.o,$(CORE_THEME_SRCS))
CORE_FONT_OBJS := $(patsubst $(CORE_FONT_DIR)/src/%.c,$(BUILD_DIR)/core_font/%.o,$(CORE_FONT_SRCS))
KIT_VIZ_OBJS := $(patsubst $(KIT_VIZ_DIR)/src/%.c,$(BUILD_DIR)/kit_viz/%.o,$(KIT_VIZ_SRCS))
OBJS += $(CORE_BASE_OBJS) $(CORE_IO_OBJS) $(CORE_DATA_OBJS) $(CORE_PACK_OBJS) $(CORE_SCENE_OBJS) $(CORE_SCENE_COMPILE_OBJS) $(CORE_THEME_OBJS) $(CORE_FONT_OBJS) $(KIT_VIZ_OBJS)
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
STABLE_TEST_TARGETS := \
	test-manifest-to-trace-export \
	test-vf2d-pack-dataset-parity \
	test-trio-scene-contract-diff \
	test-kitviz-field-adapter \
	test-sim-mode-route-contract \
	test-preset-io-dimensional-contract \
	test-runtime-scene-bridge-contract \
	test-structural-runtime-split-contract

LEGACY_TEST_TARGETS := \
	test-shared-theme-font-adapter

.PHONY: all run run-ide-theme run-daw-theme run-headless-smoke visual-harness package-desktop package-desktop-smoke package-desktop-self-test package-desktop-copy-desktop package-desktop-sync package-desktop-open package-desktop-remove package-desktop-refresh release-contract release-clean release-build release-bundle-audit release-sign release-verify release-verify-signed release-notarize release-staple release-verify-notarized release-artifact release-distribute release-desktop-refresh clean video vf2d_pack_tool vf2d_to_pack vf2d_dataset_tool physics_trace_tool manifest_to_trace test-stable test-legacy test-kitviz-field-adapter test-sim-mode-route-contract test-preset-io-dimensional-contract test-runtime-scene-bridge-contract test-structural-runtime-split-contract test-vf2d-dataset-export test-manifest-to-trace-export test-vf2d-pack-dataset-parity test-trio-scene-contract-diff shim-parse-smoke shim-parse-parity shim-compile-subset shim-gate test-shared-theme-font-adapter

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

PRESET_IO_DIMENSIONAL_TEST_SRCS := \
	tests/preset_io_dimensional_contract_test.c \
	$(SRC_DIR)/app/preset_io.c \
	$(SRC_DIR)/app/scene_presets.c

SIM_MODE_ROUTE_CONTRACT_TEST_SRCS := \
	tests/sim_mode_route_contract_test.c \
	$(SRC_DIR)/app/sim_modes/sim_mode_dispatch.c

RUNTIME_SCENE_BRIDGE_TEST_SRCS := \
	tests/runtime_scene_bridge_contract_test.c \
	$(SRC_DIR)/import/runtime_scene_bridge.c \
	$(SRC_DIR)/app/app_config.c \
	$(SRC_DIR)/app/data_paths.c \
	$(SRC_DIR)/app/scene_presets.c \
	$(CORE_SCENE_COMPILE_DIR)/src/core_scene_compile.c \
	$(CORE_IO_DIR)/src/core_io.c \
	$(CORE_BASE_DIR)/src/core_base.c

STRUCTURAL_RUNTIME_SPLIT_TEST_SRCS := \
	tests/structural_runtime_split_contract_test.c \
	$(SRC_DIR)/app/structural/structural_controller_runtime.c \
	$(SRC_DIR)/physics/structural/structural_scene.c \
	$(SRC_DIR)/physics/structural/structural_solver.c

ifeq ($(UNAME_S),Darwin)
STRUCTURAL_RUNTIME_SPLIT_TEST_PLATFORM_CFLAGS := -I/opt/homebrew/include -D_THREAD_SAFE
else
STRUCTURAL_RUNTIME_SPLIT_TEST_PLATFORM_CFLAGS := $(if $(SDL_CFLAGS),$(SDL_CFLAGS),-I/usr/include/SDL2)
endif

test-sim-mode-route-contract: $(SIM_MODE_ROUTE_CONTRACT_TEST_SRCS)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CSTD) $(WARN) $(DEBUG) \
		-I$(INC_DIR) -I$(SRC_DIR) \
		-o $(BUILD_DIR)/sim_mode_route_contract_test $(SIM_MODE_ROUTE_CONTRACT_TEST_SRCS) -lm
	$(BUILD_DIR)/sim_mode_route_contract_test

test-preset-io-dimensional-contract: $(PRESET_IO_DIMENSIONAL_TEST_SRCS)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CSTD) $(WARN) $(DEBUG) \
		-I$(INC_DIR) -I$(SRC_DIR) \
		-o $(BUILD_DIR)/preset_io_dimensional_contract_test $(PRESET_IO_DIMENSIONAL_TEST_SRCS) -lm
	$(BUILD_DIR)/preset_io_dimensional_contract_test

test-runtime-scene-bridge-contract: $(RUNTIME_SCENE_BRIDGE_TEST_SRCS)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CSTD) $(WARN) $(DEBUG) \
		-I$(INC_DIR) -I$(SRC_DIR) \
		-I$(CORE_SCENE_COMPILE_DIR)/include -I$(CORE_IO_DIR)/include -I$(CORE_BASE_DIR)/include \
		$(JSON_CFLAGS) \
		-o $(BUILD_DIR)/runtime_scene_bridge_contract_test $(RUNTIME_SCENE_BRIDGE_TEST_SRCS) $(JSON_LIBS) -lm
	$(BUILD_DIR)/runtime_scene_bridge_contract_test

test-structural-runtime-split-contract: $(STRUCTURAL_RUNTIME_SPLIT_TEST_SRCS)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CSTD) $(WARN) $(DEBUG) \
		-I$(INC_DIR) -I$(SRC_DIR) $(STRUCTURAL_RUNTIME_SPLIT_TEST_PLATFORM_CFLAGS) \
		-o $(BUILD_DIR)/structural_runtime_split_contract_test $(STRUCTURAL_RUNTIME_SPLIT_TEST_SRCS) -lm
	$(BUILD_DIR)/structural_runtime_split_contract_test

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

$(BUILD_DIR)/core_scene_compile/%.o: $(CORE_SCENE_COMPILE_DIR)/src/%.c
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

run-headless-smoke: all test-stable
	@echo "physics_sim headless smoke passed (non-interactive)"

visual-harness: $(TARGET)
	@echo "visual harness binary ready: $(TARGET)"

package-desktop: all
	@echo "Preparing desktop package..."
	@rm -rf "$(PACKAGE_APP_DIR)"
	@mkdir -p "$(PACKAGE_MACOS_DIR)" "$(PACKAGE_RESOURCES_DIR)" "$(PACKAGE_FRAMEWORKS_DIR)"
	@cp "$(PACKAGE_INFO_PLIST_SRC)" "$(PACKAGE_CONTENTS_DIR)/Info.plist"
	@cp "$(TARGET)" "$(PACKAGE_MACOS_DIR)/physics-sim-bin"
	@cp "$(PACKAGE_LAUNCHER_SRC)" "$(PACKAGE_MACOS_DIR)/physics-sim-launcher"
	@"$(PACKAGE_DYLIB_BUNDLER)" "$(PACKAGE_MACOS_DIR)/physics-sim-bin" "$(PACKAGE_FRAMEWORKS_DIR)"
	@chmod +x "$(PACKAGE_MACOS_DIR)/physics-sim-bin" "$(PACKAGE_MACOS_DIR)/physics-sim-launcher"
	@cp -R config "$(PACKAGE_RESOURCES_DIR)/"
	@mkdir -p "$(PACKAGE_RESOURCES_DIR)/data/runtime" "$(PACKAGE_RESOURCES_DIR)/data/snapshots"
	@if [ -d "$(SHARED_ASSETS_DIR)/fonts" ]; then \
		mkdir -p "$(PACKAGE_RESOURCES_DIR)/shared/assets"; \
		cp -R "$(SHARED_ASSETS_DIR)/fonts" "$(PACKAGE_RESOURCES_DIR)/shared/assets/"; \
	fi
	@mkdir -p "$(PACKAGE_RESOURCES_DIR)/vk_renderer" "$(PACKAGE_RESOURCES_DIR)/shaders"
	@cp -R "$(VK_RENDERER_DIR)/shaders" "$(PACKAGE_RESOURCES_DIR)/vk_renderer/"
	@cp -R "$(VK_RENDERER_DIR)/shaders/." "$(PACKAGE_RESOURCES_DIR)/shaders/"
	@for dylib in $$(/usr/bin/find "$(PACKAGE_FRAMEWORKS_DIR)" -type f -name '*.dylib' 2>/dev/null); do \
		/usr/bin/codesign --force --sign "$(PACKAGE_ADHOC_SIGN_IDENTITY)" --timestamp=none "$$dylib"; \
	done
	@/usr/bin/codesign --force --sign "$(PACKAGE_ADHOC_SIGN_IDENTITY)" --timestamp=none "$(PACKAGE_MACOS_DIR)/physics-sim-bin"
	@/usr/bin/codesign --force --sign "$(PACKAGE_ADHOC_SIGN_IDENTITY)" --timestamp=none "$(PACKAGE_MACOS_DIR)/physics-sim-launcher"
	@/usr/bin/codesign --force --sign "$(PACKAGE_ADHOC_SIGN_IDENTITY)" --timestamp=none "$(PACKAGE_APP_DIR)"
	@echo "Desktop package ready: $(PACKAGE_APP_DIR)"

package-desktop-smoke: package-desktop
	@test -x "$(PACKAGE_MACOS_DIR)/physics-sim-launcher" || (echo "Missing launcher"; exit 1)
	@test -x "$(PACKAGE_MACOS_DIR)/physics-sim-bin" || (echo "Missing app binary"; exit 1)
	@test -f "$(PACKAGE_FRAMEWORKS_DIR)/libvulkan.1.dylib" || (echo "Missing bundled libvulkan.1.dylib"; exit 1)
	@test -f "$(PACKAGE_FRAMEWORKS_DIR)/libMoltenVK.dylib" || (echo "Missing bundled libMoltenVK.dylib"; exit 1)
	@test -f "$(PACKAGE_CONTENTS_DIR)/Info.plist" || (echo "Missing Info.plist"; exit 1)
	@test -f "$(PACKAGE_RESOURCES_DIR)/config/app.json" || (echo "Missing config/app.json"; exit 1)
	@test -f "$(PACKAGE_RESOURCES_DIR)/config/custom_preset.txt" || (echo "Missing config/custom_preset.txt"; exit 1)
	@test -f "$(PACKAGE_RESOURCES_DIR)/config/structural_scene.txt" || (echo "Missing config/structural_scene.txt"; exit 1)
	@test -f "$(PACKAGE_RESOURCES_DIR)/config/objects/Hexagon.asset.json" || (echo "Missing bundled shape assets"; exit 1)
	@test -d "$(PACKAGE_RESOURCES_DIR)/data/runtime" || (echo "Missing runtime dir"; exit 1)
	@test -d "$(PACKAGE_RESOURCES_DIR)/data/snapshots" || (echo "Missing snapshots dir"; exit 1)
	@test -f "$(PACKAGE_RESOURCES_DIR)/vk_renderer/shaders/textured.vert.spv" || (echo "Missing bundled vk_renderer shader"; exit 1)
	@test -f "$(PACKAGE_RESOURCES_DIR)/shaders/textured.vert.spv" || (echo "Missing bundled runtime shader"; exit 1)
	@echo "package-desktop-smoke passed."

package-desktop-self-test: package-desktop-smoke
	@"$(PACKAGE_MACOS_DIR)/physics-sim-launcher" --self-test || (echo "package-desktop self-test failed."; exit 1)
	@echo "package-desktop-self-test passed."

package-desktop-copy-desktop: package-desktop
	@mkdir -p "$(dir $(DESKTOP_APP_DIR))"
	@rm -rf "$(DESKTOP_APP_DIR)"
	@/usr/bin/ditto "$(PACKAGE_APP_DIR)" "$(DESKTOP_APP_DIR)"
	@echo "Copied $(PACKAGE_APP_NAME) to $(DESKTOP_APP_DIR)"

package-desktop-sync: package-desktop-copy-desktop
	@echo "Desktop package synchronized: $(DESKTOP_APP_DIR)"

package-desktop-open: package-desktop
	@open "$(PACKAGE_APP_DIR)"

package-desktop-remove:
	@rm -rf "$(PACKAGE_APP_DIR)"
	@echo "Removed desktop package: $(PACKAGE_APP_DIR)"

package-desktop-refresh: package-desktop
	@mkdir -p "$(dir $(DESKTOP_APP_DIR))"
	@rm -rf "$(DESKTOP_APP_DIR)"
	@/usr/bin/ditto "$(PACKAGE_APP_DIR)" "$(DESKTOP_APP_DIR)"
	@echo "Refreshed $(PACKAGE_APP_NAME) at $(DESKTOP_APP_DIR)"

release-contract:
	@mkdir -p "$(RELEASE_DIR)"
	@echo "release-contract:"
	@echo "  product: $(RELEASE_PRODUCT_NAME)"
	@echo "  program: $(RELEASE_PROGRAM_KEY)"
	@echo "  version: $(RELEASE_VERSION)"
	@echo "  channel: $(RELEASE_CHANNEL)"
	@echo "  bundle_id: $(RELEASE_BUNDLE_ID)"
	@echo "  app_name: $(PACKAGE_APP_NAME)"
	@echo "  artifact_base: $(RELEASE_ARTIFACT_BASENAME)"
	@echo "  release_zip: $(RELEASE_APP_ZIP)"
	@echo "  signing_identity: $(RELEASE_CODESIGN_IDENTITY)"
	@echo "  notary_profile_set: $$( [ -n \"$(APPLE_NOTARY_PROFILE)\" ] && echo yes || echo no )"
	@echo "  team_id_set: $$( [ -n \"$(APPLE_TEAM_ID)\" ] && echo yes || echo no )"

release-clean:
	@rm -rf "$(RELEASE_DIR)"
	@echo "Removed release dir: $(RELEASE_DIR)"

release-build: all
	@echo "Release build complete: $(TARGET)"

release-bundle-audit: package-desktop-self-test
	@mkdir -p "$(RELEASE_DIR)"
	@/usr/libexec/PlistBuddy -c 'Print :CFBundleIdentifier' "$(PACKAGE_CONTENTS_DIR)/Info.plist" > "$(RELEASE_DIR)/bundle_id.txt"
	@test "$$(cat "$(RELEASE_DIR)/bundle_id.txt")" = "$(RELEASE_BUNDLE_ID)" || (echo "bundle id mismatch: expected $(RELEASE_BUNDLE_ID), got $$(cat "$(RELEASE_DIR)/bundle_id.txt")"; exit 1)
	@env -i HOME="$(HOME)" PATH="$(PATH)" "$(PACKAGE_MACOS_DIR)/physics-sim-launcher" --print-config > "$(RELEASE_DIR)/print_config.txt"
	@runtime_dir="$$(/usr/bin/grep '^PHYSICS_SIM_RUNTIME_DIR=' "$(RELEASE_DIR)/print_config.txt" | /usr/bin/cut -d= -f2-)"; \
	if [ -z "$$runtime_dir" ]; then echo "runtime dir missing from print-config"; exit 1; fi; \
	case "$$runtime_dir" in *"/Contents/Resources"*) echo "runtime dir incorrectly points into app bundle: $$runtime_dir"; exit 1;; esac; \
	case "$$runtime_dir" in /tmp/*|/var/*|"$(HOME)"/*) ;; *) echo "runtime dir is not user-writable rooted: $$runtime_dir"; exit 1;; esac
	@/usr/bin/grep -q '^VK_ICD_FILENAMES=' "$(RELEASE_DIR)/print_config.txt" || (echo "missing VK_ICD_FILENAMES in print-config"; exit 1)
	@/usr/bin/grep -q '^VK_DRIVER_FILES=' "$(RELEASE_DIR)/print_config.txt" || (echo "missing VK_DRIVER_FILES in print-config"; exit 1)
	@otool -L "$(PACKAGE_MACOS_DIR)/physics-sim-bin" > "$(RELEASE_DIR)/otool_physics_sim_bin.txt"
	@if /usr/bin/grep -Eq '/opt/homebrew|/usr/local/Cellar|/Users/.*/CodeWork' "$(RELEASE_DIR)/otool_physics_sim_bin.txt"; then \
		echo "non-portable dylib dependency detected in $(PACKAGE_MACOS_DIR)/physics-sim-bin"; \
		cat "$(RELEASE_DIR)/otool_physics_sim_bin.txt"; \
		exit 1; \
	fi
	@for file in $$(/usr/bin/find "$(PACKAGE_FRAMEWORKS_DIR)" -type f -name '*.dylib' 2>/dev/null); do \
		base="$$(/usr/bin/basename "$$file")"; \
		otool -L "$$file" > "$(RELEASE_DIR)/otool_$$base.txt" || exit 1; \
		if /usr/bin/grep -Eq '/opt/homebrew|/usr/local/Cellar|/Users/.*/CodeWork' "$(RELEASE_DIR)/otool_$$base.txt"; then \
			echo "non-portable dylib dependency detected in $$file"; \
			cat "$(RELEASE_DIR)/otool_$$base.txt"; \
			exit 1; \
		fi; \
	done
	@echo "release-bundle-audit passed."

release-sign: release-bundle-audit
	@echo "Signing with identity: $(RELEASE_CODESIGN_IDENTITY)"
	@if [ "$(RELEASE_CODESIGN_IDENTITY)" = "-" ]; then \
		for dylib in $$(/usr/bin/find "$(PACKAGE_FRAMEWORKS_DIR)" -type f -name '*.dylib' 2>/dev/null); do \
			codesign --force --sign "$(RELEASE_CODESIGN_IDENTITY)" --timestamp=none "$$dylib"; \
		done; \
		codesign --force --sign "$(RELEASE_CODESIGN_IDENTITY)" --timestamp=none "$(PACKAGE_MACOS_DIR)/physics-sim-bin"; \
		codesign --force --sign "$(RELEASE_CODESIGN_IDENTITY)" --timestamp=none "$(PACKAGE_MACOS_DIR)/physics-sim-launcher"; \
		codesign --force --sign "$(RELEASE_CODESIGN_IDENTITY)" --timestamp=none "$(PACKAGE_APP_DIR)"; \
	else \
		for dylib in $$(/usr/bin/find "$(PACKAGE_FRAMEWORKS_DIR)" -type f -name '*.dylib' 2>/dev/null); do \
			codesign --force --sign "$(RELEASE_CODESIGN_IDENTITY)" --timestamp "$$dylib"; \
		done; \
		codesign --force --sign "$(RELEASE_CODESIGN_IDENTITY)" --timestamp --options runtime "$(PACKAGE_MACOS_DIR)/physics-sim-bin"; \
		codesign --force --sign "$(RELEASE_CODESIGN_IDENTITY)" --timestamp --options runtime "$(PACKAGE_MACOS_DIR)/physics-sim-launcher"; \
		codesign --force --sign "$(RELEASE_CODESIGN_IDENTITY)" --timestamp --options runtime "$(PACKAGE_APP_DIR)"; \
	fi
	@echo "release-sign complete."

release-verify:
	@codesign --verify --deep --strict "$(PACKAGE_APP_DIR)"
	@if [ "$(RELEASE_CODESIGN_IDENTITY)" = "-" ]; then \
		echo "release-verify note: ad-hoc identity in use; skipping spctl Gatekeeper assessment"; \
	else \
		spctl_output="$$(spctl --assess --type execute --verbose=2 "$(PACKAGE_APP_DIR)" 2>&1)"; \
		spctl_status=$$?; \
		if [ $$spctl_status -ne 0 ]; then \
			if printf '%s\n' "$$spctl_output" | /usr/bin/grep -qi "internal error in Code Signing subsystem"; then \
				echo "release-verify note: spctl internal subsystem error on this host; codesign verification remains authoritative"; \
			elif printf '%s\n' "$$spctl_output" | /usr/bin/grep -qi "Unnotarized Developer ID"; then \
				echo "release-verify note: app is Developer ID signed but not notarized yet"; \
			else \
				printf '%s\n' "$$spctl_output"; \
				exit $$spctl_status; \
			fi; \
		else \
			printf '%s\n' "$$spctl_output"; \
		fi; \
	fi
	@echo "release-verify passed."

release-verify-signed: release-sign release-verify
	@echo "release-verify-signed passed."

release-notarize: release-sign
	@if [ -z "$(APPLE_NOTARY_PROFILE)" ]; then \
		echo "APPLE_NOTARY_PROFILE is required for release-notarize"; \
		exit 1; \
	fi
	@if [ "$(RELEASE_CODESIGN_IDENTITY)" = "-" ]; then \
		echo "release-notarize requires a real Developer ID signing identity (APPLE_SIGN_IDENTITY)"; \
		exit 1; \
	fi
	@mkdir -p "$(RELEASE_DIR)"
	@/usr/bin/ditto -c -k --sequesterRsrc --keepParent "$(PACKAGE_APP_DIR)" "$(RELEASE_APP_ZIP)"
	@submission_json="$$(xcrun notarytool submit "$(RELEASE_APP_ZIP)" --keychain-profile "$(APPLE_NOTARY_PROFILE)" --wait --output-format json)"; \
	echo "$$submission_json" > "$(RELEASE_DIR)/notary_submit.json"; \
	status="$$(printf '%s\n' "$$submission_json" | /usr/bin/sed -n 's/.*\"status\"[[:space:]]*:[[:space:]]*\"\([^\"]*\)\".*/\1/p' | /usr/bin/tail -n 1)"; \
	if [ "$$status" != "Accepted" ]; then \
		submission_id="$$(printf '%s\n' "$$submission_json" | /usr/bin/sed -n 's/.*\"id\"[[:space:]]*:[[:space:]]*\"\([^\"]*\)\".*/\1/p' | /usr/bin/head -n 1)"; \
		echo "release-notarize failed: status=$$status id=$$submission_id"; \
		if [ -n "$$submission_id" ]; then \
			xcrun notarytool log "$$submission_id" --keychain-profile "$(APPLE_NOTARY_PROFILE)" > "$(RELEASE_DIR)/notary_log_$$submission_id.json" || true; \
			echo "notary log: $(RELEASE_DIR)/notary_log_$$submission_id.json"; \
		fi; \
		exit 1; \
	fi
	@echo "release-notarize passed."

release-staple:
	@attempt=1; \
	while [ $$attempt -le "$(STAPLE_MAX_ATTEMPTS)" ]; do \
		if xcrun stapler staple "$(PACKAGE_APP_DIR)"; then \
			break; \
		fi; \
		if [ $$attempt -eq "$(STAPLE_MAX_ATTEMPTS)" ]; then \
			echo "release-staple failed after $$attempt attempts"; \
			exit 1; \
		fi; \
		echo "release-staple retry $$attempt/$(STAPLE_MAX_ATTEMPTS) in $(STAPLE_RETRY_DELAY_SEC)s"; \
		sleep "$(STAPLE_RETRY_DELAY_SEC)"; \
		attempt=$$((attempt + 1)); \
	done
	@xcrun stapler validate "$(PACKAGE_APP_DIR)"
	@echo "release-staple passed."

release-verify-notarized: release-verify
	@xcrun stapler validate "$(PACKAGE_APP_DIR)"
	@echo "release-verify-notarized passed."

release-artifact:
	@mkdir -p "$(RELEASE_DIR)"
	@/usr/bin/ditto -c -k --sequesterRsrc --keepParent "$(PACKAGE_APP_DIR)" "$(RELEASE_APP_ZIP)"
	@shasum -a 256 "$(RELEASE_APP_ZIP)" > "$(RELEASE_APP_ZIP).sha256"
	@{ \
		echo "product=$(RELEASE_PRODUCT_NAME)"; \
		echo "program=$(RELEASE_PROGRAM_KEY)"; \
		echo "bundle_id=$(RELEASE_BUNDLE_ID)"; \
		echo "version=$(RELEASE_VERSION)"; \
		echo "channel=$(RELEASE_CHANNEL)"; \
		echo "artifact=$(RELEASE_APP_ZIP)"; \
		echo "sha256_file=$(RELEASE_APP_ZIP).sha256"; \
	} > "$(RELEASE_MANIFEST)"
	@echo "release-artifact complete: $(RELEASE_APP_ZIP)"

release-distribute: release-notarize release-staple release-verify-notarized release-artifact
	@echo "release-distribute passed."

release-desktop-refresh:
	@if [ ! -d "$(PACKAGE_APP_DIR)" ]; then \
		echo "release-desktop-refresh requires an existing built app at $(PACKAGE_APP_DIR)"; \
		echo "run release-distribute first"; \
		exit 1; \
	fi
	@mkdir -p "$$(dirname "$(DESKTOP_APP_DIR)")"
	@rm -rf "$(DESKTOP_APP_DIR)"
	@cp -R "$(PACKAGE_APP_DIR)" "$(DESKTOP_APP_DIR)"
	@echo "Release app refreshed at $(DESKTOP_APP_DIR)"

test-stable:
	@$(MAKE) $(STABLE_TEST_TARGETS)
	@echo "physics_sim stable test lane passed"

test-legacy:
	@set +e; \
	fails=0; \
	for t in $(LEGACY_TEST_TARGETS); do \
		echo "[legacy] running $$t"; \
		$(MAKE) $$t || fails=1; \
	done; \
	if [ $$fails -ne 0 ]; then \
		echo "[legacy] one or more legacy tests failed"; \
		exit 1; \
	fi

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

test-trio-scene-contract-diff:
	tests/integration/run_trio_scene_contract_diff.sh

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
