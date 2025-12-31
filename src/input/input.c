#include "input/input.h"

#include <SDL2/SDL.h>
#include <stdbool.h>
#include <string.h>

static BrushMode s_brush_mode = BRUSH_MODE_DENSITY;

bool input_poll_events(InputCommands *out,
                       CommandBus *bus,
                       InputContextManager *context_mgr) {
    if (!out) return false;

    memset(out, 0, sizeof(*out));
    out->brush_mode = s_brush_mode;

    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        InputContext *ctx = context_mgr
                                ? input_context_manager_current(context_mgr)
                                : NULL;
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
            case SDLK_v:
                if (bus) {
                    Command cmd = {.type = COMMAND_TOGGLE_VORTICITY};
                    command_bus_push(bus, &cmd);
                }
                break;
            case SDLK_b:
                if (bus) {
                    Command cmd = {.type = COMMAND_TOGGLE_PRESSURE};
                    command_bus_push(bus, &cmd);
                }
                break;
            case SDLK_s:
                if (bus) {
                    Command cmd = {
                        .type = (mod & KMOD_SHIFT)
                                    ? COMMAND_TOGGLE_VELOCITY_MODE
                                    : COMMAND_TOGGLE_VELOCITY_VECTORS};
                    command_bus_push(bus, &cmd);
                }
                break;
            case SDLK_l:
                if (bus) {
                    Command cmd = {.type = COMMAND_TOGGLE_PARTICLE_FLOW};
                    command_bus_push(bus, &cmd);
                }
                break;
            case SDLK_g:
                if (bus) {
                    Command cmd = {.type = COMMAND_TOGGLE_OBJECT_GRAVITY};
                    command_bus_push(bus, &cmd);
                }
                break;
            case SDLK_h:
                if (bus) {
                    Command cmd = {.type = COMMAND_TOGGLE_ELASTIC_COLLISIONS};
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
            if (ctx && ctx->on_key_down) {
                ctx->on_key_down(ctx->user_data, e.key.keysym.sym, mod);
            }
            break;
        }
        case SDL_KEYUP: {
            SDL_Keymod mod = SDL_GetModState();
            if (ctx && ctx->on_key_up) {
                ctx->on_key_up(ctx->user_data, e.key.keysym.sym, mod);
            }
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
                if (ctx) {
                    if (down && ctx->on_pointer_down) {
                        ctx->on_pointer_down(ctx->user_data, &state);
                    } else if (!down && ctx->on_pointer_up) {
                        ctx->on_pointer_up(ctx->user_data, &state);
                    }
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
            if (ctx && ctx->on_pointer_move) {
                ctx->on_pointer_move(ctx->user_data, &state);
            }
            break;
        }
        case SDL_MOUSEWHEEL: {
            if (ctx && ctx->on_wheel) {
                InputWheelState wheel = {
                    .x = e.wheel.x,
                    .y = (e.wheel.direction == SDL_MOUSEWHEEL_FLIPPED) ? -e.wheel.y : e.wheel.y,
                    .flipped = (e.wheel.direction == SDL_MOUSEWHEEL_FLIPPED)
                };
                ctx->on_wheel(ctx->user_data, &wheel);
            }
            break;
        }
        case SDL_TEXTINPUT: {
            if (ctx && ctx->on_text_input) {
                ctx->on_text_input(ctx->user_data, e.text.text);
            }
            break;
        }
        default:
            break;
        }
    }

    return !out->quit;
}
