#include "config/config_loader.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct JsonBlock {
    const char *start;
    size_t      len;
} JsonBlock;

static bool read_file_contents(const char *path, char **out_buffer, size_t *out_size) {
    if (!path || !out_buffer) return false;
    FILE *f = fopen(path, "rb");
    if (!f) return false;

    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return false;
    }
    long file_size = ftell(f);
    if (file_size < 0) {
        fclose(f);
        return false;
    }
    rewind(f);

    char *buffer = (char *)malloc((size_t)file_size + 1);
    if (!buffer) {
        fclose(f);
        return false;
    }

    size_t read_bytes = fread(buffer, 1, (size_t)file_size, f);
    fclose(f);
    if (read_bytes != (size_t)file_size) {
        free(buffer);
        return false;
    }
    buffer[file_size] = '\0';

    *out_buffer = buffer;
    if (out_size) {
        *out_size = (size_t)file_size;
    }
    return true;
}

static bool json_find_object(const char *json, const char *key, JsonBlock *out_block) {
    if (!json || !key || !out_block) return false;
    char pattern[64];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    const char *key_pos = strstr(json, pattern);
    if (!key_pos) return false;

    const char *brace = strchr(key_pos, '{');
    if (!brace) return false;

    int depth = 0;
    const char *p = brace;
    while (*p) {
        if (*p == '{') depth++;
        else if (*p == '}') {
            depth--;
            if (depth == 0) {
                ++p; // include closing brace
                break;
            }
        }
        ++p;
    }

    if (depth != 0) return false;

    out_block->start = brace;
    out_block->len   = (size_t)(p - brace);
    return true;
}

static char *copy_block_text(const JsonBlock *block) {
    if (!block || !block->start || block->len == 0) return NULL;
    char *buf = (char *)malloc(block->len + 1);
    if (!buf) return NULL;
    memcpy(buf, block->start, block->len);
    buf[block->len] = '\0';
    return buf;
}

static bool json_block_number(const JsonBlock *block, const char *key, double *out_value) {
    if (!block || !key || !out_value) return false;
    char *copy = copy_block_text(block);
    if (!copy) return false;

    char pattern[64];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    char *key_pos = strstr(copy, pattern);
    if (!key_pos) {
        free(copy);
        return false;
    }

    char *colon = strchr(key_pos + strlen(pattern), ':');
    if (!colon) {
        free(copy);
        return false;
    }

    colon++;
    while (*colon && isspace((unsigned char)*colon)) {
        ++colon;
    }

    char *endptr = NULL;
    double value = strtod(colon, &endptr);
    if (colon == endptr) {
        free(copy);
        return false;
    }

    *out_value = value;
    free(copy);
    return true;
}

static void apply_window_settings(const char *json, AppConfig *cfg) {
    JsonBlock block;
    if (!json_find_object(json, "window", &block)) return;

    double val;
    if (json_block_number(&block, "width", &val))  cfg->window_w = (int)val;
    if (json_block_number(&block, "height", &val)) cfg->window_h = (int)val;
}

static void apply_grid_settings(const char *json, AppConfig *cfg) {
    JsonBlock block;
    if (!json_find_object(json, "grid", &block)) return;

    double val;
    if (json_block_number(&block, "width", &val))  cfg->grid_w = (int)val;
    if (json_block_number(&block, "height", &val)) cfg->grid_h = (int)val;
}

static void apply_timing_settings(const char *json, AppConfig *cfg) {
    JsonBlock block;
    if (!json_find_object(json, "timing", &block)) return;

    double val;
    if (json_block_number(&block, "min_dt", &val))    cfg->min_dt = val;
    if (json_block_number(&block, "max_dt", &val))    cfg->max_dt = val;
    if (json_block_number(&block, "substeps", &val))  cfg->physics_substeps = (int)val;
    if (json_block_number(&block, "fixed_dt", &val))  cfg->physics_fixed_dt = val;
    if (json_block_number(&block, "max_steps_per_frame", &val)) {
        cfg->max_physics_steps_per_frame = (int)val;
    }
}

