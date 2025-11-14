#include "app/scene_menu.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdbool.h>
#include <stdio.h>

#include "app/editor/scene_editor.h"
#include "input/input.h"
#include "ui/text_input.h"

#define MENU_WIDTH 820
#define MENU_HEIGHT 660
#define CUSTOM_SLOT_COUNT CUSTOM_PRESET_SLOT_COUNT
#define DOUBLE_CLICK_MS 350

typedef struct MenuButton {
    SDL_Rect rect;
    const char *label;
    int id;
} MenuButton;

typedef struct SceneMenuInteraction {
    AppConfig *cfg;
    const FluidScenePreset *presets;
    size_t preset_count;
    SceneMenuSelection *selection;
    CustomPresetLibrary *library;
    FluidScenePreset *preset_output;
    FluidScenePreset preview_preset;
    FluidScenePreset *active_preset;
    SDL_Window *window;
    SDL_Renderer *renderer;
    TTF_Font *font;
    TTF_Font *font_small;
    MenuButton start_button;
    MenuButton edit_button;
    MenuButton quit_button;
    MenuButton grid_dec_button;
    MenuButton grid_inc_button;
    bool *running;
    bool *start_requested;
    InputContextManager *context_mgr;

    TextInputField rename_input;
    int renaming_slot;
    Uint32 last_click_ticks;
    int last_clicked_slot;
} SceneMenuInteraction;

static SDL_Color COLOR_BG       = {18, 18, 22, 255};
static SDL_Color COLOR_PANEL    = {32, 36, 40, 255};
static SDL_Color COLOR_TEXT     = {245, 247, 250, 255};
static SDL_Color COLOR_TEXT_DIM = {210, 216, 226, 255};
static SDL_Color COLOR_ACCENT   = {90, 170, 255, 255};

static bool point_in_rect(int x, int y, const SDL_Rect *rect) {
    return x >= rect->x && x < rect->x + rect->w &&
           y >= rect->y && y < rect->y + rect->h;
}

static SDL_Rect custom_slot_rect(int slot_index) {
    SDL_Rect rect = {
        .x = 40,
        .y = 140 + slot_index * 60,
        .w = 320,
        .h = 48
    };
    return rect;
}

static void draw_text(SDL_Renderer *renderer,
                      TTF_Font *font,
                      const char *text,
                      int x,
                      int y,
                      SDL_Color color) {
    if (!renderer || !font || !text) return;
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

static void draw_panel(SDL_Renderer *renderer, const SDL_Rect *rect) {
    SDL_SetRenderDrawColor(renderer, COLOR_PANEL.r, COLOR_PANEL.g, COLOR_PANEL.b, COLOR_PANEL.a);
    SDL_RenderFillRect(renderer, rect);
}

static void draw_button(SDL_Renderer *renderer,
                        const SDL_Rect *rect,
                        const char *label,
                        TTF_Font *font,
                        bool selected) {
    SDL_Color color = selected ? COLOR_ACCENT : COLOR_PANEL;
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, 255);
    SDL_RenderFillRect(renderer, rect);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 200);
    SDL_RenderDrawRect(renderer, rect);
    draw_text(renderer, font, label, rect->x + 12, rect->y + 14, COLOR_TEXT);
}

static void draw_text_input(SDL_Renderer *renderer,
                            TTF_Font *font,
                            const SDL_Rect *rect,
                            const TextInputField *field) {
    SDL_SetRenderDrawColor(renderer, 20, 22, 26, 250);
    SDL_RenderFillRect(renderer, rect);
    SDL_SetRenderDrawColor(renderer, COLOR_ACCENT.r, COLOR_ACCENT.g, COLOR_ACCENT.b, 255);
    SDL_RenderDrawRect(renderer, rect);

    const char *text = text_input_value(field);
    SDL_Surface *surf = TTF_RenderUTF8_Blended(font, text, COLOR_TEXT);
    int text_w = 0;
    if (surf) {
        SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer, surf);
        text_w = surf->w;
        SDL_Rect dst = {rect->x + 8, rect->y + rect->h / 2 - surf->h / 2,
                        surf->w, surf->h};
        SDL_RenderCopy(renderer, tex, NULL, &dst);
        SDL_DestroyTexture(tex);
        SDL_FreeSurface(surf);
    }
    if (field->caret_visible) {
        int caret_x = rect->x + 8 + text_w + 2;
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 220);
        SDL_RenderDrawLine(renderer, caret_x, rect->y + 8,
                           caret_x, rect->y + rect->h - 8);
    }
}

