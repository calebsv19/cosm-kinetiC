#define _DARWIN_C_SOURCE 1
#include "app/session_workspace.h"
#include "app/menu/shared_theme_font_adapter.h"
#include "app/session_workspace_text.h"
#include "core_viewport2d.h"
#include "font_paths.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <errno.h>
#include <json-c/json.h>
#include <math.h>
#include <pthread.h>
#include <spawn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

extern char **environ;
#ifndef PHYSICS_SIM_REPO_ROOT
#define PHYSICS_SIM_REPO_ROOT "."
#endif

typedef enum ClientOperation {
    CLIENT_IDLE,
    CLIENT_CREATE,
    CLIENT_VALIDATE,
    CLIENT_START,
    CLIENT_CONTROL,
    CLIENT_INSPECT,
    CLIENT_SAMPLE
} ClientOperation;
typedef struct Workspace {
    char root[1024], response[1200], run_id[65], revision[65], scene_id[65], message[256];
    pid_t child;
    ClientOperation operation;
    struct json_object *snapshot, *inspection, *sample_args;
    int plane, field, probe;
    double slice, range_low, range_high;
    bool vectors, locked_range, refit;
    char queued_action[32];
    CoreViewport2D viewport;
    uint64_t commands;
    char last_action[32], last_command_id[65];
    bool retry_control;
    int test_stage;
    int64_t paused_tick;
} Workspace;

