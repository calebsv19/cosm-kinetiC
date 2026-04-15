#include "app/structural/structural_preset_editor.h"
#include "app/structural/structural_preset_editor_internal.h"
#include "app/structural/structural_preset_editor_render_helpers.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_vulkan.h>

#include "app/editor/scene_editor_widgets.h"
#include "app/structural/structural_editor.h"
#include "input/input.h"
#include "physics/structural/structural_scene.h"
#include "physics/structural/structural_solver.h"
#include "render/text_upload_policy.h"
#include "vk_renderer.h"
#include "render/vk_shared_device.h"

static SDL_Window *g_struct_window = NULL;
static VkRenderer g_struct_renderer_storage;
static SDL_Renderer *g_struct_renderer = NULL;
static bool g_struct_initialized = false;
static bool g_struct_use_shared_device = false;

static void struct_log_window_sizes(SDL_Window *window, const char *tag) {
    if (!window || !tag) return;
    int win_w = 0;
    int win_h = 0;
    int drawable_w = 0;
    int drawable_h = 0;
    SDL_GetWindowSize(window, &win_w, &win_h);
    SDL_Vulkan_GetDrawableSize(window, &drawable_w, &drawable_h);
    fprintf(stderr, "[struct-editor] %s win=%dx%d drawable=%dx%d\n",
            tag, win_w, win_h, drawable_w, drawable_h);
}

static bool struct_wait_for_drawable(SDL_Window *window, const char *tag) {
    if (!window) return false;
    for (int i = 0; i < 60; ++i) {
        int drawable_w = 0;
        int drawable_h = 0;
        SDL_PumpEvents();
        SDL_Vulkan_GetDrawableSize(window, &drawable_w, &drawable_h);
        if (drawable_w > 0 && drawable_h > 0) {
            return true;
        }
        SDL_Delay(16);
    }
    struct_log_window_sizes(window, tag);
    return false;
}

