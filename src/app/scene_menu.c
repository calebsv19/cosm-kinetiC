#include "app/scene_menu.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdbool.h>
#include <stdio.h>

#include "app/editor/scene_editor.h"
#include "input/input.h"
#include "ui/text_input.h"
#include "ui/scrollbar.h"

#define MENU_WIDTH 820
#define MENU_HEIGHT 660
#define PRESET_ROW_HEIGHT 60
#define PRESET_LIST_WIDTH 360
#define PRESET_LIST_MARGIN_X 40
#define PRESET_LIST_MARGIN_Y 120
#define PRESET_LIST_HEIGHT (MENU_HEIGHT - 220)
#define SCROLLBAR_WIDTH 6
#define ADD_ENTRY_GAP 0
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
    SDL_Rect volume_toggle_rect;
    SDL_Rect render_toggle_rect;
    bool *running;
    bool *start_requested;
    InputContextManager *context_mgr;

    TextInputField rename_input;
    int renaming_slot;
    Uint32 last_click_ticks;
    int last_clicked_slot;
    SDL_Rect list_rect;
    ScrollBar scrollbar;
    bool scrollbar_dragging;
    int hover_slot;
    bool hover_add_entry;
} SceneMenuInteraction;

static void begin_rename(SceneMenuInteraction *ctx, int slot_index);
static void finish_rename(SceneMenuInteraction *ctx, bool apply);

static SDL_Color COLOR_BG       = {18, 18, 22, 255};
static SDL_Color COLOR_PANEL    = {32, 36, 40, 255};
static SDL_Color COLOR_TEXT     = {245, 247, 250, 255};
static SDL_Color COLOR_TEXT_DIM = {210, 216, 226, 255};
static SDL_Color COLOR_ACCENT   = {90, 170, 255, 255};

static bool point_in_rect(int x, int y, const SDL_Rect *rect) {
    return x >= rect->x && x < rect->x + rect->w &&
           y >= rect->y && y < rect->y + rect->h;
}

static SDL_Rect preset_list_rect(void) {
    SDL_Rect rect = {
        .x = PRESET_LIST_MARGIN_X,
        .y = PRESET_LIST_MARGIN_Y,
        .w = PRESET_LIST_WIDTH,
        .h = PRESET_LIST_HEIGHT
    };
    return rect;
}

