#include "app/editor/scene_editor.h"

#include <math.h>

#include "input/input.h"

typedef enum EditorDragMode {
    DRAG_NONE = 0,
    DRAG_POSITION,
    DRAG_DIRECTION
} EditorDragMode;

typedef struct SceneEditorState {
    SDL_Window   *window;
    SDL_Renderer *renderer;
    TTF_Font     *font_main;
    TTF_Font     *font_small;
    AppConfig    cfg;
    FluidScenePreset *preset;

    int canvas_x;
    int canvas_y;
    int canvas_size;

    int  selected_emitter;
    EditorDragMode drag_mode;
    bool dragging;
    float drag_offset_x;
    float drag_offset_y;
} SceneEditorState;

static SDL_Color COLOR_BG        = {20, 22, 26, 255};
static SDL_Color COLOR_CANVAS    = {12, 14, 18, 255};
static SDL_Color COLOR_SOURCE    = {252, 163, 17, 255};
static SDL_Color COLOR_JET       = {64, 201, 255, 255};
static SDL_Color COLOR_SINK      = {200, 80, 255, 255};
static SDL_Color COLOR_TEXT      = {245, 247, 250, 255};
static SDL_Color COLOR_SELECTED  = {255, 255, 255, 255};

static float clamp01(float v) {
    if (v < 0.0f) return 0.0f;
    if (v > 1.0f) return 1.0f;
    return v;
}

static void project_to_canvas(const SceneEditorState *state,
                              float px, float py,
                              int *out_x, int *out_y);
static void canvas_to_normalized(const SceneEditorState *state,
                                 int sx, int sy,
                                 float *out_x, float *out_y);
static int hit_test_emitter(SceneEditorState *state, int px, int py, EditorDragMode *mode);
static void editor_handle_key(SceneEditorState *state, SDL_Keycode key);
static void editor_pointer_down(const InputPointerState *ptr, void *user) {
    SceneEditorState *state = (SceneEditorState *)user;
    if (!state || !ptr) return;

    EditorDragMode mode = DRAG_NONE;
    int hit = hit_test_emitter(state, ptr->x, ptr->y, &mode);
    if (hit < 0) {
        state->dragging = false;
        state->drag_mode = DRAG_NONE;
        return;
    }

    state->selected_emitter = hit;
    state->drag_mode = mode;
    state->dragging = true;
    state->drag_offset_x = 0.0f;
    state->drag_offset_y = 0.0f;

    if (mode == DRAG_POSITION) {
        FluidEmitter *em = &state->preset->emitters[hit];
        float nx, ny;
        canvas_to_normalized(state, ptr->x, ptr->y, &nx, &ny);
        state->drag_offset_x = nx - em->position_x;
        state->drag_offset_y = ny - em->position_y;
    }
}

static void editor_pointer_up(const InputPointerState *ptr, void *user) {
    (void)ptr;
    SceneEditorState *state = (SceneEditorState *)user;
    if (!state) return;
    state->dragging = false;
    state->drag_mode = DRAG_NONE;
}

static void editor_pointer_move(const InputPointerState *ptr, void *user) {
    SceneEditorState *state = (SceneEditorState *)user;
    if (!state || !ptr || !state->dragging || state->selected_emitter < 0) return;

    FluidEmitter *em = &state->preset->emitters[state->selected_emitter];
    if (state->drag_mode == DRAG_POSITION) {
        float nx, ny;
        canvas_to_normalized(state, ptr->x, ptr->y, &nx, &ny);
        nx -= state->drag_offset_x;
        ny -= state->drag_offset_y;
        em->position_x = clamp01(nx);
        em->position_y = clamp01(ny);
    } else if (state->drag_mode == DRAG_DIRECTION) {
        int cx, cy;
        project_to_canvas(state, em->position_x, em->position_y, &cx, &cy);
        float dx = (float)(ptr->x - cx);
        float dy = (float)(ptr->y - cy);
        float len = sqrtf(dx * dx + dy * dy);
        if (len > 0.0001f) {
            em->dir_x = dx / len;
            em->dir_y = dy / len;
        }
    }
}
static void editor_key_down(SDL_Keycode key, SDL_Keymod mod, void *user);

static SDL_Color emitter_color(const FluidEmitter *em) {
    switch (em->type) {
    case EMITTER_DENSITY_SOURCE: return COLOR_SOURCE;
    case EMITTER_VELOCITY_JET:   return COLOR_JET;
    case EMITTER_SINK:           return COLOR_SINK;
    default:                     return COLOR_JET;
    }
}

