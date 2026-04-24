#ifndef INPUT_H
#define INPUT_H

#include <stdbool.h>
#include <stdint.h>

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

    bool text_zoom_in_requested;
    bool text_zoom_out_requested;
    bool text_zoom_reset_requested;
} InputCommands;

bool input_poll_events(InputCommands *out,
                       CommandBus *bus,
                       InputContextManager *context_mgr);

bool input_poll_events_with_wait(InputCommands *out,
                                 CommandBus *bus,
                                 InputContextManager *context_mgr,
                                 int wait_timeout_ms,
                                 uint32_t *out_wait_blocked_ms,
                                 uint32_t *out_wait_call_count,
                                 uint32_t *out_event_count);

#endif // INPUT_H