static void apply_command_settings(const char *json, AppConfig *cfg) {
    JsonBlock block;
    if (!json_find_object(json, "commands", &block)) return;

    double val;
    if (json_block_number(&block, "max_per_frame", &val)) {
        cfg->command_batch_limit = (int)val;
    }
}

static void apply_fluid_settings(const char *json, AppConfig *cfg) {
    JsonBlock block;
    if (!json_find_object(json, "fluid", &block)) return;

    double val;
    if (json_block_number(&block, "diffusion", &val) ||
        json_block_number(&block, "density_diffusion", &val)) {
        cfg->density_diffusion = (float)val;
    }
    if (json_block_number(&block, "viscosity", &val) ||
        json_block_number(&block, "velocity_damping", &val)) {
        cfg->velocity_damping = (float)val;
    }
    if (json_block_number(&block, "density_decay", &val) ||
        json_block_number(&block, "decay", &val)) {
        cfg->density_decay = (float)val;
    }
    if (json_block_number(&block, "buoyancy", &val) ||
        json_block_number(&block, "buoyancy_force", &val)) {
        cfg->fluid_buoyancy_force = (float)val;
    }
    if (json_block_number(&block, "solver_iterations", &val) ||
        json_block_number(&block, "iterations", &val)) {
        cfg->fluid_solver_iterations = (int)val;
    }
}

static void apply_input_settings(const char *json, AppConfig *cfg) {
    JsonBlock block;
    if (!json_find_object(json, "input", &block)) return;

    double val;
    if (json_block_number(&block, "stroke_sample_rate", &val) && val > 0.0) {
        cfg->stroke_sample_rate = val;
    }
    if (json_block_number(&block, "stroke_spacing", &val) && val > 0.0) {
        cfg->stroke_spacing = (float)val;
    }
}

static void apply_emitter_settings(const char *json, AppConfig *cfg) {
    JsonBlock block;
    if (!json_find_object(json, "emitters", &block)) return;

    double val;
    if (json_block_number(&block, "density_multiplier", &val)) {
        cfg->emitter_density_multiplier = (float)val;
    }
    if (json_block_number(&block, "velocity_multiplier", &val)) {
        cfg->emitter_velocity_multiplier = (float)val;
    }
    if (json_block_number(&block, "sink_multiplier", &val)) {
        cfg->emitter_sink_multiplier = (float)val;
    }
}

static void apply_render_settings(const char *json, AppConfig *cfg) {
    JsonBlock block;
    if (!json_find_object(json, "render", &block)) return;

    double val;
    if (json_block_number(&block, "blur_enabled", &val)) {
        cfg->enable_render_blur = (val != 0.0);
    }
    if (json_block_number(&block, "black_level", &val)) {
        if (val < 0.0) val = 0.0;
        if (val > 255.0) val = 255.0;
        cfg->render_black_level = (int)val;
    }
}

static void apply_headless_settings(const char *json, AppConfig *cfg) {
    JsonBlock block;
    if (!json_find_object(json, "headless", &block)) return;

    double val;
    if (json_block_number(&block, "enabled", &val)) {
        cfg->headless_enabled = (val != 0.0);
    }
    if (json_block_number(&block, "frame_count", &val)) {
        cfg->headless_frame_count = (int)val;
    }
    if (json_block_number(&block, "custom_slot_index", &val)) {
        cfg->headless_custom_slot = (int)val;
    }
    if (json_block_number(&block, "quality_index", &val)) {
        cfg->headless_quality_index = (int)val;
    }
    if (json_block_number(&block, "skip_present", &val)) {
        cfg->headless_skip_present = (val != 0.0);
    }

    char *copy = copy_block_text(&block);
    if (copy) {
        char pattern[64];
        snprintf(pattern, sizeof(pattern), "\"output_dir\"");
        char *key_pos = strstr(copy, pattern);
        if (key_pos) {
            char *colon = strchr(key_pos + strlen(pattern), ':');
            if (colon) {
                colon++;
                while (*colon && isspace((unsigned char)*colon)) colon++;
                if (*colon == '"') {
                    colon++;
                    char *end = strchr(colon, '"');
                    if (end) {
                        size_t len = (size_t)(end - colon);
                        if (len >= sizeof(cfg->headless_output_dir)) {
                            len = sizeof(cfg->headless_output_dir) - 1;
                        }
                        memcpy(cfg->headless_output_dir, colon, len);
                        cfg->headless_output_dir[len] = '\0';
                    }
                }
            }
        }
        free(copy);
    }
}

