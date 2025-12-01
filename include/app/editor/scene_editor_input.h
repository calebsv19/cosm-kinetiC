#ifndef SCENE_EDITOR_INPUT_H
#define SCENE_EDITOR_INPUT_H

#include <SDL2/SDL.h>

#include "input/input_context.h"

void editor_pointer_down(void *user, const InputPointerState *ptr);
void editor_pointer_up(void *user, const InputPointerState *ptr);
void editor_pointer_move(void *user, const InputPointerState *ptr);
void editor_on_wheel(void *user, const InputWheelState *wheel);
void editor_text_input(void *user, const char *text);
void editor_key_down(void *user, SDL_Keycode key, SDL_Keymod mod);
void editor_key_up(void *user, SDL_Keycode key, SDL_Keymod mod);

#endif // SCENE_EDITOR_INPUT_H
