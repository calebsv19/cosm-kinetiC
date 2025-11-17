$(info USING MAKEFILE AT: $(realpath $(lastword $(MAKEFILE_LIST))))

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
#  Base flags
# =========================
WARN      := -Wall -Wextra -Wpedantic
DEBUG     := -g

CFLAGS    := $(CSTD) $(WARN) $(DEBUG) -I$(INC_DIR) -I$(SRC_DIR)
LDFLAGS   :=
LIBS      := -lm

# =========================
#  SDL (common)
# =========================
CFLAGS    += -I/usr/include/SDL2 -D_POSIX_C_SOURCE=200809L -D_GNU_SOURCE
LIBS      += -lSDL2 -lSDL2_ttf -lrt

ifeq ($(UNAME_S),Darwin)
    CFLAGS  += -I/opt/homebrew/include/SDL2 -D_THREAD_SAFE
    LDFLAGS += -L/opt/homebrew/lib
endif

# =========================
#  Source / object discovery
# =========================
SRCS := $(shell find $(SRC_DIR) -name '*.c')
OBJS := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRCS))
DEPS := $(OBJS:.o=.d)

# =========================
#  Main targets
# =========================
.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

run: $(TARGET)
	./$(TARGET)

clean:
	rm -rf $(BUILD_DIR) $(TARGET)

-include $(DEPS)