static void apply_collider_settings(const char *json, AppConfig *cfg) {
    JsonBlock block;
    if (!json_find_object(json, "collider", &block)) return;

    double val;
    if (json_block_number(&block, "max_loops", &val)) {
        cfg->collider_max_loops = (int)val;
    }
    if (json_block_number(&block, "max_loop_vertices", &val)) {
        cfg->collider_max_loop_vertices = (int)val;
    }
    if (json_block_number(&block, "max_parts", &val)) {
        cfg->collider_max_parts = (int)val;
    }
    if (json_block_number(&block, "max_part_vertices", &val)) {
        cfg->collider_max_part_vertices = (int)val;
    }
    if (json_block_number(&block, "simplify_epsilon", &val)) {
        cfg->collider_simplify_epsilon = (float)val;
    }
    if (json_block_number(&block, "curve_sample_rate", &val)) {
        cfg->collider_curve_sample_rate = (float)val;
    }
    if (json_block_number(&block, "raster_padding", &val)) {
        cfg->collider_raster_padding = (float)val;
    }
    if (json_block_number(&block, "primitives_enabled", &val)) {
        cfg->collider_primitives_enabled = (val != 0.0);
    }
    if (json_block_number(&block, "corner_angle_deg", &val)) {
        cfg->collider_corner_angle_deg = (float)val;
    }
    if (json_block_number(&block, "corner_simplify_eps", &val)) {
        cfg->collider_corner_simplify_eps = (float)val;
    }
    if (json_block_number(&block, "max_primitives", &val)) {
        cfg->collider_max_primitives = (int)val;
    }
    if (json_block_number(&block, "max_hull_vertices", &val)) {
        cfg->collider_max_hull_vertices = (int)val;
    }
    if (json_block_number(&block, "capsule_max_len_ratio", &val)) {
        cfg->collider_capsule_max_len_ratio = (float)val;
    }
    if (json_block_number(&block, "region_grid_res", &val)) {
        cfg->collider_region_grid_res = (int)val;
    }
    if (json_block_number(&block, "region_min_cells", &val)) {
        cfg->collider_region_min_cells = (int)val;
    }
    if (json_block_number(&block, "region_offset_eps", &val)) {
        cfg->collider_region_offset_eps = (float)val;
    }
}

static void apply_broadphase_settings(const char *json, AppConfig *cfg) {
    JsonBlock block;
    if (!json_find_object(json, "broadphase", &block)) return;
    double val;
    if (json_block_number(&block, "enabled", &val)) {
        cfg->physics_broadphase_enabled = (val != 0.0);
    }
    if (json_block_number(&block, "cell_size", &val)) {
        cfg->physics_broadphase_cell_size = (float)val;
    }
}

static void apply_debug_settings(const char *json, AppConfig *cfg) {
    JsonBlock block;
    if (!json_find_object(json, "debug", &block)) return;
    double val;
    if (json_block_number(&block, "collider_logs", &val)) {
        cfg->collider_debug_logs = (val != 0.0);
    }
}

static void apply_export_settings(const char *json, AppConfig *cfg) {
    JsonBlock block;
    if (!json_find_object(json, "exports", &block)) return;

    double val;
    if (json_block_number(&block, "save_volume_frames", &val)) {
        cfg->save_volume_frames = (val != 0.0);
    }
    if (json_block_number(&block, "save_render_frames", &val)) {
        cfg->save_render_frames = (val != 0.0);
    }
}

