#ifndef PHYSICS_SIM_MENU_SHARED_TEXT_H
#define PHYSICS_SIM_MENU_SHARED_TEXT_H
#include <stdbool.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include "ui/text_input.h"
#include "app/app_config.h"
#include "kit_ui_text_edit.h"
#include "kit_ui_text_presentation.h"
struct SceneMenuInteraction;
typedef struct PhysicsSimMenuText {
    TextInputField *owner;
    char input_root[sizeof(((AppConfig *)0)->input_root)];
    char output_root[sizeof(((AppConfig *)0)->headless_output_dir)];
    char warm_start[sizeof(((AppConfig *)0)->atmospheric_warm_start_path)];
    size_t path_capacity[3];
    KitUiTextEdit edit;
    KitUiTextPresentation view;
    TTF_Font *font;
    SDL_Rect hit_clip;
    bool view_ready, dragging, native_session;
} PhysicsSimMenuText;
void menu_shared_text_begin(struct SceneMenuInteraction *, TextInputField *, const char *, size_t);
const char *menu_shared_text_value(struct SceneMenuInteraction *, const TextInputField *);
void menu_shared_text_insert(struct SceneMenuInteraction *, const char *);
void menu_shared_text_key(struct SceneMenuInteraction *, SDL_Keycode, SDL_Keymod);
void menu_shared_text_sync(struct SceneMenuInteraction *);
void menu_shared_text_cancel_transient(struct SceneMenuInteraction *);
bool menu_shared_text_event(struct SceneMenuInteraction *, const SDL_Event *);
void menu_draw_shared_text_input(struct SceneMenuInteraction *, TTF_Font *, const SDL_Rect *, TextInputField *, SDL_Color, SDL_Color, SDL_Color);
#endif
