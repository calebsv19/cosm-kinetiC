#ifndef MENU_TYPES_H
#define MENU_TYPES_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdbool.h>

#include "app/scene_menu.h"
#include "input/input.h"
#include "ui/text_input.h"
#include "ui/scrollbar.h"
#include "vk_renderer.h"

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
    VkRenderer renderer_storage;
    TTF_Font *font;
    TTF_Font *font_small;
    TTF_Font *font_title;
    VkRendererConfig vk_cfg;
    bool use_shared_device;
    MenuButton start_button;
    MenuButton edit_button;
    MenuButton quit_button;
    MenuButton grid_dec_button;
    MenuButton grid_inc_button;
    MenuButton quality_prev_button;
    MenuButton quality_next_button;
    MenuButton headless_toggle_button;
    MenuButton mode_toggle_button;
    MenuButton space_toggle_button;
    SDL_Rect config_panel_rect;
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
    bool suppress_pointer_until_up;
} SceneMenuInteraction;

#endif
