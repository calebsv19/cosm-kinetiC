#include "app/menu/shared_theme_font_adapter.h"
#include "app/ui/physics_sim_ui_button.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int physics_sim_text_logical_pixels(SDL_Renderer *renderer, int raster_pixels) {
    (void)renderer;
    return raster_pixels;
}

int physics_sim_text_draw_utf8(SDL_Renderer *renderer,
                               TTF_Font *font,
                               const char *text,
                               SDL_Color color,
                               SDL_Rect *io_dst) {
    (void)renderer;
    (void)font;
    (void)text;
    (void)color;
    (void)io_dst;
    return 1;
}

static int fail(const char *msg) {
    fprintf(stderr, "physics_sim_ui_button_contract_test: %s\n", msg);
    return 1;
}

static bool color_equals(SDL_Color a, SDL_Color b) {
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

static PhysicsSimUIButtonPalette button_palette_from_menu_palette(PhysicsSimMenuThemePalette menu) {
    PhysicsSimUIButtonPalette palette;
    palette.idle_fill = menu.button_fill;
    palette.selected_fill = menu.button_active_fill;
    palette.hover_fill = menu.accent_primary;
    palette.positive_fill = (SDL_Color){36, 172, 86, 255};
    palette.outline_idle = menu.text_muted;
    palette.outline_highlight = menu.accent_primary;
    palette.text_primary = menu.text_primary;
    palette.text_muted = menu.text_muted;
    return palette;
}

static bool resolve_button_style(const PhysicsSimUIButtonPalette *palette,
                                 PhysicsSimUIButtonVariant variant,
                                 bool enabled,
                                 bool hovered,
                                 bool selected,
                                 bool pressed,
                                 PhysicsSimUIButtonResolvedStyle *out_style) {
    SDL_Rect rect = {100, 100, 96, 28};
    PhysicsSimUIButtonParams params;
    memset(&params, 0, sizeof(params));
    params.rect = &rect;
    params.label = "Test";
    params.palette = *palette;
    params.variant = variant;
    params.enabled = enabled;
    params.hovered = hovered;
    params.use_explicit_hover = true;
    params.selected = selected;
    params.pressed = pressed;
    return physics_sim_ui_button_resolve_style(&params, out_style);
}

int main(void) {
    PhysicsSimMenuThemePalette menu_palette = {0};
    PhysicsSimUIButtonPalette button_palette;
    PhysicsSimUIButtonResolvedStyle idle = {0};
    PhysicsSimUIButtonResolvedStyle hovered = {0};
    PhysicsSimUIButtonResolvedStyle selected = {0};
    PhysicsSimUIButtonResolvedStyle pressed = {0};
    PhysicsSimUIButtonResolvedStyle disabled = {0};
    PhysicsSimUIButtonResolvedStyle positive = {0};

    unsetenv("PHYSICS_SIM_USE_SHARED_THEME_FONT");
    unsetenv("PHYSICS_SIM_USE_SHARED_THEME");
    unsetenv("PHYSICS_SIM_THEME_PRESET");
    setenv("PHYSICS_SIM_THEME_PRESET", "standard_grey", 1);

    if (!physics_sim_shared_theme_resolve_menu_palette(&menu_palette)) {
        return fail("shared menu theme should resolve");
    }
    button_palette = button_palette_from_menu_palette(menu_palette);

    if (!resolve_button_style(&button_palette,
                              PHYSICS_SIM_UI_BUTTON_VARIANT_DEFAULT,
                              true,
                              false,
                              false,
                              false,
                              &idle)) {
        return fail("idle style should resolve");
    }
    if (!color_equals(idle.fill, button_palette.idle_fill)) {
        return fail("idle fill should route from menu button fill");
    }
    if (!color_equals(idle.outline, button_palette.outline_idle)) {
        return fail("idle outline should route from muted text outline");
    }
    if (!color_equals(idle.text, button_palette.text_primary)) {
        return fail("idle text should route from primary text");
    }

    if (!resolve_button_style(&button_palette,
                              PHYSICS_SIM_UI_BUTTON_VARIANT_DEFAULT,
                              true,
                              true,
                              false,
                              false,
                              &hovered)) {
        return fail("hovered style should resolve");
    }
    if (color_equals(hovered.fill, idle.fill)) {
        return fail("explicit hover should change fill");
    }
    if (!color_equals(hovered.outline, button_palette.outline_highlight)) {
        return fail("hover outline should use highlight outline");
    }

    if (!resolve_button_style(&button_palette,
                              PHYSICS_SIM_UI_BUTTON_VARIANT_DEFAULT,
                              true,
                              false,
                              true,
                              false,
                              &selected)) {
        return fail("selected style should resolve");
    }
    if (!color_equals(selected.fill, button_palette.selected_fill)) {
        return fail("selected fill should route from menu active button fill");
    }

    if (!resolve_button_style(&button_palette,
                              PHYSICS_SIM_UI_BUTTON_VARIANT_DEFAULT,
                              true,
                              false,
                              false,
                              true,
                              &pressed)) {
        return fail("pressed style should resolve");
    }
    if (color_equals(pressed.fill, idle.fill)) {
        return fail("pressed idle state should modify idle fill");
    }
    if (!color_equals(pressed.outline, button_palette.outline_highlight)) {
        return fail("pressed outline should use highlight outline");
    }

    if (!resolve_button_style(&button_palette,
                              PHYSICS_SIM_UI_BUTTON_VARIANT_DEFAULT,
                              false,
                              true,
                              false,
                              false,
                              &disabled)) {
        return fail("disabled style should resolve");
    }
    if (!color_equals(disabled.text, button_palette.text_muted)) {
        return fail("disabled text should route from muted text");
    }
    if (color_equals(disabled.fill, idle.fill)) {
        return fail("disabled fill should differ from idle fill");
    }

    if (!resolve_button_style(&button_palette,
                              PHYSICS_SIM_UI_BUTTON_VARIANT_POSITIVE,
                              true,
                              false,
                              true,
                              false,
                              &positive)) {
        return fail("positive selected style should resolve");
    }
    if (!color_equals(positive.fill, button_palette.positive_fill)) {
        return fail("positive selected fill should route from positive fill");
    }

    puts("physics_sim_ui_button_contract_test: success");
    return 0;
}
