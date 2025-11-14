#ifndef INPUT_CONTEXT_H
#define INPUT_CONTEXT_H

#include <SDL2/SDL.h>
#include <stdbool.h>

typedef struct InputPointerState {
    int x;
    int y;
    bool down;
} InputPointerState;

typedef struct InputContext {
    void (*on_pointer_down)(void *user, const InputPointerState *state);
    void (*on_pointer_up)(void *user, const InputPointerState *state);
    void (*on_pointer_move)(void *user, const InputPointerState *state);
    void (*on_key_down)(void *user, SDL_Keycode key, SDL_Keymod mod);
    void (*on_key_up)(void *user, SDL_Keycode key, SDL_Keymod mod);
    void (*on_text_input)(void *user, const char *text);
    void *user_data;
} InputContext;

#define INPUT_CONTEXT_STACK_CAPACITY 8

typedef struct InputContextManager {
    InputContext stack[INPUT_CONTEXT_STACK_CAPACITY];
    int          top;
} InputContextManager;

void input_context_manager_init(InputContextManager *mgr);
bool input_context_manager_push(InputContextManager *mgr, const InputContext *ctx);
bool input_context_manager_pop(InputContextManager *mgr);
InputContext *input_context_manager_current(InputContextManager *mgr);

#endif // INPUT_CONTEXT_H
