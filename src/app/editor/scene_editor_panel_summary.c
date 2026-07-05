#include "app/editor/scene_editor_panel_internal.h"

#include "app/editor/scene_editor_model.h"
#include "app/editor/scene_editor_wind_setup.h"

#include <stdio.h>
#include <string.h>

static void draw_status_card(SceneEditorState *state,
                             int x,
                             int y,
                             int w,
                             int card_bottom_y) {
    char overlay_line[192];
    char save_line[192];
    char overlay_wrap[2][192];
    char save_wrap[2][192];
    SDL_Color overlay_color = COLOR_TEXT_DIM;
    SDL_Color save_color = COLOR_TEXT_DIM;
    SDL_Rect card_rect;
    int overlay_lines = 0;
    int save_lines = 0;
    int line_step = 0;
    int line_h = 0;
    int card_h = 0;
    int current_y = 0;
    bool overlay_default = false;
    bool save_default = false;
    if (!state || !state->renderer || !physics_sim_editor_session_has_retained_scene(&state->session)) return;

    line_h = panel_font_height(state->renderer, state->font_small, 15);
    if (line_h < 14) line_h = 14;
    line_step = line_h + 8;

    SDL_Color card_fill = lighten_color(COLOR_PANEL, 0.05f);
    if (state->dirty) {
        snprintf(overlay_line, sizeof(overlay_line), "Overlay: unapplied changes");
        overlay_color = COLOR_STATUS_WARN;
    } else if (state->overlay_apply_diagnostics[0]) {
        snprintf(overlay_line, sizeof(overlay_line), "Overlay: %s", state->overlay_apply_diagnostics);
        overlay_color = state->overlay_apply_success ? COLOR_STATUS_OK : COLOR_STATUS_ERR;
    } else {
        snprintf(overlay_line, sizeof(overlay_line), "Overlay: no local changes");
    }
    overlay_default = !state->dirty &&
                      (!state->overlay_apply_diagnostics[0] ||
                       strstr(state->overlay_apply_diagnostics, "not yet applied") != NULL ||
                       strstr(state->overlay_apply_diagnostics, "no local changes") != NULL);

    if (state->save_scene_diagnostics[0]) {
        snprintf(save_line, sizeof(save_line), "Scene Save: %s", state->save_scene_diagnostics);
        save_color = state->save_scene_success ? COLOR_STATUS_OK : COLOR_STATUS_ERR;
        if (state->dirty) {
            save_color = COLOR_TEXT_DIM;
        }
    } else {
        snprintf(save_line, sizeof(save_line), "Scene Save: not yet saved");
        save_color = COLOR_STATUS_WARN;
    }
    save_default = !state->dirty &&
                   (!state->save_scene_diagnostics[0] ||
                    strstr(state->save_scene_diagnostics, "not yet saved") != NULL ||
                    strstr(state->save_scene_diagnostics, "First save target") != NULL ||
                    strstr(state->save_scene_diagnostics, "Save target") != NULL);
    if (overlay_default && save_default) {
        return;
    }

    wrap_text_lines(state->renderer,
                    state->font_small,
                    overlay_line,
                    w - 16,
                    2,
                    overlay_wrap,
                    &overlay_lines);
    wrap_text_lines(state->renderer,
                    state->font_small,
                    save_line,
                    w - 16,
                    2,
                    save_wrap,
                    &save_lines);
    if (overlay_lines < 1) overlay_lines = 1;
    if (save_lines < 1) save_lines = 1;

    card_h = 16 + line_step * (1 + overlay_lines + save_lines);
    if (card_bottom_y > y && card_h > (card_bottom_y - y)) {
        card_h = card_bottom_y - y;
    }
    if (card_h < line_step * 3) {
        card_h = line_step * 3;
    }
    card_rect = (SDL_Rect){x, y, w, card_h};

    SDL_SetRenderDrawColor(state->renderer, card_fill.r, card_fill.g, card_fill.b, 255);
    SDL_RenderFillRect(state->renderer, &card_rect);
    SDL_SetRenderDrawColor(state->renderer, COLOR_BG.r, COLOR_BG.g, COLOR_BG.b, 220);
    SDL_RenderDrawRect(state->renderer, &card_rect);

    draw_text(state->renderer, state->font_small, "Status", x + 8, y + 8, COLOR_TEXT);
    current_y = y + 8 + line_step;
    for (int i = 0; i < overlay_lines; ++i) {
        if (!overlay_wrap[i][0]) continue;
        draw_text(state->renderer, state->font_small, overlay_wrap[i], x + 8, current_y, overlay_color);
        current_y += line_step;
    }
    for (int i = 0; i < save_lines; ++i) {
        if (!save_wrap[i][0]) continue;
        draw_text(state->renderer, state->font_small, save_wrap[i], x + 8, current_y, save_color);
        current_y += line_step;
    }
}

