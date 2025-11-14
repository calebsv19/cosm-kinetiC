#include "app/scene_menu.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdbool.h>
#include <stdio.h>

#include "app/editor/scene_editor.h"
#include "input/input.h"

#define MENU_WIDTH 820
#define MENU_HEIGHT 640

typedef struct MenuButton {
    SDL_Rect rect;
    const char *label;
    int id;
} MenuButton;

typedef struct SceneMenuInteraction {
    AppConfig *cfg;
    const FluidScenePreset *presets;
    size_t preset_count;
    int *selected_index;       // pointer to local index state
    int *selected_index_ptr;   // pointer to persistent index in main
    FluidScenePreset *preset;  // pointer to persistent preset in main
    FluidScenePreset *active_preset;
    SDL_Window *window;
    SDL_Renderer *renderer;
    TTF_Font *font;
    TTF_Font *font_small;
    MenuButton *start_button;
    MenuButton *edit_button;
    MenuButton *quit_button;
    MenuButton *grid_dec_button;
    MenuButton *grid_inc_button;
    bool *running;
    bool *start_requested;
} SceneMenuInteraction;

enum {
    BUTTON_START = 1,
    BUTTON_QUIT,
    BUTTON_GRID_DEC,
    BUTTON_GRID_INC,
    BUTTON_EDIT,
};

static SDL_Color COLOR_BG       = {18, 18, 22, 255};
static SDL_Color COLOR_PANEL    = {32, 36, 40, 255};
static SDL_Color COLOR_TEXT     = {245, 247, 250, 255};
static SDL_Color COLOR_TEXT_DIM = {210, 216, 226, 255};
static SDL_Color COLOR_ACCENT   = {90, 170, 255, 255};
static SDL_Color COLOR_HOVER    = {70, 120, 200, 255};

static bool point_in_rect(int x, int y, const SDL_Rect *rect) {
    return x >= rect->x && x < rect->x + rect->w &&
           y >= rect->y && y < rect->y + rect->h;
}

static void menu_pointer_up(const InputPointerState *state, void *user) {
    SceneMenuInteraction *ctx = (SceneMenuInteraction *)user;
    if (!ctx || !state) return;

    int x = state->x;
    int y = state->y;
    MenuButton *start_btn = ctx->start_button;
    MenuButton *edit_btn  = ctx->edit_button;
    MenuButton *quit_btn  = ctx->quit_button;
    MenuButton *dec_btn   = ctx->grid_dec_button;
    MenuButton *inc_btn   = ctx->grid_inc_button;

    if (point_in_rect(x, y, &start_btn->rect)) {
        if (ctx->preset) {
            *ctx->preset = *ctx->active_preset;
        }
        if (ctx->selected_index_ptr && ctx->selected_index) {
            *ctx->selected_index_ptr = *ctx->selected_index;
        }
        *ctx->start_requested = true;
        *ctx->running = false;
        return;
    }

    if (point_in_rect(x, y, &edit_btn->rect)) {
        FluidScenePreset edited = *ctx->active_preset;
        if (scene_editor_run(ctx->window, ctx->renderer,
                             ctx->font, ctx->font_small,
                             ctx->cfg, &edited)) {
            *ctx->active_preset = edited;
        }
        return;
    }

    if (point_in_rect(x, y, &quit_btn->rect)) {
        *ctx->running = false;
        return;
    }

    if (point_in_rect(x, y, &dec_btn->rect)) {
        ctx->cfg->grid_w = ctx->cfg->grid_h =
            (ctx->cfg->grid_w > 32) ? ctx->cfg->grid_w - 32 : 32;
        return;
    }

    if (point_in_rect(x, y, &inc_btn->rect)) {
        ctx->cfg->grid_w = ctx->cfg->grid_h =
            (ctx->cfg->grid_w < 512) ? ctx->cfg->grid_w + 32 : 512;
        return;
    }

    // Preset list
    for (size_t i = 0; i < ctx->preset_count; ++i) {
        SDL_Rect rect = {
            40,
            140 + (int)i * 60,
            320,
            48
        };
        if (point_in_rect(x, y, &rect)) {
            *ctx->selected_index = (int)i;
            *ctx->active_preset = ctx->presets[i];
            break;
        }
    }
}