static void update_layout(StructuralPresetEditor *editor) {
    if (!editor || !editor->renderer) return;
    int w = 0;
    int h = 0;
    int small_h = 16;
    int main_h = 22;
    int compact_h = 34;
    int action_h = 34;
    int save_h = 38;
    int button_gap = 8;
    SDL_GetWindowSize(editor->window, &w, &h);

    int margin = 16;
    if (editor->font_small) {
        small_h = TTF_FontHeight(editor->font_small);
        if (small_h > 0) {
            small_h = physics_sim_text_logical_pixels(editor->renderer, small_h);
        }
    }
    if (editor->font_main) {
        main_h = TTF_FontHeight(editor->font_main);
        if (main_h > 0) {
            main_h = physics_sim_text_logical_pixels(editor->renderer, main_h);
        }
    }
    if (small_h < 14) small_h = 14;
    if (main_h < 18) main_h = 18;
    compact_h = small_h + 14;
    if (compact_h < 32) compact_h = 32;
    action_h = small_h + 16;
    if (action_h < 34) action_h = 34;
    save_h = main_h + 14;
    if (save_h < 38) save_h = 38;

    editor->panel_w = 320 + (small_h - 16) * 4;
    if (editor->panel_w < 320) editor->panel_w = 320;
    if (editor->panel_w > 440) editor->panel_w = 440;
    editor->panel_x = w - editor->panel_w - margin;
    editor->panel_y = margin;
    editor->panel_h = h - margin * 2;

    editor->canvas_x = margin;
    editor->canvas_y = margin;
    editor->canvas_w = editor->panel_x - margin - editor->canvas_x;
    editor->canvas_h = h - margin * 2;
    if (editor->canvas_w < 50) editor->canvas_w = 50;
    if (editor->canvas_h < 50) editor->canvas_h = 50;

    float scale_x = (float)editor->canvas_w / (float)editor->cfg.window_w;
    float scale_y = (float)editor->canvas_h / (float)editor->cfg.window_h;
    editor->scale = (scale_x < scale_y) ? scale_x : scale_y;
    if (editor->scale < 0.01f) editor->scale = 0.01f;

    editor->preview_w = (int)(editor->cfg.window_w * editor->scale);
    editor->preview_h = (int)(editor->cfg.window_h * editor->scale);
    editor->preview_x = editor->canvas_x + (editor->canvas_w - editor->preview_w) / 2;
    editor->preview_y = editor->canvas_y + (editor->canvas_h - editor->preview_h) / 2;

    editor->ground_y = (float)editor->cfg.window_h - editor->scene.ground_offset;
    if (editor->ground_y < 0.0f) editor->ground_y = 0.0f;

    editor->btn_save.rect = (SDL_Rect){editor->panel_x + 16,
                                       editor->panel_y + editor->panel_h - save_h - 12,
                                       editor->panel_w - 32,
                                       save_h};
    editor->btn_save.label = "Save Preset";
    editor->btn_save.enabled = true;

    editor->btn_cancel.rect = (SDL_Rect){editor->panel_x + 16,
                                         editor->btn_save.rect.y - compact_h - button_gap,
                                         editor->panel_w - 32,
                                         compact_h};
    editor->btn_cancel.label = "Cancel";
    editor->btn_cancel.enabled = true;

    editor->btn_ground.rect = (SDL_Rect){editor->panel_x + 16,
                                         editor->btn_cancel.rect.y - action_h - button_gap,
                                         editor->panel_w - 32,
                                         action_h};
    editor->btn_ground.label = "Attach to Ground";
    editor->btn_ground.enabled = true;

    editor->btn_gravity.rect = (SDL_Rect){editor->panel_x + 16,
                                          editor->btn_ground.rect.y - action_h - button_gap,
                                          editor->panel_w - 32,
                                          action_h};
    editor->btn_gravity.label = editor->scene.gravity_enabled ? "Gravity: On" : "Gravity: Off";
    editor->btn_gravity.enabled = true;

    int half_w = (editor->panel_w - 40) / 2;
    editor->btn_gravity_minus.rect = (SDL_Rect){editor->panel_x + 16,
                                                editor->btn_gravity.rect.y - compact_h - button_gap,
                                                half_w,
                                                compact_h};
    editor->btn_gravity_minus.label = "G-";
    editor->btn_gravity_minus.enabled = true;

    editor->btn_gravity_plus.rect = (SDL_Rect){editor->panel_x + 24 + half_w,
                                               editor->btn_gravity_minus.rect.y,
                                               half_w,
                                               compact_h};
    editor->btn_gravity_plus.label = "G+";
    editor->btn_gravity_plus.enabled = true;
}

static void handle_solve(StructuralPresetEditor *editor) {
    if (!editor) return;
    StructuralScene *scene = &editor->scene;
    bool has_ground = false;
    for (size_t i = 0; i < scene->node_count; ++i) {
        const StructNode *node = &scene->nodes[i];
        if (node->fixed_y && fabsf(node->y - editor->ground_y) <= 1.0f) {
            has_ground = true;
            break;
        }
    }
    if (!has_ground) {
        structural_editor_set_status(&editor->editor, "Warning: no nodes attached to ground.");
    }

    StructuralSolveResult result = {0};
    bool ok = structural_solve_frame(scene, &result);
    editor->last_result = result;
    if (ok) {
        snprintf(editor->last_result.warning, sizeof(editor->last_result.warning),
                 "Solve ok (%d iters, r=%.3f).", result.iterations, result.residual);
    }
}

static void attach_selected_to_ground(StructuralPresetEditor *editor) {
    if (!editor) return;
    StructuralScene *scene = &editor->scene;
    bool changed = false;
    for (size_t i = 0; i < scene->node_count; ++i) {
        StructNode *node = &scene->nodes[i];
        if (!node->selected) continue;
        node->y = editor->ground_y;
        node->fixed_x = true;
        node->fixed_y = true;
        node->fixed_theta = true;
        changed = true;
    }
    if (changed) {
        structural_scene_clear_solution(scene);
        structural_editor_set_status(&editor->editor, "Attached selected nodes to ground.");
    }
}

