# Compiler and standard
CC        := cc
CSTD      := -std=c11

# Project structure
SRC_DIR   := src
INC_DIR   := include
BUILD_DIR := build
TARGET    := physics_sim

# SDL2 via sdl2-config (same style as your ray_tracing project)
SDL_CFLAGS := $(shell sdl2-config --cflags 2>/dev/null)
SDL_LIBS   := $(shell sdl2-config --libs 2>/dev/null)
SDL_PREFIX := $(shell sdl2-config --prefix 2>/dev/null)
SDL_EXTRA_INC := $(if $(SDL_PREFIX),-I$(SDL_PREFIX)/include,)

# Compiler flags
CFLAGS := $(CSTD) -Wall -Wextra -Wpedantic -g \
          $(SDL_CFLAGS) $(SDL_EXTRA_INC)      \
          -I$(INC_DIR)                        \
          -I$(INC_DIR)/app                    \
          -I$(INC_DIR)/command                \
          -I$(INC_DIR)/input                  \
          -I$(INC_DIR)/render                 \
          -I$(INC_DIR)/physics                \
          -I$(SRC_DIR)

LDFLAGS := $(SDL_LIBS) -lSDL2_ttf -lm

# Source files (discovered recursively)
SRCS := $(shell find $(SRC_DIR) -type f -name '*.c')

# Object and dep files mirroring src tree inside build/
OBJS := $(SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
DEPS := $(OBJS:.o=.d)

# Default target
all: $(TARGET)

# Link step
$(TARGET): $(OBJS)
	@echo "Linking $(TARGET)"
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Compile step for any .c under src/ into build/
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	@echo "Compiling $<"
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

# Run the program (build if needed)
run: $(TARGET)
	./$(TARGET)

VIDEO_FPS ?= 30
video:
	ffmpeg -y -framerate $(VIDEO_FPS) -i export/render_frames/frame_%06d.bmp -pix_fmt yuv420p export/render_vid/output.mp4

# Remove build outputs
clean:
	rm -rf $(BUILD_DIR) $(TARGET)

# Include auto-generated dependency files if they exist
-include $(DEPS)