static void project_to_canvas(const SceneEditorState *state,
                              float px, float py,
                              int *out_x, int *out_y) {
    *out_x = state->canvas_x + (int)lroundf(px * (float)state->canvas_size);
    *out_y = state->canvas_y + (int)lroundf(py * (float)state->canvas_size);
}

static void canvas_to_normalized(const SceneEditorState *state,
                                 int sx, int sy,
                                 float *out_x, float *out_y) {
    float nx = (float)(sx - state->canvas_x) / (float)state->canvas_size;
    float ny = (float)(sy - state->canvas_y) / (float)state->canvas_size;
    if (nx < 0.0f) nx = 0.0f;
    if (nx > 1.0f) nx = 1.0f;
    if (ny < 0.0f) ny = 0.0f;
    if (ny > 1.0f) ny = 1.0f;
    *out_x = nx;
    *out_y = ny;
}

static void draw_circle(SDL_Renderer *renderer, int cx, int cy, int radius, SDL_Color color) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    for (int dy = -radius; dy <= radius; ++dy) {
        for (int dx = -radius; dx <= radius; ++dx) {
            if (dx * dx + dy * dy <= radius * radius) {
                SDL_RenderDrawPoint(renderer, cx + dx, cy + dy);
            }
        }
    }
}

static void draw_line(SDL_Renderer *renderer, int x0, int y0, int x1, int y1, SDL_Color color) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_RenderDrawLine(renderer, x0, y0, x1, y1);
}

static void draw_text_center(SDL_Renderer *renderer, TTF_Font *font,
                             const char *text, int center_x, int y,
                             SDL_Color color) {
    if (!font) return;
    SDL_Surface *surf = TTF_RenderUTF8_Blended(font, text, color);
    if (!surf) return;
    SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer, surf);
    SDL_Rect dst = {center_x - surf->w / 2, y, surf->w, surf->h};
    SDL_RenderCopy(renderer, tex, NULL, &dst);
    SDL_DestroyTexture(tex);
    SDL_FreeSurface(surf);
}

static void draw_editor(SceneEditorState *state) {
    SDL_SetRenderDrawColor(state->renderer, COLOR_BG.r, COLOR_BG.g, COLOR_BG.b, 255);
    SDL_RenderClear(state->renderer);

    SDL_Rect canvas = {state->canvas_x, state->canvas_y,
                       state->canvas_size, state->canvas_size};
    SDL_SetRenderDrawColor(state->renderer, COLOR_CANVAS.r, COLOR_CANVAS.g, COLOR_CANVAS.b, 255);
    SDL_RenderFillRect(state->renderer, &canvas);

    for (size_t i = 0; i < state->preset->emitter_count; ++i) {
        const FluidEmitter *em = &state->preset->emitters[i];
        int cx, cy;
        project_to_canvas(state, em->position_x, em->position_y, &cx, &cy);
        int radius_px = (int)(em->radius * state->canvas_size);
        if (radius_px < 4) radius_px = 4;
        SDL_Color color = emitter_color(em);
        draw_circle(state->renderer, cx, cy, radius_px, color);

        if ((int)i == state->selected_emitter) {
            draw_circle(state->renderer, cx, cy, radius_px + 4, COLOR_SELECTED);
        }

        if (em->type != EMITTER_DENSITY_SOURCE) {
        int arrow_len = radius_px + 40;
        int hx = cx + (int)(em->dir_x * arrow_len);
        int hy = cy + (int)(em->dir_y * arrow_len);
            draw_line(state->renderer, cx, cy, hx, hy, COLOR_SELECTED);
            draw_circle(state->renderer, hx, hy, 5, COLOR_SELECTED);
        }
    }

    const char *hint = "Click emitter to edit. Drag to move, drag arrow to rotate, +/- adjusts strength.";
    draw_text_center(state->renderer,
                     state->font_small ? state->font_small : state->font_main,
                     hint,
                     state->canvas_x + state->canvas_size / 2,
                     state->canvas_y + state->canvas_size + 12,
                     COLOR_TEXT);
}

