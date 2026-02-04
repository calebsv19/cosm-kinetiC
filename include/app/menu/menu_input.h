#ifndef MENU_INPUT_H
#define MENU_INPUT_H

#include "app/menu/menu_types.h"

void menu_pointer_up(void *user, const InputPointerState *state);
void menu_pointer_down(void *user, const InputPointerState *state);
void menu_pointer_move(void *user, const InputPointerState *state);
void menu_wheel(void *user, const InputWheelState *wheel);
void menu_key_down(void *user, SDL_Keycode key, SDL_Keymod mod);
void menu_text_input(void *user, const char *text);

#endif