static void select_custom(SceneMenuInteraction *ctx, int slot_index) {
    if (!ctx || !ctx->library) return;
    CustomPresetSlot *slot = preset_library_get_slot(ctx->library, slot_index);
    if (!slot) return;
    ctx->selection->custom_slot_index = slot_index;
    ctx->library->active_slot = slot_index;
    ctx->active_preset = &slot->preset;
}

static void begin_rename(SceneMenuInteraction *ctx, int slot_index) {
    if (!ctx) return;
    CustomPresetSlot *slot = preset_library_get_slot(ctx->library, slot_index);
    const char *initial = slot ? slot->name : "";
    ctx->renaming_slot = slot_index;
    text_input_begin(&ctx->rename_input,
                     initial,
                     CUSTOM_PRESET_NAME_MAX - 1);
}

static void finish_rename(SceneMenuInteraction *ctx, bool apply) {
    if (!ctx) return;
    if (!ctx->rename_input.active || ctx->renaming_slot < 0) {
        return;
    }
    if (apply) {
        CustomPresetSlot *slot =
            preset_library_get_slot(ctx->library, ctx->renaming_slot);
        if (slot) {
            const char *value = text_input_value(&ctx->rename_input);
            if (!value || value[0] == '\0') {
                snprintf(slot->name, sizeof(slot->name),
                         "Custom Slot %d", ctx->renaming_slot + 1);
            } else {
                snprintf(slot->name, sizeof(slot->name), "%s", value);
            }
            slot->preset.name = slot->name;
            slot->occupied = true;
        }
    }
    text_input_end(&ctx->rename_input);
    ctx->renaming_slot = -1;
}

static void clamp_grid_size(AppConfig *cfg) {
    if (cfg->grid_w < 32) cfg->grid_w = 32;
    if (cfg->grid_h < 32) cfg->grid_h = 32;
    if (cfg->grid_w > 512) cfg->grid_w = 512;
    if (cfg->grid_h > 512) cfg->grid_h = 512;
}

static void menu_pointer_up(void *user, const InputPointerState *state) {
    SceneMenuInteraction *ctx = (SceneMenuInteraction *)user;
    if (!ctx || !state) return;

    int x = state->x;
    int y = state->y;
    Uint32 now = SDL_GetTicks();

    if (ctx->rename_input.active) {
        bool clicked_name = false;
        for (int slot = 0; slot < CUSTOM_SLOT_COUNT; ++slot) {
            if (slot == ctx->renaming_slot) {
                SDL_Rect rect = custom_slot_rect(slot);
                if (point_in_rect(x, y, &rect)) {
                    clicked_name = true;
                    break;
                }
            }
        }
        if (!clicked_name) {
            finish_rename(ctx, false);
        }
    }

    if (point_in_rect(x, y, &ctx->start_button.rect)) {
        CustomPresetSlot *start_slot = preset_library_get_slot(
            ctx->library, ctx->selection->custom_slot_index);
        if (start_slot) start_slot->occupied = true;
        if (ctx->preset_output && ctx->active_preset) {
            *ctx->preset_output = *ctx->active_preset;
        }
        *ctx->start_requested = true;
        *ctx->running = false;
        return;
    }

    if (point_in_rect(x, y, &ctx->edit_button.rect)) {
        if (!ctx->active_preset) return;
        FluidScenePreset edited = *ctx->active_preset;
        FluidScenePreset *target = ctx->active_preset;
        char *name_buffer = NULL;
        size_t name_capacity = 0;
        CustomPresetSlot *slot = preset_library_get_slot(
            ctx->library, ctx->selection->custom_slot_index);
        if (slot) {
            name_buffer = slot->name;
            name_capacity = sizeof(slot->name);
        } else {
            target = &edited;
        }
        if (scene_editor_run(ctx->window,
                             ctx->renderer,
                             ctx->font,
                             ctx->font_small,
                             ctx->cfg,
                             target,
                             ctx->context_mgr,
                             name_buffer,
                             name_capacity)) {
            if (slot) {
                slot->preset = *target;
                slot->preset.name = slot->name;
                slot->occupied = true;
            }
        }
        return;
    }

    if (point_in_rect(x, y, &ctx->quit_button.rect)) {
        *ctx->running = false;
        return;
    }

    if (point_in_rect(x, y, &ctx->grid_dec_button.rect)) {
        ctx->cfg->grid_w = ctx->cfg->grid_h =
            (ctx->cfg->grid_w > 32) ? ctx->cfg->grid_w - 32 : 32;
        return;
    }

    if (point_in_rect(x, y, &ctx->grid_inc_button.rect)) {
        ctx->cfg->grid_w = ctx->cfg->grid_h =
            (ctx->cfg->grid_w < 512) ? ctx->cfg->grid_w + 32 : 512;
        return;
    }

    for (int slot = 0; slot < CUSTOM_SLOT_COUNT; ++slot) {
        SDL_Rect rect = custom_slot_rect(slot);
        if (point_in_rect(x, y, &rect)) {
            bool double_click = (ctx->last_clicked_slot == slot) &&
                                (now - ctx->last_click_ticks <= DOUBLE_CLICK_MS);
            ctx->last_clicked_slot = slot;
            ctx->last_click_ticks = now;

            if (double_click) {
                begin_rename(ctx, slot);
            } else {
                select_custom(ctx, slot);
            }
            return;
        }
    }
}

