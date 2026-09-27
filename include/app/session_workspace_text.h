#ifndef PHYSICS_SIM_SESSION_WORKSPACE_TEXT_H
#define PHYSICS_SIM_SESSION_WORKSPACE_TEXT_H
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdbool.h>
typedef struct SessionTextEntry {
    SDL_Texture *texture;
    char text[512];
    SDL_Color color;
    int role, w, h;
} SessionTextEntry;
typedef struct SessionText {
    TTF_Font *fonts[3];
    int logical_sizes[3], raster_sizes[3];
    float scale, zoom, loaded_zoom;
    SessionTextEntry entries[96];
    unsigned next;
} SessionText;
bool session_text_refresh(SessionText *text, SDL_Renderer *renderer, SDL_Window *window);
void session_text_draw(SessionText *text, SDL_Renderer *renderer, int role, int x, int y,
                       SDL_Color color, const char *label);
void session_text_destroy(SessionText *text);
#endif