static int hit_test_emitter(SceneEditorState *state, int px, int py, EditorDragMode *mode) {
    int closest = -1;
    float best_dist = 1e9f;

    for (size_t i = 0; i < state->preset->emitter_count; ++i) {
        FluidEmitter *em = &state->preset->emitters[i];
        int cx, cy;
        project_to_canvas(state, em->position_x, em->position_y, &cx, &cy);
        int radius_px = (int)(em->radius * state->canvas_size);
        if (radius_px < 4) radius_px = 4;
        float dx = (float)px - (float)cx;
        float dy = (float)py - (float)cy;
        float dist = sqrtf(dx * dx + dy * dy);
        if (dist <= (float)radius_px) {
            if (dist < best_dist) {
                closest = (int)i;
                best_dist = dist;
                if (mode) *mode = DRAG_POSITION;
            }
        } else if (em->type != EMITTER_DENSITY_SOURCE) {
            int arrow_len = radius_px + 40;
            int hx = cx + (int)(em->dir_x * arrow_len);
            int hy = cy + (int)(em->dir_y * arrow_len);
            float adx = (float)px - (float)hx;
            float ady = (float)py - (float)hy;
            float adist = sqrtf(adx * adx + ady * ady);
            if (adist <= 20.0f && adist < best_dist) {
                closest = (int)i;
                best_dist = adist;
                if (mode) *mode = DRAG_DIRECTION;
            }
        }
    }
    return closest;
}

static void adjust_emitter_radius(FluidEmitter *em, float scale) {
    float new_radius = em->radius * scale;
    if (new_radius < 0.02f) new_radius = 0.02f;
    if (new_radius > 0.5f) new_radius = 0.5f;
    float ratio = new_radius / em->radius;
    em->radius = new_radius;
    em->strength *= ratio;
}


static void editor_handle_key(SceneEditorState *state, SDL_Keycode key) {
    if (state->selected_emitter < 0 || state->selected_emitter >= (int)state->preset->emitter_count) {
        return;
    }
    FluidEmitter *em = &state->preset->emitters[state->selected_emitter];
    switch (key) {
    case SDLK_EQUALS:
    case SDLK_PLUS:
    case SDLK_KP_PLUS:
        adjust_emitter_radius(em, 1.1f);
        break;
    case SDLK_MINUS:
    case SDLK_UNDERSCORE:
    case SDLK_KP_MINUS:
        adjust_emitter_radius(em, 0.9f);
        break;
    default:
        break;
    }
}

bool scene_editor_run(SDL_Window *window,
                      SDL_Renderer *renderer,
                      TTF_Font *font_main,
                      TTF_Font *font_small,
                      const AppConfig *cfg,
                      FluidScenePreset *preset) {
    if (!window || !renderer || !preset) return false;

    SceneEditorState state = {
        .window = window,
        .renderer = renderer,
        .font_main = font_main,
        .font_small = font_small,
        .cfg = *cfg,
        .preset = preset,
        .selected_emitter = (preset->emitter_count > 0) ? 0 : -1,
        .drag_mode = DRAG_NONE,
        .dragging = false,
        .drag_offset_x = 0.0f,
        .drag_offset_y = 0.0f,
    };
    int winW = 0, winH = 0;
    SDL_GetWindowSize(window, &winW, &winH);
    int max_canvas = winH - 200;
    int alt_canvas = winW - 220;
    if (max_canvas < 200) max_canvas = 200;
    if (alt_canvas < max_canvas) max_canvas = alt_canvas;
    state.canvas_size = max_canvas;
    state.canvas_x = (winW - state.canvas_size) / 2;
    state.canvas_y = 100;

    bool running = true;
    bool applied = false;

    InputHandlers handlers = {
        .on_pointer_down = editor_pointer_down,
        .on_pointer_up = editor_pointer_up,
        .on_pointer_move = editor_pointer_move,
        .on_key_down = editor_key_down,
        .user_data = &state
    };

    while (running) {
        InputCommands cmds;
        input_poll_events(&cmds, NULL, &handlers);
        if (cmds.quit) {
            running = false;
            break;
        }

        const Uint8 *keys = SDL_GetKeyboardState(NULL);
        if (keys[SDL_SCANCODE_RETURN]) {
            applied = true;
            running = false;
        } else if (keys[SDL_SCANCODE_ESCAPE]) {
            running = false;
        }

        draw_editor(&state);
        SDL_RenderPresent(renderer);
    }

    return applied;
}
static void editor_key_down(SDL_Keycode key, SDL_Keymod mod, void *user) {
    (void)mod;
    editor_handle_key((SceneEditorState *)user, key);
}