static void apply_simulation_settings(const char *json, AppConfig *cfg) {
    JsonBlock block;
    if (!json_find_object(json, "simulation", &block)) return;

    double val;
    if (json_block_number(&block, "mode", &val)) {
        int mode = (int)val;
        if (mode < SIM_MODE_BOX || mode >= SIMULATION_MODE_COUNT) {
            mode = SIM_MODE_BOX;
        }
        cfg->sim_mode = (SimulationMode)mode;
    }
    if (json_block_number(&block, "tunnel_inflow_speed", &val)) {
        cfg->tunnel_inflow_speed = (float)val;
    }
    if (json_block_number(&block, "tunnel_inflow_density", &val)) {
        cfg->tunnel_inflow_density = (float)val;
    }
    if (json_block_number(&block, "tunnel_viscosity_scale", &val)) {
        cfg->tunnel_viscosity_scale = (float)val;
    }
}

static void apply_json_overrides(const char *json, AppConfig *cfg) {
    apply_window_settings(json, cfg);
    apply_grid_settings(json, cfg);
    apply_simulation_settings(json, cfg);
    apply_timing_settings(json, cfg);
    apply_command_settings(json, cfg);
    apply_fluid_settings(json, cfg);
    apply_input_settings(json, cfg);
    apply_emitter_settings(json, cfg);
    apply_render_settings(json, cfg);
    apply_collider_settings(json, cfg);
    apply_broadphase_settings(json, cfg);
    apply_debug_settings(json, cfg);
    apply_headless_settings(json, cfg);
    apply_export_settings(json, cfg);
}

bool config_loader_load(AppConfig *cfg, const ConfigLoadOptions *opts) {
    if (!cfg) return false;
    *cfg = app_config_default();

    if (!opts || !opts->path) {
        fprintf(stderr, "[config] No config path provided; using defaults.\n");
        return true;
    }

    char *json = NULL;
    size_t json_size = 0;
    if (!read_file_contents(opts->path, &json, &json_size)) {
        if (opts->allow_missing) {
            fprintf(stderr,
                    "[config] Could not open %s, continuing with defaults.\n",
                    opts->path);
            return true;
        }
        fprintf(stderr,
                "[config] Failed to open %s and allow_missing=false.\n",
                opts->path);
        return false;
    }

    apply_json_overrides(json, cfg);
    fprintf(stderr, "[config] Loaded %s (%zu bytes).\n", opts->path, json_size);
    free(json);
    return true;
}

