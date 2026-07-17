# =========================
#  Dependency and compiler flags
# =========================
SDL_CFLAGS :=
SDL_LIBS   :=
SDL_TTF_CFLAGS :=
SDL_TTF_LIBS :=
VULKAN_CFLAGS :=
VULKAN_LIBS :=
JSON_CFLAGS :=
JSON_LIBS :=

ifneq ($(UNAME_S),Darwin)
SDL_CFLAGS := $(shell sdl2-config --cflags 2>/dev/null)
SDL_LIBS   := $(shell sdl2-config --libs 2>/dev/null)
SDL_TTF_CFLAGS := $(shell pkg-config --cflags SDL2_ttf 2>/dev/null)
SDL_TTF_LIBS := $(shell pkg-config --libs SDL2_ttf 2>/dev/null)
JSON_CFLAGS := $(shell pkg-config --cflags json-c 2>/dev/null)
JSON_LIBS := $(shell pkg-config --libs json-c 2>/dev/null)
endif

WARN      := -Wall -Wextra -Wpedantic
DEBUG     := -g

CFLAGS    := $(CSTD) $(WARN) $(DEBUG) $(ARCH_FLAGS) -I$(INC_DIR) -I$(SRC_DIR) -I$(SRC_DIR)/tools
CFLAGS    += -DPHYSICS_SIM_REPO_ROOT=\"$(abspath .)\"
CFLAGS    += -Wno-unknown-attributes
CFLAGS    += -Wno-c23-extensions
LDFLAGS   := $(ARCH_FLAGS)
LIBS      :=
FISICS_FLAGS := --overlay=physics-units

ifeq ($(SHIM_MODE),shadow)
	CFLAGS += -I$(SYS_SHIMS_OVERLAY_DIR) -I$(SYS_SHIMS_INCLUDE_DIR) -DSYS_SHIM_MODE_SHADOW=1
endif

ifeq ($(UNAME_S),Linux)
    # Needed for clock_gettime + friends
    CFLAGS += -D_POSIX_C_SOURCE=200809L -D_GNU_SOURCE

    ifneq ($(SDL_CFLAGS),)
        # Prefer sdl2-config if present
        CFLAGS += $(SDL_CFLAGS) $(SDL_TTF_CFLAGS)
        ifneq ($(strip $(SDL_TTF_LIBS)),)
            LIBS += $(SDL_TTF_LIBS) $(SDL_LIBS)
        else
            LIBS += -lSDL2_ttf $(SDL_LIBS)
        endif
    else
        # Fallback: system headers
        CFLAGS += -I/usr/include/SDL2
        LIBS   += -lSDL2_ttf -lSDL2
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
    SDL_CFLAGS := $(shell env PKG_CONFIG_LIBDIR="$(TARGET_PKG_CONFIG_LIBDIR)" $(PKG_CONFIG) --cflags sdl2 2>/dev/null)
    SDL_LIBS := $(shell env PKG_CONFIG_LIBDIR="$(TARGET_PKG_CONFIG_LIBDIR)" $(PKG_CONFIG) --libs sdl2 2>/dev/null)
    SDL_TTF_CFLAGS := $(shell env PKG_CONFIG_LIBDIR="$(TARGET_PKG_CONFIG_LIBDIR)" $(PKG_CONFIG) --cflags SDL2_ttf 2>/dev/null)
    SDL_TTF_LIBS := $(shell env PKG_CONFIG_LIBDIR="$(TARGET_PKG_CONFIG_LIBDIR)" $(PKG_CONFIG) --libs SDL2_ttf 2>/dev/null)
    JSON_CFLAGS := $(shell env PKG_CONFIG_LIBDIR="$(TARGET_PKG_CONFIG_LIBDIR)" $(PKG_CONFIG) --cflags json-c 2>/dev/null)
    JSON_LIBS := $(shell env PKG_CONFIG_LIBDIR="$(TARGET_PKG_CONFIG_LIBDIR)" $(PKG_CONFIG) --libs json-c 2>/dev/null)
    CFLAGS  += -D_POSIX_C_SOURCE=200809L -D_THREAD_SAFE
    ifneq ($(strip $(SDL_CFLAGS)),)
        CFLAGS += $(SDL_CFLAGS) $(SDL_TTF_CFLAGS)
    else
        CFLAGS += -I$(TARGET_HOMEBREW_PREFIX)/include
    endif
    ifneq ($(strip $(JSON_CFLAGS)),)
        CFLAGS += $(JSON_CFLAGS)
    else
        CFLAGS += -I$(TARGET_HOMEBREW_PREFIX)/include
    endif
    ifneq ($(strip $(SDL_LIBS)),)
        ifneq ($(strip $(SDL_TTF_LIBS)),)
            LIBS += $(SDL_TTF_LIBS) $(SDL_LIBS)
        else
            LIBS += -lSDL2_ttf $(SDL_LIBS)
        endif
    else
        LDFLAGS += -L$(TARGET_HOMEBREW_PREFIX)/lib
        LIBS += -lSDL2_ttf -lSDL2
    endif
    ifeq ($(strip $(JSON_LIBS)),)
        JSON_LIBS := -L$(TARGET_HOMEBREW_PREFIX)/lib -ljson-c
    endif

    VULKAN_CFLAGS := $(shell env PKG_CONFIG_LIBDIR="$(TARGET_PKG_CONFIG_LIBDIR)" $(PKG_CONFIG) --cflags vulkan 2>/dev/null)
    VULKAN_LIBS := $(shell env PKG_CONFIG_LIBDIR="$(TARGET_PKG_CONFIG_LIBDIR)" $(PKG_CONFIG) --libs vulkan 2>/dev/null)
    ifeq ($(strip $(VULKAN_CFLAGS)$(VULKAN_LIBS)),)
        VULKAN_CFLAGS := -I$(TARGET_HOMEBREW_PREFIX)/include
        VULKAN_LIBS := -L$(TARGET_HOMEBREW_PREFIX)/lib -lvulkan
    endif
    VULKAN_LIBS += -framework Metal -framework QuartzCore -framework Cocoa -framework IOKit -framework CoreVideo
    CFLAGS += -DVK_USE_PLATFORM_METAL_EXT