static struct json_object *get(struct json_object *o, const char *key) {
    struct json_object *v = NULL;
    if (o)
        json_object_object_get_ex(o, key, &v);
    return v;
}
static struct json_object *item(struct json_object *array, size_t index) {
    return array && json_object_get_type(array) == json_type_array
               ? json_object_array_get_idx(array, index)
               : NULL;
}
static const char *text(struct json_object *o, const char *key) {
    const char *s = json_object_get_string(get(o, key));
    return s ? s : "";
}
static void add(struct json_object *o, const char *k, const char *v) {
    json_object_object_add(o, k, json_object_new_string(v));
}
static void mkdirs(const char *path) {
    char buf[1024];
    snprintf(buf, sizeof(buf), "%s", path);
    for (char *p = buf + 1; *p; p++)
        if (*p == '/') {
            *p = 0;
            mkdir(buf, 0700);
            *p = '/';
        }
    mkdir(buf, 0700);
}
static bool dispatch(Workspace *w, ClientOperation op, const char *tool, struct json_object *args) {
    if (w->child)
        return false;
    char script[1200];
    const char *configured = getenv("PHYSICS_SIM_SESSION_TOOL");
    if (configured)
        snprintf(script, sizeof(script), "%s", configured);
    else
        snprintf(script, sizeof(script), "%s/scripts/physics_sim_session.py",
                 PHYSICS_SIM_REPO_ROOT);
    char *argv[] = {
        "python3",    script,
        "--root",     w->root,
        "--output",   w->response,
        (char *)tool, (char *)json_object_to_json_string_ext(args, JSON_C_TO_STRING_PLAIN),
        NULL};
    unlink(w->response);
    int result = posix_spawnp(&w->child, "python3", NULL, NULL, argv, environ);
    if (result) {
        w->child = 0;
        snprintf(w->message, sizeof(w->message), "Could not start session client (%d)", result);
        return false;
    }
    w->operation = op;
    return true;
}
static void control(Workspace *w, const char *action) {
    if (!w->run_id[0])
        return;
    if (w->child) {
        snprintf(w->queued_action, sizeof(w->queued_action), "%s", action);
        return;
    }
    struct json_object *args = json_object_new_object();
    char id[65];
    if (w->retry_control)
        snprintf(id, sizeof(id), "%s", w->last_command_id);
    else
        snprintf(id, sizeof(id), "desktop_%ld_%llu", (long)getpid(),
                 (unsigned long long)++w->commands);
    snprintf(w->last_command_id, sizeof(w->last_command_id), "%s", id);
    w->retry_control = false;
    add(args, "run_id", w->run_id);
    add(args, "scene_revision", w->revision);
    add(args, "command_id", id);
    add(args, "action", action);
    json_object_object_add(args, "wait_ms", json_object_new_int(5000));
    if (dispatch(w, CLIENT_CONTROL, "run_control", args)) {
        snprintf(w->last_action, sizeof(w->last_action), "%s", action);
        snprintf(w->message, sizeof(w->message), "%s requested; waiting for solver acknowledgement",
                 action);
    }
    json_object_put(args);
}
static void new_scene(Workspace *w) {
    if (w->child)
        return;
    snprintf(w->scene_id, sizeof(w->scene_id), "desktop_%ld_%ld", (long)getpid(), (long)time(NULL));
    struct json_object *args = json_object_new_object();
    add(args, "scene_id", w->scene_id);
    if (dispatch(w, CLIENT_CREATE, "scene_create", args))
        snprintf(w->message, sizeof(w->message), "Creating Wind scene...");
    json_object_put(args);
}
static void poll_client(Workspace *w) {
    if (!w->child)
        return;
    int status = 0;
    if (waitpid(w->child, &status, WNOHANG) <= 0)
        return;
    w->child = 0;
    struct json_object *reply = json_object_from_file(w->response);
    if (!reply || !WIFEXITED(status) || WEXITSTATUS(status) != 0 || *text(reply, "error")) {
        snprintf(w->message, sizeof(w->message), "%s",
                 reply && *text(reply, "error") ? text(reply, "error") : "Session client failed");
        json_object_put(reply);
        if (w->operation == CLIENT_SAMPLE) {
            json_object_put(w->sample_args);
            w->sample_args = NULL;
        }
        w->operation = CLIENT_IDLE;
        return;
    }
    ClientOperation finished = w->operation;
    w->operation = CLIENT_IDLE;
    struct json_object *args = json_object_new_object();
    if (finished == CLIENT_SAMPLE) {
        if (!strcmp(text(reply, "status"), "ready")) {
            struct json_object *old = get(w->inspection, "preview"), *next = get(reply, "preview");
            if (!old || strcmp(text(old, "plane"), text(next, "plane")) ||
                json_object_get_double(get(old, "extent_u_m")) !=
                    json_object_get_double(get(next, "extent_u_m")) ||
                json_object_get_double(get(old, "extent_v_m")) !=
                    json_object_get_double(get(next, "extent_v_m")))
                w->refit = true;
            json_object_put(w->inspection);
            w->inspection = json_object_get(reply);
            json_object_put(w->sample_args);
            w->sample_args = NULL;
        } else if (strcmp(text(reply, "status"), "pending")) {
            json_object_put(w->sample_args);
            w->sample_args = NULL;
        }
    } else if (finished == CLIENT_CREATE) {
        snprintf(w->revision, sizeof(w->revision), "%s", text(reply, "scene_revision"));
        add(args, "scene_id", w->scene_id);
        add(args, "scene_revision", w->revision);
        dispatch(w, CLIENT_VALIDATE, "scene_validate", args);
        snprintf(w->message, sizeof(w->message), "Validating actual simulation grid...");
    } else if (finished == CLIENT_VALIDATE) {
        add(args, "scene_id", w->scene_id);
        add(args, "scene_revision", w->revision);
        add(args, "request_id", w->scene_id);
        json_object_object_add(args, "steps", json_object_new_int(2000));
        dispatch(w, CLIENT_START, "run_start", args);
        snprintf(w->message, sizeof(w->message), "Starting background session...");
    } else if (finished == CLIENT_CONTROL) {
        w->retry_control = !strcmp(text(reply, "status"), "pending");
        snprintf(w->message, sizeof(w->message), "%s: %s at tick %lld", w->last_action,
                 text(reply, "status"), (long long)json_object_get_int64(get(reply, "tick")));
        if (w->test_stage == 2 && !strcmp(w->last_action, "pause") &&
            !strcmp(text(reply, "status"), "applied")) {
            w->paused_tick = json_object_get_int64(get(reply, "tick"));
            w->test_stage = 3;
        } else if (w->test_stage == 4 && !strcmp(w->last_action, "step") &&
                   !strcmp(text(reply, "status"), "applied")) {
            if (json_object_get_int64(get(reply, "tick")) == w->paused_tick + 1)
                w->test_stage = 5;
            else
                w->test_stage = -1;
        }
    } else if (finished == CLIENT_START)
        snprintf(w->message, sizeof(w->message), "Session ready. Continue to evolve the flow.");
    json_object_put(args);
    json_object_put(reply);
}
static void read_snapshot(Workspace *w) {
    char path[1200];
    snprintf(path, sizeof(path), "%s/active.json", w->root);
    struct json_object *active = json_object_from_file(path);
    if (!active)
        return;
    const char *run = text(active, "run_id");
    if (strlen(run) < sizeof(w->run_id) && !strchr(run, '/') && *run) {
        if (strcmp(w->run_id, run)) {
            json_object_put(w->inspection);
            w->inspection = NULL;
            json_object_put(w->sample_args);
            w->sample_args = NULL;
            w->probe = -1;
        }
        snprintf(w->run_id, sizeof(w->run_id), "%s", run);
        snprintf(path, sizeof(path), "%s/runs/%s/snapshot.json", w->root, run);
        struct json_object *next = json_object_from_file(path);
        if (next) {
            json_object_put(w->snapshot);
            w->snapshot = next;
            snprintf(w->revision, sizeof(w->revision), "%s", text(next, "scene_revision"));
        }
    }
    json_object_put(active);
}
static void label(SDL_Renderer *r, SessionText *font, int x, int y, SDL_Color color,
                  const char *s) {
    session_text_draw(font, r, 0, x, y, color, s);
}
static void fill(SDL_Renderer *r, SDL_Rect rect, SDL_Color c) {
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
    SDL_RenderFillRect(r, &rect);
}
static SDL_Rect button_rect(int i) { return (SDL_Rect){24 + i * 132, 78, 122, 36}; }
static bool hit(SDL_Rect r, int x, int y) {
    return x >= r.x && y >= r.y && x < r.x + r.w && y < r.y + r.h;
}
static const char *fields[] = {"speed",          "dye",        "solid",    "vx", "vy", "vz",
                               "pressure_proxy", "divergence", "vorticity"};