void draw_center_pane_summary(SceneEditorState *state) {
    char line_a[160];
    char line_b[160];
    int x = 0;
    int y = 0;
    if (!state || !state->renderer) return;

    x = state->center_summary_rect.x;
    y = state->center_summary_rect.y;
    if (state->viewport.requested_mode == SPACE_MODE_3D) {
        snprintf(line_a,
                 sizeof(line_a),
                 "3D Scaffold  Dist: %.2f  Orbit: %.0f/%.0f  Focus: %.2f, %.2f",
                 state->viewport.orbit_distance,
                 state->viewport.orbit_yaw_deg,
                 state->viewport.orbit_pitch_deg,
                 state->viewport.center_x,
                 state->viewport.center_y);
        snprintf(line_b,
                 sizeof(line_b),
                 "Proj: 2D lane  Alt+LMB orbit  MMB pan  Wheel dolly  F frame");
    } else {
        snprintf(line_a,
                 sizeof(line_a),
                 "2D Ortho  Zoom: %.2fx  Focus: %.2f, %.2f",
                 scene_editor_viewport_active_zoom(&state->viewport),
                 state->viewport.center_x,
                 state->viewport.center_y);
        snprintf(line_b,
                 sizeof(line_b),
                 "MMB pan  Wheel zoom  F frame");
    }
    draw_text(state->renderer, state->font_small, line_a, x, y, COLOR_TEXT_DIM);
    draw_text(state->renderer, state->font_small, line_b, x, y + 18, COLOR_TEXT_DIM);
}

static const char *summary_path_leaf(const char *path) {
    const char *slash = NULL;
    if (!path || !path[0]) return "--";
    slash = strrchr(path, '/');
    return slash && slash[1] ? slash + 1 : path;
}

static const char *mesh_preview_state_label(const PhysicsSimRuntimeMeshPreviewInstance *preview) {
    if (!preview) return "none";
    if (!preview->preview_path_resolved) return "unresolved";
    if (!preview->preview_file_exists) return "missing";
    if (!preview->preview_file_readable) return "unreadable";
    if (!preview->preview_schema_supported) return "unsupported";
    if (!preview->preview_metadata_valid) return "metadata missing";
    return "metadata ok";
}

static const char *mesh_solver_effect_label(PhysicsSimRuntimeMeshEditorRole role) {
    switch (role) {
    case PHYSICS_SIM_RUNTIME_MESH_EDITOR_ROLE_SOLID:
        return "solid obstacle";
    case PHYSICS_SIM_RUNTIME_MESH_EDITOR_ROLE_VISUAL_ONLY:
        return "visual only";
    case PHYSICS_SIM_RUNTIME_MESH_EDITOR_ROLE_SURFACE_EMITTER:
        return "surface emitter";
    case PHYSICS_SIM_RUNTIME_MESH_EDITOR_ROLE_SURFACE_HEAT_EMITTER:
        return "heat emitter";
    case PHYSICS_SIM_RUNTIME_MESH_EDITOR_ROLE_BOUNDARY_FLOW_EMITTER:
        return "flow emitter";
    default:
        return "unknown";
    }
}

static void draw_summary_wrapped_line(SceneEditorState *state,
                                      const char *line,
                                      int x,
                                      int text_w,
                                      int *current_y,
                                      int line_step,
                                      int bottom_y,
                                      SDL_Color color) {
    char wrapped[2][192];
    if (!state || !line || !line[0] || !current_y) return;
    if (bottom_y > 0 && *current_y + line_step > bottom_y) return;
    wrap_text_lines(state->renderer, state->font_small, line, text_w, 2, wrapped, NULL);
    for (int i = 0; i < 2 && wrapped[i][0]; ++i) {
        if (bottom_y > 0 && *current_y + line_step > bottom_y) return;
        draw_text(state->renderer, state->font_small, wrapped[i], x, *current_y, color);
        *current_y += line_step;
    }
}

