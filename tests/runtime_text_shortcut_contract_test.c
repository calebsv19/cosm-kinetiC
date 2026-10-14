#include "input/input.h"
#include <SDL2/SDL.h>
#include <assert.h>
#include <stdio.h>

static InputCommands replay(SDL_Keycode key, SDL_Keymod recorded, SDL_Keymod current,
                            Uint8 repeat) {
    SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);
    SDL_Event event = {.type = SDL_KEYDOWN};
    event.key.keysym.sym = key;
    event.key.keysym.mod = recorded;
    event.key.repeat = repeat;
    assert(SDL_PushEvent(&event) == 1);
    SDL_SetModState(current);
    InputCommands commands = {0};
    (void)input_poll_events(&commands, NULL, NULL);
    return commands;
}

int main(void) {
    assert(SDL_Init(SDL_INIT_EVENTS) == 0);
    /* Modifier release may already be pumped when the recorded key is drained. */
    assert(replay(SDLK_EQUALS, KMOD_GUI, KMOD_NONE, 0).text_zoom_in_requested);
    assert(replay(SDLK_MINUS, KMOD_CTRL, KMOD_NONE, 0).text_zoom_out_requested);
    assert(replay(SDLK_0, KMOD_GUI, KMOD_NONE, 0).text_zoom_reset_requested);
    assert(!replay(SDLK_EQUALS, KMOD_NONE, KMOD_GUI, 0).text_zoom_in_requested);
    assert(!replay(SDLK_EQUALS, KMOD_GUI, KMOD_GUI, 1).text_zoom_in_requested);
    SDL_Quit();
    puts("runtime_text_shortcut_contract_test: success");
    return 0;
}