static float preset_total_height(const SceneMenuInteraction *ctx) {
    if (!ctx) return (float)PRESET_ROW_HEIGHT;
    int count = preset_library_count(ctx->library);
    if (count < 0) count = 0;
    return (float)count * (float)PRESET_ROW_HEIGHT +
           ADD_ENTRY_GAP +
           (float)PRESET_ROW_HEIGHT;
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

static void update_scrollbar(SceneMenuInteraction *ctx) {
    if (!ctx) return;
    ScrollBar *bar = &ctx->scrollbar;
    scrollbar_set_track(bar, (SDL_Rect){
        ctx->list_rect.x + ctx->list_rect.w - SCROLLBAR_WIDTH,
        ctx->list_rect.y,
        SCROLLBAR_WIDTH,
        ctx->list_rect.h
    });
    scrollbar_set_content(bar,
                          preset_total_height(ctx),
                          (float)ctx->list_rect.h);
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

static void draw_toggle(SDL_Renderer *renderer,
                        TTF_Font *font,
                        const SDL_Rect *rect,
                        const char *label,
                        bool enabled) {
    if (!renderer || !rect || !label || !font) return;
    SDL_Color fill = enabled ? COLOR_ACCENT : COLOR_PANEL;
    SDL_SetRenderDrawColor(renderer, fill.r, fill.g, fill.b, 255);
    SDL_RenderFillRect(renderer, rect);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 160);
    SDL_RenderDrawRect(renderer, rect);
    SDL_Color text_color = enabled ? COLOR_TEXT : COLOR_TEXT_DIM;
    draw_text(renderer, font, label, rect->x + 10, rect->y + 10, text_color);
}
static int preset_index_from_point(SceneMenuInteraction *ctx,
                                   int x,
                                   int y,
                                   bool *is_add_entry) {
    if (!ctx) return -1;
    if (!point_in_rect(x, y, &ctx->list_rect)) {
        if (is_add_entry) *is_add_entry = false;
        return -1;
    }

    float local_y = (float)(y - ctx->list_rect.y) + scrollbar_offset(&ctx->scrollbar);
    if (local_y < 0.0f) local_y = 0.0f;
    int count = preset_library_count(ctx->library);
    float add_start = (float)count * (float)PRESET_ROW_HEIGHT + (float)ADD_ENTRY_GAP;
    float add_end = add_start + (float)PRESET_ROW_HEIGHT;
    if (local_y >= add_start) {
        bool inside_add = local_y < add_end;
        if (is_add_entry) *is_add_entry = inside_add;
        return inside_add ? count : -1;
    }

    int row = (int)(local_y / (float)PRESET_ROW_HEIGHT);
    if (row < 0 || row >= count) {
        if (is_add_entry) *is_add_entry = false;
        return -1;
    }
    if (is_add_entry) *is_add_entry = false;
    return row;
}

static bool preset_row_rect(SceneMenuInteraction *ctx,
                            int row_index,
                            bool is_add_entry,
                            SDL_Rect *out_rect) {
    if (!ctx || !out_rect) return false;
    float offset = scrollbar_offset(&ctx->scrollbar);
    float y = (float)ctx->list_rect.y +
              (float)row_index * (float)PRESET_ROW_HEIGHT -
              offset;
    if (is_add_entry) {
        y += (float)ADD_ENTRY_GAP;
    }
    SDL_Rect rect = {
        .x = ctx->list_rect.x + 8,
        .y = (int)(y + 6.0f),
        .w = ctx->list_rect.w - SCROLLBAR_WIDTH - 16,
        .h = PRESET_ROW_HEIGHT - 12
    };
    *out_rect = rect;
    return rect.y + rect.h >= ctx->list_rect.y &&
           rect.y <= ctx->list_rect.y + ctx->list_rect.h;
}

static SDL_Rect preset_delete_button_rect(const SDL_Rect *row_rect) {
    SDL_Rect rect = {
        .x = row_rect->x + row_rect->w - 34,
        .y = row_rect->y + 8,
        .w = 26,
        .h = row_rect->h - 16
    };
    return rect;
}

static SDL_Color lighten_color(SDL_Color color, float factor) {
    if (factor < 0.0f) factor = 0.0f;
    if (factor > 1.0f) factor = 1.0f;
    SDL_Color result = color;
    result.r = (Uint8)(color.r + (Uint8)((255 - color.r) * factor));
    result.g = (Uint8)(color.g + (Uint8)((255 - color.g) * factor));
    result.b = (Uint8)(color.b + (Uint8)((255 - color.b) * factor));
    return result;
}

static void draw_preset_list(SceneMenuInteraction *ctx) {
    if (!ctx || !ctx->renderer) return;
    SDL_Renderer *renderer = ctx->renderer;
    SDL_Rect panel = ctx->list_rect;
    draw_panel(renderer, &panel);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 200);
    SDL_RenderDrawRect(renderer, &panel);

    SDL_Rect clip = {
        panel.x + 2,
        panel.y + 2,
        panel.w - 4,
        panel.h - 4
    };
    SDL_RenderSetClipRect(renderer, &clip);

    int count = preset_library_count(ctx->library);
    int total_rows = count + 1;

    for (int row = 0; row < total_rows; ++row) {
        bool is_add_entry = (row == count);
        SDL_Rect row_rect;
        if (!preset_row_rect(ctx, row, is_add_entry, &row_rect)) continue;
        bool selected = (!is_add_entry &&
                         row == ctx->selection->custom_slot_index);
        bool hovered = (!is_add_entry && row == ctx->hover_slot) ||
                       (is_add_entry && ctx->hover_add_entry);

        if (!is_add_entry) {
            SDL_Color row_color = COLOR_PANEL;
            if (selected) row_color = COLOR_ACCENT;
            else if (hovered) row_color = lighten_color(COLOR_PANEL, 0.25f);

            SDL_SetRenderDrawColor(renderer, row_color.r, row_color.g, row_color.b, 255);
            SDL_RenderFillRect(renderer, &row_rect);
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 140);
            SDL_RenderDrawRect(renderer, &row_rect);

            SDL_Rect label_rect = {
                .x = row_rect.x + 12,
                .y = row_rect.y + 10,
                .w = row_rect.w - 60,
                .h = row_rect.h - 20
            };

            if (ctx->rename_input.active && ctx->renaming_slot == row) {
                draw_text_input(renderer, ctx->font, &label_rect, &ctx->rename_input);
            } else {
                const CustomPresetSlot *slot =
                    preset_library_get_slot_const(ctx->library, row);
                const char *label = (slot && slot->name[0] != '\0')
                                        ? slot->name
                                        : "Untitled Preset";
                draw_text(renderer, ctx->font, label,
                          label_rect.x, label_rect.y, COLOR_TEXT);
            }

            SDL_Rect delete_rect = preset_delete_button_rect(&row_rect);
            SDL_Color del_color = hovered ? (SDL_Color){200, 110, 110, 255}
                                          : COLOR_TEXT_DIM;
            SDL_SetRenderDrawColor(renderer, del_color.r, del_color.g, del_color.b, del_color.a);
            SDL_RenderDrawRect(renderer, &delete_rect);
            draw_text(renderer, ctx->font_small, "x",
                      delete_rect.x + 6,
                      delete_rect.y + 4,
                      del_color);
        } else {
            SDL_Color text_color = hovered ? COLOR_TEXT : COLOR_TEXT_DIM;
            draw_text(renderer,
                      ctx->font_small,
                      "+ Click to add preset",
                      row_rect.x + 12,
                      row_rect.y + (row_rect.h / 2) - 8,
                      text_color);
        }
    }

    SDL_RenderSetClipRect(renderer, NULL);
    scrollbar_draw(renderer, &ctx->scrollbar);
}