static void menu_pointer_down(void *user, const InputPointerState *state) {
    (void)user;
    (void)state;
}

static void menu_pointer_move(void *user, const InputPointerState *state) {
    (void)user;
    (void)state;
}

static void menu_key_down(void *user, SDL_Keycode key, SDL_Keymod mod) {
    SceneMenuInteraction *ctx = (SceneMenuInteraction *)user;
    (void)mod;
    if (!ctx) return;

    if (ctx->rename_input.active) {
        if (key == SDLK_RETURN || key == SDLK_KP_ENTER) {
            finish_rename(ctx, true);
        } else if (key == SDLK_ESCAPE) {
            finish_rename(ctx, false);
        } else {
            text_input_handle_key(&ctx->rename_input, key);
        }
    }
}

static void menu_text_input(void *user, const char *text) {
    SceneMenuInteraction *ctx = (SceneMenuInteraction *)user;
    if (!ctx || !text) return;
    if (ctx->rename_input.active) {
        text_input_handle_text(&ctx->rename_input, text);
    }
}

bool scene_menu_run(AppConfig *cfg,
                    FluidScenePreset *preset_state,
                    SceneMenuSelection *selection,
                    CustomPresetLibrary *library) {
    if (!cfg || !preset_state || !selection || !library) return false;

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

    SDL_Renderer *renderer = SDL_CreateRenderer(
        window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
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
    clamp_grid_size(cfg);

    SceneMenuSelection current_selection = *selection;
    if (current_selection.custom_slot_index < 0 ||
        current_selection.custom_slot_index >= CUSTOM_SLOT_COUNT) {
        current_selection.custom_slot_index = 0;
    }

    bool run = true;
    bool start_requested = false;

    SceneMenuInteraction ctx = {
        .cfg = cfg,
        .presets = presets,
        .preset_count = preset_count,
        .selection = &current_selection,
        .library = library,
        .preset_output = preset_state,
        .preview_preset = *preset_state,
        .active_preset = NULL,
        .window = window,
        .renderer = renderer,
        .font = font_body,
        .font_small = font_small,
        .start_button = {.rect = {MENU_WIDTH - 220, MENU_HEIGHT - 80, 180, 50}, .label = "Start"},
        .edit_button = {.rect = {MENU_WIDTH - 420, MENU_HEIGHT - 80, 180, 50}, .label = "Edit Preset"},
        .quit_button = {.rect = {20, MENU_HEIGHT - 70, 120, 40}, .label = "Quit"},
        .grid_dec_button = {.rect = {MENU_WIDTH - 260, 180, 40, 40}, .label = "-"},
        .grid_inc_button = {.rect = {MENU_WIDTH - 100, 180, 40, 40}, .label = "+"},
        .running = &run,
        .start_requested = &start_requested,
        .context_mgr = NULL,
        .renaming_slot = -1,
        .last_click_ticks = 0,
        .last_clicked_slot = -1,
    };

    InputContextManager context_mgr;
    input_context_manager_init(&context_mgr);
    ctx.context_mgr = &context_mgr;

    select_custom(&ctx, current_selection.custom_slot_index);
    *selection = current_selection;

    InputContext menu_ctx = {
        .on_pointer_down = menu_pointer_down,
        .on_pointer_up = menu_pointer_up,
        .on_pointer_move = menu_pointer_move,
        .on_key_down = menu_key_down,
        .on_text_input = menu_text_input,
        .user_data = &ctx
    };
    input_context_manager_push(&context_mgr, &menu_ctx);

    Uint32 prev_ticks = SDL_GetTicks();
    while (run) {
        Uint32 now_ticks = SDL_GetTicks();
        double dt = (double)(now_ticks - prev_ticks) / 1000.0;
        prev_ticks = now_ticks;
        text_input_update(&ctx.rename_input, dt);

        InputCommands cmds;
        input_poll_events(&cmds, NULL, &context_mgr);
        if (cmds.quit) {
            run = false;
            break;
        }
        clamp_grid_size(cfg);

        SDL_SetRenderDrawColor(renderer, COLOR_BG.r, COLOR_BG.g, COLOR_BG.b, COLOR_BG.a);
        SDL_RenderClear(renderer);

        SDL_Rect slots_panel = {30, 100, 360, (int)(CUSTOM_SLOT_COUNT * 60 + 40)};
        draw_panel(renderer, &slots_panel);
        if (font_title) {
            draw_text(renderer, font_title, "Custom Presets", 30, 20, COLOR_TEXT);
        }
        draw_text(renderer, font_body, "Saved Slots", 40, 110, COLOR_TEXT_DIM);

        for (int slot = 0; slot < CUSTOM_SLOT_COUNT; ++slot) {
            SDL_Rect rect = {
                40,
                140 + slot * 60,
                320,
                48
            }; 
            CustomPresetSlot *slot_data = preset_library_get_slot(library, slot);
            char label[160];
            if (slot_data && slot_data->occupied && slot_data->name[0]) {
                snprintf(label, sizeof(label), "%s", slot_data->name);
            } else {
                snprintf(label, sizeof(label), "Empty Slot %d", slot + 1);
            }
            bool selected = slot == current_selection.custom_slot_index;
            if (ctx.rename_input.active && slot == ctx.renaming_slot) {
                draw_text_input(renderer, font_body, &rect, &ctx.rename_input);
            } else {
                draw_button(renderer, &rect, label, font_body, selected);
            }
        }

        SDL_Rect config_panel = {420, 100, 360, 220};
        draw_panel(renderer, &config_panel);
        draw_text(renderer, font_body, "Grid Resolution", config_panel.x + 12, config_panel.y + 12, COLOR_TEXT_DIM);
        char buffer[64];
        snprintf(buffer, sizeof(buffer), "%dx%d cells", cfg->grid_w, cfg->grid_h);
        draw_text(renderer, font_body, buffer, config_panel.x + 12, config_panel.y + 42, COLOR_TEXT);
        draw_text(renderer, font_body, "Active Preset", config_panel.x + 12, config_panel.y + 86, COLOR_TEXT_DIM);
        const char *preset_name =
            (ctx.active_preset && ctx.active_preset->name && ctx.active_preset->name[0])
                ? ctx.active_preset->name
                : "Preset";
        char preset_label[128];
        snprintf(preset_label, sizeof(preset_label),
                 "%s (custom)",
                 preset_name);
        draw_text(renderer, font_body, preset_label, config_panel.x + 12, config_panel.y + 116, COLOR_TEXT);

        ctx.grid_dec_button.rect = (SDL_Rect){config_panel.x + 240, config_panel.y + 40, 40, 40};
        ctx.grid_inc_button.rect = (SDL_Rect){config_panel.x + 290, config_panel.y + 40, 40, 40};
        draw_button(renderer, &ctx.grid_dec_button.rect, ctx.grid_dec_button.label, font_body, false);
        draw_button(renderer, &ctx.grid_inc_button.rect, ctx.grid_inc_button.label, font_body, false);
        draw_button(renderer, &ctx.start_button.rect, ctx.start_button.label, font_body, false);
        draw_button(renderer, &ctx.edit_button.rect, ctx.edit_button.label, font_body, false);
        draw_button(renderer, &ctx.quit_button.rect, ctx.quit_button.label, font_body, false);

        SDL_RenderPresent(renderer);
    }

    input_context_manager_pop(&context_mgr);

    if (ctx.rename_input.active) {
        finish_rename(&ctx, false);
    }

    if (font_title) TTF_CloseFont(font_title);
    if (font_body) TTF_CloseFont(font_body);
    if (font_small) TTF_CloseFont(font_small);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();

    if (!start_requested && ctx.active_preset) {
        *preset_state = *ctx.active_preset;
    }
    *selection = current_selection;

    return start_requested;
}