static void editor_save(StructuralPresetEditor *editor) {
    if (!editor) return;
    if (structural_scene_save(&editor->scene, editor->preset_path)) {
        structural_editor_set_status(&editor->editor, "Saved structural preset.");
        editor->applied = true;
        editor->running = false;
    } else {
        structural_editor_set_status(&editor->editor, "Save failed.");
    }
}

static void editor_load(StructuralPresetEditor *editor) {
    if (!editor) return;
    if (structural_scene_load(&editor->scene, editor->preset_path)) {
        structural_editor_init(&editor->editor, &editor->scene);
        structural_editor_set_status(&editor->editor, "Loaded structural preset.");
    } else {
        structural_editor_set_status(&editor->editor, "Load failed.");
    }
}

static bool button_hit(const SDL_Rect *rect, int x, int y) {
    if (!rect) return false;
    return x >= rect->x && x <= rect->x + rect->w &&
           y >= rect->y && y <= rect->y + rect->h;
}

static void on_pointer_down(void *user, const InputPointerState *state) {
    StructuralPresetEditor *editor = (StructuralPresetEditor *)user;
    if (!editor || !state) return;
    if (button_hit(&editor->btn_ground.rect, state->x, state->y)) {
        attach_selected_to_ground(editor);
        return;
    }
    if (button_hit(&editor->btn_gravity.rect, state->x, state->y)) {
        editor->scene.gravity_enabled = !editor->scene.gravity_enabled;
        structural_editor_set_status(&editor->editor,
                                     editor->scene.gravity_enabled ? "Gravity enabled." : "Gravity disabled.");
        return;
    }
    if (button_hit(&editor->btn_gravity_minus.rect, state->x, state->y)) {
        editor->scene.gravity_strength = fmaxf(0.0f, editor->scene.gravity_strength - 1.0f);
        return;
    }
    if (button_hit(&editor->btn_gravity_plus.rect, state->x, state->y)) {
        editor->scene.gravity_strength = fminf(100.0f, editor->scene.gravity_strength + 1.0f);
        return;
    }
    if (button_hit(&editor->btn_save.rect, state->x, state->y)) {
        editor_save(editor);
        return;
    }
    if (button_hit(&editor->btn_cancel.rect, state->x, state->y)) {
        editor->running = false;
        return;
    }
    if (!structural_preset_editor_point_in_preview(editor, state->x, state->y)) return;

    if (state->button == SDL_BUTTON_LEFT && state->down &&
        editor->editor.edge_start_node_id >= 0) {
        float wx = 0.0f;
        float wy = 0.0f;
        structural_preset_editor_screen_to_world(editor, state->x, state->y, &wx, &wy);
        structural_preset_editor_apply_snap(editor, &wx, &wy);
        structural_preset_editor_apply_ground_snap(editor, &wx, &wy);
        int node_id = structural_scene_find_node_at(&editor->scene, wx, wy, 10.0f);
        if (node_id < 0) {
            editor->editor.edge_start_node_id = -1;
            structural_editor_set_status(&editor->editor, "Beam chain cancelled.");
            return;
        }
    }

    if (state->button == SDL_BUTTON_RIGHT && state->down) {
        float wx = 0.0f;
        float wy = 0.0f;
        structural_preset_editor_screen_to_world(editor, state->x, state->y, &wx, &wy);
        structural_preset_editor_apply_snap(editor, &wx, &wy);
        structural_preset_editor_apply_ground_snap(editor, &wx, &wy);
        bool keep_chain = (SDL_GetModState() & KMOD_SHIFT) == 0;
        StructuralScene *scene = &editor->scene;
        int node_id = structural_scene_find_node_at(scene, wx, wy, 10.0f);
        if (node_id >= 0) {
            if (editor->editor.edge_start_node_id >= 0 &&
                editor->editor.edge_start_node_id != node_id) {
                int edge_id = structural_scene_add_edge(scene, editor->editor.edge_start_node_id, node_id);
                StructEdge *edge = structural_scene_get_edge(scene, edge_id);
                if (edge) edge->material_index = editor->editor.active_material;
                if (keep_chain) {
                    editor->editor.edge_start_node_id = node_id;
                }
                structural_scene_clear_solution(scene);
                structural_editor_set_status(&editor->editor, "Added beam.");
            } else {
                if (keep_chain) {
                    editor->editor.edge_start_node_id = node_id;
                    structural_editor_set_status(&editor->editor, "Beam start set.");
                } else {
                    structural_editor_set_status(&editor->editor, "Beam start unchanged.");
                }
            }
            return;
        }

        int new_id = structural_scene_add_node(scene, wx, wy);
        if (new_id < 0) return;
        if (editor->editor.edge_start_node_id >= 0) {
            int edge_id = structural_scene_add_edge(scene, editor->editor.edge_start_node_id, new_id);
            StructEdge *edge = structural_scene_get_edge(scene, edge_id);
            if (edge) edge->material_index = editor->editor.active_material;
            structural_editor_set_status(&editor->editor,
                                         keep_chain ? "Added node + beam." : "Added node + beam (chain paused).");
        } else {
            structural_editor_set_status(&editor->editor,
                                         keep_chain ? "Added node." : "Added node (chain paused).");
        }
        if (keep_chain) {
            editor->editor.edge_start_node_id = new_id;
        }
        structural_scene_clear_solution(scene);
        return;
    }

    InputPointerState local = *state;
    float wx = 0.0f;
    float wy = 0.0f;
    structural_preset_editor_screen_to_world(editor, state->x, state->y, &wx, &wy);
    structural_preset_editor_apply_ground_snap(editor, &wx, &wy);
    local.x = (int)wx;
    local.y = (int)wy;
    SDL_Keymod mod = SDL_GetModState();
    structural_editor_handle_pointer_down(&editor->editor, &local, mod);
}