static void select_custom(SceneMenuInteraction *ctx, int slot_index) {
    if (!ctx || !ctx->library || !ctx->selection) return;
    int count = preset_library_count(ctx->library);
    if (count <= 0) {
        ctx->selection->custom_slot_index = -1;
        ctx->active_preset = ctx->preset_output;
        return;
    }
    if (slot_index < 0) slot_index = 0;
    if (slot_index >= count) slot_index = count - 1;

    CustomPresetSlot *slot = preset_library_get_slot(ctx->library, slot_index);
    if (!slot) {
        ctx->active_preset = ctx->preset_output;
        return;
    }
    ctx->selection->custom_slot_index = slot_index;
    ctx->library->active_slot = slot_index;
    ctx->active_preset = &slot->preset;
}

static void scroll_to_row(SceneMenuInteraction *ctx, int row_index) {
    if (!ctx) return;
    float row_top = (float)row_index * (float)PRESET_ROW_HEIGHT;
    float row_bottom = row_top + (float)PRESET_ROW_HEIGHT;
    float offset = scrollbar_offset(&ctx->scrollbar);
    float view = (float)ctx->list_rect.h;

    if (row_top < offset) {
        scrollbar_set_offset(&ctx->scrollbar, row_top);
    } else if (row_bottom > offset + view) {
        scrollbar_set_offset(&ctx->scrollbar, row_bottom - view);
    }
}

static void add_new_preset(SceneMenuInteraction *ctx) {
    if (!ctx || !ctx->library) return;
    int new_index = preset_library_count(ctx->library);
    char default_name[CUSTOM_PRESET_NAME_MAX];
    snprintf(default_name, sizeof(default_name), "Custom Preset %d", new_index + 1);
    CustomPresetSlot *slot = preset_library_add_slot(ctx->library,
                                                     default_name,
                                                     NULL);
    if (!slot) return;
    slot->preset.name = slot->name;
    slot->preset.is_custom = true;
    slot->preset.emitter_count = 0;
    slot->occupied = true;
    select_custom(ctx, new_index);
    scroll_to_row(ctx, new_index);
    begin_rename(ctx, new_index);
}

