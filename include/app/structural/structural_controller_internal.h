#ifndef STRUCTURAL_CONTROLLER_INTERNAL_H
#define STRUCTURAL_CONTROLLER_INTERNAL_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdbool.h>
#include <stddef.h>

#include "physics/structural/structural_scene.h"
#include "physics/structural/structural_solver.h"

/* Private split-contract header for structural_controller*.c units only. */
typedef struct StructuralRuntimeView {
    float *u;
    float *v;
    float *a;
    float *mass;
    size_t dof_count;
} StructuralRuntimeView;

typedef enum StructuralIntegrator {
    STRUCT_INTEGRATOR_EXPLICIT = 0,
    STRUCT_INTEGRATOR_NEWMARK = 1
} StructuralIntegrator;

typedef struct StructuralController {
    StructuralScene  scene;
    StructuralSolveResult last_result;
    StructuralRuntimeView runtime;
    bool show_constraints;
    bool show_loads;
    bool show_ids;
    bool show_deformed;
    bool show_stress;
    bool show_bending;
    bool show_shear;
    bool show_combined;
    bool scale_use_percentile;
    bool scale_freeze;
    bool scale_initialized;
    bool scale_thickness;
    float scale_gamma;
    float scale_percentile;
    float scale_stress;
    float scale_moment;
    float scale_shear;
    float scale_combined;
    float thickness_gain;
    float deform_scale;
    bool solve_requested;
    bool dynamic_mode;
    bool dynamic_playing;
    bool dynamic_step;
    StructuralIntegrator integrator;
    float time_scale;
    float damping_alpha;
    float damping_beta;
    float newmark_beta;
    float newmark_gamma;
    bool gravity_ramp_enabled;
    float gravity_ramp_time;
    float gravity_ramp_duration;
    float sim_time;
    int window_w;
    int window_h;
    int pointer_x;
    int pointer_y;
    bool running;
    TTF_Font *font_small;
    TTF_Font *font_hud;
    char preset_path[256];
} StructuralController;

void structural_controller_runtime_view_clear(StructuralRuntimeView *view);
void structural_controller_runtime_view_resize(StructuralRuntimeView *view, size_t node_count);
void structural_controller_runtime_view_sync_from_scene(StructuralRuntimeView *view,
                                                        const StructuralScene *scene);
void structural_controller_runtime_step_dynamic(StructuralController *ctrl, float dt);

void structural_controller_render_scene(SDL_Renderer *renderer, StructuralController *ctrl);

#endif // STRUCTURAL_CONTROLLER_INTERNAL_H