static void on_pointer_up(void *user, const InputPointerState *state) {
    StructuralPresetEditor *editor = (StructuralPresetEditor *)user;
    if (!editor || !state) return;

    InputPointerState local = *state;
    float wx = 0.0f;
    float wy = 0.0f;
    structural_preset_editor_screen_to_world(editor, state->x, state->y, &wx, &wy);
    structural_preset_editor_apply_ground_snap(editor, &wx, &wy);
    local.x = (int)wx;
    local.y = (int)wy;
    SDL_Keymod mod = SDL_GetModState();
    structural_editor_handle_pointer_up(&editor->editor, &local, mod);
}

static void on_pointer_move(void *user, const InputPointerState *state) {
    StructuralPresetEditor *editor = (StructuralPresetEditor *)user;
    if (!editor || !state) return;
    editor->pointer_x = state->x;
    editor->pointer_y = state->y;

    InputPointerState local = *state;
    float wx = 0.0f;
    float wy = 0.0f;
    structural_preset_editor_screen_to_world(editor, state->x, state->y, &wx, &wy);
    if (editor->editor.dragging) {
        structural_preset_editor_apply_ground_snap(editor, &wx, &wy);
    }
    local.x = (int)wx;
    local.y = (int)wy;
    SDL_Keymod mod = SDL_GetModState();
    structural_editor_handle_pointer_move(&editor->editor, &local, mod);
}

