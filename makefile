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

CFLAGS    := $(CSTD) $(WARN) $(DEBUG) -I$(INC_DIR) -I$(SRC_DIR)
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
# Find all .c files under src/
SRCS := $(shell find $(SRC_DIR) -name '*.c')
# Map src/foo/bar.c -> build/foo/bar.o
OBJS := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRCS))
DEPS := $(OBJS:.o=.d)

# =========================
#  Top-level targets
# =========================
.PHONY: all run clean video

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)

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

# =========================
#  Auto-generated deps
# =========================
-include $(DEPS)

