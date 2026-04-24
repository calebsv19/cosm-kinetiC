#include "input/input.h"

#include <SDL2/SDL.h>
#include <stdbool.h>
#include <string.h>

static BrushMode s_brush_mode = BRUSH_MODE_DENSITY;

static void input_handle_event(const SDL_Event *event,
                               InputCommands *out,
                               CommandBus *bus,
                               InputContextManager *context_mgr) {
    InputContext *ctx = context_mgr
                            ? input_context_manager_current(context_mgr)
                            : NULL;
    if (!event || !out) {
        return;
    }

    switch (event->type) {
    case SDL_QUIT:
        out->quit = true;
        break;
    case SDL_WINDOWEVENT:
        if (event->window.event == SDL_WINDOWEVENT_CLOSE) {
            out->quit = true;
        }
        break;
    case SDL_KEYDOWN: {
        SDL_Keymod mod = SDL_GetModState();
        bool ctrl_or_cmd = (mod & (KMOD_CTRL | KMOD_GUI)) != 0;
        if (ctrl_or_cmd && event->key.repeat == 0) {
            switch (event->key.keysym.sym) {
            case SDLK_EQUALS:
            case SDLK_PLUS:
            case SDLK_KP_PLUS:
                out->text_zoom_in_requested = true;
                break;
            case SDLK_MINUS:
            case SDLK_UNDERSCORE:
            case SDLK_KP_MINUS:
                out->text_zoom_out_requested = true;
                break;
            case SDLK_0:
            case SDLK_KP_0:
                out->text_zoom_reset_requested = true;
                break;
            default:
                break;
            }
        }
        if (out->text_zoom_in_requested ||
            out->text_zoom_out_requested ||
            out->text_zoom_reset_requested) {
            break;
        }
        switch (event->key.keysym.sym) {
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
                Command cmd = {
                    .type = (mod & KMOD_SHIFT)
                                ? COMMAND_TOGGLE_KIT_VIZ_VORTICITY
                                : COMMAND_TOGGLE_VORTICITY};
                command_bus_push(bus, &cmd);
            }
            break;
        case SDLK_b:
            if (bus) {
                Command cmd = {
                    .type = (mod & KMOD_SHIFT)
                                ? COMMAND_TOGGLE_KIT_VIZ_PRESSURE
                                : COMMAND_TOGGLE_PRESSURE};
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
                Command cmd = {
                    .type = (mod & KMOD_SHIFT)
                                ? COMMAND_TOGGLE_KIT_VIZ_PARTICLES
                                : COMMAND_TOGGLE_PARTICLE_FLOW};
                command_bus_push(bus, &cmd);
            }
            break;
        case SDLK_k:
            if (bus) {
                Command cmd = {.type = COMMAND_TOGGLE_KIT_VIZ_DENSITY};
                command_bus_push(bus, &cmd);
            }
            break;
        case SDLK_j:
            if (bus) {
                Command cmd = {.type = COMMAND_TOGGLE_KIT_VIZ_VELOCITY};
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
            ctx->on_key_down(ctx->user_data, event->key.keysym.sym, mod);
        }
        break;
    }
    case SDL_KEYUP: {
        SDL_Keymod mod = SDL_GetModState();
        if (ctx && ctx->on_key_up) {
            ctx->on_key_up(ctx->user_data, event->key.keysym.sym, mod);
        }
        break;
    }
    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEBUTTONUP: {
        bool down = (event->type == SDL_MOUSEBUTTONDOWN);
        if (event->button.button == SDL_BUTTON_LEFT) {
            out->mouse_down = down;
            out->mouse_x = event->button.x;
            out->mouse_y = event->button.y;
        }
        InputPointerState state = {
            .x = event->button.x,
            .y = event->button.y,
            .down = down,
            .button = event->button.button
        };
        if (ctx) {
            if (down && ctx->on_pointer_down) {
                ctx->on_pointer_down(ctx->user_data, &state);
            } else if (!down && ctx->on_pointer_up) {
                ctx->on_pointer_up(ctx->user_data, &state);
            }
        }
        break;
    }
    case SDL_MOUSEMOTION: {
        out->mouse_x = event->motion.x;
        out->mouse_y = event->motion.y;
        if (event->motion.state & SDL_BUTTON_LMASK) {
            out->mouse_down = true;
        }
        InputPointerState state = {
            .x = event->motion.x,
            .y = event->motion.y,
            .down = (event->motion.state & SDL_BUTTON_LMASK) != 0,
            .button = 0
        };
        if (ctx && ctx->on_pointer_move) {
            ctx->on_pointer_move(ctx->user_data, &state);
        }
        break;
    }
    case SDL_MOUSEWHEEL: {
        if (ctx && ctx->on_wheel) {
            InputWheelState wheel = {
                .x = event->wheel.x,
                .y = (event->wheel.direction == SDL_MOUSEWHEEL_FLIPPED)
                         ? -event->wheel.y
                         : event->wheel.y,
                .flipped = (event->wheel.direction == SDL_MOUSEWHEEL_FLIPPED)
            };
            ctx->on_wheel(ctx->user_data, &wheel);
        }
        break;
    }
    case SDL_TEXTINPUT: {
        if (ctx && ctx->on_text_input) {
            ctx->on_text_input(ctx->user_data, event->text.text);
        }
        break;
    }
    default:
        break;
    }
}

bool input_poll_events_with_wait(InputCommands *out,
                                 CommandBus *bus,
                                 InputContextManager *context_mgr,
                                 int wait_timeout_ms,
                                 uint32_t *out_wait_blocked_ms,
                                 uint32_t *out_wait_call_count,
                                 uint32_t *out_event_count) {
    if (!out) {
        return false;
    }
    if (out_wait_blocked_ms) {
        *out_wait_blocked_ms = 0u;
    }
    if (out_wait_call_count) {
        *out_wait_call_count = 0u;
    }
    if (out_event_count) {
        *out_event_count = 0u;
    }

    memset(out, 0, sizeof(*out));
    out->brush_mode = s_brush_mode;

    SDL_Event event;
    if (wait_timeout_ms > 0) {
        Uint32 wait_begin = SDL_GetTicks();
        if (SDL_WaitEventTimeout(&event, wait_timeout_ms) == 1) {
            input_handle_event(&event, out, bus, context_mgr);
            if (out_event_count) {
                *out_event_count += 1u;
            }
        }
        Uint32 wait_end = SDL_GetTicks();
        if (out_wait_blocked_ms) {
            *out_wait_blocked_ms += (wait_end - wait_begin);
        }
        if (out_wait_call_count) {
            *out_wait_call_count += 1u;
        }
    }

    while (SDL_PollEvent(&event)) {
        input_handle_event(&event, out, bus, context_mgr);
        if (out_event_count) {
            *out_event_count += 1u;
        }
    }

    return !out->quit;
}

bool input_poll_events(InputCommands *out,
                       CommandBus *bus,
                       InputContextManager *context_mgr) {
    return input_poll_events_with_wait(out, bus, context_mgr, 0, NULL, NULL, NULL);
}
