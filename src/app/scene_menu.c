#include "app/scene_menu.h"

#include "font_paths.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "app/editor/scene_editor.h"
#include "app/quality_profiles.h"
#include "app/scene_controller.h"
#include "input/input.h"
#include "ui/text_input.h"
#include "ui/scrollbar.h"
#include "geo/shape_library.h"

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
    const ShapeAssetLibrary *shape_library;
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
    MenuButton quality_prev_button;
    MenuButton quality_next_button;
    MenuButton headless_toggle_button;
    MenuButton mode_toggle_button;
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
    int quality_index;
    bool headless_pending;
    bool headless_running;
    char status_text[128];
    bool status_visible;
    bool status_wait_ack;
    bool headless_run_requested;
    TextInputField headless_frames_input;
    bool editing_headless_frames;
    SDL_Rect headless_frames_rect;
    TextInputField viscosity_input;
    bool editing_viscosity;
    SDL_Rect viscosity_rect;
    TextInputField inflow_input;
    bool editing_inflow;
    SDL_Rect inflow_rect;
    Uint32 last_headless_click_ticks;
    Uint32 last_viscosity_click_ticks;
    Uint32 last_inflow_click_ticks;
    SimulationMode active_mode;
} SceneMenuInteraction;

static void begin_rename(SceneMenuInteraction *ctx, int slot_index);
static void finish_rename(SceneMenuInteraction *ctx, bool apply);
static void clamp_grid_size(AppConfig *cfg);
static void set_status(SceneMenuInteraction *ctx, const char *text, bool wait_ack);
static void clear_status(SceneMenuInteraction *ctx);
static void begin_headless_frames_edit(SceneMenuInteraction *ctx);
static void finish_headless_frames_edit(SceneMenuInteraction *ctx, bool apply);
static void begin_viscosity_edit(SceneMenuInteraction *ctx);
static void finish_viscosity_edit(SceneMenuInteraction *ctx, bool apply);
static void begin_inflow_edit(SceneMenuInteraction *ctx);
static void finish_inflow_edit(SceneMenuInteraction *ctx, bool apply);
static void begin_viscosity_edit(SceneMenuInteraction *ctx);
static void finish_viscosity_edit(SceneMenuInteraction *ctx, bool apply);

static SDL_Color COLOR_BG       = {18, 18, 22, 255};
static SDL_Color COLOR_PANEL    = {32, 36, 40, 255};
static SDL_Color COLOR_TEXT     = {245, 247, 250, 255};
static SDL_Color COLOR_TEXT_DIM = {210, 216, 226, 255};
static SDL_Color COLOR_ACCENT   = {90, 170, 255, 255};
static SDL_Color COLOR_BUTTON_BG = {45, 50, 58, 255};
static SDL_Color COLOR_BUTTON_BG_ACTIVE = {60, 120, 200, 255};

static int quality_count(void) {
    return quality_profile_count();
}

static void apply_quality_profile_index(SceneMenuInteraction *ctx, int index) {
    if (!ctx || !ctx->cfg) return;
    quality_profile_apply(ctx->cfg, index);
    clamp_grid_size(ctx->cfg);
    ctx->quality_index = (index >= 0 && index < quality_count()) ? index : -1;
    if (ctx->selection) {
        ctx->selection->quality_index = ctx->quality_index;
    }
}

static void set_custom_quality(SceneMenuInteraction *ctx) {
    if (!ctx || !ctx->cfg) return;
    ctx->quality_index = -1;
    ctx->cfg->quality_index = -1;
    if (ctx->selection) {
        ctx->selection->quality_index = -1;
    }
}

static void cycle_quality(SceneMenuInteraction *ctx, int delta) {
    if (!ctx) return;
    int count = quality_count();
    if (count <= 0) return;
    int current = ctx->quality_index;
    if (current < 0) current = 0;
    current = (current + delta + count) % count;
    apply_quality_profile_index(ctx, current);
}

static const char *current_quality_name(const SceneMenuInteraction *ctx) {
    if (!ctx) return "Custom";
    return quality_profile_name(ctx->quality_index);
}

static void run_headless_batch(SceneMenuInteraction *ctx) {
    if (!ctx || !ctx->cfg) return;
    AppConfig cfg_copy = *ctx->cfg;
    if (ctx->quality_index >= 0) {
        quality_profile_apply(&cfg_copy, ctx->quality_index);
    }
    HeadlessOptions opts = {
        .enabled = true,
        .frame_limit = cfg_copy.headless_frame_count,
        .skip_present = cfg_copy.headless_skip_present,
        .ignore_input = false,
        .preserve_sdl_state = true
    };
    const char *output_dir = cfg_copy.headless_output_dir[0]
                                 ? cfg_copy.headless_output_dir
                                 : "data/snapshots";
    CustomPresetSlot *slot = preset_library_get_slot(ctx->library,
                                                     ctx->selection->custom_slot_index);
    FluidScenePreset preset_copy = slot ? slot->preset : ctx->preview_preset;
    int result = scene_controller_run(&cfg_copy, &preset_copy, ctx->shape_library, output_dir, &opts);
    if (result == 2) {
        set_status(ctx, "Headless run canceled.", true);
    } else {
        set_status(ctx, "Headless run complete.", true);
    }
}

static void set_status(SceneMenuInteraction *ctx, const char *text, bool wait_ack) {
    if (!ctx || !text) return;
    snprintf(ctx->status_text, sizeof(ctx->status_text), "%s", text);
    ctx->status_visible = true;
    ctx->status_wait_ack = wait_ack;
}

