#include "render/particle_overlay.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "physics/fluid2d/fluid2d.h"
#include "physics/math/math2d.h"

#define FLOW_PARTICLE_CAPACITY   1500
#define FLOW_SPAWN_RATE_PER_SEC  600.0f
#define FLOW_MIN_LIFETIME        4.0f
#define FLOW_MAX_LIFETIME        12.5f
#define FLOW_TRAIL_MAX_POINTS    32

typedef struct FlowParticle {
    float x;
    float y;
    float life;
    float max_life;
    float trail_x[FLOW_TRAIL_MAX_POINTS];
    float trail_y[FLOW_TRAIL_MAX_POINTS];
    int   trail_count;
} FlowParticle;

static FlowParticle *g_particles = NULL;
static int g_capacity            = 0;
static int g_grid_w              = 0;
static int g_grid_h              = 0;
static float g_spawn_accumulator = 0.0f;
static int g_next_slot           = 0;

static float randf01(void) {
    return (float)rand() / (float)RAND_MAX;
}

static inline int clamp_index(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static bool point_in_solid(const SceneState *scene, float x, float y) {
    if (!scene || !scene->config || !scene->obstacle_mask) return false;
    int w = scene->config->grid_w;
    int h = scene->config->grid_h;
    if (w <= 0 || h <= 0) return false;
    int ix = clamp_index((int)lroundf(x), 0, w - 1);
    int iy = clamp_index((int)lroundf(y), 0, h - 1);
    return scene->obstacle_mask[(size_t)iy * (size_t)w + (size_t)ix] != 0;
}

static bool sample_velocity_safe(const SceneState *scene,
                                 float x,
                                 float y,
                                 Vec2 *out_vel) {
    if (!scene || !scene->smoke || !out_vel) return false;
    if (!point_in_solid(scene, x, y)) {
        fluid2d_sample_velocity(scene->smoke, x, y, out_vel);
        return true;
    }

    const int max_radius = 3;
    for (int r = 1; r <= max_radius; ++r) {
        for (int dy = -r; dy <= r; ++dy) {
            for (int dx = -r; dx <= r; ++dx) {
                int nx = (int)lroundf(x) + dx;
                int ny = (int)lroundf(y) + dy;
                if (nx < 0 || ny < 0 || nx >= g_grid_w || ny >= g_grid_h) continue;
                if (!point_in_solid(scene, (float)nx, (float)ny)) {
                    fluid2d_sample_velocity(scene->smoke, (float)nx, (float)ny, out_vel);
                    return true;
                }
            }
        }
    }
    out_vel->x = 0.0f;
    out_vel->y = 0.0f;
    return false;
}

bool particle_overlay_init(int grid_w, int grid_h) {
    if (g_particles) return true;
    g_capacity = FLOW_PARTICLE_CAPACITY;
    g_particles = (FlowParticle *)calloc((size_t)g_capacity, sizeof(FlowParticle));
    if (!g_particles) {
        g_capacity = 0;
        return false;
    }

    if (grid_w <= 0) grid_w = 1;
    if (grid_h <= 0) grid_h = 1;
    g_grid_w = grid_w;
    g_grid_h = grid_h;
    g_spawn_accumulator = 0.0f;
    g_next_slot = 0;
    srand((unsigned int)time(NULL));
    return true;
}

void particle_overlay_shutdown(void) {
    free(g_particles);
    g_particles = NULL;
    g_capacity = 0;
    g_spawn_accumulator = 0.0f;
    g_next_slot = 0;
}

void particle_overlay_reset(void) {
    if (!g_particles) return;
    memset(g_particles, 0, (size_t)g_capacity * sizeof(FlowParticle));
    g_spawn_accumulator = 0.0f;
    g_next_slot = 0;
}

static void flow_particle_reset_trail(FlowParticle *pt, float x, float y) {
    pt->trail_count = 0;
    for (int i = 0; i < FLOW_TRAIL_MAX_POINTS; ++i) {
        pt->trail_x[i] = x;
        pt->trail_y[i] = y;
    }
    pt->trail_count = 1;
    pt->trail_x[0] = x;
    pt->trail_y[0] = y;
}

static void flow_particle_push_trail(FlowParticle *pt, float x, float y) {
    if (!pt) return;
    if (pt->trail_count < FLOW_TRAIL_MAX_POINTS) {
        pt->trail_x[pt->trail_count] = x;
        pt->trail_y[pt->trail_count] = y;
        pt->trail_count++;
    } else {
        memmove(pt->trail_x, pt->trail_x + 1, (FLOW_TRAIL_MAX_POINTS - 1) * sizeof(float));
        memmove(pt->trail_y, pt->trail_y + 1, (FLOW_TRAIL_MAX_POINTS - 1) * sizeof(float));
        pt->trail_x[FLOW_TRAIL_MAX_POINTS - 1] = x;
        pt->trail_y[FLOW_TRAIL_MAX_POINTS - 1] = y;
    }
}

static void spawn_particle(const SceneState *scene) {
    if (!g_particles || g_capacity == 0) return;
    FlowParticle *pt = &g_particles[g_next_slot];
    g_next_slot = (g_next_slot + 1) % g_capacity;

    float x = randf01() * (float)g_grid_w;
    float y = randf01() * (float)g_grid_h;
    if (scene) {
        int attempts = 0;
        while (point_in_solid(scene, x, y) && attempts < 5) {
            x = randf01() * (float)g_grid_w;
            y = randf01() * (float)g_grid_h;
            attempts++;
        }
        if (point_in_solid(scene, x, y)) {
            pt->life = 0.0f;
            pt->max_life = 0.0f;
            return;
        }
    }
    float life = FLOW_MIN_LIFETIME +
                 randf01() * (FLOW_MAX_LIFETIME - FLOW_MIN_LIFETIME);
    pt->x = x;
    pt->y = y;
    pt->life = life;
    pt->max_life = life;
    flow_particle_reset_trail(pt, x, y);
}

static void update_particle(FlowParticle *pt,
                            const SceneState *scene,
                            float dt_seconds) {
    if (!pt || pt->life <= 0.0f) return;
    pt->life -= dt_seconds;
    if (pt->life <= 0.0f) {
        return;
    }

    Vec2 vel = vec2(0.0f, 0.0f);
    if (!sample_velocity_safe(scene, pt->x, pt->y, &vel)) {
        pt->life = 0.0f;
        return;
    }

    float new_x = pt->x + vel.x * dt_seconds;
    float new_y = pt->y + vel.y * dt_seconds;

    if (new_x < 0.0f || new_x >= (float)g_grid_w ||
        new_y < 0.0f || new_y >= (float)g_grid_h) {
        pt->life = 0.0f;
        return;
    }

    if (point_in_solid(scene, new_x, new_y)) {
        pt->life = 0.0f;
        return;
    }

    pt->x = new_x;
    pt->y = new_y;
    flow_particle_push_trail(pt, pt->x, pt->y);
}

void particle_overlay_update(const SceneState *scene, double dt) {
    if (!scene || !scene->smoke || !g_particles) return;
    if (dt <= 0.0) dt = 1.0 / 60.0;
    float fdt = (float)dt;

    g_spawn_accumulator += FLOW_SPAWN_RATE_PER_SEC * fdt;
    int spawn_count = (int)g_spawn_accumulator;
    if (spawn_count > 0) {
        g_spawn_accumulator -= (float)spawn_count;
        if (spawn_count > g_capacity) spawn_count = g_capacity;
        for (int i = 0; i < spawn_count; ++i) {
            spawn_particle(scene);
        }
    }

    for (int i = 0; i < g_capacity; ++i) {
        update_particle(&g_particles[i], scene, fdt);
    }
}

void particle_overlay_draw(const SceneState *scene,
                           SDL_Renderer *renderer,
                           int window_w,
                           int window_h) {
    if (!scene || !scene->smoke || !renderer || !g_particles) return;

    const Fluid2D *grid = scene->smoke;
    if (grid->w <= 0 || grid->h <= 0) return;

    float scale_x = (float)window_w / (float)grid->w;
    float scale_y = (float)window_h / (float)grid->h;

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    for (int i = 0; i < g_capacity; ++i) {
        FlowParticle *pt = &g_particles[i];
        if (pt->life <= 0.0f || pt->max_life <= 0.0f) continue;

        if (pt->trail_count < 2) continue;

        float life_ratio = pt->life / pt->max_life;
        for (int j = 1; j < pt->trail_count; ++j) {
            float t = (float)j / (float)(pt->trail_count - 1);
            float strength = (0.2f + 0.8f * t) * (0.6f + 0.4f * life_ratio);
            if (strength > 1.0f) strength = 1.0f;
            Uint8 r = (Uint8)lroundf(30.0f * (1.0f - t));
            Uint8 g = (Uint8)lroundf(140.0f + 110.0f * t);
            Uint8 b = (Uint8)lroundf(50.0f * (1.0f - t));
            Uint8 a = (Uint8)lroundf(220.0f * strength);

            float x0 = pt->trail_x[j - 1] * scale_x;
            float y0 = pt->trail_y[j - 1] * scale_y;
            float x1 = pt->trail_x[j] * scale_x;
            float y1 = pt->trail_y[j] * scale_y;

            SDL_SetRenderDrawColor(renderer, r, g, b, a);
            SDL_RenderDrawLine(renderer,
                               (int)lroundf(x0),
                               (int)lroundf(y0),
                               (int)lroundf(x1),
                               (int)lroundf(y1));
        }
    }
}
