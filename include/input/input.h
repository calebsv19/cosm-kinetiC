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

bool input_poll_events(InputCommands *out,
                       CommandBus *bus,
                       InputContextManager *context_mgr);

#endif // INPUT_H