static const char *planes[] = {"XY", "XZ", "YZ"};
static struct json_object *current_preview(Workspace *w) {
    return w->inspection ? get(w->inspection, "preview") : get(w->snapshot, "preview");
}
static void range(Workspace *w, struct json_object *preview, double *lo, double *hi) {
    struct json_object *stat = get(get(preview, "statistics"), fields[w->field]);
    *lo = stat ? json_object_get_double(get(stat, "min")) : 0;
    *hi = stat ? json_object_get_double(get(stat, "max"))
               : json_object_get_double(get(preview, "speed_max"));
    if (w->locked_range) {
        *lo = w->range_low;
        *hi = w->range_high;
    }
    if (*hi <= *lo)
        *hi = *lo + 1e-9;
}
static SDL_Color heat(double t, bool signed_field) {
    t = fmax(0, fmin(1, t));
    if (signed_field) {
        if (t < .5)
            return (SDL_Color){(Uint8)(35 + 420 * t), (Uint8)(75 + 340 * t), 240, 255};
        return (SDL_Color){245, (Uint8)(245 - 390 * (t - .5)), (Uint8)(240 - 430 * (t - .5)), 255};
    }
    return (SDL_Color){(Uint8)(30 + 220 * t), (Uint8)(45 + 140 * sqrt(t)),
                       (Uint8)(100 + 110 * (1 - t)), 255};
}
static void draw_preview(SDL_Renderer *r, Workspace *w, SDL_Rect view) {
    struct json_object *preview = current_preview(w);
    int cols = json_object_get_int(get(preview, "width")),
        rows = json_object_get_int(get(preview, "height"));
    if (cols <= 0 || rows <= 0 || cols > 64 || rows > 64)
        return;
    double extent_u = json_object_get_double(get(preview, "extent_u_m")),
           extent_v = json_object_get_double(get(preview, "extent_v_m"));
    if (extent_u <= 0) {
        extent_u = 2;
        extent_v = 1;
    }
    float dx = extent_u / cols, dy = extent_v / rows;
    double lo, hi;
    range(w, preview, &lo, &hi);
    struct json_object *samples = get(preview, "samples");
    int u = json_object_get_int(get(preview, "u_axis")),
        v = json_object_get_int(get(preview, "v_axis"));
    if (!get(preview, "u_axis")) {
        u = 0;
        v = 1;
    }
    double peak = fmax(1e-12, json_object_get_double(get(preview, "speed_max")));
    SDL_RenderSetClipRect(r, &view);
    for (int y = 0; y < rows; y++)
        for (int x = 0; x < cols; x++) {
            int index = y * cols + x;
            struct json_object *cell = item(samples, index), *value = item(cell, w->field);
            bool solid = json_object_get_boolean(item(cell, 2));
            double t = (json_object_get_double(value) - lo) / (hi - lo);
            SDL_Color color = !value  ? (SDL_Color){255, 0, 255, 255}
                              : solid ? (SDL_Color){210, 218, 229, 255}
                                      : heat(t, lo < 0);
            float sx, sy;
            core_viewport2d_content_to_screen(&w->viewport, x * dx, y * dy, &sx, &sy);
            SDL_Rect rect = {view.x + (int)sx, view.y + (int)sy,
                             (int)ceilf(w->viewport.zoom * dx) + 1,
                             (int)ceilf(w->viewport.zoom * dy) + 1};
            fill(r, rect, color);
            if (index == w->probe) {
                SDL_SetRenderDrawColor(r, 255, 230, 70, 255);
                SDL_RenderDrawRect(r, &rect);
            }
        }
    if (w->vectors)
        for (int y = 2; y < rows; y += 4)
            for (int x = 2; x < cols; x += 4) {
                struct json_object *cell = item(samples, y * cols + x);
                if (json_object_get_boolean(item(cell, 2)))
                    continue;
                double vx = json_object_get_double(item(cell, 3 + u)),
                       vy = json_object_get_double(item(cell, 3 + v));
                float sx, sy;
                core_viewport2d_content_to_screen(&w->viewport, (x + .5f) * dx, (y + .5f) * dy, &sx,
                                                  &sy);
                double len = fmin(28, 3 * fmin(dx, dy) * w->viewport.zoom) / peak;
                float ax = view.x + sx, ay = view.y + sy, bx = ax + vx * len, by = ay + vy * len;
                SDL_SetRenderDrawColor(r, 245, 248, 255, 220);
                SDL_RenderDrawLineF(r, ax, ay, bx, by);
                double angle = atan2(by - ay, bx - ax);
                if (hypot(bx - ax, by - ay) > 2) {
                    SDL_RenderDrawLineF(r, bx, by, bx - 4 * cos(angle - .5),
                                        by - 4 * sin(angle - .5));
                    SDL_RenderDrawLineF(r, bx, by, bx - 4 * cos(angle + .5),
                                        by - 4 * sin(angle + .5));
                }
            }
    SDL_RenderSetClipRect(r, NULL);
}
static SDL_Rect inspection_button(int i) {
    const int x[] = {24, 214, 314, 364, 414, 534, 674}, width[] = {180, 90, 40, 40, 110, 130, 110};
    return (SDL_Rect){x[i], 218, width[i], 34};
}
static void request_sample(Workspace *w) {
    if (!w->run_id[0] || w->child)
        return;
    const char *state = text(w->snapshot, "state");
    if (strcmp(state, "paused") && strcmp(state, "running"))
        return;
    if (!w->sample_args) {
        w->sample_args = json_object_new_object();
        char id[65];
        snprintf(id, sizeof(id), "view_%ld_%llu", (long)getpid(),
                 (unsigned long long)++w->commands);
        add(w->sample_args, "run_id", w->run_id);
        add(w->sample_args, "request_id", id);
        add(w->sample_args, "plane", planes[w->plane]);
        json_object_object_add(w->sample_args, "position", json_object_new_double(w->slice));
        json_object_object_add(w->sample_args, "resolution", json_object_new_int(48));
    }
    dispatch(w, CLIENT_SAMPLE, "run_sample", w->sample_args);
}
static void history_plot(SDL_Renderer *renderer, struct json_object *history, SDL_Rect rect) {
    size_t n = history ? json_object_array_length(history) : 0;
    if (n < 2)
        return;
    double peak = 1e-12;
    for (size_t i = 0; i < n; i++)
        peak = fmax(peak, json_object_get_double(get(item(history, i), "max_divergence")));
    SDL_SetRenderDrawColor(renderer, 110, 185, 235, 255);
    for (size_t i = 1; i < n; i++) {
        double a = json_object_get_double(get(item(history, i - 1), "max_divergence")),
               b = json_object_get_double(get(item(history, i), "max_divergence"));
        SDL_RenderDrawLine(renderer, rect.x + (int)((i - 1) * rect.w / (n - 1)),
                           rect.y + rect.h - (int)(a / peak * rect.h),
                           rect.x + (int)(i * rect.w / (n - 1)),
                           rect.y + rect.h - (int)(b / peak * rect.h));
    }
}

