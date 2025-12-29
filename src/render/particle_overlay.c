#include "render/particle_overlay.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "physics/fluid2d/fluid2d.h"
#include "physics/math/math2d.h"

// Tunables: adjust these to change how the flow trails look/behave.
static const float FLOW_COLOR_MIX        = 0.3f; // 0 = white, 1 = bright green
static const float FLOW_ALPHA            = 0.1f; // 0 = invisible, 1 = fully opaque
static const float FLOW_SPAWN_RATE_PER_SEC = 600.0f;
static const float FLOW_MIN_LIFETIME     = 4.0f;
static const float FLOW_MAX_LIFETIME     = 50.0f;
static const float FLOW_LIFE_JITTER      = 0.25f; // +/- jitter around chosen lifetime
static const float FLOW_FADE_FRAMES      = 90.0f; // frame-equivalents for fade-out
static const int   FLOW_TRAIL_MAX_POINTS = 32;
static const int   FLOW_PARTICLE_CAPACITY = 2400;
static const int   FLOW_FADE_CAPACITY     = 3000;

typedef struct FlowParticle {
    float x;
    float y;
    float life;
    float max_life;
    float fade_frames_left;
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
static float g_flow_alpha        = FLOW_ALPHA; // 0-1 opacity multiplier

typedef struct FlowFadeTrail {
    float frames_left;
    float start_strength;
    int   count;
    float xs[FLOW_TRAIL_MAX_POINTS];
    float ys[FLOW_TRAIL_MAX_POINTS];
} FlowFadeTrail;

static FlowFadeTrail *g_fade_trails = NULL;
static int g_fade_capacity          = 0;
static SDL_Color g_color_base       = {255, 255, 255, 255};
static SDL_Color g_color_mix        = {80, 255, 120, 255};

static float randf01(void) {
    return (float)rand() / (float)RAND_MAX;
}

static inline int clamp_index(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static SDL_Color mix_color(SDL_Color a, SDL_Color b, float t) {
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    SDL_Color out;
    out.r = (Uint8)lroundf((1.0f - t) * (float)a.r + t * (float)b.r);
    out.g = (Uint8)lroundf((1.0f - t) * (float)a.g + t * (float)b.g);
    out.b = (Uint8)lroundf((1.0f - t) * (float)a.b + t * (float)b.b);
    out.a = 255;
    return out;
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
    if (!g_particles) {
        g_capacity = FLOW_PARTICLE_CAPACITY;
        g_particles = (FlowParticle *)calloc((size_t)g_capacity, sizeof(FlowParticle));
        if (!g_particles) {
            g_capacity = 0;
            return false;
        }
    }

    if (!g_fade_trails) {
        g_fade_capacity = FLOW_FADE_CAPACITY;
        g_fade_trails = (FlowFadeTrail *)calloc((size_t)g_fade_capacity, sizeof(FlowFadeTrail));
        if (!g_fade_trails) {
            free(g_particles);
            g_particles = NULL;
            g_capacity = 0;
            g_fade_capacity = 0;
            return false;
        }
    }

    if (grid_w <= 0) grid_w = 1;
    if (grid_h <= 0) grid_h = 1;
    g_grid_w = grid_w;
    g_grid_h = grid_h;
    g_spawn_accumulator = 0.0f;
    g_next_slot = 0;
    g_flow_alpha = FLOW_ALPHA;
    srand((unsigned int)time(NULL));
    g_color_base = (SDL_Color){255, 255, 255, 255};
    g_color_mix  = (SDL_Color){80, 255, 120, 255};
    return true;
}

void particle_overlay_set_alpha(float alpha) {
    if (alpha < 0.0f) alpha = 0.0f;
    if (alpha > 1.0f) alpha = 1.0f;
    g_flow_alpha = alpha;
}

void particle_overlay_shutdown(void) {
    free(g_particles);
    g_particles = NULL;
    g_capacity = 0;
    free(g_fade_trails);
    g_fade_trails = NULL;
    g_fade_capacity = 0;
    g_spawn_accumulator = 0.0f;
    g_next_slot = 0;
}

void particle_overlay_reset(void) {
    if (g_particles) {
        memset(g_particles, 0, (size_t)g_capacity * sizeof(FlowParticle));
    }
    if (g_fade_trails && g_fade_capacity > 0) {
        memset(g_fade_trails, 0, (size_t)g_fade_capacity * sizeof(FlowFadeTrail));
    }
    g_spawn_accumulator = 0.0f;
    g_next_slot = 0;
    g_flow_alpha = FLOW_ALPHA;
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

static void capture_fade_trail(const FlowParticle *pt) {
    if (!pt || !g_fade_trails || g_fade_capacity <= 0) return;
    int slot = -1;
    int oldest_idx = 0;
    float oldest_time = 1e9f;
    for (int i = 0; i < g_fade_capacity; ++i) {
        int idx = i;
        float tl = g_fade_trails[idx].frames_left;
        if (tl <= 0.0f) {
            slot = idx;
            break;
        }
        if (tl < oldest_time) {
            oldest_time = tl;
            oldest_idx = idx;
        }
    }
    if (slot < 0) slot = oldest_idx;
    FlowFadeTrail *fade = &g_fade_trails[slot];
    fade->frames_left = FLOW_FADE_FRAMES;
    float life_ratio = (pt->max_life > 0.0f) ? fmaxf(pt->life / pt->max_life, 0.0f) : 1.0f;
    fade->start_strength = (0.6f + 0.4f * life_ratio);
    int count = pt->trail_count;
    if (count < 2) count = 2;
    if (count > FLOW_TRAIL_MAX_POINTS) count = FLOW_TRAIL_MAX_POINTS;
    fade->count = count;
    for (int i = 0; i < count; ++i) {
        int src = i;
        if (src >= pt->trail_count) src = pt->trail_count - 1;
        fade->xs[i] = pt->trail_x[src];
        fade->ys[i] = pt->trail_y[src];
    }
}

static void spawn_particle(const SceneState *scene) {
    if (!g_particles || g_capacity == 0) return;
    int slot = g_next_slot;
    for (int i = 0; i < g_capacity; ++i) {
        int idx = (g_next_slot + i) % g_capacity;
        FlowParticle *candidate = &g_particles[idx];
        if (candidate->life <= 0.0f && candidate->fade_frames_left <= 0.0f) {
            slot = idx;
            break;
        }
    }
    g_next_slot = (slot + 1) % g_capacity;
    FlowParticle *pt = &g_particles[slot];

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
    float base_life = FLOW_MIN_LIFETIME +
                      randf01() * (FLOW_MAX_LIFETIME - FLOW_MIN_LIFETIME);
    float jitter = 1.0f + FLOW_LIFE_JITTER * (randf01() * 2.0f - 1.0f);
    if (jitter < 0.2f) jitter = 0.2f;
    float life = base_life * jitter;
    pt->x = x;
    pt->y = y;
    pt->life = life;
    pt->max_life = life;
    pt->fade_frames_left = 0.0f;
    flow_particle_reset_trail(pt, x, y);
}

static void update_particle(FlowParticle *pt,
                            const SceneState *scene,
                            float dt_seconds,
                            float frame_equiv) {
    if (!pt) return;

    if (pt->life > 0.0f) {
        pt->life -= dt_seconds;
        if (pt->life <= 0.0f) {
            pt->life = 0.0f;
            pt->fade_frames_left = FLOW_FADE_FRAMES;
            capture_fade_trail(pt);
            return;
        }
    } else if (pt->fade_frames_left > 0.0f) {
        pt->fade_frames_left -= frame_equiv;
        if (pt->fade_frames_left < 0.0f) pt->fade_frames_left = 0.0f;
        return;
    } else {
        return;
    }

    Vec2 vel = vec2(0.0f, 0.0f);
    if (!sample_velocity_safe(scene, pt->x, pt->y, &vel)) {
        pt->life = 0.0f;
        pt->fade_frames_left = FLOW_FADE_FRAMES;
        capture_fade_trail(pt);
        return;
    }

    float new_x = pt->x + vel.x * dt_seconds;
    float new_y = pt->y + vel.y * dt_seconds;

    if (new_x < 0.0f || new_x >= (float)g_grid_w ||
        new_y < 0.0f || new_y >= (float)g_grid_h) {
        pt->life = 0.0f;
        pt->fade_frames_left = FLOW_FADE_FRAMES;
        capture_fade_trail(pt);
        return;
    }

    if (point_in_solid(scene, new_x, new_y)) {
        pt->life = 0.0f;
        pt->fade_frames_left = FLOW_FADE_FRAMES;
        capture_fade_trail(pt);
        return;
    }

    pt->x = new_x;
    pt->y = new_y;
    flow_particle_push_trail(pt, pt->x, pt->y);
}

void particle_overlay_update(const SceneState *scene, double dt, bool spawn_enabled) {
    if (!scene || !scene->smoke || !g_particles) return;
    if (dt <= 0.0) dt = 1.0 / 60.0;
    float fdt = (float)dt;
    float frame_equiv = fdt / (1.0f / 60.0f);
    if (frame_equiv < 0.1f) frame_equiv = 0.1f;
    if (frame_equiv > 8.0f) frame_equiv = 8.0f;

    if (spawn_enabled) {
        g_spawn_accumulator += FLOW_SPAWN_RATE_PER_SEC * fdt;
        int spawn_count = (int)g_spawn_accumulator;
        if (spawn_count > 0) {
            g_spawn_accumulator -= (float)spawn_count;
            if (spawn_count > g_capacity) spawn_count = g_capacity;
            for (int i = 0; i < spawn_count; ++i) {
                spawn_particle(scene);
            }
        }
    }

    for (int i = 0; i < g_capacity; ++i) {
        update_particle(&g_particles[i], scene, fdt, frame_equiv);
    }

    if (g_fade_trails && g_fade_capacity > 0) {
        for (int i = 0; i < g_fade_capacity; ++i) {
            FlowFadeTrail *ft = &g_fade_trails[i];
            if (ft->frames_left <= 0.0f) continue;
            ft->frames_left -= frame_equiv;
            if (ft->frames_left < 0.0f) ft->frames_left = 0.0f;
        }
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
        if (pt->life <= 0.0f && pt->fade_frames_left <= 0.0f) continue;
        if (pt->trail_count < 2) continue;

        float visibility = 0.0f;
        bool fading = false;
        if (pt->life > 0.0f && pt->max_life > 0.0f) {
            visibility = pt->life / pt->max_life;
        } else if (pt->fade_frames_left > 0.0f) {
            visibility = pt->fade_frames_left / FLOW_FADE_FRAMES;
            fading = true;
        }
        if (visibility <= 0.0f) continue;

        float life_ratio = (pt->max_life > 0.0f) ? fmaxf(pt->life / pt->max_life, 0.0f) : 0.0f;
        SDL_Color base_col = mix_color(g_color_base, g_color_mix, FLOW_COLOR_MIX);
        SDL_Color tip_col  = mix_color(base_col, g_color_base, 0.35f); // soften toward the tail
        for (int j = 1; j < pt->trail_count; ++j) {
            float t = (float)j / (float)(pt->trail_count - 1);
            float strength = (0.2f + 0.8f * t) * (0.6f + 0.4f * life_ratio) * visibility * g_flow_alpha;
            if (fading) {
                strength *= 0.8f; // keep fades subtle
            }
            if (strength > 1.0f) strength = 1.0f;

            SDL_Color col = mix_color(tip_col, base_col, t);
            Uint8 a = (Uint8)lroundf(255.0f * strength);
            if (a == 0) continue;

            float x0 = pt->trail_x[j - 1] * scale_x;
            float y0 = pt->trail_y[j - 1] * scale_y;
            float x1 = pt->trail_x[j] * scale_x;
            float y1 = pt->trail_y[j] * scale_y;

            SDL_SetRenderDrawColor(renderer, col.r, col.g, col.b, a);
            SDL_RenderDrawLine(renderer,
                               (int)lroundf(x0),
                               (int)lroundf(y0),
                               (int)lroundf(x1),
                               (int)lroundf(y1));
        }
    }

    if (g_fade_trails && g_fade_capacity > 0) {
        for (int i = 0; i < g_fade_capacity; ++i) {
            FlowFadeTrail *ft = &g_fade_trails[i];
            if (ft->frames_left <= 0.0f || ft->count < 2) continue;
            float visibility = ft->frames_left / FLOW_FADE_FRAMES;
            for (int j = 1; j < ft->count; ++j) {
                float t = (float)j / (float)(ft->count - 1);
                float strength = (0.2f + 0.8f * t) * ft->start_strength * visibility * g_flow_alpha;
                if (strength > 1.0f) strength = 1.0f;
                SDL_Color base_col = mix_color(g_color_base, g_color_mix, FLOW_COLOR_MIX);
                SDL_Color tip_col  = mix_color(base_col, g_color_base, 0.35f);
                SDL_Color col = mix_color(tip_col, base_col, t);
                Uint8 a = (Uint8)lroundf(255.0f * strength);
                if (a == 0) continue;

                float x0 = ft->xs[j - 1] * scale_x;
                float y0 = ft->ys[j - 1] * scale_y;
                float x1 = ft->xs[j] * scale_x;
                float y1 = ft->ys[j] * scale_y;

                SDL_SetRenderDrawColor(renderer, col.r, col.g, col.b, a);
                SDL_RenderDrawLine(renderer,
                                   (int)lroundf(x0),
                                   (int)lroundf(y0),
                                   (int)lroundf(x1),
                                   (int)lroundf(y1));
            }
        }
    }
}