static void clear_status(SceneMenuInteraction *ctx) {
    if (!ctx) return;
    ctx->status_text[0] = '\0';
    ctx->status_visible = false;
    ctx->status_wait_ack = false;
}

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

static SimulationMode normalize_sim_mode(SimulationMode mode) {
    if (mode < SIM_MODE_BOX || mode >= SIMULATION_MODE_COUNT) {
        return SIM_MODE_BOX;
    }
    return mode;
}

static FluidSceneDomainType domain_for_mode(SimulationMode mode) {
    return (mode == SIM_MODE_WIND_TUNNEL)
               ? SCENE_DOMAIN_WIND_TUNNEL
               : SCENE_DOMAIN_BOX;
}

static FluidSceneDomainType current_domain(const SceneMenuInteraction *ctx) {
    if (!ctx) return SCENE_DOMAIN_BOX;
    return domain_for_mode(normalize_sim_mode(ctx->active_mode));
}

static const char *mode_label(SimulationMode mode) {
    return (mode == SIM_MODE_WIND_TUNNEL) ? "Wind Tunnel" : "Grid";
}

static int visible_slot_count(const SceneMenuInteraction *ctx) {
    if (!ctx || !ctx->library) return 0;
    FluidSceneDomainType domain = current_domain(ctx);
    int total = preset_library_count(ctx->library);
    int visible = 0;
    for (int i = 0; i < total; ++i) {
        const CustomPresetSlot *slot = preset_library_get_slot_const(ctx->library, i);
        if (slot && slot->preset.domain == domain) {
            ++visible;
        }
    }
    return visible;
}

static int slot_index_from_visible_row(const SceneMenuInteraction *ctx, int row_index) {
    if (!ctx || !ctx->library || row_index < 0) return -1;
    FluidSceneDomainType domain = current_domain(ctx);
    int total = preset_library_count(ctx->library);
    int visible = 0;
    for (int i = 0; i < total; ++i) {
        const CustomPresetSlot *slot = preset_library_get_slot_const(ctx->library, i);
        if (!slot) continue;
        if (slot->preset.domain != domain) continue;
        if (visible == row_index) {
            return i;
        }
        ++visible;
    }
    return -1;
}

static bool slot_matches_current_mode(const SceneMenuInteraction *ctx, int slot_index) {
    if (!ctx || !ctx->library) return false;
    const CustomPresetSlot *slot = preset_library_get_slot_const(ctx->library, slot_index);
    if (!slot) return false;
    return slot->preset.domain == current_domain(ctx);
}

static int find_first_slot_for_mode(const SceneMenuInteraction *ctx, FluidSceneDomainType domain) {
    if (!ctx || !ctx->library) return -1;
    int total = preset_library_count(ctx->library);
    for (int i = 0; i < total; ++i) {
        const CustomPresetSlot *slot = preset_library_get_slot_const(ctx->library, i);
        if (slot && slot->preset.domain == domain) {
            return i;
        }
    }
    return -1;
}

static int visible_row_from_slot(const SceneMenuInteraction *ctx, int slot_index) {
    if (!ctx || !ctx->library) return -1;
    if (slot_index < 0 || slot_index >= preset_library_count(ctx->library)) return -1;
    FluidSceneDomainType domain = current_domain(ctx);
    int visible = 0;
    for (int i = 0; i < preset_library_count(ctx->library); ++i) {
        const CustomPresetSlot *slot = preset_library_get_slot_const(ctx->library, i);
        if (!slot) continue;
        if (slot->preset.domain != domain) continue;
        if (i == slot_index) {
            return visible;
        }
        ++visible;
    }
    return -1;
}