static void delete_preset(SceneMenuInteraction *ctx, int slot_index) {
    if (!ctx || !ctx->library) return;
    if (ctx->rename_input.active) {
        finish_rename(ctx, false);
    }
    if (!preset_library_remove_slot(ctx->library, slot_index)) {
        return;
    }
    int count = preset_library_count(ctx->library);
    if (count == 0) {
        ctx->selection->custom_slot_index = -1;
        ctx->active_preset = ctx->preset_output;
        return;
    }
    int new_index = ctx->selection->custom_slot_index;
    if (new_index >= count) new_index = count - 1;
    select_custom(ctx, new_index);
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

    if (ctx->scrollbar_dragging) {
        ctx->scrollbar_dragging = false;
        scrollbar_handle_pointer_up(&ctx->scrollbar);
        return;
    }

    int x = state->x;
    int y = state->y;
    Uint32 now = SDL_GetTicks();

    if (ctx->rename_input.active && ctx->renaming_slot >= 0) {
        SDL_Rect rename_rect;
        bool visible = preset_row_rect(ctx,
                                       ctx->renaming_slot,
                                       false,
                                       &rename_rect);
        if (!visible || !point_in_rect(x, y, &rename_rect)) {
            finish_rename(ctx, false);
        }
    }

    if (point_in_rect(x, y, &ctx->start_button.rect)) {
        if (ctx->preset_output && ctx->active_preset) {
            CustomPresetSlot *start_slot = preset_library_get_slot(
                ctx->library, ctx->selection->custom_slot_index);
            if (start_slot) start_slot->occupied = true;
            *ctx->preset_output = *ctx->active_preset;
            *ctx->start_requested = true;
            *ctx->running = false;
        }
        return;
    }

    if (point_in_rect(x, y, &ctx->edit_button.rect)) {
        CustomPresetSlot *slot = preset_library_get_slot(
            ctx->library, ctx->selection->custom_slot_index);
        if (!slot) return;
        FluidScenePreset *target = &slot->preset;
        char *name_buffer = slot->name;
        size_t name_capacity = sizeof(slot->name);
        if (scene_editor_run(ctx->window,
                             ctx->renderer,
                             ctx->font,
                             ctx->font_small,
                             ctx->cfg,
                             target,
                             ctx->context_mgr,
                             name_buffer,
                             name_capacity)) {
            slot->preset = *target;
            slot->preset.name = slot->name;
            slot->preset.is_custom = true;
            slot->occupied = true;
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

    if (point_in_rect(x, y, &ctx->volume_toggle_rect)) {
        ctx->cfg->save_volume_frames = !ctx->cfg->save_volume_frames;
        return;
    }

    if (point_in_rect(x, y, &ctx->render_toggle_rect)) {
        ctx->cfg->save_render_frames = !ctx->cfg->save_render_frames;
        return;
    }

    bool is_add = false;
    int row = preset_index_from_point(ctx, x, y, &is_add);
    if (row < 0) return;

    if (is_add) {
        add_new_preset(ctx);
        return;
    }

    int count = preset_library_count(ctx->library);
    if (row >= count) return;

    SDL_Rect row_rect;
    if (!preset_row_rect(ctx, row, false, &row_rect)) {
        // Clicked outside visible rows due to rounding.
        return;
    }

    SDL_Rect delete_rect = preset_delete_button_rect(&row_rect);
    if (point_in_rect(x, y, &delete_rect)) {
        delete_preset(ctx, row);
        return;
    }

    bool double_click = (ctx->last_clicked_slot == row) &&
                        (now - ctx->last_click_ticks <= DOUBLE_CLICK_MS);
    ctx->last_clicked_slot = row;
    ctx->last_click_ticks = now;

    select_custom(ctx, row);
    scroll_to_row(ctx, row);
    if (double_click) {
        begin_rename(ctx, row);
    }
}

static void menu_pointer_down(void *user, const InputPointerState *state) {
    SceneMenuInteraction *ctx = (SceneMenuInteraction *)user;
    if (!ctx || !state) return;
    if (scrollbar_handle_pointer_down(&ctx->scrollbar, state->x, state->y)) {
        ctx->scrollbar_dragging = true;
    }
}

static void menu_pointer_move(void *user, const InputPointerState *state) {
    SceneMenuInteraction *ctx = (SceneMenuInteraction *)user;
    if (!ctx || !state) return;
    if (ctx->scrollbar_dragging) {
        scrollbar_handle_pointer_move(&ctx->scrollbar, state->x, state->y);
        return;
    }
    bool is_add = false;
    int row = preset_index_from_point(ctx, state->x, state->y, &is_add);
    int count = preset_library_count(ctx->library);
    if (row >= 0 && row < count) {
        ctx->hover_slot = row;
        ctx->hover_add_entry = false;
    } else {
        ctx->hover_slot = -1;
        ctx->hover_add_entry = (is_add && row == count);
    }
}

static void menu_wheel(void *user, const InputWheelState *wheel) {
    SceneMenuInteraction *ctx = (SceneMenuInteraction *)user;
    if (!ctx || !wheel) return;
    scrollbar_handle_wheel(&ctx->scrollbar, wheel->y);
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
    int slot_count = preset_library_count(library);
    if (slot_count <= 0) {
        current_selection.custom_slot_index = -1;
        library->active_slot = -1;
    } else {
        if (current_selection.custom_slot_index < 0 ||
            current_selection.custom_slot_index >= slot_count) {
            int active = library->active_slot;
            if (active < 0 || active >= slot_count) active = 0;
            current_selection.custom_slot_index = active;
        }
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
        .active_preset = preset_state,
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
        .scrollbar_dragging = false,
        .hover_slot = -1,
        .hover_add_entry = false
    };

    ctx.list_rect = preset_list_rect();
    scrollbar_init(&ctx.scrollbar);
    update_scrollbar(&ctx);

    InputContextManager context_mgr;
    input_context_manager_init(&context_mgr);
    ctx.context_mgr = &context_mgr;

    select_custom(&ctx, current_selection.custom_slot_index);
    *selection = current_selection;

    InputContext menu_ctx = {
        .on_pointer_down = menu_pointer_down,
        .on_pointer_up = menu_pointer_up,
        .on_pointer_move = menu_pointer_move,
        .on_wheel = menu_wheel,
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
        update_scrollbar(&ctx);

        SDL_SetRenderDrawColor(renderer, COLOR_BG.r, COLOR_BG.g, COLOR_BG.b, COLOR_BG.a);
        SDL_RenderClear(renderer);

        if (font_title) {
            draw_text(renderer, font_title, "Custom Presets", PRESET_LIST_MARGIN_X, 40, COLOR_TEXT);
        }
        draw_preset_list(&ctx);

        SDL_Rect config_panel = {420, 120, 360, 320};
        draw_panel(renderer, &config_panel);
        draw_text(renderer, font_body, "Grid Resolution", config_panel.x + 12, config_panel.y + 12, COLOR_TEXT_DIM);
        char buffer[64];
        snprintf(buffer, sizeof(buffer), "%dx%d cells", cfg->grid_w, cfg->grid_h);
        draw_text(renderer, font_body, buffer, config_panel.x + 12, config_panel.y + 42, COLOR_TEXT);
        draw_text(renderer, font_body, "Active Preset", config_panel.x + 12, config_panel.y + 86, COLOR_TEXT_DIM);
        const char *preset_name = (ctx.active_preset && ctx.active_preset->name && ctx.active_preset->name[0])
                                      ? ctx.active_preset->name
                                      : "Preset";
        char preset_label[128];
        snprintf(preset_label,
                 sizeof(preset_label),
                 "%s (%s)",
                 preset_name,
                 (ctx.active_preset && ctx.active_preset->is_custom) ? "custom" : "built-in");
        draw_text(renderer, font_body, preset_label, config_panel.x + 12, config_panel.y + 116, COLOR_TEXT);

        ctx.grid_dec_button.rect = (SDL_Rect){config_panel.x + 240, config_panel.y + 40, 40, 40};
        ctx.grid_inc_button.rect = (SDL_Rect){config_panel.x + 290, config_panel.y + 40, 40, 40};
        draw_button(renderer, &ctx.grid_dec_button.rect, ctx.grid_dec_button.label, font_body, false);
        draw_button(renderer, &ctx.grid_inc_button.rect, ctx.grid_inc_button.label, font_body, false);

        ctx.volume_toggle_rect = (SDL_Rect){config_panel.x + 12,
                                            config_panel.y + 160,
                                            config_panel.w - 24,
                                            36};
        ctx.render_toggle_rect = (SDL_Rect){config_panel.x + 12,
                                            config_panel.y + 210,
                                            config_panel.w - 24,
                                            36};

        TTF_Font *toggle_font = ctx.font_small ? ctx.font_small : ctx.font;
        if (!toggle_font) toggle_font = font_body;
        draw_toggle(renderer, toggle_font, &ctx.volume_toggle_rect,
                    "Save Volume Frames", ctx.cfg->save_volume_frames);
        draw_toggle(renderer, toggle_font, &ctx.render_toggle_rect,
                    "Save Render Frames", ctx.cfg->save_render_frames);

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