static void on_key_down(void *user, SDL_Keycode key, SDL_Keymod mod) {
    StructuralPresetEditor *editor = (StructuralPresetEditor *)user;
    if (!editor) return;
    if ((mod & KMOD_CTRL) && key == SDLK_s) {
        editor_save(editor);
        return;
    }
    if ((mod & KMOD_CTRL) && key == SDLK_o) {
        editor_load(editor);
        return;
    }
    if (mod & KMOD_CTRL) {
        if (key == SDLK_q) {
            editor->editor.show_combined = !editor->editor.show_combined;
            structural_editor_set_status(&editor->editor,
                                         editor->editor.show_combined ? "Combined stress on." : "Combined stress off.");
            return;
        }
        if (key == SDLK_y) {
            editor->editor.scale_use_percentile = !editor->editor.scale_use_percentile;
            editor->scale_initialized = false;
            structural_editor_set_status(&editor->editor,
                                         editor->editor.scale_use_percentile ? "Percentile scale on." : "Percentile scale off.");
            return;
        }
        if (key == SDLK_k) {
            editor->editor.scale_freeze = !editor->editor.scale_freeze;
            editor->scale_initialized = false;
            structural_editor_set_status(&editor->editor,
                                         editor->editor.scale_freeze ? "Scale frozen." : "Scale live.");
            return;
        }
        if (key == SDLK_g) {
            if (editor->editor.scale_gamma > 0.9f) editor->editor.scale_gamma = 0.7f;
            else if (editor->editor.scale_gamma > 0.6f) editor->editor.scale_gamma = 0.5f;
            else if (editor->editor.scale_gamma > 0.4f) editor->editor.scale_gamma = 0.35f;
            else editor->editor.scale_gamma = 1.0f;
            editor->scale_initialized = false;
            structural_editor_set_status(&editor->editor, "Gamma changed.");
            return;
        }
        if (key == SDLK_x) {
            editor->editor.scale_thickness = !editor->editor.scale_thickness;
            structural_editor_set_status(&editor->editor,
                                         editor->editor.scale_thickness ? "Thickness scale on." : "Thickness scale off.");
            return;
        }
    }
    if (key == SDLK_ESCAPE) {
        editor->running = false;
        return;
    }
    if (key == SDLK_SPACE || key == SDLK_RETURN) {
        editor->solve_requested = true;
        return;
    }
    if (key == SDLK_r) {
        structural_scene_reset(&editor->scene);
        structural_editor_init(&editor->editor, &editor->scene);
        memset(&editor->last_result, 0, sizeof(editor->last_result));
        editor->scale_initialized = false;
        return;
    }
    if (key == SDLK_n) {
        int index = structural_scene_add_load_case(&editor->scene, NULL);
        if (index >= 0) {
            editor->scene.active_load_case = index;
            structural_editor_set_status(&editor->editor, "Added load case.");
        }
        return;
    }
    if (key == SDLK_f) {
        attach_selected_to_ground(editor);
        return;
    }
    structural_editor_handle_key_down(&editor->editor, key, mod);
    if (key == SDLK_y || key == SDLK_g || key == SDLK_k) {
        editor->scale_initialized = false;
    }
}