static float preset_total_height(const SceneMenuInteraction *ctx) {
    if (!ctx) return (float)PRESET_ROW_HEIGHT;
    int count = visible_slot_count(ctx);
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
    SDL_Color color = selected ? COLOR_BUTTON_BG_ACTIVE : COLOR_BUTTON_BG;
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
    int count = visible_slot_count(ctx);
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

    int count = visible_slot_count(ctx);
    int total_rows = count + 1;

    for (int row = 0; row < total_rows; ++row) {
        bool is_add_entry = (row == count);
        SDL_Rect row_rect;
        if (!preset_row_rect(ctx, row, is_add_entry, &row_rect)) continue;
        int slot_index = is_add_entry ? -1 : slot_index_from_visible_row(ctx, row);
        bool selected = (!is_add_entry &&
                         slot_index >= 0 &&
                         ctx->selection->custom_slot_index == slot_index);
        bool hovered = (!is_add_entry && slot_index >= 0 && ctx->hover_slot == slot_index) ||
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

            if (ctx->rename_input.active && slot_index >= 0 && ctx->renaming_slot == slot_index) {
                draw_text_input(renderer, ctx->font, &label_rect, &ctx->rename_input);
            } else {
                const CustomPresetSlot *slot =
                    preset_library_get_slot_const(ctx->library, slot_index);
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
            char label[64];
            snprintf(label, sizeof(label), "+ Add %s preset", mode_label(ctx->active_mode));
            draw_text(renderer,
                      ctx->font_small,
                      label,
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
    if (slot_index < 0 || slot_index >= count || !slot_matches_current_mode(ctx, slot_index)) {
        slot_index = find_first_slot_for_mode(ctx, current_domain(ctx));
    }
    if (slot_index < 0 || slot_index >= count) {
        ctx->selection->custom_slot_index = -1;
        ctx->selection->last_mode_slot[ctx->active_mode] = -1;
        ctx->active_preset = ctx->preset_output;
        return;
    }

    CustomPresetSlot *slot = preset_library_get_slot(ctx->library, slot_index);
    if (!slot) {
        ctx->selection->custom_slot_index = -1;
        ctx->selection->last_mode_slot[ctx->active_mode] = -1;
        ctx->active_preset = ctx->preset_output;
        return;
    }
    ctx->selection->custom_slot_index = slot_index;
    ctx->library->active_slot = slot_index;
    ctx->selection->last_mode_slot[ctx->active_mode] = slot_index;
    ctx->active_preset = &slot->preset;
}

static void ensure_slot_for_mode(SceneMenuInteraction *ctx) {
    if (!ctx || !ctx->library) return;
    if (visible_slot_count(ctx) > 0) return;
    char default_name[CUSTOM_PRESET_NAME_MAX];
    int slot_count = preset_library_count(ctx->library);
    snprintf(default_name, sizeof(default_name), "%s Preset %d",
             (ctx->active_mode == SIM_MODE_WIND_TUNNEL) ? "Tunnel" : "Grid",
             slot_count + 1);
    FluidSceneDomainType domain = current_domain(ctx);
    const FluidScenePreset *base = scene_presets_get_default_for_domain(domain);
    CustomPresetSlot *slot = preset_library_add_slot(ctx->library,
                                                     default_name,
                                                     base);
    if (!slot) return;
    slot->preset.name = slot->name;
    slot->preset.is_custom = true;
    slot->preset.domain = domain;
    int new_index = preset_library_count(ctx->library) - 1;
    select_custom(ctx, new_index);
}

static void switch_mode(SceneMenuInteraction *ctx, SimulationMode new_mode) {
    if (!ctx) return;
    SimulationMode normalized = normalize_sim_mode(new_mode);
    if (ctx->active_mode == normalized) return;
    if (ctx->rename_input.active) {
        finish_rename(ctx, false);
    }
    ctx->active_mode = normalized;
    if (ctx->cfg) ctx->cfg->sim_mode = normalized;
    if (ctx->selection) ctx->selection->sim_mode = normalized;
    ensure_slot_for_mode(ctx);
    int preferred = -1;
    if (ctx->selection &&
        normalized >= 0 && normalized < SIMULATION_MODE_COUNT) {
        preferred = ctx->selection->last_mode_slot[normalized];
    }
    select_custom(ctx, preferred);
    scrollbar_set_offset(&ctx->scrollbar, 0.0f);
    update_scrollbar(ctx);
    ctx->hover_slot = -1;
    ctx->hover_add_entry = false;
    ctx->last_clicked_slot = -1;
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

static void scroll_to_slot(SceneMenuInteraction *ctx, int slot_index) {
    if (!ctx) return;
    int row = visible_row_from_slot(ctx, slot_index);
    if (row >= 0) {
        scroll_to_row(ctx, row);
    }
}

static void add_new_preset(SceneMenuInteraction *ctx) {
    if (!ctx || !ctx->library) return;
    char default_name[CUSTOM_PRESET_NAME_MAX];
    int slot_count = preset_library_count(ctx->library);
    snprintf(default_name, sizeof(default_name), "Custom Preset %d", slot_count + 1);
    FluidSceneDomainType domain = current_domain(ctx);
    const FluidScenePreset *base = scene_presets_get_default_for_domain(domain);
    CustomPresetSlot *slot = preset_library_add_slot(ctx->library,
                                                     default_name,
                                                     base);
    if (!slot) return;
    slot->preset.name = slot->name;
    slot->preset.is_custom = true;
    slot->preset.domain = domain;
    slot->occupied = true;
    int new_index = preset_library_count(ctx->library) - 1;
    select_custom(ctx, new_index);
    scroll_to_slot(ctx, new_index);
    begin_rename(ctx, new_index);
}

static void adjust_slot_indices_after_delete(SceneMenuInteraction *ctx, int removed_index) {
    if (!ctx) return;
    if (ctx->selection) {
        for (int mode = 0; mode < SIMULATION_MODE_COUNT; ++mode) {
            int stored = ctx->selection->last_mode_slot[mode];
            if (stored == removed_index) {
                ctx->selection->last_mode_slot[mode] = -1;
            } else if (stored > removed_index) {
                ctx->selection->last_mode_slot[mode] = stored - 1;
            }
        }
        if (ctx->selection->custom_slot_index == removed_index) {
            ctx->selection->custom_slot_index = -1;
        } else if (ctx->selection->custom_slot_index > removed_index) {
            ctx->selection->custom_slot_index--;
        }
    }
    if (ctx->library) {
        if (ctx->library->active_slot == removed_index) {
            ctx->library->active_slot = -1;
        } else if (ctx->library->active_slot > removed_index) {
            ctx->library->active_slot--;
        }
    }
    if (ctx->hover_slot == removed_index) {
        ctx->hover_slot = -1;
    } else if (ctx->hover_slot > removed_index) {
        ctx->hover_slot--;
    }
    if (ctx->last_clicked_slot == removed_index) {
        ctx->last_clicked_slot = -1;
    } else if (ctx->last_clicked_slot > removed_index) {
        ctx->last_clicked_slot--;
    }
}

static void delete_preset(SceneMenuInteraction *ctx, int slot_index) {
    if (!ctx || !ctx->library) return;
    if (ctx->rename_input.active) {
        finish_rename(ctx, false);
    }
    if (!preset_library_remove_slot(ctx->library, slot_index)) {
        return;
    }
    adjust_slot_indices_after_delete(ctx, slot_index);
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

static void begin_headless_frames_edit(SceneMenuInteraction *ctx) {
    if (!ctx) return;
    char buffer[32];
    int frames = ctx->cfg ? ctx->cfg->headless_frame_count : 0;
    if (frames < 0) frames = 0;
    snprintf(buffer, sizeof(buffer), "%d", frames);
    text_input_begin(&ctx->headless_frames_input, buffer, sizeof(buffer) - 1);
    ctx->editing_headless_frames = true;
}

static void finish_headless_frames_edit(SceneMenuInteraction *ctx, bool apply) {
    if (!ctx || !ctx->editing_headless_frames) return;
    if (apply) {
        const char *value = text_input_value(&ctx->headless_frames_input);
        if (value && value[0]) {
            char *end = NULL;
            long frames = strtol(value, &end, 10);
            if (end != value) {
                if (frames < 0) frames = 0;
                if (ctx->cfg) ctx->cfg->headless_frame_count = (int)frames;
                if (ctx->selection) ctx->selection->headless_frame_count = (int)frames;
            }
        }
    }
    text_input_end(&ctx->headless_frames_input);
    ctx->editing_headless_frames = false;
}

static void begin_viscosity_edit(SceneMenuInteraction *ctx) {
    if (!ctx || !ctx->cfg) return;
    char buffer[32];
    float v = ctx->cfg ? ctx->cfg->velocity_damping : 0.0f;
    if (v < 0.0f) v = 0.0f;
    snprintf(buffer, sizeof(buffer), "%.8f", v);
    text_input_begin(&ctx->viscosity_input, buffer, sizeof(buffer) - 1);
    ctx->editing_viscosity = true;
}

static void finish_viscosity_edit(SceneMenuInteraction *ctx, bool apply) {
    if (!ctx || !ctx->editing_viscosity) return;
    if (apply && ctx->cfg) {
        const char *value = text_input_value(&ctx->viscosity_input);
        if (value && value[0]) {
            char *end = NULL;
            double v = strtod(value, &end);
            if (end != value && isfinite(v)) {
                if (v < 0.0) v = 0.0;
                if (v > 0.1) v = 0.1;
                ctx->cfg->velocity_damping = (float)v;
            }
        }
    }
    text_input_end(&ctx->viscosity_input);
    ctx->editing_viscosity = false;
}

static void begin_inflow_edit(SceneMenuInteraction *ctx) {
    if (!ctx || !ctx->cfg) return;
    char buffer[32];
    float v = ctx->cfg ? ctx->cfg->tunnel_inflow_speed : 0.0f;
    snprintf(buffer, sizeof(buffer), "%.6f", v);
    text_input_begin(&ctx->inflow_input, buffer, sizeof(buffer) - 1);
    ctx->editing_inflow = true;
}

static void finish_inflow_edit(SceneMenuInteraction *ctx, bool apply) {
    if (!ctx || !ctx->editing_inflow) return;
    if (apply && ctx->cfg) {
        const char *value = text_input_value(&ctx->inflow_input);
        if (value && value[0]) {
            char *end = NULL;
            double v = strtod(value, &end);
            if (end != value && isfinite(v)) {
                if (v < 0.0) v = 0.0;
                if (v > 500.0) v = 500.0;
                ctx->cfg->tunnel_inflow_speed = (float)v;
                if (ctx->selection) {
                    ctx->selection->tunnel_inflow_speed = (float)v;
                }
            }
        }
    }
    text_input_end(&ctx->inflow_input);
    ctx->editing_inflow = false;
}

static void menu_pointer_up(void *user, const InputPointerState *state) {
    SceneMenuInteraction *ctx = (SceneMenuInteraction *)user;
    if (!ctx || !state) return;
    if (ctx->headless_running) return;

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

    if (point_in_rect(x, y, &ctx->headless_toggle_button.rect)) {
        ctx->cfg->headless_enabled = !ctx->cfg->headless_enabled;
        return;
    }

    if (point_in_rect(x, y, &ctx->inflow_rect)) {
        Uint32 now = SDL_GetTicks();
        bool double_click = (now - ctx->last_inflow_click_ticks) <= DOUBLE_CLICK_MS;
        ctx->last_inflow_click_ticks = now;
        if (double_click) {
            begin_inflow_edit(ctx);
        }
        return;
    } else if (ctx->editing_inflow) {
        finish_inflow_edit(ctx, true);
    }

    if (point_in_rect(x, y, &ctx->viscosity_rect)) {
        Uint32 now = SDL_GetTicks();
        bool double_click = (now - ctx->last_viscosity_click_ticks) <= DOUBLE_CLICK_MS;
        ctx->last_viscosity_click_ticks = now;
        if (double_click) {
            begin_viscosity_edit(ctx);
        }
        return;
    } else if (ctx->editing_viscosity) {
        finish_viscosity_edit(ctx, true);
    }

    if (point_in_rect(x, y, &ctx->headless_frames_rect)) {
        Uint32 now = SDL_GetTicks();
        bool double_click = (now - ctx->last_headless_click_ticks) <= DOUBLE_CLICK_MS;
        ctx->last_headless_click_ticks = now;
        if (double_click) {
            begin_headless_frames_edit(ctx);
        }
        return;
    } else if (ctx->editing_headless_frames) {
        finish_headless_frames_edit(ctx, true);
    }

    if (point_in_rect(x, y, &ctx->start_button.rect)) {
        if (ctx->rename_input.active) {
            finish_rename(ctx, true);
        }
        if (ctx->editing_headless_frames) {
            finish_headless_frames_edit(ctx, true);
        }
        if (ctx->editing_inflow) {
            finish_inflow_edit(ctx, true);
        }
        if (ctx->editing_viscosity) {
            finish_viscosity_edit(ctx, true);
        }
        if (ctx->cfg->headless_enabled) {
            ctx->headless_pending = true;
            set_status(ctx, "Headless run queued...", false);
            return;
        }
        if (ctx->preset_output && ctx->active_preset) {
            CustomPresetSlot *start_slot = preset_library_get_slot(
                ctx->library, ctx->selection->custom_slot_index);
            if (start_slot) start_slot->occupied = true;
            *ctx->preset_output = *ctx->active_preset;
            ctx->selection->headless_frame_count = ctx->cfg ? ctx->cfg->headless_frame_count : ctx->selection->headless_frame_count;
            *ctx->start_requested = true;
            *ctx->running = false;
        }
        return;
    }

    if (point_in_rect(x, y, &ctx->edit_button.rect)) {
        if (ctx->rename_input.active) {
            finish_rename(ctx, true);
        }
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
                             ctx->shape_library,
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
        set_custom_quality(ctx);
        return;
    }

    if (point_in_rect(x, y, &ctx->grid_inc_button.rect)) {
        ctx->cfg->grid_w = ctx->cfg->grid_h =
            (ctx->cfg->grid_w < 512) ? ctx->cfg->grid_w + 32 : 512;
        set_custom_quality(ctx);
        return;
    }

    if (point_in_rect(x, y, &ctx->quality_prev_button.rect)) {
        cycle_quality(ctx, -1);
        return;
    }
    if (point_in_rect(x, y, &ctx->quality_next_button.rect)) {
        cycle_quality(ctx, 1);
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

    if (point_in_rect(x, y, &ctx->mode_toggle_button.rect)) {
        SimulationMode new_mode = (ctx->active_mode == SIM_MODE_BOX)
                                      ? SIM_MODE_WIND_TUNNEL
                                      : SIM_MODE_BOX;
        switch_mode(ctx, new_mode);
        return;
    }

    SDL_Rect frames_rect = ctx->headless_frames_rect;
    bool in_frames_rect = point_in_rect(x, y, &frames_rect);
    if (!in_frames_rect && ctx->editing_headless_frames) {
        finish_headless_frames_edit(ctx, true);
    }
    SDL_Rect inflow_rect = ctx->inflow_rect;
    bool in_inflow_rect = point_in_rect(x, y, &inflow_rect);
    if (!in_inflow_rect && ctx->editing_inflow) {
        finish_inflow_edit(ctx, true);
    }
    SDL_Rect viscosity_rect = ctx->viscosity_rect;
    bool in_viscosity_rect = point_in_rect(x, y, &viscosity_rect);
    if (!in_viscosity_rect && ctx->editing_viscosity) {
        finish_viscosity_edit(ctx, true);
    }

    bool is_add = false;
    int row = preset_index_from_point(ctx, x, y, &is_add);
    if (row < 0) return;

    if (is_add) {
        add_new_preset(ctx);
        return;
    }

    int slot_index = slot_index_from_visible_row(ctx, row);
    if (slot_index < 0) return;

    SDL_Rect row_rect;
    if (!preset_row_rect(ctx, row, false, &row_rect)) {
        // Clicked outside visible rows due to rounding.
        return;
    }

    SDL_Rect delete_rect = preset_delete_button_rect(&row_rect);
    if (point_in_rect(x, y, &delete_rect)) {
        delete_preset(ctx, slot_index);
        return;
    }

    bool double_click = (ctx->last_clicked_slot == slot_index) &&
                        (now - ctx->last_click_ticks <= DOUBLE_CLICK_MS);
    ctx->last_clicked_slot = slot_index;
    ctx->last_click_ticks = now;

    select_custom(ctx, slot_index);
    scroll_to_slot(ctx, slot_index);
    if (double_click) {
        begin_rename(ctx, slot_index);
    }
}

static void menu_pointer_down(void *user, const InputPointerState *state) {
    SceneMenuInteraction *ctx = (SceneMenuInteraction *)user;
    if (!ctx || !state) return;
    if (ctx->headless_running) return;
    if (ctx->status_wait_ack && ctx->status_visible) {
        clear_status(ctx);
    }
    if (scrollbar_handle_pointer_down(&ctx->scrollbar, state->x, state->y)) {
        ctx->scrollbar_dragging = true;
    }
}

static void menu_pointer_move(void *user, const InputPointerState *state) {
    SceneMenuInteraction *ctx = (SceneMenuInteraction *)user;
    if (!ctx || !state) return;
    if (ctx->headless_running) return;
    if (ctx->scrollbar_dragging) {
        scrollbar_handle_pointer_move(&ctx->scrollbar, state->x, state->y);
        return;
    }
    bool is_add = false;
    int row = preset_index_from_point(ctx, state->x, state->y, &is_add);
    int visible_count = visible_slot_count(ctx);
    if (!is_add && row >= 0 && row < visible_count) {
        ctx->hover_slot = slot_index_from_visible_row(ctx, row);
        ctx->hover_add_entry = false;
    } else {
        ctx->hover_slot = -1;
        ctx->hover_add_entry = (is_add && row == visible_count);
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
    if (ctx->headless_running) {
        if (key == SDLK_ESCAPE) {
            ctx->headless_run_requested = false;
            ctx->headless_running = false;
            set_status(ctx, "Headless run canceled.", true);
        }
        return;
    }

    if (ctx->rename_input.active) {
        if (key == SDLK_RETURN || key == SDLK_KP_ENTER) {
            finish_rename(ctx, true);
        } else if (key == SDLK_ESCAPE) {
            finish_rename(ctx, false);
        } else {
            text_input_handle_key(&ctx->rename_input, key);
        }
        return;
    }

    if (ctx->editing_headless_frames) {
        if (key == SDLK_RETURN || key == SDLK_KP_ENTER) {
            finish_headless_frames_edit(ctx, true);
        } else if (key == SDLK_ESCAPE) {
            finish_headless_frames_edit(ctx, false);
        } else {
            text_input_handle_key(&ctx->headless_frames_input, key);
        }
        return;
    }

    if (ctx->editing_inflow) {
        if (key == SDLK_RETURN || key == SDLK_KP_ENTER) {
            finish_inflow_edit(ctx, true);
        } else if (key == SDLK_ESCAPE) {
            finish_inflow_edit(ctx, false);
        } else {
            text_input_handle_key(&ctx->inflow_input, key);
        }
        return;
    }

    if (ctx->editing_viscosity) {
        if (key == SDLK_RETURN || key == SDLK_KP_ENTER) {
            finish_viscosity_edit(ctx, true);
        } else if (key == SDLK_ESCAPE) {
            finish_viscosity_edit(ctx, false);
        } else {
            text_input_handle_key(&ctx->viscosity_input, key);
        }
        return;
    }
}

static void menu_text_input(void *user, const char *text) {
    SceneMenuInteraction *ctx = (SceneMenuInteraction *)user;
    if (!ctx || !text) return;
    if (ctx->headless_running) return;
    if (ctx->rename_input.active) {
        text_input_handle_text(&ctx->rename_input, text);
        return;
    }
    if (ctx->editing_headless_frames) {
        text_input_handle_text(&ctx->headless_frames_input, text);
        return;
    }
    if (ctx->editing_inflow) {
        text_input_handle_text(&ctx->inflow_input, text);
        return;
    }
    if (ctx->editing_viscosity) {
        text_input_handle_text(&ctx->viscosity_input, text);
        return;
    }
}

bool scene_menu_run(AppConfig *cfg,
                    FluidScenePreset *preset_state,
                    SceneMenuSelection *selection,
                    CustomPresetLibrary *library,
                    const ShapeAssetLibrary *shape_library) {
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

	TTF_Font *font_title = TTF_OpenFont(FONT_TITLE_PATH_1, 32);
	if (!font_title) {
	    font_title = TTF_OpenFont(FONT_TITLE_PATH_2, 32);
	}
	if (!font_title) {
	    fprintf(stderr, "Failed to open title font: %s\n", TTF_GetError());
	}
	
	TTF_Font *font_body = TTF_OpenFont(FONT_BODY_PATH_1, 22);
	if (!font_body) {
	    font_body = TTF_OpenFont(FONT_BODY_PATH_2, 22);
	}
	if (!font_body) {
	    fprintf(stderr, "Failed to open body font: %s\n", TTF_GetError());
	}
	
	TTF_Font *font_small = TTF_OpenFont(FONT_BODY_PATH_1, 18);
	if (!font_small) {
	    font_small = TTF_OpenFont(FONT_BODY_PATH_2, 18);
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
    for (int mode = 0; mode < SIMULATION_MODE_COUNT; ++mode) {
        int stored = current_selection.last_mode_slot[mode];
        if (stored < 0 || stored >= slot_count) {
            current_selection.last_mode_slot[mode] = -1;
        }
    }
    SimulationMode selection_mode = normalize_sim_mode(cfg->sim_mode);
    current_selection.sim_mode = selection_mode;
    if (selection_mode >= 0 && selection_mode < SIMULATION_MODE_COUNT &&
        current_selection.last_mode_slot[selection_mode] < 0) {
        current_selection.last_mode_slot[selection_mode] = current_selection.custom_slot_index;
    }

    if (current_selection.headless_frame_count > 0) {
        cfg->headless_frame_count = current_selection.headless_frame_count;
    } else {
        current_selection.headless_frame_count = cfg->headless_frame_count;
    }
    if (current_selection.tunnel_inflow_speed > 0.0f) {
        cfg->tunnel_inflow_speed = current_selection.tunnel_inflow_speed;
    } else {
        current_selection.tunnel_inflow_speed = cfg->tunnel_inflow_speed;
    }

    bool run = true;
    bool start_requested = false;

    SceneMenuInteraction ctx = {
        .cfg = cfg,
        .presets = presets,
        .preset_count = preset_count,
        .selection = &current_selection,
        .library = library,
        .shape_library = shape_library,
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
        .quality_prev_button = {.rect = {0, 0, 0, 0}, .label = "<"},
        .quality_next_button = {.rect = {0, 0, 0, 0}, .label = ">"},
        .headless_toggle_button = {.rect = {MENU_WIDTH - 220, MENU_HEIGHT - 130, 180, 40}, .label = "Headless"},
        .mode_toggle_button = {.rect = {MENU_WIDTH - 220, 60, 180, 36}, .label = "Mode"},
        .running = &run,
        .start_requested = &start_requested,
        .context_mgr = NULL,
        .renaming_slot = -1,
        .last_click_ticks = 0,
        .last_clicked_slot = -1,
        .scrollbar_dragging = false,
        .hover_slot = -1,
        .hover_add_entry = false,
        .headless_pending = false,
        .headless_running = false,
        .status_visible = false,
        .status_wait_ack = false,
        .headless_run_requested = false,
        .editing_headless_frames = false,
        .editing_inflow = false,
        .editing_viscosity = false,
        .last_headless_click_ticks = 0,
        .last_inflow_click_ticks = 0,
        .last_viscosity_click_ticks = 0,
        .headless_frames_rect = {0, 0, 0, 0},
        .inflow_rect = {0, 0, 0, 0},
        .viscosity_rect = {0, 0, 0, 0},
        .active_mode = selection_mode
    };

    ctx.list_rect = preset_list_rect();
    scrollbar_init(&ctx.scrollbar);
    update_scrollbar(&ctx);

    InputContextManager context_mgr;
    input_context_manager_init(&context_mgr);
    ctx.context_mgr = &context_mgr;

    ctx.quality_index = current_selection.quality_index;
    if (ctx.quality_index >= 0) {
        apply_quality_profile_index(&ctx, ctx.quality_index);
    } else {
        ctx.quality_index = -1;
        if (ctx.cfg) ctx.cfg->quality_index = -1;
    }

    ensure_slot_for_mode(&ctx);
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
        if (ctx.editing_headless_frames) {
            text_input_update(&ctx.headless_frames_input, dt);
        }
        if (ctx.editing_inflow) {
            text_input_update(&ctx.inflow_input, dt);
        }
        if (ctx.editing_viscosity) {
            text_input_update(&ctx.viscosity_input, dt);
        }

        InputCommands cmds;
        input_poll_events(&cmds, NULL, &context_mgr);
        if (cmds.quit) {
            run = false;
            break;
        }
        clamp_grid_size(cfg);
        update_scrollbar(&ctx);

        if (ctx.headless_pending && !ctx.headless_running) {
            ctx.headless_pending = false;
            ctx.headless_run_requested = true;
            ctx.headless_running = true;
            char msg[128];
            if (ctx.cfg->headless_frame_count <= 0) {
                snprintf(msg, sizeof(msg), "Headless running...");
            } else {
                snprintf(msg, sizeof(msg), "Headless running %d frames...",
                         ctx.cfg->headless_frame_count);
            }
            set_status(&ctx, msg, false);
        }

        SDL_SetRenderDrawColor(renderer, COLOR_BG.r, COLOR_BG.g, COLOR_BG.b, COLOR_BG.a);
        SDL_RenderClear(renderer);

        if (font_title) {
            draw_text(renderer, font_title, "Custom Presets", PRESET_LIST_MARGIN_X, 40, COLOR_TEXT);
        }
        draw_preset_list(&ctx);

        TTF_Font *toggle_font = ctx.font_small ? ctx.font_small : ctx.font;
        if (!toggle_font) toggle_font = font_body;
        ctx.mode_toggle_button.rect.y = 70;
        char mode_text[48];
        snprintf(mode_text, sizeof(mode_text), "Mode: %s", mode_label(ctx.active_mode));
        draw_toggle(renderer,
                    toggle_font,
                    &ctx.mode_toggle_button.rect,
                    mode_text,
                    ctx.active_mode == SIM_MODE_WIND_TUNNEL);

        SDL_Rect config_panel = {420, 120, 360, 320};
        draw_panel(renderer, &config_panel);
        draw_text(renderer, font_body, "Grid Resolution", config_panel.x + 12, config_panel.y + 12, COLOR_TEXT_DIM);
        char buffer[64];
        snprintf(buffer, sizeof(buffer), "%dx%d cells", cfg->grid_w, cfg->grid_h);
        draw_text(renderer, font_body, buffer, config_panel.x + 12, config_panel.y + 42, COLOR_TEXT);
        ctx.grid_dec_button.rect = (SDL_Rect){config_panel.x + 240, config_panel.y + 40, 40, 40};
        ctx.grid_inc_button.rect = (SDL_Rect){config_panel.x + 290, config_panel.y + 40, 40, 40};
        draw_button(renderer, &ctx.grid_dec_button.rect, ctx.grid_dec_button.label, font_body, false);
        draw_button(renderer, &ctx.grid_inc_button.rect, ctx.grid_inc_button.label, font_body, false);

        draw_text(renderer, font_body, "Quality", config_panel.x + 12, config_panel.y + 86, COLOR_TEXT_DIM);
        ctx.quality_prev_button.rect = (SDL_Rect){config_panel.x + 12, config_panel.y + 110, 36, 36};
        ctx.quality_next_button.rect = (SDL_Rect){config_panel.x + config_panel.w - 48, config_panel.y + 110, 36, 36};
        draw_button(renderer, &ctx.quality_prev_button.rect, ctx.quality_prev_button.label, font_body, false);
        draw_button(renderer, &ctx.quality_next_button.rect, ctx.quality_next_button.label, font_body, false);
        const char *quality_name = current_quality_name(&ctx);
        draw_text(renderer,
                  font_body,
                  quality_name,
                  ctx.quality_prev_button.rect.x + ctx.quality_prev_button.rect.w + 8,
                  ctx.quality_prev_button.rect.y + 8,
                  COLOR_TEXT);

        ctx.volume_toggle_rect = (SDL_Rect){config_panel.x + 12,
                                            config_panel.y + 160,
                                            config_panel.w - 24,
                                            36};
        ctx.render_toggle_rect = (SDL_Rect){config_panel.x + 12,
                                            config_panel.y + 210,
                                            config_panel.w - 24,
                                            36};

        draw_toggle(renderer, toggle_font, &ctx.volume_toggle_rect,
                    "Save Volume Frames", ctx.cfg->save_volume_frames);
        draw_toggle(renderer, toggle_font, &ctx.render_toggle_rect,
                    "Save Render Frames", ctx.cfg->save_render_frames);

        SDL_Rect headless_rect = ctx.headless_toggle_button.rect;
        headless_rect.y = ctx.start_button.rect.y - 50;
        headless_rect.x = ctx.start_button.rect.x;
        headless_rect.w = ctx.start_button.rect.w;
        ctx.headless_toggle_button.rect = headless_rect;

        draw_button(renderer, &ctx.headless_toggle_button.rect, ctx.headless_toggle_button.label, font_body,
                    cfg->headless_enabled);
        SDL_Rect frames_rect = {ctx.headless_toggle_button.rect.x,
                                ctx.headless_toggle_button.rect.y - 35,
                                ctx.headless_toggle_button.rect.w,
                                28};
        ctx.headless_frames_rect = frames_rect;
        SDL_SetRenderDrawColor(renderer, COLOR_PANEL.r, COLOR_PANEL.g, COLOR_PANEL.b, 255);
        SDL_RenderFillRect(renderer, &frames_rect);
        SDL_SetRenderDrawColor(renderer, COLOR_ACCENT.r, COLOR_ACCENT.g, COLOR_ACCENT.b, 180);
        SDL_RenderDrawRect(renderer, &frames_rect);

        if (ctx.editing_headless_frames) {
            draw_text_input(renderer, font_body, &frames_rect, &ctx.headless_frames_input);
        } else {
            char frames_label[64];
            snprintf(frames_label, sizeof(frames_label), "Frames: %d", ctx.cfg->headless_frame_count);
            draw_text(renderer, font_body, frames_label, frames_rect.x + 8, frames_rect.y + 6, COLOR_TEXT);
        }

        SDL_Rect viscosity_rect = {frames_rect.x,
                                   frames_rect.y - 35,
                                   frames_rect.w,
                                   frames_rect.h};
        ctx.viscosity_rect = viscosity_rect;
        SDL_SetRenderDrawColor(renderer, COLOR_PANEL.r, COLOR_PANEL.g, COLOR_PANEL.b, 255);
        SDL_RenderFillRect(renderer, &viscosity_rect);
        SDL_SetRenderDrawColor(renderer, COLOR_ACCENT.r, COLOR_ACCENT.g, COLOR_ACCENT.b, 140);
        SDL_RenderDrawRect(renderer, &viscosity_rect);
        if (ctx.editing_viscosity) {
            draw_text_input(renderer, font_small, &viscosity_rect, &ctx.viscosity_input);
        } else {
            char viscosity_label[64];
            snprintf(viscosity_label, sizeof(viscosity_label), "Viscosity: %.6g", ctx.cfg->velocity_damping);
            draw_text(renderer, font_small, viscosity_label, viscosity_rect.x + 8, viscosity_rect.y + 6, COLOR_TEXT);
        }

        SDL_Rect inflow_rect = {viscosity_rect.x,
                                viscosity_rect.y - 35,
                                viscosity_rect.w,
                                viscosity_rect.h};
        ctx.inflow_rect = inflow_rect;
        SDL_SetRenderDrawColor(renderer, COLOR_PANEL.r, COLOR_PANEL.g, COLOR_PANEL.b, 255);
        SDL_RenderFillRect(renderer, &inflow_rect);
        SDL_SetRenderDrawColor(renderer, COLOR_ACCENT.r, COLOR_ACCENT.g, COLOR_ACCENT.b, 120);
        SDL_RenderDrawRect(renderer, &inflow_rect);
        if (ctx.editing_inflow) {
            draw_text_input(renderer, font_small, &inflow_rect, &ctx.inflow_input);
        } else {
            char inflow_label[64];
            snprintf(inflow_label, sizeof(inflow_label), "Inflow: %.3f", ctx.cfg->tunnel_inflow_speed);
            draw_text(renderer, font_small, inflow_label, inflow_rect.x + 8, inflow_rect.y + 6, COLOR_TEXT);
        }
        draw_button(renderer, &ctx.start_button.rect, ctx.start_button.label, font_body, false);
        draw_button(renderer, &ctx.edit_button.rect, ctx.edit_button.label, font_body, false);
        draw_button(renderer, &ctx.quit_button.rect, ctx.quit_button.label, font_body, false);

        if (ctx.status_visible && font_body && ctx.status_text[0]) {
            int text_w = 0;
            int text_h = 0;
            TTF_SizeUTF8(font_body, ctx.status_text, &text_w, &text_h);
            int status_x = ctx.headless_frames_rect.x;
            int max_x = MENU_WIDTH - text_w - 20;
            if (status_x > max_x) status_x = max_x;
            if (status_x < 20) status_x = 20;
            int status_y = ctx.headless_frames_rect.y - text_h - 10;
            if (status_y < 20) status_y = 20;
            draw_text(renderer,
                      font_body,
                      ctx.status_text,
                      status_x,
                      status_y,
                      ctx.status_wait_ack ? COLOR_ACCENT : COLOR_TEXT_DIM);
        }

        SDL_RenderPresent(renderer);

        if (ctx.headless_run_requested) {
            ctx.headless_run_requested = false;
            run_headless_batch(&ctx);
            ctx.headless_running = false;
        }
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
    current_selection.headless_frame_count = ctx.cfg ? ctx.cfg->headless_frame_count : current_selection.headless_frame_count;
    current_selection.tunnel_inflow_speed = ctx.cfg ? ctx.cfg->tunnel_inflow_speed : current_selection.tunnel_inflow_speed;
    *selection = current_selection;

    return start_requested;
}
