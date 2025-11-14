#include "input/input.h"

#include <SDL2/SDL.h>
#include <stdbool.h>
#include <string.h>

static BrushMode s_brush_mode = BRUSH_MODE_DENSITY;

static void dispatch_pointer(const InputHandlers *handlers,
                             void (*fn)(const InputPointerState *, void *),
                             const InputPointerState *state) {
    if (handlers && fn) {
        fn(state, handlers->user_data);
    }
}

static void dispatch_key(const InputHandlers *handlers,
                         void (*fn)(SDL_Keycode, SDL_Keymod, void *),
                         SDL_Keycode key,
                         SDL_Keymod mod) {
    if (handlers && fn) {
        fn(key, mod, handlers->user_data);
    }
}

bool input_poll_events(InputCommands *out,
                       CommandBus *bus,
                       const InputHandlers *handlers) {
    if (!out) return false;

    memset(out, 0, sizeof(*out));
    out->brush_mode = s_brush_mode;

    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
        case SDL_QUIT:
            out->quit = true;
            break;
        case SDL_KEYDOWN: {
            SDL_Keymod mod = SDL_GetModState();
            switch (e.key.keysym.sym) {
            case SDLK_ESCAPE:
                out->quit = true;
                break;
            case SDLK_p:
                if (bus) {
                    Command cmd = {.type = COMMAND_TOGGLE_PAUSE};
                    command_bus_push(bus, &cmd);
                }
                break;
            case SDLK_c:
                if (bus) {
                    Command cmd = {.type = COMMAND_CLEAR_SMOKE};
                    command_bus_push(bus, &cmd);
                }
                break;
            case SDLK_e:
                if (bus) {
                    Command cmd = {.type = COMMAND_EXPORT_SNAPSHOT};
                    command_bus_push(bus, &cmd);
                }
                break;
            case SDLK_1:
                s_brush_mode = BRUSH_MODE_DENSITY;
                out->brush_mode = s_brush_mode;
                out->brush_mode_changed = true;
                break;
            case SDLK_2:
                s_brush_mode = BRUSH_MODE_VELOCITY;
                out->brush_mode = s_brush_mode;
                out->brush_mode_changed = true;
                break;
            default:
                break;
            }
            dispatch_key(handlers, handlers ? handlers->on_key_down : NULL,
                         e.key.keysym.sym, mod);
            break;
        }
        case SDL_KEYUP: {
            SDL_Keymod mod = SDL_GetModState();
            dispatch_key(handlers, handlers ? handlers->on_key_up : NULL,
                         e.key.keysym.sym, mod);
            break;
        }
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP: {
            bool down = (e.type == SDL_MOUSEBUTTONDOWN);
            if (e.button.button == SDL_BUTTON_LEFT) {
                out->mouse_down = down;
                out->mouse_x = e.button.x;
                out->mouse_y = e.button.y;
                InputPointerState state = {
                    .x = e.button.x,
                    .y = e.button.y,
                    .down = down
                };
                if (down) {
                    dispatch_pointer(handlers, handlers ? handlers->on_pointer_down : NULL, &state);
                } else {
                    dispatch_pointer(handlers, handlers ? handlers->on_pointer_up : NULL, &state);
                }
            }
            break;
        }
        case SDL_MOUSEMOTION: {
            out->mouse_x = e.motion.x;
            out->mouse_y = e.motion.y;
            if (e.motion.state & SDL_BUTTON_LMASK) {
                out->mouse_down = true;
            }
            InputPointerState state = {
                .x = e.motion.x,
                .y = e.motion.y,
                .down = (e.motion.state & SDL_BUTTON_LMASK) != 0
            };
            dispatch_pointer(handlers, handlers ? handlers->on_pointer_move : NULL, &state);
            break;
        }
        default:
            break;
        }
    }

    return !out->quit;
}