bool structural_preset_editor_run(SDL_Window *window,
                                  SDL_Renderer *renderer,
                                  TTF_Font *font_main,
                                  TTF_Font *font_small,
                                  const AppConfig *cfg,
                                  const char *preset_path,
                                  InputContextManager *ctx_mgr) {
    if (!cfg) return false;
    (void)renderer;

    int base_w = cfg->window_w > 0 ? cfg->window_w : 900;
    int base_h = cfg->window_h > 0 ? cfg->window_h : 700;
    if (window) {
        SDL_GetWindowSize(window, &base_w, &base_h);
    }

    int restart_attempts = 0;
retry:
    fprintf(stderr, "[struct-editor] run: base size=%dx%d\n", base_w, base_h);
    SDL_Window *local_window = g_struct_window;
    SDL_Renderer *local_renderer = g_struct_renderer;
    bool use_shared_device = g_struct_use_shared_device;
    for (int attempt = 0; attempt < 2; ++attempt) {
        if (!g_struct_initialized) {
            fprintf(stderr, "[struct-editor] init: creating window\n");
            local_window = SDL_CreateWindow(
                "Structural Preset Editor",
                SDL_WINDOWPOS_CENTERED,
                SDL_WINDOWPOS_CENTERED,
                base_w,
                base_h,
                SDL_WINDOW_SHOWN | SDL_WINDOW_VULKAN);
            if (!local_window) return false;

            struct_log_window_sizes(local_window, "after create");
            g_struct_window = local_window;
            g_struct_renderer = (SDL_Renderer *)&g_struct_renderer_storage;
            local_renderer = g_struct_renderer;

            VkRendererConfig vk_cfg;
            vk_renderer_config_set_defaults(&vk_cfg);
            vk_cfg.enable_validation = SDL_FALSE;
            vk_cfg.clear_color[0] = 0.0f;
            vk_cfg.clear_color[1] = 0.0f;
            vk_cfg.clear_color[2] = 0.0f;
            vk_cfg.clear_color[3] = 1.0f;
#if defined(__APPLE__)
            vk_cfg.frames_in_flight = 1;
#endif
#if defined(__APPLE__)
            use_shared_device = true;
#else
            use_shared_device = true;
#endif
            g_struct_use_shared_device = use_shared_device;

            fprintf(stderr, "[struct-editor] init: use_shared_device=%d\n",
                    use_shared_device ? 1 : 0);
            if (use_shared_device) {
                if (!vk_shared_device_init(local_window, &vk_cfg)) {
                    fprintf(stderr, "[struct-editor] Failed to init shared Vulkan device.\n");
                    SDL_DestroyWindow(local_window);
                    g_struct_window = NULL;
                    g_struct_renderer = NULL;
                    return false;
                }

                VkRendererDevice* shared_device = vk_shared_device_get();
                if (!shared_device) {
                    fprintf(stderr, "[struct-editor] Failed to access shared Vulkan device.\n");
                    SDL_DestroyWindow(local_window);
                    g_struct_window = NULL;
                    g_struct_renderer = NULL;
                    return false;
                }

                if (vk_renderer_init_with_device((VkRenderer *)local_renderer, shared_device, local_window, &vk_cfg) != VK_SUCCESS) {
                    fprintf(stderr, "[struct-editor] vk_renderer_init_with_device failed\n");
                    SDL_DestroyWindow(local_window);
                    g_struct_window = NULL;
                    g_struct_renderer = NULL;
                    return false;
                }
                vk_shared_device_acquire();
            } else {
                if (vk_renderer_init((VkRenderer *)local_renderer, local_window, &vk_cfg) != VK_SUCCESS) {
                    fprintf(stderr, "[struct-editor] vk_renderer_init failed\n");
                    SDL_DestroyWindow(local_window);
                    g_struct_window = NULL;
                    g_struct_renderer = NULL;
                    return false;
                }
            }
            g_struct_initialized = true;
        } else {
            fprintf(stderr, "[struct-editor] reuse: showing cached window\n");
            if (base_w > 0 && base_h > 0) {
                SDL_SetWindowSize(local_window, base_w, base_h);
            }
            SDL_ShowWindow(local_window);
        }
        SDL_RaiseWindow(local_window);
        if (struct_wait_for_drawable(local_window, "drawable still 0 after show")) {
            struct_log_window_sizes(local_window, "after show");
            SDL_PumpEvents();
            SDL_Delay(32);
            break;
        }

        fprintf(stderr, "[struct-editor] window drawable size never became valid, resetting window.\n");
        vk_renderer_wait_idle((VkRenderer *)local_renderer);
        if (use_shared_device) {
            vk_renderer_shutdown_surface((VkRenderer *)local_renderer);
            vk_shared_device_release();
        } else {
            vk_renderer_shutdown((VkRenderer *)local_renderer);
        }
        SDL_DestroyWindow(local_window);
        g_struct_window = NULL;
        g_struct_renderer = NULL;
        g_struct_initialized = false;
        local_window = NULL;
        local_renderer = NULL;
        if (attempt == 1) {
            return false;
        }
    }
    int init_w = 0;
    int init_h = 0;
    SDL_GetWindowSize(local_window, &init_w, &init_h);
    if (init_w <= 0) init_w = base_w;
    if (init_h <= 0) init_h = base_h;
    vk_renderer_set_logical_size((VkRenderer *)local_renderer, (float)init_w, (float)init_h);
    {
        VkRenderer *vk_renderer = (VkRenderer *)local_renderer;
        fprintf(stderr,
                "[struct-editor] renderer: frames=%u swapchain images=%u format=%u extent=%ux%u\n",
                vk_renderer->frame_count,
                vk_renderer->context.swapchain.image_count,
                (unsigned)vk_renderer->context.swapchain.image_format,
                vk_renderer->context.swapchain.extent.width,
                vk_renderer->context.swapchain.extent.height);
    }

    StructuralPresetEditor editor = {0};
    editor.window = local_window;
    editor.renderer = local_renderer;
    editor.font_main = font_main;
    editor.font_small = font_small;
    editor.cfg = *cfg;
    editor.running = true;
    editor.applied = false;
    bool device_lost = false;
    if (preset_path && preset_path[0] != '\0') {
        snprintf(editor.preset_path, sizeof(editor.preset_path), "%s", preset_path);
    } else {
        snprintf(editor.preset_path, sizeof(editor.preset_path), "%s", "config/structural_scene.txt");
    }

    structural_scene_init(&editor.scene);
    structural_editor_init(&editor.editor, &editor.scene);
    editor.editor.snap_to_grid = true;
    editor.editor.show_stress = false;
    editor.scale_initialized = false;
    editor.scale_stress = 0.0f;
    editor.scale_moment = 0.0f;
    editor.scale_shear = 0.0f;
    editor.scale_combined = 0.0f;
    editor.ground_snap_enabled = true;
    editor.ground_snap_dist = 10.0f;
    editor.pointer_x = -1;
    editor.pointer_y = -1;
    if (!structural_scene_load(&editor.scene, editor.preset_path)) {
        structural_editor_set_status(&editor.editor, "Preset load failed (new scene).");
    }

    update_layout(&editor);

    InputContextManager local_mgr;
    input_context_manager_init(&local_mgr);
    ctx_mgr = &local_mgr;

    InputContext edit_ctx = {
        .on_pointer_down = on_pointer_down,
        .on_pointer_up = on_pointer_up,
        .on_pointer_move = on_pointer_move,
        .on_key_down = on_key_down,
        .user_data = &editor
    };
    input_context_manager_push(ctx_mgr, &edit_ctx);

    int frame_attempts = 0;
    while (editor.running) {
        InputCommands cmds;
        if (!input_poll_events(&cmds, NULL, ctx_mgr)) {
            editor.running = false;
            break;
        }
        if (cmds.quit) {
            editor.running = false;
            break;
        }

        if (editor.solve_requested) {
            editor.solve_requested = false;
            handle_solve(&editor);
        }

        update_layout(&editor);
        int win_w = 0;
        int win_h = 0;
        SDL_GetWindowSize(editor.window, &win_w, &win_h);
        if (win_w > 0 && win_h > 0) {
            int drawable_w = win_w;
            int drawable_h = win_h;
            SDL_Vulkan_GetDrawableSize(editor.window, &drawable_w, &drawable_h);
            if (drawable_w <= 0 || drawable_h <= 0) {
                struct_log_window_sizes(editor.window, "drawable 0, waiting");
                SDL_Delay(16);
                continue;
            }
            VkExtent2D swap_extent = ((VkRenderer *)editor.renderer)->context.swapchain.extent;
            if ((uint32_t)drawable_w != swap_extent.width ||
                (uint32_t)drawable_h != swap_extent.height) {
                fprintf(stderr, "[struct-editor] swapchain resize %ux%u -> %dx%d\n",
                        swap_extent.width, swap_extent.height, drawable_w, drawable_h);
                vk_renderer_recreate_swapchain((VkRenderer *)editor.renderer, editor.window);
                vk_renderer_set_logical_size((VkRenderer *)editor.renderer, (float)win_w, (float)win_h);
                SDL_Delay(8);
                continue;
            }
            VkCommandBuffer cmd = VK_NULL_HANDLE;
            VkFramebuffer fb = VK_NULL_HANDLE;
            VkExtent2D extent = {0};
            VkResult frame = vk_renderer_begin_frame((VkRenderer *)editor.renderer, &cmd, &fb, &extent);
            if (frame_attempts < 8) {
                fprintf(stderr, "[struct-editor] begin_frame attempt=%d result=%d extent=%ux%u\n",
                        frame_attempts, frame, extent.width, extent.height);
                frame_attempts++;
            }
            if (frame == VK_ERROR_OUT_OF_DATE_KHR || frame == VK_SUBOPTIMAL_KHR) {
                fprintf(stderr, "[struct-editor] begin_frame out-of-date/suboptimal\n");
                VkResult recreate = vk_renderer_recreate_swapchain((VkRenderer *)editor.renderer, editor.window);
                fprintf(stderr, "[struct-editor] recreate_swapchain result=%d\n", recreate);
                vk_renderer_set_logical_size((VkRenderer *)editor.renderer, (float)win_w, (float)win_h);
            } else if (frame == VK_ERROR_DEVICE_LOST) {
                static int logged_device_lost = 0;
                if (!logged_device_lost) {
                    fprintf(stderr, "[struct-editor] Vulkan device lost; closing editor.\n");
                    logged_device_lost = 1;
                }
                if (use_shared_device) {
                    vk_shared_device_mark_lost();
                }
                device_lost = true;
                editor.running = false;
                break;
            } else if (frame == VK_SUCCESS) {
                vk_renderer_set_logical_size((VkRenderer *)editor.renderer, (float)win_w, (float)win_h);
                structural_preset_editor_render_scene(&editor);
                VkResult end = vk_renderer_end_frame((VkRenderer *)editor.renderer, cmd);
                if (frame_attempts < 8) {
                    fprintf(stderr, "[struct-editor] end_frame result=%d\n", end);
                }
                if (end == VK_ERROR_OUT_OF_DATE_KHR || end == VK_SUBOPTIMAL_KHR) {
                    fprintf(stderr, "[struct-editor] end_frame out-of-date/suboptimal\n");
                    VkResult recreate = vk_renderer_recreate_swapchain((VkRenderer *)editor.renderer, editor.window);
                    fprintf(stderr, "[struct-editor] recreate_swapchain result=%d\n", recreate);
                    vk_renderer_set_logical_size((VkRenderer *)editor.renderer, (float)win_w, (float)win_h);
                } else if (end == VK_ERROR_DEVICE_LOST) {
                    static int logged_device_lost_end = 0;
                    if (!logged_device_lost_end) {
                        fprintf(stderr, "[struct-editor] Vulkan device lost at end; closing editor.\n");
                        logged_device_lost_end = 1;
                    }
                    if (use_shared_device) {
                        vk_shared_device_mark_lost();
                    }
                    device_lost = true;
                    editor.running = false;
                    break;
                } else if (end != VK_SUCCESS) {
                    fprintf(stderr, "[struct-editor] vk_renderer_end_frame failed: %d\n", end);
                }
#if defined(__APPLE__)
                if (end == VK_SUCCESS) {
                    vk_renderer_wait_idle((VkRenderer *)editor.renderer);
                }
#endif
            } else {
                fprintf(stderr, "[struct-editor] vk_renderer_begin_frame failed: %d\n", frame);
            }
        }
        SDL_Delay(8);
    }

    input_context_manager_pop(ctx_mgr);

    SDL_FlushEvent(SDL_MOUSEBUTTONDOWN);
    SDL_FlushEvent(SDL_MOUSEBUTTONUP);
    SDL_FlushEvent(SDL_MOUSEMOTION);

    vk_renderer_wait_idle((VkRenderer *)local_renderer);
    if (device_lost) {
        if (use_shared_device) {
            vk_renderer_shutdown_surface((VkRenderer *)local_renderer);
            vk_shared_device_release();
        } else {
            vk_renderer_shutdown((VkRenderer *)local_renderer);
        }
        SDL_DestroyWindow(local_window);
        g_struct_window = NULL;
        g_struct_renderer = NULL;
        g_struct_initialized = false;
        if (!use_shared_device && restart_attempts == 0) {
            fprintf(stderr, "[struct-editor] device lost; retrying with fresh window.\n");
            restart_attempts++;
            goto retry;
        }
    } else {
        SDL_HideWindow(local_window);
    }
    return editor.applied;
}
