#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <SDL2/SDL.h>
#include "input/input.h"
static int allocation_fails;
static void *test_calloc(size_t n,size_t size) {return allocation_fails?NULL:calloc(n,size);}
#define calloc test_calloc
#include "../src/command/command_bus.c"
#undef calloc
static void key(int repeat) {
    SDL_Event e;SDL_zero(e);e.type=SDL_KEYDOWN;e.key.keysym.sym=SDLK_p;e.key.repeat=repeat;
    assert(SDL_PushEvent(&e)==1);
}
int main(void) {
    CommandBus b={0};Command c={.type=COMMAND_TOGGLE_PAUSE},out;
    assert(!command_bus_push(NULL,&c));command_bus_init(&b,1);assert(!command_bus_push(&b,NULL));
    c.payload.ints[0]=7;assert(command_bus_push(&b,&c));c.payload.ints[0]=9;
    allocation_fails=1;assert(!command_bus_push(&b,&c));assert(b.count==1&&b.next_id==2);
    allocation_fails=0;assert(command_bus_push(&b,&c));assert(command_bus_pop(&b,&out));
    assert(out.payload.ints[0]==7&&out.sequence_id==1);assert(command_bus_pop(&b,&out));
    assert(out.payload.ints[0]==9&&out.sequence_id==2);
    b.next_id=UINT64_MAX;assert(!command_bus_push(&b,&c)&&!b.count);
    command_bus_clear(&b);assert(command_bus_push(&b,&c));command_bus_clear(&b);assert(!command_bus_pop(&b,&out));
    SDL_setenv("SDL_VIDEODRIVER","dummy",1);assert(!SDL_Init(SDL_INIT_VIDEO));
    InputCommands input={0};key(0);key(1);key(1);assert(input_poll_events(&input,&b,NULL));
    assert(command_bus_count(&b)==1);assert(command_bus_pop(&b,&out)&&out.type==COMMAND_TOGGLE_PAUSE);
    key(0);assert(input_poll_events(&input,&b,NULL));assert(command_bus_count(&b)==1);
    command_bus_shutdown(&b);SDL_Quit();puts("Owned FIFO payload, failed admission, expiry and native repeat suppression passed");return 0;
}
