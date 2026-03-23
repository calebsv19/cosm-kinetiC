#include "app/menu/shared_theme_font_adapter.h"

#include <stdio.h>
#include <stdlib.h>

static int fail(const char* msg) {
    fprintf(stderr, "shared_theme_font_adapter_test: %s\n", msg);
    return 1;
}

int main(void) {
    PhysicsSimMenuThemePalette palette = {0};
    char path[256] = {0};
    int point_size = 0;
    size_t i = 0;
    const char* theme_presets[] = {
        "studio_blue",
        "harbor_blue",
        "midnight_contrast",
        "soft_light",
        "standard_grey",
        "greyscale"
    };

    unsetenv("PHYSICS_SIM_USE_SHARED_THEME_FONT");
    unsetenv("PHYSICS_SIM_USE_SHARED_THEME");
    unsetenv("PHYSICS_SIM_USE_SHARED_FONT");
    unsetenv("PHYSICS_SIM_THEME_PRESET");
    unsetenv("PHYSICS_SIM_FONT_PRESET");

    if (physics_sim_shared_theme_resolve_menu_palette(&palette)) {
        return fail("theme should be disabled by default");
    }
    if (physics_sim_shared_font_resolve_menu_body(path, sizeof(path), &point_size)) {
        return fail("font should be disabled by default");
    }

    setenv("PHYSICS_SIM_USE_SHARED_THEME_FONT", "1", 1);
    setenv("PHYSICS_SIM_THEME_PRESET", "standard_grey", 1);
    if (!physics_sim_shared_theme_resolve_menu_palette(&palette)) {
        return fail("theme should resolve when shared toggle is enabled");
    }

    setenv("PHYSICS_SIM_USE_SHARED_FONT", "0", 1);
    if (physics_sim_shared_font_resolve_menu_body(path, sizeof(path), &point_size)) {
        return fail("font should be disabled via per-domain toggle");
    }

    setenv("PHYSICS_SIM_USE_SHARED_FONT", "1", 1);
    setenv("PHYSICS_SIM_FONT_PRESET", "studio_blue", 1);
    if (!physics_sim_shared_font_resolve_menu_title(path, sizeof(path), &point_size)) {
        return fail("title font should resolve when enabled");
    }
    if (path[0] == '\0' || point_size <= 0) {
        return fail("font resolution returned invalid path or point size");
    }

    for (i = 0; i < sizeof(theme_presets) / sizeof(theme_presets[0]); ++i) {
        setenv("PHYSICS_SIM_THEME_PRESET", theme_presets[i], 1);
        if (!physics_sim_shared_theme_resolve_menu_palette(&palette)) {
            return fail("theme preset matrix should resolve");
        }
    }

    puts("shared_theme_font_adapter_test: success");
    return 0;
}
