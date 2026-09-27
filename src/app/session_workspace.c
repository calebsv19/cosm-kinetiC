#define _DARWIN_C_SOURCE 1
#include "app/session_workspace.h"
#include "app/menu/shared_theme_font_adapter.h"
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
    CLIENT_INSPECT
} ClientOperation;
typedef struct Workspace {
    char root[1024], response[1200], run_id[65], revision[65], scene_id[65], message[256];
    pid_t child;
    ClientOperation operation;
    struct json_object *snapshot;
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
    if (!w->run_id[0] || w->child)
        return;
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
        w->operation = CLIENT_IDLE;
        return;
    }
    ClientOperation finished = w->operation;
    w->operation = CLIENT_IDLE;
    struct json_object *args = json_object_new_object();
    if (finished == CLIENT_CREATE) {
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
static void label(SDL_Renderer *r, TTF_Font *font, int x, int y, SDL_Color color, const char *s) {
    SDL_Surface *surface = TTF_RenderUTF8_Blended(font, s, color);
    if (!surface)
        return;
    SDL_Texture *texture = SDL_CreateTextureFromSurface(r, surface);
    SDL_Rect dst = {x, y, surface->w, surface->h};
    SDL_FreeSurface(surface);
    if (texture) {
        SDL_RenderCopy(r, texture, NULL, &dst);
        SDL_DestroyTexture(texture);
    }
}
static void fill(SDL_Renderer *r, SDL_Rect rect, SDL_Color c) {
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
    SDL_RenderFillRect(r, &rect);
}
static SDL_Rect button_rect(int i) { return (SDL_Rect){24 + i * 132, 78, 122, 36}; }
static bool hit(SDL_Rect r, int x, int y) {
    return x >= r.x && y >= r.y && x < r.x + r.w && y < r.y + r.h;
}
static void draw_preview(SDL_Renderer *r, Workspace *w, SDL_Rect view) {
    struct json_object *preview = get(w->snapshot, "preview");
    int cols = json_object_get_int(get(preview, "width")),
        rows = json_object_get_int(get(preview, "height"));
    if (cols <= 0 || rows <= 0 || cols > 64 || rows > 64)
        return;
    struct json_object *grid = get(w->snapshot, "effective_grid");
    float dx = (float)json_object_get_int(json_object_array_get_idx(grid, 0)) / cols;
    float dy = (float)json_object_get_int(json_object_array_get_idx(grid, 1)) / rows;
    double peak = json_object_get_double(get(preview, "speed_max"));
    if (peak <= 0)
        peak = 1;
    struct json_object *samples = get(preview, "samples");
    SDL_RenderSetClipRect(r, &view);
    for (int y = 0; y < rows; y++)
        for (int x = 0; x < cols; x++) {
            struct json_object *cell = json_object_array_get_idx(samples, (size_t)y * cols + x);
            double speed = json_object_get_double(json_object_array_get_idx(cell, 0));
            int solid = json_object_get_int(json_object_array_get_idx(cell, 2));
            double t = fmax(0, fmin(1, speed / peak));
            SDL_Color color = solid
                                  ? (SDL_Color){210, 218, 229, 255}
                                  : (SDL_Color){(Uint8)(30 + 220 * t), (Uint8)(45 + 140 * sqrt(t)),
                                                (Uint8)(100 + 110 * (1 - t)), 255};
            float sx, sy;
            core_viewport2d_content_to_screen(&w->viewport, (float)x * dx, (float)y * dy, &sx, &sy);
            SDL_Rect rect = {view.x + (int)sx, view.y + (int)sy,
                             (int)ceilf(w->viewport.zoom * dx) + 1,
                             (int)ceilf(w->viewport.zoom * dy) + 1};
            fill(r, rect, color);
        }
    SDL_RenderSetClipRect(r, NULL);
}

static void *reap_client(void *value) {
    pid_t pid = (pid_t)(intptr_t)value;
    while (waitpid(pid, NULL, 0) < 0 && errno == EINTR) {
    }
    return NULL;
}

int physics_sim_session_workspace_run(const char *root) {
    Workspace w = {0};
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
    SDL_Window *window = SDL_CreateWindow("kinetiC | Agent simulation workspace",
                                          SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1180, 760,
                                          SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (window)
        SDL_SetWindowMinimumSize(window, 900, 640);
    SDL_Renderer *renderer =
        window
            ? SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC)
            : NULL;
    if (window && !renderer)
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    char font_path[512] = {0};
    int size = 18;
    physics_sim_shared_font_resolve_menu_body(font_path, sizeof(font_path), &size);
    TTF_Font *font = TTF_OpenFont(*font_path ? font_path : FONT_BODY_PATH_1, 18);
    if (!renderer || !font) {
        if (renderer)
            SDL_DestroyRenderer(renderer);
        if (window)
            SDL_DestroyWindow(window);
        return 1;
    }
    PhysicsSimMenuThemePalette theme = {0};
    physics_sim_shared_theme_resolve_menu_palette(&theme);
    core_viewport2d_init(&w.viewport);
    w.viewport.min_zoom = .1f;
    w.viewport.max_zoom = 200;
    snprintf(w.message, sizeof(w.message),
             "Create a Wind scene, or attach to the latest agent run in this workspace.");
    bool running = true, dragging = false, fitted = false;
    Uint32 last_poll = 0, last_reconcile = 0, begin = SDL_GetTicks(), previous = begin;
    double max_gap = 0;
    unsigned frames = 0, inputs = 0;
    const char *test = getenv("PHYSICS_SIM_SESSION_UI_TEST");
    const char *capture = getenv("PHYSICS_SIM_SESSION_CAPTURE");
    while (running) {
        Uint32 now = SDL_GetTicks();
        if (frames > 5 && now - previous > max_gap)
            max_gap = now - previous;
        previous = now;
        int width, height;
        SDL_GetWindowSize(window, &width, &height);
        SDL_RenderSetLogicalSize(renderer, width, height);
        SDL_Rect view = {24, 238, width - 344, height - 306};
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT ||
                (e.type == SDL_WINDOWEVENT && e.window.windowID == SDL_GetWindowID(window) &&
                 e.window.event == SDL_WINDOWEVENT_CLOSE))
                running = false;
            if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE)
                running = false;
            if (e.type == SDL_MOUSEWHEEL) {
                int x, y;
                SDL_GetMouseState(&x, &y);
                core_viewport2d_zoom_at_screen_anchor(&w.viewport, (float)(x - view.x),
                                                      (float)(y - view.y),
                                                      e.wheel.y > 0 ? 1.1f : 1 / 1.1f);
                inputs++;
            }
            if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
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
        poll_client(&w);
        if (w.retry_control && !w.child) {
            char action[32];
            snprintf(action, sizeof(action), "%s", w.last_action);
            control(&w, action);
        }
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
        struct json_object *preview = get(w.snapshot, "preview");
        int cols = json_object_get_int(get(preview, "width")),
            rows = json_object_get_int(get(preview, "height"));
        if (!fitted && cols > 0 && rows > 0) {
            core_viewport2d_reset_to_fit(&w.viewport, view.w, view.h,
                                         json_object_get_int(json_object_array_get_idx(
                                             get(w.snapshot, "effective_grid"), 0)),
                                         json_object_get_int(json_object_array_get_idx(
                                             get(w.snapshot, "effective_grid"), 1)));
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
        fill(renderer, (SDL_Rect){0, 0, width, height}, theme.background_fill);
        label(renderer, font, 24, 20, theme.text_primary, "kinetiC  /  Fluid simulation workspace");
        label(renderer, font, 24, 48, theme.text_muted,
              "Shared agent + desktop session  |  Wind model: approximate");
        const char *buttons[] = {"New Wind", "Continue", "Pause", "Step", "Cancel", "Fit view"};
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
        label(renderer, font, 24, 214, theme.text_primary,
              "Velocity magnitude  /  XY midpoint slice  /  m/s");
        fill(renderer, view, (SDL_Color){12, 20, 36, 255});
        draw_preview(renderer, &w, view);
        int side = width - 296;
        label(renderer, font, side, 238, theme.text_primary, "Solver health");
        struct json_object *health = get(w.snapshot, "health");
        snprintf(line, sizeof(line), "Divergence: %.4g",
                 json_object_get_double(get(health, "max_divergence")));
        label(renderer, font, side, 272, theme.text_muted, line);
        snprintf(line, sizeof(line), "Clamped cells: %lld",
                 (long long)json_object_get_int64(get(health, "velocity_clamped_cells")));
        label(renderer, font, side, 302, theme.text_muted, line);
        snprintf(line, sizeof(line), "Skipped regions: %lld",
                 (long long)json_object_get_int64(get(health, "skipped_clusters")));
        label(renderer, font, side, 332, theme.text_muted, line);
        snprintf(line, sizeof(line), "Active bricks: %lld",
                 (long long)json_object_get_int64(get(health, "active_bricks")));
        label(renderer, font, side, 362, theme.text_muted, line);
        snprintf(line, sizeof(line), "Speed range: 0 - %.3g",
                 json_object_get_double(get(preview, "speed_max")));
        label(renderer, font, side, 412, theme.text_primary, line);
        label(renderer, font, side, 444, theme.text_muted, "Auto-scaled per snapshot");
        label(renderer, font, side, 474, theme.text_muted, "White cells: solid obstacle");
        label(renderer, font, side, 520, theme.text_muted, "Pressure/drag are proxies.");
        label(renderer, font, side, 548, theme.text_muted, "Convergence not validated.");
        if (*text(w.snapshot, "error"))
            label(renderer, font, 24, height - 60, (SDL_Color){255, 120, 100, 255},
                  text(w.snapshot, "error"));
        label(renderer, font, 24, height - 32, theme.text_muted,
              "Drag to pan  |  Wheel to zoom  |  Fit view to reset  |  Closing this window leaves "
              "the run available to agents");
        if (test && (w.test_stage == 5 || w.test_stage < 0 || now - begin > 20000)) {
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
            json_object_object_add(proof, "completed_control_sequence",
                                   json_object_new_boolean(w.test_stage == 5));
            json_object_object_add(proof, "frames", json_object_new_int64(frames));
            json_object_object_add(proof, "navigation_events", json_object_new_int64(inputs));
            json_object_object_add(proof, "max_frame_gap_ms", json_object_new_double(max_gap));
            json_object_object_add(
                proof, "last_solver_tick_ms",
                json_object_new_double(json_object_get_double(get(w.snapshot, "step_ms"))));
            json_object_to_file_ext(test, proof, JSON_C_TO_STRING_PRETTY);
            json_object_put(proof);
            running = false;
        }
        SDL_RenderPresent(renderer);
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
    TTF_CloseFont(font);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    if (own_ttf)
        TTF_Quit();
    if (own_sdl)
        SDL_Quit();
    return test && w.test_stage != 5 ? 1 : 0;
}
