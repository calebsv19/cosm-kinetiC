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

# =========================
#  Diagnostics
# =========================
$(info USING MAKEFILE AT: $(abspath $(lastword $(MAKEFILE_LIST))))

# =========================
#  SDL2 via sdl2-config (if available)
# =========================
SDL_CFLAGS := $(shell sdl2-config --cflags 2>/dev/null)
SDL_LIBS   := $(shell sdl2-config --libs 2>/dev/null)

# =========================
#  Base flags
# =========================
WARN      := -Wall -Wextra -Wpedantic
DEBUG     := -g

CFLAGS    := $(CSTD) $(WARN) $(DEBUG) -I$(INC_DIR) -I$(SRC_DIR) -I$(SRC_DIR)/tools
LDFLAGS   :=
LIBS      :=

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
endif

ifeq ($(UNAME_S),Darwin)
    # macOS: clock_gettime is in libSystem, no -lrt needed
    # We *do not* use SDL_CFLAGS here, because it adds -I/opt/homebrew/include/SDL2,
    # which conflicts with our #include <SDL2/SDL.h> pattern.
    CFLAGS  += -D_POSIX_C_SOURCE=200809L -I/opt/homebrew/include -D_THREAD_SAFE
    LDFLAGS += -L/opt/homebrew/lib
    LIBS    += -lSDL2 -lSDL2_ttf
endif


# Always link libm
LIBS += -lm

# =========================
#  Source / object discovery
# =========================
# Find all .c files under src/ excluding CLI tools
SRCS := $(shell find $(SRC_DIR) -name '*.c' ! -path '$(SRC_DIR)/tools/cli/*')
# Map src/foo/bar.c -> build/foo/bar.o
OBJS := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRCS))
DEPS := $(OBJS:.o=.d)

# CLI tool sources (explicit to avoid multiple mains)
SHAPE_MASK_TOOL_SRC   := $(SRC_DIR)/tools/cli/shape_import_tool.c
SHAPE_MASK_TOOL_OBJ   := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SHAPE_MASK_TOOL_SRC))
SHAPE_ASSET_TOOL_SRC  := $(SRC_DIR)/tools/cli/shape_asset_tool.c
SHAPE_ASSET_TOOL_OBJ  := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SHAPE_ASSET_TOOL_SRC))
SHAPE_SANITY_TOOL_SRC := $(SRC_DIR)/tools/cli/shape_sanity_tool.c
SHAPE_SANITY_TOOL_OBJ := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SHAPE_SANITY_TOOL_SRC))

# Shared ShapeLib/import pieces reused by the CLI
SHAPE_SHARED_SRCS := \
	$(SRC_DIR)/tools/ShapeLib/shape_core.c \
	$(SRC_DIR)/tools/ShapeLib/shape_flatten.c \
	$(SRC_DIR)/tools/ShapeLib/shape_json.c \
	$(SRC_DIR)/import/shape_import.c \
	$(SRC_DIR)/geo/shape_asset.c \
	$(SRC_DIR)/render/TimerHUD/external/cJSON.c
SHAPE_SHARED_OBJS := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SHAPE_SHARED_SRCS))

# =========================
#  Top-level targets
# =========================
.PHONY: all run clean video

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

shape_sanity_tool: $(SHAPE_SANITY_TOOL_OBJ)
	@mkdir -p $(dir $(SHAPE_SANITY_TOOL_OBJ))
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

# =========================
#  Compile rule (with depgen)
# =========================
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

# =========================
#  Convenience targets
# =========================
run: $(TARGET)
	./$(TARGET)

VIDEO_FPS ?= 30
video:
	ffmpeg -y -framerate $(VIDEO_FPS) -i export/render_frames/frame_%06d.bmp -pix_fmt yuv420p export/render_vid/output.mp4

clean:
	rm -rf $(BUILD_DIR) $(TARGET)

shape_mask_tool: $(SHAPE_MASK_TOOL_OBJ) $(SHAPE_SHARED_OBJS)
	$(CC) $(LDFLAGS) -o $@ $(SHAPE_MASK_TOOL_OBJ) $(SHAPE_SHARED_OBJS) -lm

shape_asset_tool: $(SHAPE_ASSET_TOOL_OBJ) $(SHAPE_SHARED_OBJS)
	$(CC) $(LDFLAGS) -o $@ $(SHAPE_ASSET_TOOL_OBJ) $(SHAPE_SHARED_OBJS) -lm

# legacy alias
shape_import_tool: shape_mask_tool
		
# =========================
#  Auto-generated deps
# =========================
-include $(DEPS)