static void draw_text(SDL_Renderer *renderer,
                      TTF_Font *font,
                      const char *text,
                      int x,
                      int y,
                      SDL_Color color) {
    SDL_Surface *surf = TTF_RenderUTF8_Blended(font, text, color);
    if (!surf) return;
    SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer, surf);
    if (!tex) {
        SDL_FreeSurface(surf);
        return;
    }

    SDL_Rect dst = {x, y, surf->w, surf->h};
    SDL_RenderCopy(renderer, tex, NULL, &dst);

    SDL_DestroyTexture(tex);
    SDL_FreeSurface(surf);
}

static void draw_button(SDL_Renderer *renderer,
                        TTF_Font *font,
                        const MenuButton *button,
                        bool selected,
                        bool hovered) {
    SDL_Color color = COLOR_PANEL;
    if (selected) {
        color = COLOR_ACCENT;
    } else if (hovered) {
        color = COLOR_HOVER;
    }
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, 255);
    SDL_RenderFillRect(renderer, &button->rect);

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 200);
    SDL_RenderDrawRect(renderer, &button->rect);

    int text_x = button->rect.x + 12;
    int text_y = button->rect.y + (button->rect.h / 2) - 12;
    draw_text(renderer, font, button->label, text_x, text_y, COLOR_TEXT);
}

static void draw_panel(SDL_Renderer *renderer, const SDL_Rect *rect) {
    SDL_SetRenderDrawColor(renderer, COLOR_PANEL.r, COLOR_PANEL.g, COLOR_PANEL.b, COLOR_PANEL.a);
    SDL_RenderFillRect(renderer, rect);
}

static void menu_pointer_down(const InputPointerState *state, void *user) {
    (void)state;
    (void)user;
}

static void menu_pointer_move(const InputPointerState *state, void *user) {
    (void)state;
    (void)user;
}

static void menu_key_down(SDL_Keycode key, SDL_Keymod mod, void *user) {
    (void)key;
    (void)mod;
    (void)user;
}

static void clamp_grid_size(AppConfig *cfg) {
    if (cfg->grid_w < 32) cfg->grid_w = 32;
    if (cfg->grid_h < 32) cfg->grid_h = 32;
    if (cfg->grid_w > 512) cfg->grid_w = 512;
    if (cfg->grid_h > 512) cfg->grid_h = 512;
}

