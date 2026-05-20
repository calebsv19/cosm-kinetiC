#ifndef PHYSICS_SIM_UI_BUTTON_H
#define PHYSICS_SIM_UI_BUTTON_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdbool.h>

#include <kit_ui.h>

typedef enum PhysicsSimUIButtonVariant {
    PHYSICS_SIM_UI_BUTTON_VARIANT_DEFAULT = 0,
    PHYSICS_SIM_UI_BUTTON_VARIANT_PRIMARY = 1,
    PHYSICS_SIM_UI_BUTTON_VARIANT_POSITIVE = 2
} PhysicsSimUIButtonVariant;

typedef struct PhysicsSimUIButtonPalette {
    SDL_Color idle_fill;
    SDL_Color selected_fill;
    SDL_Color hover_fill;
    SDL_Color positive_fill;
    SDL_Color outline_idle;
    SDL_Color outline_highlight;
    SDL_Color text_primary;
    SDL_Color text_muted;
} PhysicsSimUIButtonPalette;

typedef struct PhysicsSimUIButtonParams {
    const SDL_Rect *rect;
    const char *label;
    TTF_Font *font;
    PhysicsSimUIButtonPalette palette;
    PhysicsSimUIButtonVariant variant;
    bool enabled;
    bool hovered;
    bool use_explicit_hover;
    bool selected;
    bool pressed;
    int min_left_pad;
    int min_top_pad;
} PhysicsSimUIButtonParams;

typedef struct PhysicsSimUIButtonResolvedStyle {
    SDL_Color fill;
    SDL_Color outline;
    SDL_Color text;
} PhysicsSimUIButtonResolvedStyle;

typedef struct PhysicsSimUIButtonResolvedDrawParams {
    const SDL_Rect *rect;
    const char *label;
    TTF_Font *font;
    SDL_Color fill;
    SDL_Color outline;
    SDL_Color text;
    int min_left_pad;
    int min_top_pad;
} PhysicsSimUIButtonResolvedDrawParams;

bool physics_sim_ui_button_draw_frame(SDL_Renderer *renderer,
                                      const SDL_Rect *rect,
                                      SDL_Color fill,
                                      SDL_Color outline);
bool physics_sim_ui_button_resolve_style(const PhysicsSimUIButtonParams *params,
                                         PhysicsSimUIButtonResolvedStyle *out_style);
bool physics_sim_ui_button_draw_resolved(SDL_Renderer *renderer,
                                         const PhysicsSimUIButtonResolvedDrawParams *params);
bool physics_sim_ui_button_draw(SDL_Renderer *renderer,
                                const PhysicsSimUIButtonParams *params);

#endif