bool config_loader_save(const AppConfig *cfg, const char *path) {
    if (!cfg || !path) return false;
    FILE *f = fopen(path, "w");
    if (!f) return false;

    fprintf(f, "{\n");
    fprintf(f, "  \"window\": {\n");
    fprintf(f, "    \"width\": %d,\n", cfg->window_w);
    fprintf(f, "    \"height\": %d\n", cfg->window_h);
    fprintf(f, "  },\n");
    fprintf(f, "  \"grid\": {\n");
    fprintf(f, "    \"width\": %d,\n", cfg->grid_w);
    fprintf(f, "    \"height\": %d\n", cfg->grid_h);
    fprintf(f, "  },\n");
    fprintf(f, "  \"simulation\": {\n");
    fprintf(f, "    \"mode\": %d,\n", cfg->sim_mode);
    fprintf(f, "    \"tunnel_inflow_speed\": %.6f,\n", cfg->tunnel_inflow_speed);
    fprintf(f, "    \"tunnel_inflow_density\": %.6f,\n", cfg->tunnel_inflow_density);
    fprintf(f, "    \"tunnel_viscosity_scale\": %.6f\n", cfg->tunnel_viscosity_scale);
    fprintf(f, "  },\n");
    fprintf(f, "  \"timing\": {\n");
    fprintf(f, "    \"min_dt\": %.9f,\n", cfg->min_dt);
    fprintf(f, "    \"max_dt\": %.9f,\n", cfg->max_dt);
    fprintf(f, "    \"substeps\": %d\n", cfg->physics_substeps);
    fprintf(f, "  },\n");
    fprintf(f, "  \"commands\": {\n");
    fprintf(f, "    \"max_per_frame\": %d\n", cfg->command_batch_limit);
    fprintf(f, "  },\n");
    fprintf(f, "  \"fluid\": {\n");
    fprintf(f, "    \"diffusion\": %.6f,\n", cfg->density_diffusion);
    fprintf(f, "    \"viscosity\": %.6f,\n", cfg->velocity_damping);
    fprintf(f, "    \"density_decay\": %.6f,\n", cfg->density_decay);
    fprintf(f, "    \"buoyancy\": %.6f,\n", cfg->fluid_buoyancy_force);
    fprintf(f, "    \"solver_iterations\": %d\n", cfg->fluid_solver_iterations);
    fprintf(f, "  },\n");
    fprintf(f, "  \"render\": {\n");
    fprintf(f, "    \"blur_enabled\": %s,\n", cfg->enable_render_blur ? "true" : "false");
    fprintf(f, "    \"black_level\": %d\n", cfg->render_black_level);
    fprintf(f, "  },\n");
    fprintf(f, "  \"collider\": {\n");
    fprintf(f, "    \"max_loops\": %d,\n", cfg->collider_max_loops);
    fprintf(f, "    \"max_loop_vertices\": %d,\n", cfg->collider_max_loop_vertices);
    fprintf(f, "    \"max_parts\": %d,\n", cfg->collider_max_parts);
    fprintf(f, "    \"max_part_vertices\": %d,\n", cfg->collider_max_part_vertices);
    fprintf(f, "    \"simplify_epsilon\": %.6f,\n", cfg->collider_simplify_epsilon);
    fprintf(f, "    \"curve_sample_rate\": %.6f,\n", cfg->collider_curve_sample_rate);
    fprintf(f, "    \"raster_padding\": %.6f,\n", cfg->collider_raster_padding);
    fprintf(f, "    \"primitives_enabled\": %s,\n", cfg->collider_primitives_enabled ? "true" : "false");
    fprintf(f, "    \"corner_angle_deg\": %.6f,\n", cfg->collider_corner_angle_deg);
    fprintf(f, "    \"corner_simplify_eps\": %.6f,\n", cfg->collider_corner_simplify_eps);
    fprintf(f, "    \"max_primitives\": %d,\n", cfg->collider_max_primitives);
    fprintf(f, "    \"max_hull_vertices\": %d,\n", cfg->collider_max_hull_vertices);
    fprintf(f, "    \"capsule_max_len_ratio\": %.6f,\n", cfg->collider_capsule_max_len_ratio);
    fprintf(f, "    \"region_grid_res\": %d,\n", cfg->collider_region_grid_res);
    fprintf(f, "    \"region_min_cells\": %d,\n", cfg->collider_region_min_cells);
    fprintf(f, "    \"region_offset_eps\": %.6f\n", cfg->collider_region_offset_eps);
    fprintf(f, "  },\n");
    fprintf(f, "  \"broadphase\": {\n");
    fprintf(f, "    \"enabled\": %s,\n", cfg->physics_broadphase_enabled ? "true" : "false");
    fprintf(f, "    \"cell_size\": %.6f\n", cfg->physics_broadphase_cell_size);
    fprintf(f, "  },\n");
    fprintf(f, "  \"debug\": {\n");
    fprintf(f, "    \"collider_logs\": %d\n", cfg->collider_debug_logs ? 1 : 0);
    fprintf(f, "  },\n");
    fprintf(f, "  \"headless\": {\n");
    fprintf(f, "    \"enabled\": %s,\n", cfg->headless_enabled ? "true" : "false");
    fprintf(f, "    \"frame_count\": %d,\n", cfg->headless_frame_count);
    fprintf(f, "    \"custom_slot_index\": %d,\n", cfg->headless_custom_slot);
    fprintf(f, "    \"quality_index\": %d,\n", cfg->headless_quality_index);
    fprintf(f, "    \"skip_present\": %s\n", cfg->headless_skip_present ? "true" : "false");
    fprintf(f, "  }\n");
    fprintf(f, "}\n");

    fclose(f);
    return true;
}