void draw_right_panel_summary(SceneEditorState *state) {
    char selected_line_a[160];
    char selected_line_b[160];
    char emitter_line[160];
    char domain_line[160];
    char wind_line[160];
    char mesh_solver_line[160];
    char mesh_path_line[160];
    char mesh_preview_line[160];
    char mesh_runtime_line[160];
    char mesh_wind_line[160];
    const CoreSceneObjectContract *selected_retained = NULL;
    const PhysicsSimObjectOverlay *selected_overlay = NULL;
    const PhysicsSimEmitterOverlay *selected_emitter = NULL;
    const PhysicsSimRuntimeMeshPreviewInstance *selected_mesh_preview = NULL;
    const PhysicsSimRuntimeMeshOverlay *selected_mesh_overlay = NULL;
    const PhysicsSimEmitterOverlay *selected_mesh_emitter = NULL;
    const PhysicsSimRuntimeMeshDiagnostic *selected_mesh_diag = NULL;
    const PhysicsSimDomainOverlay *scene_domain = NULL;
    SceneEditorWindSetupSummary wind_setup;
    int line_y = 0;
    int line_step = 0;
    int x = 0;
    int text_w = 0;
    int summary_bottom = 0;
    int current_y = 0;
    if (!state || !state->renderer) return;

    x = state->right_panel_rect.x + 12;
    line_y = state->overlay_summary_rect.y;
    line_step = panel_font_height(state->renderer, state->font_small, 17) + 7;
    if (line_step < 22) line_step = 22;
    text_w = state->overlay_summary_rect.w;

    selected_line_a[0] = '\0';
    selected_line_b[0] = '\0';
    emitter_line[0] = '\0';
    domain_line[0] = '\0';
    wind_line[0] = '\0';
    mesh_solver_line[0] = '\0';
    mesh_path_line[0] = '\0';
    mesh_preview_line[0] = '\0';
    mesh_runtime_line[0] = '\0';
    mesh_wind_line[0] = '\0';
    wind_setup = scene_editor_wind_setup_summary(&state->cfg, &state->session);
    if (state->selection_kind == SELECTION_EMITTER &&
        state->selected_emitter >= 0 &&
        state->selected_emitter < (int)state->working.emitter_count) {
        const FluidEmitter *em = &state->working.emitters[state->selected_emitter];
        snprintf(selected_line_a, sizeof(selected_line_a),
                 "Selected: %s emitter #%d",
                 emitter_type_name(em->type),
                 state->selected_emitter);
        snprintf(selected_line_b, sizeof(selected_line_b),
                 "Radius %.2f  Strength %.1f  Pos %.2f, %.2f",
                 em->radius,
                 em->strength,
                 em->position_x,
                 em->position_y);
    } else if (state->selection_kind == SELECTION_OBJECT &&
               state->selected_object >= 0 &&
               state->selected_object < (int)state->working.object_count) {
        const PresetObject *obj = &state->working.objects[state->selected_object];
        snprintf(selected_line_a, sizeof(selected_line_a),
                 "Selected: %s object #%d",
                 (obj->type == PRESET_OBJECT_BOX) ? "Box" : "Circle",
                 state->selected_object);
        snprintf(selected_line_b, sizeof(selected_line_b),
                 "Size %.2f x %.2f x %.2f  Pos %.2f, %.2f",
                 obj->size_x,
                 obj->size_y,
                 obj->size_z,
                 obj->position_x,
                 obj->position_y);
    } else if (state->selection_kind == SELECTION_IMPORT &&
               state->selected_row >= 0 &&
               state->selected_row < (int)state->working.import_shape_count) {
        const ImportedShape *imp = &state->working.import_shapes[state->selected_row];
        snprintf(selected_line_a, sizeof(selected_line_a),
                 "Selected: import #%d",
                 state->selected_row);
        snprintf(selected_line_b, sizeof(selected_line_b),
                 "Scale %.2f  Rot %.1f  Pos %.2f, %.2f",
                 imp->scale,
                 imp->rotation_deg,
                 imp->position_x,
                 imp->position_y);
    } else {
        snprintf(selected_line_a, sizeof(selected_line_a), "Selected: none");
    }

    selected_retained = physics_sim_editor_session_selected_object(&state->session);
    selected_overlay = physics_sim_editor_session_selected_object_overlay(&state->session);
    selected_emitter = physics_sim_editor_session_selected_object_emitter(&state->session);
    selected_mesh_preview = physics_sim_editor_session_selected_runtime_mesh_preview(&state->session);
    selected_mesh_overlay = physics_sim_editor_session_selected_runtime_mesh_overlay(&state->session);
    selected_mesh_emitter = physics_sim_editor_session_selected_runtime_mesh_emitter(&state->session);
    if (selected_retained) {
        const char *object_id = selected_retained->object.object_id[0]
                                    ? selected_retained->object.object_id
                                    : "(unnamed)";
        snprintf(selected_line_a, sizeof(selected_line_a),
                 "Selected: %s",
                 object_id);
        snprintf(selected_line_b, sizeof(selected_line_b),
                 "%s",
                 physics_sim_editor_session_object_kind_label(selected_retained->kind));
        if (selected_overlay) {
            snprintf(selected_line_b,
                     sizeof(selected_line_b),
                     "%s  %s",
                     physics_sim_editor_session_object_kind_label(selected_retained->kind),
                     physics_sim_editor_session_motion_mode_label(selected_overlay->motion_mode));
        }
        if (selected_mesh_overlay) {
            snprintf(selected_line_b,
                     sizeof(selected_line_b),
                     "%s  Mesh %s",
                     physics_sim_editor_session_object_kind_label(selected_retained->kind),
                     physics_sim_editor_session_runtime_mesh_role_label(selected_mesh_overlay->role));
        }
        if (selected_mesh_emitter) {
            snprintf(emitter_line,
                     sizeof(emitter_line),
                     "Mesh Emitter: %s  %s  Thermal %.1f",
                     physics_sim_editor_session_emitter_type_label(selected_mesh_emitter->type),
                     physics_sim_editor_session_emitter_source_mode_3d_label(selected_mesh_emitter->source_mode_3d),
                     selected_mesh_emitter->thermal_buoyancy_3d);
        } else if (selected_emitter) {
            snprintf(emitter_line,
                     sizeof(emitter_line),
                     "Emitter: %s  %s  Thermal %.1f",
                     physics_sim_editor_session_emitter_type_label(selected_emitter->type),
                     physics_sim_editor_session_emitter_source_mode_3d_label(selected_emitter->source_mode_3d),
                     selected_emitter->thermal_buoyancy_3d);
        } else {
            snprintf(emitter_line, sizeof(emitter_line), "Emitter: none");
        }
    }
    if (selected_mesh_overlay) {
        snprintf(mesh_solver_line,
                 sizeof(mesh_solver_line),
                 "Mesh solver: %s  %s",
                 physics_sim_editor_session_runtime_mesh_role_label(selected_mesh_overlay->role),
                 mesh_solver_effect_label(selected_mesh_overlay->role));
    }
    if (selected_mesh_preview) {
        const char *mesh_path =
            selected_mesh_preview->runtime_path_resolved
                ? selected_mesh_preview->runtime_mesh_path
                : selected_mesh_preview->runtime_mesh_path_hint;
        snprintf(mesh_path_line,
                 sizeof(mesh_path_line),
                 "Mesh file: %s%s%s",
                 selected_mesh_preview->runtime_path_resolved ? "" : "missing ",
                 summary_path_leaf(mesh_path),
                 selected_mesh_preview->runtime_path_recovered ? " recovered" : "");
        if (selected_mesh_preview->preview_metadata_valid) {
            snprintf(mesh_preview_line,
                     sizeof(mesh_preview_line),
                     "Preview: %s  src %zuv/%zut  edge %zu",
                     mesh_preview_state_label(selected_mesh_preview),
                     selected_mesh_preview->metadata.source_vertex_count,
                     selected_mesh_preview->metadata.source_triangle_count,
                     selected_mesh_preview->metadata.preview_edge_count);
        } else {
            snprintf(mesh_preview_line,
                     sizeof(mesh_preview_line),
                     "Preview: %s",
                     mesh_preview_state_label(selected_mesh_preview));
        }
    }
    if (state->selected_mesh_diagnostic_cache.attempted) {
        selected_mesh_diag = &state->selected_mesh_diagnostic_cache.diagnostic;
        if (state->selected_mesh_diagnostic_cache.valid && selected_mesh_diag->valid) {
            size_t footprint_cells =
                selected_mesh_diag->role == PHYSICS_SIM_RUNTIME_MESH_FLUID_ROLE_EMITTER
                    ? selected_mesh_diag->emitter_footprint_voxel_count
                    : selected_mesh_diag->obstacle_voxel_count;
            if ((!selected_mesh_diag->runtime_path_resolved ||
                 (selected_mesh_diag->domain_overlap_tested &&
                  !selected_mesh_diag->world_bounds_intersect_domain)) &&
                strcmp(selected_mesh_diag->diagnostics, "ok") != 0) {
                snprintf(mesh_runtime_line,
                         sizeof(mesh_runtime_line),
                         "Runtime: %s",
                         selected_mesh_diag->diagnostics);
            } else {
                snprintf(mesh_runtime_line,
                         sizeof(mesh_runtime_line),
                         "Runtime: %zu vox  %zu tris  BVH %zu/%zu d%zu",
                         footprint_cells,
                         selected_mesh_diag->runtime_triangle_count,
                         selected_mesh_diag->accel_node_count,
                         selected_mesh_diag->accel_leaf_count,
                         selected_mesh_diag->accel_max_depth);
                snprintf(mesh_wind_line,
                         sizeof(mesh_wind_line),
                         "Wind mesh: area %.3f  file %s%s",
                         selected_mesh_diag->wind_projected_area_yz,
                         selected_mesh_diag->file_signature_valid ? "sig ok" : "sig --",
                         selected_mesh_diag->budget_limited ? "  budget fallback" : "");
            }
        } else {
            snprintf(mesh_runtime_line,
                     sizeof(mesh_runtime_line),
                     "Runtime: %s",
                     selected_mesh_diag->diagnostics[0]
                         ? selected_mesh_diag->diagnostics
                         : "diagnostic unavailable");
        }
    }
    scene_domain = physics_sim_editor_session_scene_domain(&state->session);
    if (scene_domain) {
        double width = 0.0;
        double height = 0.0;
        double depth = 0.0;
        physics_sim_editor_session_scene_domain_dimensions(&state->session, &width, &height, &depth);
        snprintf(domain_line,
                 sizeof(domain_line),
                 "Domain: %s  %.2f x %.2f x %.2f",
                 scene_domain->seeded_from_retained_bounds ? "derived" : "manual",
                 width,
                 height,
                 depth);
        if (wind_setup.active) {
            snprintf(wind_line,
                     sizeof(wind_line),
                     "Wind: %s -> %s  %.1f / %.1f",
                     wind_setup.inlet_label,
                     wind_setup.outlet_label,
                     wind_setup.config.inflow_speed,
                     wind_setup.config.inflow_density);
        }
    }

    current_y = line_y;
    summary_bottom = physics_sim_editor_session_has_retained_scene(&state->session)
                         ? state->btn_apply_overlay.rect.y - 12
                         : state->btn_save.rect.y - 12;
    draw_summary_wrapped_line(state, selected_line_a, x, text_w, &current_y, line_step, summary_bottom, COLOR_TEXT);
    draw_summary_wrapped_line(state, selected_line_b, x, text_w, &current_y, line_step, summary_bottom, COLOR_TEXT_DIM);
    draw_summary_wrapped_line(state, emitter_line, x, text_w, &current_y, line_step, summary_bottom, COLOR_TEXT_DIM);
    draw_summary_wrapped_line(state, mesh_solver_line, x, text_w, &current_y, line_step, summary_bottom, COLOR_TEXT_DIM);
    draw_summary_wrapped_line(state, mesh_path_line, x, text_w, &current_y, line_step, summary_bottom, COLOR_TEXT_DIM);
    draw_summary_wrapped_line(state, mesh_preview_line, x, text_w, &current_y, line_step, summary_bottom, COLOR_TEXT_DIM);
    draw_summary_wrapped_line(state, mesh_runtime_line, x, text_w, &current_y, line_step, summary_bottom, COLOR_TEXT_DIM);
    draw_summary_wrapped_line(state, mesh_wind_line, x, text_w, &current_y, line_step, summary_bottom, COLOR_TEXT_DIM);
    draw_summary_wrapped_line(state, domain_line, x, text_w, &current_y, line_step, summary_bottom, COLOR_TEXT_DIM);
    draw_summary_wrapped_line(state, wind_line, x, text_w, &current_y, line_step, summary_bottom, COLOR_TEXT_DIM);
    draw_status_card(state,
                     x,
                     current_y,
                     state->overlay_summary_rect.w,
                     summary_bottom);
}