static void *reap_client(void *value) {
    pid_t pid = (pid_t)(intptr_t)value;
    while (waitpid(pid, NULL, 0) < 0 && errno == EINTR) {
    }
    return NULL;
}

int physics_sim_session_workspace_run(const char *root) {
    Workspace w = {.slice = .5, .probe = -1};
    const char *configured = root ? root : getenv("PHYSICS_SIM_SESSION_ROOT");
    snprintf(w.root, sizeof(w.root), "%s",
             configured ? configured : PHYSICS_SIM_REPO_ROOT "/data/runtime/agent_sessions");
    mkdirs(w.root);
    snprintf(w.response, sizeof(w.response), "%s/desktop-%ld.json", w.root, (long)getpid());
    bool own_sdl = !(SDL_WasInit(SDL_INIT_VIDEO) & SDL_INIT_VIDEO), own_ttf = !TTF_WasInit();
    if (own_sdl && SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0)
        return 1;
    if (own_ttf && TTF_Init() != 0)
        return 1;
    SDL_Window *window = SDL_CreateWindow("kinetiC | Live fluid inspection",
                                          SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 860,
                                          SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (window)
        SDL_SetWindowMinimumSize(window, 1100, 800);
    SDL_Renderer *renderer =
        window
            ? SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC)
            : NULL;
    if (window && !renderer)
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    SessionText text_system = {0};
    SessionText *font = &text_system;
    physics_sim_shared_font_load_persisted();
    if (!renderer || !session_text_refresh(font, renderer, window)) {
        session_text_destroy(font);
        if (renderer)
            SDL_DestroyRenderer(renderer);
        if (window)
            SDL_DestroyWindow(window);
        return 1;
    }
    PhysicsSimMenuThemePalette theme = {0};
    physics_sim_shared_theme_resolve_menu_palette(&theme);
    core_viewport2d_init(&w.viewport);
    w.viewport.min_zoom = .0001f;
    w.viewport.max_zoom = 1000000;
    snprintf(w.message, sizeof(w.message),
             "Create a Wind scene, or attach to the latest agent run in this workspace.");
    bool running = true, dragging = false, fitted = false, return_to_setup = false;
    Uint32 last_poll = 0, last_reconcile = 0, last_sample = 0, begin = SDL_GetTicks(),
           previous = begin;
    double max_gap = 0;
    Uint32 max_input = 0, max_client = 0, max_snapshot = 0, max_draw = 0, max_present = 0;
    unsigned frames = 0, inputs = 0;
    const char *test = getenv("PHYSICS_SIM_SESSION_UI_TEST");
    const char *capture = getenv("PHYSICS_SIM_SESSION_CAPTURE");
    bool s2_test = getenv("PHYSICS_SIM_SESSION_S2_TEST") != NULL, s2_requested = false,
         probe_requested = false;
    while (running) {
        Uint32 now = SDL_GetTicks();
        if (frames > 5 && now - previous > max_gap)
            max_gap = now - previous;
        previous = now;
        int width, height;
        SDL_GetWindowSize(window, &width, &height);
        SDL_RenderSetLogicalSize(renderer, width, height);
        session_text_refresh(font, renderer, window);
        SDL_Rect view = {24, 300, width - 360, height - 370};
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_KEYDOWN && (e.key.keysym.mod & (KMOD_CTRL | KMOD_GUI))) {
                if (e.key.keysym.sym == SDLK_EQUALS || e.key.keysym.sym == SDLK_PLUS)
                    font->zoom = fminf(1.5f, font->zoom + .1f);
                if (e.key.keysym.sym == SDLK_MINUS)
                    font->zoom = fmaxf(.8f, font->zoom - .1f);
                if (e.key.keysym.sym == SDLK_0)
                    font->zoom = 1.25f;
            }
            if (e.type == SDL_QUIT ||
                (e.type == SDL_WINDOWEVENT && e.window.windowID == SDL_GetWindowID(window) &&
                 e.window.event == SDL_WINDOWEVENT_CLOSE))
                running = false;
            if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) {
                return_to_setup = true;
                running = false;
            }
            if (e.type == SDL_MOUSEWHEEL) {
                int x, y;
                SDL_GetMouseState(&x, &y);
                core_viewport2d_zoom_at_screen_anchor(&w.viewport, (float)(x - view.x),
                                                      (float)(y - view.y),
                                                      e.wheel.y > 0 ? 1.1f : 1 / 1.1f);
                inputs++;
            }
            if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_RIGHT &&
                hit(view, e.button.x, e.button.y)) {
                struct json_object *p = current_preview(&w);
                int cols = json_object_get_int(get(p, "width")),
                    rows = json_object_get_int(get(p, "height"));
                double eu = json_object_get_double(get(p, "extent_u_m")),
                       ev = json_object_get_double(get(p, "extent_v_m"));
                float x, y;
                core_viewport2d_screen_to_content(&w.viewport, e.button.x - view.x,
                                                  e.button.y - view.y, &x, &y);
                int cx = eu > 0 ? (int)floor(x / eu * cols) : -1,
                    cy = ev > 0 ? (int)floor(y / ev * rows) : -1;
                w.probe = cx >= 0 && cy >= 0 && cx < cols && cy < rows ? cy * cols + cx : -1;
            }
            if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
                if(hit((SDL_Rect){width-250,16,226,36},e.button.x,e.button.y)){
                    return_to_setup=true;running=false;
                }
                for (int i = 0; i < 7; i++)
                    if (hit(inspection_button(i), e.button.x, e.button.y)) {
                        if (i == 0) {
                            w.field = (w.field + 1) % 9;
                            w.locked_range = false;
                        }
                        if (i == 1) {
                            w.plane = (w.plane + 1) % 3;
                            w.probe = -1;
                            fitted = false;
                        }
                        if (i == 2)
                            w.slice = fmax(0, w.slice - .1);
                        if (i == 3)
                            w.slice = fmin(1, w.slice + .1);
                        if (i == 4)
                            w.vectors = !w.vectors;
                        if (i == 5) {
                            if (!w.locked_range) {
                                range(&w, current_preview(&w), &w.range_low, &w.range_high);
                                w.locked_range = true;
                            } else
                                w.locked_range = false;
                        }
                        if (i == 6) {
                            fitted = false;
                            w.slice = .5;
                            w.probe = -1;
                        }
                    }
                for (int i = 0; i < 6; i++)
                    if (hit(button_rect(i), e.button.x, e.button.y)) {
                        if (i == 0)
                            new_scene(&w);
                        if (i == 1)
                            control(&w, "continue");
                        if (i == 2)
                            control(&w, "pause");
                        if (i == 3)
                            control(&w, "step");
                        if (i == 4)
                            control(&w, "cancel");
                        if (i == 5)
                            fitted = false;
                    }
                dragging = hit(view, e.button.x, e.button.y);
                inputs++;
            }
            if (e.type == SDL_MOUSEBUTTONUP)
                dragging = false;
            if (e.type == SDL_MOUSEMOTION && dragging) {
                core_viewport2d_pan_by(&w.viewport, e.motion.xrel, e.motion.yrel);
                inputs++;
            }
        }
        Uint32 phase = SDL_GetTicks();
        if (phase - now > max_input)
            max_input = phase - now;
        poll_client(&w);
        if (w.refit) {
            fitted = false;
            w.refit = false;
        }
        if (w.queued_action[0] && !w.child) {
            char action[32];
            snprintf(action, sizeof(action), "%s", w.queued_action);
            w.queued_action[0] = 0;
            control(&w, action);
        }
        if (w.retry_control && !w.child) {
            char action[32];
            snprintf(action, sizeof(action), "%s", w.last_action);
            control(&w, action);
        }
        Uint32 after_client = SDL_GetTicks();
        if (after_client - phase > max_client)
            max_client = after_client - phase;
        phase = after_client;
        if (now - last_poll >= 100) {
            read_snapshot(&w);
            last_poll = now;
        }
        if (now - last_reconcile >= 1000 && !w.child && w.run_id[0]) {
            struct json_object *args = json_object_new_object();
            add(args, "run_id", w.run_id);
            dispatch(&w, CLIENT_INSPECT, "run_inspect", args);
            json_object_put(args);
            last_reconcile = now;
        }
        if (now - last_sample >= 300 && !w.child) {
            request_sample(&w);
            last_sample = now;
        }
        struct json_object *preview = current_preview(&w);
        int cols = json_object_get_int(get(preview, "width")),
            rows = json_object_get_int(get(preview, "height"));
        if (!fitted && cols > 0 && rows > 0) {
            double eu = json_object_get_double(get(preview, "extent_u_m")),
                   ev = json_object_get_double(get(preview, "extent_v_m"));
            core_viewport2d_reset_to_fit(&w.viewport, view.w, view.h, eu > 0 ? eu : 2,
                                         ev > 0 ? ev : 1);
            fitted = true;
        }
        if (test && !w.child && w.snapshot) {
            if (w.test_stage == 0 && !strcmp(text(w.snapshot, "state"), "paused")) {
                control(&w, "continue");
                w.test_stage = 1;
            } else if (w.test_stage == 1 && json_object_get_int64(get(w.snapshot, "tick")) >= 2) {
                control(&w, "pause");
                w.test_stage = 2;
            } else if (w.test_stage == 3) {
                control(&w, "step");
                w.test_stage = 4;
            }
        }
        if (test && frames % 5 == 0 && w.test_stage > 0 && w.test_stage < 5) {
            SDL_Event wheel = {0};
            wheel.type = SDL_MOUSEWHEEL;
            wheel.wheel.y = frames % 10 == 0 ? 1 : -1;
            SDL_PushEvent(&wheel);
        }
        Uint32 after_snapshot = SDL_GetTicks();
        if (after_snapshot - phase > max_snapshot)
            max_snapshot = after_snapshot - phase;
        phase = after_snapshot;
        fill(renderer, (SDL_Rect){0, 0, width, height}, theme.background_fill);
        session_text_draw(font, renderer, 1, 24, 16, theme.text_primary,
                          "kinetiC  /  Live fluid inspection");
        label(renderer, font, 24, 48, theme.text_muted,
              "Independent Wind sessions  |  Presets, 2D/3D and all modes are in Setup");
        fill(renderer,(SDL_Rect){width-250,16,226,36},theme.button_fill);
        label(renderer,font,width-240,24,theme.text_primary,"Setup & modes [Esc]");
        const char *buttons[] = {"Wind example", "Continue", "Pause", "Step", "Cancel", "Fit view"};
        for (int i = 0; i < 6; i++) {
            SDL_Rect b = button_rect(i);
            fill(renderer, b, w.child ? theme.panel_fill : theme.button_fill);
            label(renderer, font, b.x + 10, b.y + 7, theme.text_primary, buttons[i]);
        }
        label(renderer, font, 24, 126, theme.text_primary, w.message);
        char line[256];
        snprintf(line, sizeof(line), "Run: %s     State: %s     Tick: %lld / %lld",
                 w.run_id[0] ? w.run_id : "none", text(w.snapshot, "state"),
                 (long long)json_object_get_int64(get(w.snapshot, "tick")),
                 (long long)json_object_get_int64(get(w.snapshot, "tick_limit")));
        label(renderer, font, 24, 160, theme.text_primary, line);
        snprintf(line, sizeof(line), "Simulated %.4f s   |   Last tick %.1f ms   |   Scene %.12s",
                 json_object_get_double(get(w.snapshot, "simulation_time")),
                 json_object_get_double(get(w.snapshot, "step_ms")), w.revision);
        label(renderer, font, 24, 187, theme.text_muted, line);
        char field_label[64], plane_label[32];
        snprintf(field_label, sizeof(field_label), "Field: %s", fields[w.field]);
        snprintf(plane_label, sizeof(plane_label), "%s", planes[w.plane]);
        const char *inspect_buttons[] = {field_label,
                                         plane_label,
                                         "-",
                                         "+",
                                         w.vectors ? "Vectors on" : "Vectors off",
                                         w.locked_range ? "Range locked" : "Auto range",
                                         "Center"};
        for (int i = 0; i < 7; i++) {
            SDL_Rect b = inspection_button(i);
            fill(renderer, b, theme.button_fill);
            session_text_draw(font, renderer, 2, b.x + 7, b.y + 8, theme.text_primary,
                              inspect_buttons[i]);
        }
        snprintf(
            line, sizeof(line), "Requested %s %.0f%% | Showing %s cell %d | Sample tick %lld | %s",
            planes[w.plane], w.slice * 100, text(preview, "plane"),
            json_object_get_int(get(preview, "slice_index")),
            (long long)json_object_get_int64(get(w.inspection ? w.inspection : w.snapshot, "tick")),
            fields[w.field]);
        label(renderer, font, 24, 267, theme.text_primary, line);
        fill(renderer, view, (SDL_Color){12, 20, 36, 255});
        draw_preview(renderer, &w, view);
        int side = width - 312;
        struct json_object *health = get(w.snapshot, "health");
        label(renderer, font, side, 300, theme.text_primary, "Solver health");
        snprintf(line, sizeof(line), "Divergence %.4g",
                 json_object_get_double(get(health, "max_divergence")));
        label(renderer, font, side, 330, theme.text_muted, line);
        snprintf(line, sizeof(line), "Clamped cells %lld",
                 (long long)json_object_get_int64(get(health, "velocity_clamped_cells")));
        label(renderer, font, side, 358, theme.text_muted, line);
        snprintf(line, sizeof(line), "Skipped regions %lld",
                 (long long)json_object_get_int64(get(health, "skipped_clusters")));
        label(renderer, font, side, 386, theme.text_muted, line);
        session_text_draw(font, renderer, 2, side, 422, theme.text_muted,
                          "Divergence / last 128 publications");
        history_plot(renderer, get(w.snapshot, "history"), (SDL_Rect){side, 448, 280, 55});
        double lo, hi;
        range(&w, preview, &lo, &hi);
        for (int i = 0; i < 280; i++)
            fill(renderer, (SDL_Rect){side + i, 523, 1, 12}, heat(i / 279.0, lo < 0));
        snprintf(line, sizeof(line), "%.3g to %.3g %s", lo, hi,
                 w.locked_range ? "(locked)" : "(auto)");
        label(renderer, font, side, 545, theme.text_primary, line);
        const char *units = json_object_get_string(item(get(preview, "units"), w.field));
        label(renderer, font, side, 573, theme.text_muted, units ? units : "m/s");
        struct json_object *cell = w.probe >= 0 ? item(get(preview, "samples"), w.probe) : NULL;
        if (cell) {
            snprintf(line, sizeof(line), "Probe sample %d: %.5g", w.probe,
                     json_object_get_double(item(cell, w.field)));
            label(renderer, font, side, 612, theme.text_primary, line);
            snprintf(line, sizeof(line), "v=(%.3g, %.3g, %.3g)",
                     json_object_get_double(item(cell, 3)), json_object_get_double(item(cell, 4)),
                     json_object_get_double(item(cell, 5)));
            session_text_draw(font, renderer, 2, side, 642, theme.text_muted, line);
            int dims[3];
            double xyz[3];
            for (int a = 0; a < 3; a++) {
                dims[a] = json_object_get_int(item(get(preview, "grid"), a));
                xyz[a] = json_object_get_double(item(get(preview, "world_bounds_m"), a));
            }
            int u = json_object_get_int(get(preview, "u_axis")),
                v = json_object_get_int(get(preview, "v_axis")),
                n = json_object_get_int(get(preview, "normal_axis"));
            int c = json_object_get_int(get(preview, "width")),
                r = json_object_get_int(get(preview, "height"));
            double dx = json_object_get_double(get(w.snapshot, "voxel_size_m"));
            if (c > 0 && r > 0) {
                xyz[u] += (w.probe % c * dims[u] / c + .5) * dx;
                xyz[v] += (w.probe / c * dims[v] / r + .5) * dx;
                xyz[n] += (json_object_get_int(get(preview, "slice_index")) + .5) * dx;
            }
            snprintf(line, sizeof(line), "XYZ %.3g, %.3g, %.3g m", xyz[0], xyz[1], xyz[2]);
            session_text_draw(font, renderer, 2, side, 666, theme.text_muted, line);
        } else
            label(renderer, font, side, 612, theme.text_muted, "Right-click a cell to probe");
        session_text_draw(font, renderer, 2, side, 682, theme.text_muted,
                          "Pressure is a solver proxy.");
        session_text_draw(font, renderer, 2, side, 706, theme.text_muted,
                          "Derivatives are local diagnostics.");
        session_text_draw(font, renderer, 2, side, 730, theme.text_muted,
                          "White: solid | Magenta: nonfinite");
        if (*text(w.snapshot, "error"))
            label(renderer, font, 24, height - 60, (SDL_Color){255, 120, 100, 255},
                  text(w.snapshot, "error"));
        session_text_draw(
            font, renderer, 2, 24, height - 32, theme.text_muted,
            "Drag: pan | Wheel: zoom | Ctrl/Cmd +/-: text size | Closing this window leaves "
            "the run available to agents");
        Uint32 after_draw = SDL_GetTicks();
        if (after_draw - phase > max_draw)
            max_draw = after_draw - phase;
        if (test && s2_test && w.test_stage == 5 && !s2_requested) {
            int actions[] = {1, 4, 0, 0, 0, 0, 0, 0, 0, 0, 5};
            for (size_t a = 0; a < sizeof(actions) / sizeof(actions[0]); a++) {
                SDL_Rect b = inspection_button(actions[a]);
                SDL_Event click = {0};
                click.type = SDL_MOUSEBUTTONDOWN;
                click.button.button = SDL_BUTTON_LEFT;
                click.button.x = b.x + 10;
                click.button.y = b.y + 10;
                SDL_PushEvent(&click);
            }
            s2_requested = true;
        }
        bool s2_ready = s2_requested && w.inspection &&
                        !strcmp(text(current_preview(&w), "plane"), "XZ") &&
                        json_object_get_int64(get(w.inspection, "tick")) == w.paused_tick + 1;
        if (test && s2_ready && !probe_requested) {
            SDL_Event click = {0};
            click.type = SDL_MOUSEBUTTONDOWN;
            click.button.button = SDL_BUTTON_RIGHT;
            click.button.x = view.x + view.w / 2;
            click.button.y = view.y + view.h / 2;
            SDL_PushEvent(&click);
            probe_requested = true;
        }
        if (test && ((w.test_stage == 5 && (!s2_test || (s2_ready && w.probe >= 0))) ||
                     w.test_stage < 0 || now - begin > 30000)) {
            if (capture) {
                int rw, rh;
                SDL_GetRendererOutputSize(renderer, &rw, &rh);
                SDL_Surface *surface =
                    SDL_CreateRGBSurfaceWithFormat(0, rw, rh, 32, SDL_PIXELFORMAT_ARGB8888);
                if (surface && SDL_RenderReadPixels(renderer, NULL, surface->format->format,
                                                    surface->pixels, surface->pitch) == 0)
                    SDL_SaveBMP(surface, capture);
                SDL_FreeSurface(surface);
            }
            struct json_object *proof = json_object_new_object();
            json_object_object_add(
                proof, "completed_control_sequence",
                json_object_new_boolean(w.test_stage == 5 && (!s2_test || s2_ready)));
            json_object_object_add(proof, "frames", json_object_new_int64(frames));
            json_object_object_add(proof, "navigation_events", json_object_new_int64(inputs));
            json_object_object_add(proof, "max_frame_gap_ms", json_object_new_double(max_gap));
            json_object_object_add(
                proof, "last_solver_tick_ms",
                json_object_new_double(json_object_get_double(get(w.snapshot, "step_ms"))));
            json_object_object_add(proof, "s2_inspection_ready",
                                   json_object_new_boolean(s2_ready && w.probe >= 0));
            json_object_object_add(proof, "inspection_plane",
                                   json_object_new_string(text(current_preview(&w), "plane")));
            json_object_object_add(proof, "inspection_field",
                                   json_object_new_string(fields[w.field]));
            json_object_object_add(proof, "font_drawable_scale",
                                   json_object_new_double(font->scale));
            json_object_object_add(proof, "font_logical_size",
                                   json_object_new_int(font->logical_sizes[0]));
            json_object_object_add(proof, "font_raster_size",
                                   json_object_new_int(font->raster_sizes[0]));
            json_object_object_add(proof, "max_input_ms", json_object_new_int(max_input));
            json_object_object_add(proof, "max_client_ms", json_object_new_int(max_client));
            json_object_object_add(proof, "max_snapshot_ms", json_object_new_int(max_snapshot));
            json_object_object_add(proof, "max_draw_ms", json_object_new_int(max_draw));
            json_object_object_add(proof, "max_present_ms", json_object_new_int(max_present));
            json_object_to_file_ext(test, proof, JSON_C_TO_STRING_PRETTY);
            json_object_put(proof);
            running = false;
        }
        Uint32 present_begin = SDL_GetTicks();
        SDL_RenderPresent(renderer);
        Uint32 present_ms = SDL_GetTicks() - present_begin;
        if (present_ms > max_present)
            max_present = present_ms;
        frames++;
        SDL_Delay(8);
    }
    if (w.child) {
        // A bounded command client can finish after the workspace closes. Reap off the UI thread.
        pthread_t reaper;
        if (pthread_create(&reaper, NULL, reap_client, (void *)(intptr_t)w.child) == 0)
            pthread_detach(reaper);
    }
    json_object_put(w.snapshot);
    json_object_put(w.inspection);
    json_object_put(w.sample_args);
    session_text_destroy(font);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    if (own_ttf)
        TTF_Quit();
    if (own_sdl)
        SDL_Quit();
    return return_to_setup ? 2 : (test && w.test_stage != 5 ? 1 : 0);
}