endif

ifeq ($(strip $(JSON_LIBS)),)
JSON_LIBS := -ljson-c
endif
ifeq ($(UNAME_S),Darwin)
ifeq ($(strip $(JSON_CFLAGS)),)
JSON_CFLAGS := -I$(TARGET_HOMEBREW_PREFIX)/include
endif
endif

LIBS += -lm
LIBS += $(JSON_LIBS)
HEADLESS_WORKER_LIBS := $(filter-out $(VULKAN_LIBS),$(LIBS))

TIMER_HUD_INCLUDE := -I$(TIMER_HUD_DIR)/include -I$(TIMER_HUD_DIR)/external

VK_RENDERER_BASE_CFLAGS := -I$(VK_RENDERER_DIR)/include $(VULKAN_CFLAGS) \
	-DUSE_VULKAN=1 -DVK_RENDERER_SHADER_ROOT=\"$(abspath $(VK_RENDERER_DIR))\"
VK_RENDERER_SDL_COMPAT_CFLAGS := -include $(VK_RENDERER_DIR)/include/vk_renderer_sdl.h

CFLAGS += $(TIMER_HUD_INCLUDE) $(VK_RENDERER_BASE_CFLAGS)
LIBS += $(VULKAN_LIBS)
CFLAGS += -I$(KIT_VIZ_DIR)/include
CFLAGS += -I$(KIT_RENDER_DIR)/include -DKIT_RENDER_ENABLE_VK_BACKEND=0
CFLAGS += -I$(KIT_UI_DIR)/include
CFLAGS += -I$(CORE_SCENE_COMPILE_DIR)/include
CFLAGS += -I$(CORE_PACK_DIR)/include -I$(CORE_IO_DIR)/include -I$(CORE_BASE_DIR)/include -I$(CORE_SCENE_DIR)/include -I$(CORE_SCENE_COMPILE_DIR)/include -I$(CORE_SCENE_VIEW_DIR)/include -I$(CORE_SCENE_VIEW_DIR)/../../shape/external -I$(CORE_MESH_ASSET_DIR)/include -I$(CORE_MESH_PREVIEW_DIR)/include -I$(CORE_MESH_PREVIEW_DIR)/../../shape/external -I$(CORE_OBJECT_DIR)/include -I$(CORE_UNITS_DIR)/include -I$(CORE_VIEWPORT2D_DIR)/include -I$(CORE_SCREEN_PICK_DIR)/include -I$(CORE_PANE_DIR)/include -I$(CORE_SIM_DIR)/include -I$(CORE_DATA_DIR)/include -I$(CORE_THEME_DIR)/include -I$(CORE_FONT_DIR)/include -I$(CORE_HEADLESS_JOB_DIR)/include -I$(KIT_PANE_DIR)/include -I$(KIT_WORKSPACE_AUTHORING_DIR)/include

ifeq ($(PACKAGE_TOOLCHAIN),fisics)
PACKAGE_SOURCE_BIN := $(FISICS_TARGET)
else
PACKAGE_SOURCE_BIN := $(CLANG_TARGET)
endif

FISICS_CFLAGS = $(filter-out -Wno-unknown-attributes,$(CFLAGS))
