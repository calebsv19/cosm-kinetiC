#ifndef INPUT_H
#define INPUT_H

#include <stdbool.h>

#include "command/command_bus.h"
#include "input/input_context.h"

typedef enum BrushMode {
    BRUSH_MODE_DENSITY = 0,
    BRUSH_MODE_VELOCITY = 1
} BrushMode;

typedef struct InputCommands {
    bool quit;

    bool mouse_down;
    int  mouse_x;
    int  mouse_y;

    BrushMode brush_mode;
    bool      brush_mode_changed;
} InputCommands;

typedef struct InputHandlers {
    void (*on_pointer_down)(const InputPointerState *state, void *user);
    void (*on_pointer_up)(const InputPointerState *state, void *user);
    void (*on_pointer_move)(const InputPointerState *state, void *user);
    void (*on_key_down)(SDL_Keycode key, SDL_Keymod mod, void *user);
    void (*on_key_up)(SDL_Keycode key, SDL_Keymod mod, void *user);
    void *user_data;
} InputHandlers;

bool input_poll_events(InputCommands *out,
                       CommandBus *bus,
                       const InputHandlers *handlers);

#endif // INPUT_H