bool scene_menu_run(AppConfig *cfg,
                    FluidScenePreset *preset_state,
                    int *preset_index) {
    if (!cfg || !preset_state) return false;

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL menu init failed: %s\n", SDL_GetError());
        return false;
    }
    if (TTF_Init() != 0) {
        fprintf(stderr, "SDL_ttf init failed: %s\n", TTF_GetError());
        SDL_Quit();
        return false;
    }

    SDL_Window *window = SDL_CreateWindow(
        "Physics Sim - Scene Editor",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        MENU_WIDTH, MENU_HEIGHT,
        SDL_WINDOW_SHOWN
    );
    if (!window) {
        fprintf(stderr, "Failed to create menu window: %s\n", SDL_GetError());
        TTF_Quit();
        SDL_Quit();
        return false;
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        fprintf(stderr, "Failed to create menu renderer: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return false;
    }

    TTF_Font *font_title = TTF_OpenFont("/System/Library/Fonts/Supplemental/Arial Bold.ttf", 32);
    if (!font_title) {
        font_title = TTF_OpenFont("/Library/Fonts/Arial Bold.ttf", 32);
    }
    if (!font_title) {
        fprintf(stderr, "Failed to open title font: %s\n", TTF_GetError());
    }
    TTF_Font *font_body = TTF_OpenFont("/System/Library/Fonts/Supplemental/Arial.ttf", 22);
    if (!font_body) {
        font_body = TTF_OpenFont("/Library/Fonts/Arial.ttf", 22);
    }
    if (!font_body) {
        fprintf(stderr, "Failed to open body font: %s\n", TTF_GetError());
    }
    TTF_Font *font_small = TTF_OpenFont("/System/Library/Fonts/Supplemental/Arial.ttf", 18);
    if (!font_small) {
        font_small = TTF_OpenFont("/Library/Fonts/Arial.ttf", 18);
    }

    size_t preset_count = 0;
    const FluidScenePreset *presets = scene_presets_get_all(&preset_count);
    int selected_preset = (preset_index && *preset_index >= 0 &&
                           *preset_index < (int)preset_count)
                              ? *preset_index
                              : 0;
    FluidScenePreset active_preset = *preset_state;
    clamp_grid_size(cfg);

    bool run = true;
    bool start_requested = false;

    MenuButton start_button = {
        .rect = {MENU_WIDTH - 220, MENU_HEIGHT - 80, 180, 50},
        .label = "Start Simulation",
        .id = BUTTON_START
    };
    MenuButton edit_button = {
        .rect = {MENU_WIDTH - 420, MENU_HEIGHT - 80, 180, 50},
        .label = "Edit Preset",
        .id = BUTTON_EDIT
    };
    MenuButton quit_button = {
        .rect = {20, MENU_HEIGHT - 70, 120, 40},
        .label = "Quit",
        .id = BUTTON_QUIT
    };
    MenuButton grid_dec_button = {
        .rect = {MENU_WIDTH - 260, 180, 40, 40},
        .label = "-",
        .id = BUTTON_GRID_DEC
    };
    MenuButton grid_inc_button = {
        .rect = {MENU_WIDTH - 100, 180, 40, 40},
        .label = "+",
        .id = BUTTON_GRID_INC
    };

    SceneMenuInteraction interaction = {
        .cfg = cfg,
        .presets = presets,
        .preset_count = preset_count,
        .selected_index = &selected_preset,
        .selected_index_ptr = preset_index,
        .preset = preset_state,
        .active_preset = &active_preset,
        .window = window,
        .renderer = renderer,
        .font = font_body,
        .font_small = font_small,
        .start_button = &start_button,
        .edit_button = &edit_button,
        .quit_button = &quit_button,
        .grid_dec_button = &grid_dec_button,
        .grid_inc_button = &grid_inc_button,
        .running = &run,
        .start_requested = &start_requested,
    };

    InputHandlers menu_handlers = {
        .on_pointer_down = menu_pointer_down,
        .on_pointer_up = menu_pointer_up,
        .on_pointer_move = menu_pointer_move,
        .on_key_down = menu_key_down,
        .user_data = &interaction,
    };

    while (run) {
        InputCommands cmds;
        input_poll_events(&cmds, NULL, &menu_handlers);
        if (cmds.quit) {
            run = false;
            break;
        }
        clamp_grid_size(cfg);

        SDL_SetRenderDrawColor(renderer, COLOR_BG.r, COLOR_BG.g, COLOR_BG.b, COLOR_BG.a);
        SDL_RenderClear(renderer);

        SDL_Rect presets_panel = {30, 100, 360, (int)(preset_count * 60 + 40)};
        draw_panel(renderer, &presets_panel);

        if (font_title) {
            draw_text(renderer, font_title, "Fluid Scene Editor", 30, 20, COLOR_TEXT);
        }
        if (font_body) {
            draw_text(renderer, font_body, "Scene Presets", 40, 110, COLOR_TEXT_DIM);
        }

        for (size_t i = 0; i < preset_count; ++i) {
            SDL_Rect rect = {
                40,
                140 + (int)i * 60,
                320,
                48
            };
            SDL_SetRenderDrawColor(renderer,
                                   (i == (size_t)selected_preset) ? COLOR_ACCENT.r : COLOR_PANEL.r,
                                   (i == (size_t)selected_preset) ? COLOR_ACCENT.g : COLOR_PANEL.g,
                                   (i == (size_t)selected_preset) ? COLOR_ACCENT.b : COLOR_PANEL.b,
                                   255);
            SDL_RenderFillRect(renderer, &rect);
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 200);
            SDL_RenderDrawRect(renderer, &rect);
            if (font_body) {
                draw_text(renderer, font_body, presets[i].name, rect.x + 12, rect.y + 14, COLOR_TEXT);
            }
        }

        SDL_Rect config_panel = {420, 100, 360, 220};
        draw_panel(renderer, &config_panel);
        if (font_body) {
            draw_text(renderer, font_body, "Grid Resolution", 440, 120, COLOR_TEXT_DIM);
            char buffer[64];
            snprintf(buffer, sizeof(buffer), "%dx%d cells", cfg->grid_w, cfg->grid_h);
            draw_text(renderer, font_body, buffer, 440, 150, COLOR_TEXT);
        }
        draw_button(renderer, font_body, &grid_dec_button, false, false);
        draw_button(renderer, font_body, &grid_inc_button, false, false);

        draw_button(renderer, font_body, &start_button, false, false);
        draw_button(renderer, font_body, &edit_button, false, false);
        draw_button(renderer, font_body, &quit_button, false, false);

        SDL_RenderPresent(renderer);
    }

    if (font_title) TTF_CloseFont(font_title);
    if (font_body) TTF_CloseFont(font_body);
    if (font_small) TTF_CloseFont(font_small);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();

    if (!start_requested) {
        *preset_state = active_preset;
        if (preset_index) {
            *preset_index = selected_preset;
        }
    }

    return start_requested;
}
