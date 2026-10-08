#define _DARWIN_C_SOURCE
#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "config/config_loader.h"
#include "app/physics_sim_persistence.h"
#include "app/physics_sim_job_json.h"

#include <ctype.h>
#include <errno.h>
#include <float.h>
#include <limits.h>
#include <math.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct JsonBlock { json_object *object; } JsonBlock;

#define CONFIG_MAX_BYTES (1024 * 1024)

/* Read admission is distinct from generated-output admission: source config is
   supported, but linked/special components and parent traversal are refused. */
static int open_config_regular(const char *path, bool *missing) {
    char copy[4096];
    *missing = false;
    if (!path || !path[0] || strlen(path) >= sizeof(copy)) return -1;
    const char *selected = path;
    if (strncmp(path, "/tmp/", 5) == 0) {
        if (snprintf(copy, sizeof(copy), "/private%s", path) >= (int)sizeof(copy)) return -1;
    } else if (strncmp(path, "/var/", 5) == 0) {
        if (snprintf(copy, sizeof(copy), "/private%s", path) >= (int)sizeof(copy)) return -1;
    } else strcpy(copy, selected);
    size_t length = strlen(copy);
    if (!length || copy[length - 1] == '/') return -1;
    char syntax[4096]; strcpy(syntax, copy);
    char *syntax_state = NULL; int syntax_depth = 0;
    for (char *item = strtok_r(syntax, "/", &syntax_state); item; item = strtok_r(NULL, "/", &syntax_state)) {
        if (++syntax_depth > 128 || strcmp(item, "..") == 0 || strcmp(item, ".git") == 0 ||
            strcmp(item, ".ssh") == 0 || strcmp(item, ".aws") == 0) return -1;
    }
    int parent = open(copy[0] == '/' ? "/" : ".", O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (parent < 0) return -1;
    char *state = NULL;
    char *part = strtok_r(copy, "/", &state);
    int depth = 0;
    while (part) {
        char *next = strtok_r(NULL, "/", &state);
        if (++depth > 128 || strcmp(part, "..") == 0 || strcmp(part, ".git") == 0 ||
            strcmp(part, ".ssh") == 0 || strcmp(part, ".aws") == 0) break;
        if (strcmp(part, ".") == 0) {
            if (!next) break;
            part = next; continue;
        }
        int descriptor = openat(parent, part, O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC |
                                (next ? O_DIRECTORY : 0));
        if (descriptor < 0) { *missing = errno == ENOENT; break; }
        close(parent); parent = descriptor;
        if (!next) {
            struct stat st;
            if (fstat(parent, &st) == 0 && S_ISREG(st.st_mode) && st.st_nlink == 1 &&
                st.st_size > 0 && st.st_size <= CONFIG_MAX_BYTES) return parent;
            break;
        }
        part = next;
    }
    close(parent);
    return -1;
}

static bool config_same_file(const struct stat *a, const struct stat *b) {
    if (a->st_dev != b->st_dev || a->st_ino != b->st_ino || a->st_mode != b->st_mode ||
        a->st_size != b->st_size || a->st_nlink != b->st_nlink) return false;
#ifdef __APPLE__
    return a->st_mtimespec.tv_sec == b->st_mtimespec.tv_sec && a->st_mtimespec.tv_nsec == b->st_mtimespec.tv_nsec &&
        a->st_ctimespec.tv_sec == b->st_ctimespec.tv_sec && a->st_ctimespec.tv_nsec == b->st_ctimespec.tv_nsec;
#else
    return a->st_mtim.tv_sec == b->st_mtim.tv_sec && a->st_mtim.tv_nsec == b->st_mtim.tv_nsec &&
        a->st_ctim.tv_sec == b->st_ctim.tv_sec && a->st_ctim.tv_nsec == b->st_ctim.tv_nsec;
#endif
}

static bool read_file_contents(const char *path, char **out_buffer, size_t *out_size, bool *missing) {
    *out_buffer = NULL;
    int descriptor = open_config_regular(path, missing);
    if (descriptor < 0) return false;
    struct stat before, after, named;
    bool valid = fstat(descriptor, &before) == 0 && S_ISREG(before.st_mode) &&
        before.st_nlink == 1 && before.st_size > 0 && before.st_size <= CONFIG_MAX_BYTES;
    size_t size = valid ? (size_t)before.st_size : 0;
    char *buffer = valid ? malloc(size + 1) : NULL;
    valid = buffer != NULL;
    size_t offset = 0;
    while (valid && offset < size) {
        ssize_t count = read(descriptor, buffer + offset, size - offset);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) { valid = false; break; }
        offset += (size_t)count;
    }
    char extra;
    ssize_t tail = -1;
    if (valid) {
        do { tail = read(descriptor, &extra, 1); } while (tail < 0 && errno == EINTR);
        valid = tail == 0 && !memchr(buffer, 0, size) &&
            fstat(descriptor, &after) == 0 && config_same_file(&before, &after);
    }
    bool now_missing = false;
    int current = valid ? open_config_regular(path, &now_missing) : -1;
    valid = valid && current >= 0 && fstat(current, &named) == 0 && config_same_file(&before, &named);
    if (current >= 0 && close(current) != 0) valid = false;
    if (close(descriptor) != 0) valid = false;
    if (!valid) { free(buffer); *missing = false; return false; }
    buffer[size] = 0;
    *out_buffer = buffer;
    if (out_size) *out_size = size;
    return true;
}

static bool json_find_object(json_object *json, const char *key, JsonBlock *block) {
    return json_object_object_get_ex(json, key, &block->object) && json_object_is_type(block->object, json_type_object);
}
static bool json_block_number(const JsonBlock *block, const char *key, double *out) {
    json_object *item = NULL;
    if (!json_object_object_get_ex(block->object, key, &item)) return false;
    if (json_object_is_type(item, json_type_boolean)) *out = json_object_get_boolean(item) ? 1 : 0;
    else if (json_object_is_type(item, json_type_int) || json_object_is_type(item, json_type_double)) *out = json_object_get_double(item);
    else return false;
    return isfinite(*out);
}
static bool json_block_string(const JsonBlock *block, const char *key, char *out, size_t capacity) {
    json_object *item = NULL;
    if (!json_object_object_get_ex(block->object, key, &item) || !json_object_is_type(item, json_type_string)) return false;
    size_t size = (size_t)json_object_get_string_len(item);
    if (size >= capacity) return false;
    memcpy(out, json_object_get_string(item), size + 1);
    return true;
}

static void apply_window_settings(json_object *json, AppConfig *cfg) {
    JsonBlock block;
    if (!json_find_object(json, "window", &block)) return;

    double val;
    if (json_block_number(&block, "width", &val))  cfg->window_w = (int)val;
    if (json_block_number(&block, "height", &val)) cfg->window_h = (int)val;
}

static void apply_grid_settings(json_object *json, AppConfig *cfg) {
    JsonBlock block;
    if (!json_find_object(json, "grid", &block)) return;

    double val;
    if (json_block_number(&block, "width", &val))  cfg->grid_w = (int)val;
    if (json_block_number(&block, "height", &val)) cfg->grid_h = (int)val;
    if (json_block_number(&block, "depth", &val))  cfg->grid_d = (int)val;
}

static void apply_timing_settings(json_object *json, AppConfig *cfg) {
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

static void apply_command_settings(json_object *json, AppConfig *cfg) {
    JsonBlock block;
    if (!json_find_object(json, "commands", &block)) return;

    double val;
    if (json_block_number(&block, "max_per_frame", &val)) {
        cfg->command_batch_limit = (int)val;
    }
}

static void apply_fluid_settings(json_object *json, AppConfig *cfg) {
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
    if (json_block_number(&block, "solver_region_cell_budget", &val)) {
        cfg->fluid_3d_solver_region_cell_budget = (int)val;
    }
    if (json_block_number(&block, "max_velocity_displacement_cells", &val)) {
        cfg->fluid_3d_max_velocity_displacement_cells = (float)val;
    }
}

static void apply_input_settings(json_object *json, AppConfig *cfg) {
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

static void apply_emitter_settings(json_object *json, AppConfig *cfg) {
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

static void apply_render_settings(json_object *json, AppConfig *cfg) {
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

static void apply_ui_settings(json_object *json, AppConfig *cfg) {
    JsonBlock block;
    if (!json_find_object(json, "ui", &block)) return;

    double val;
    if (json_block_number(&block, "text_zoom_step", &val)) {
        cfg->text_zoom_step = app_config_text_zoom_step_clamp((int)val);
    }
}

static void apply_headless_settings(json_object *json, AppConfig *cfg) {
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

    (void)json_block_string(&block, "output_dir", cfg->headless_output_dir, sizeof(cfg->headless_output_dir));
}

static void apply_collider_settings(json_object *json, AppConfig *cfg) {
    JsonBlock block;
    if (!json_find_object(json, "collider", &block)) return;

    double val;
    if (json_block_number(&block, "max_loops", &val)) cfg->collider_max_loops = (int)val;
    if (json_block_number(&block, "max_loop_vertices", &val)) cfg->collider_max_loop_vertices = (int)val;
    if (json_block_number(&block, "max_parts", &val)) cfg->collider_max_parts = (int)val;
    if (json_block_number(&block, "max_part_vertices", &val)) cfg->collider_max_part_vertices = (int)val;
    if (json_block_number(&block, "simplify_epsilon", &val)) cfg->collider_simplify_epsilon = (float)val;
    if (json_block_number(&block, "raster_padding", &val)) cfg->collider_raster_padding = (float)val;
    if (json_block_number(&block, "collider_logs", &val)) cfg->collider_debug_logs = (val != 0.0);
}

static void apply_broadphase_settings(json_object *json, AppConfig *cfg) {
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

static void apply_debug_settings(json_object *json, AppConfig *cfg) {
    JsonBlock block;
    if (!json_find_object(json, "debug", &block)) return;
    double val;
    if (json_block_number(&block, "collider_logs", &val)) {
        cfg->collider_debug_logs = (val != 0.0);
    }
}

static void apply_export_settings(json_object *json, AppConfig *cfg) {
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

static void apply_paths_settings(json_object *json, AppConfig *cfg) {
    JsonBlock block;
    if (!json_find_object(json, "paths", &block)) return;
    (void)json_block_string(&block,
                            "input_root",
                            cfg->input_root,
                            sizeof(cfg->input_root));
    (void)json_block_string(&block,
                            "atmospheric_warm_start_path",
                            cfg->atmospheric_warm_start_path,
                            sizeof(cfg->atmospheric_warm_start_path));
    (void)json_block_string(&block,
                            "retained_runtime_scene_path",
                            cfg->retained_runtime_scene_path,
                            sizeof(cfg->retained_runtime_scene_path));
}

static void apply_simulation_settings(json_object *json, AppConfig *cfg) {
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
    if (json_block_number(&block, "space_mode", &val) ||
        json_block_number(&block, "spaceMode", &val)) {
        int mode = (int)val;
        if (mode < SPACE_MODE_2D || mode >= SPACE_MODE_COUNT) {
            mode = SPACE_MODE_2D;
        }
        cfg->space_mode = (SpaceMode)mode;
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
    if (json_block_number(&block, "water_level", &val)) {
        cfg->water_level = (float)val;
    }
}

static void apply_json_overrides(json_object *json, AppConfig *cfg) {
    apply_window_settings(json, cfg);
    apply_grid_settings(json, cfg);
    apply_simulation_settings(json, cfg);
    apply_timing_settings(json, cfg);
    apply_command_settings(json, cfg);
    apply_fluid_settings(json, cfg);
    apply_input_settings(json, cfg);
    apply_emitter_settings(json, cfg);
    apply_render_settings(json, cfg);
    apply_ui_settings(json, cfg);
    apply_collider_settings(json, cfg);
    apply_broadphase_settings(json, cfg);
    apply_debug_settings(json, cfg);
    apply_headless_settings(json, cfg);
    apply_export_settings(json, cfg);
    apply_paths_settings(json, cfg);
}

typedef struct ConfigField { const char *section, *key; char kind; size_t capacity; } ConfigField;
static const ConfigField fields[] = {
    {"window", "width", 'i', 0},
    {"window", "height", 'i', 0},
    {"grid", "width", 'i', 0},
    {"grid", "height", 'i', 0},
    {"grid", "depth", 'i', 0},
    {"timing", "min_dt", 'd', 0},
    {"timing", "max_dt", 'd', 0},
    {"timing", "fixed_dt", 'd', 0},
    {"timing", "substeps", 'i', 0},
    {"timing", "max_steps_per_frame", 'i', 0},
    {"commands", "max_per_frame", 'i', 0},
    {"fluid", "diffusion", 'f', 0},
    {"fluid", "density_diffusion", 'f', 0},
    {"fluid", "viscosity", 'f', 0},
    {"fluid", "velocity_damping", 'f', 0},
    {"fluid", "density_decay", 'f', 0},
    {"fluid", "decay", 'f', 0},
    {"fluid", "buoyancy", 'f', 0},
    {"fluid", "buoyancy_force", 'f', 0},
    {"fluid", "max_velocity_displacement_cells", 'f', 0},
    {"fluid", "solver_iterations", 'i', 0},
    {"fluid", "iterations", 'i', 0},
    {"fluid", "solver_region_cell_budget", 'i', 0},
    {"input", "stroke_sample_rate", 'd', 0},
    {"input", "stroke_spacing", 'f', 0},
    {"emitters", "density_multiplier", 'f', 0},
    {"emitters", "velocity_multiplier", 'f', 0},
    {"emitters", "sink_multiplier", 'f', 0},
    {"render", "blur_enabled", 'b', 0},
    {"render", "black_level", 'i', 0},
    {"ui", "text_zoom_step", 'i', 0},
    {"headless", "enabled", 'b', 0},
    {"headless", "skip_present", 'b', 0},
    {"headless", "frame_count", 'i', 0},
    {"headless", "custom_slot_index", 'i', 0},
    {"headless", "quality_index", 'i', 0},
    {"headless", "output_dir", 's', sizeof(((AppConfig *)0)->headless_output_dir)},
    {"collider", "max_loops", 'i', 0},
    {"collider", "max_loop_vertices", 'i', 0},
    {"collider", "max_parts", 'i', 0},
    {"collider", "max_part_vertices", 'i', 0},
    {"collider", "simplify_epsilon", 'f', 0},
    {"collider", "raster_padding", 'f', 0},
    {"collider", "collider_logs", 'b', 0},
    {"broadphase", "enabled", 'b', 0},
    {"broadphase", "cell_size", 'f', 0},
    {"debug", "collider_logs", 'b', 0},
    {"exports", "save_volume_frames", 'b', 0},
    {"exports", "save_render_frames", 'b', 0},
    {"paths", "input_root", 's', sizeof(((AppConfig *)0)->input_root)},
    {"paths", "atmospheric_warm_start_path", 's', sizeof(((AppConfig *)0)->atmospheric_warm_start_path)},
    {"paths", "retained_runtime_scene_path", 's', sizeof(((AppConfig *)0)->retained_runtime_scene_path)},
    {"simulation", "mode", 'i', 0},
    {"simulation", "space_mode", 'i', 0},
    {"simulation", "spaceMode", 'i', 0},
    {"simulation", "tunnel_inflow_speed", 'f', 0},
    {"simulation", "tunnel_inflow_density", 'f', 0},
    {"simulation", "tunnel_viscosity_scale", 'f', 0},
    {"simulation", "water_level", 'f', 0},
};
static bool config_fields_valid(json_object *root) {
    for (size_t i = 0; i < sizeof(fields)/sizeof(fields[0]); ++i) {
        const ConfigField *field = &fields[i]; json_object *section = NULL, *item = NULL;
        if (!json_object_object_get_ex(root, field->section, &section)) continue;
        if (!json_object_is_type(section, json_type_object)) return false;
        if (!json_object_object_get_ex(section, field->key, &item)) continue;
        if (field->kind == 's') {
            if (!json_object_is_type(item, json_type_string) || (size_t)json_object_get_string_len(item) >= field->capacity) return false;
            continue;
        }
        if (field->kind == 'b' && json_object_is_type(item, json_type_boolean)) continue;
        if (!json_object_is_type(item, json_type_int) && !json_object_is_type(item, json_type_double)) return false;
        double number = json_object_get_double(item);
        if (!isfinite(number)) return false;
        if (field->kind == 'i' && (number < INT_MIN || number > INT_MAX || trunc(number) != number)) return false;
        if (field->kind == 'f' && (number < -FLT_MAX || number > FLT_MAX)) return false;
    }
    static const char *aliases[][3] = {
        {"fluid", "diffusion", "density_diffusion"}, {"fluid", "viscosity", "velocity_damping"},
        {"fluid", "density_decay", "decay"}, {"fluid", "buoyancy", "buoyancy_force"},
        {"fluid", "solver_iterations", "iterations"}, {"simulation", "space_mode", "spaceMode"}
    };
    for (size_t i = 0; i < sizeof(aliases)/sizeof(aliases[0]); ++i) {
        json_object *section = NULL, *first = NULL, *second = NULL;
        if (json_object_object_get_ex(root, aliases[i][0], &section) &&
            json_object_object_get_ex(section, aliases[i][1], &first) &&
            json_object_object_get_ex(section, aliases[i][2], &second) &&
            json_object_get_double(first) != json_object_get_double(second)) return false;
    }
    return true;
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
    bool missing = false;
    if (!read_file_contents(opts->path, &json, &json_size, &missing)) {
        if (opts->allow_missing && missing) {
            fprintf(stderr,
                    "[config] Missing %s, continuing with defaults.\n",
                    opts->path);
            return true;
        }
        fprintf(stderr,
                "[config] Configuration read held for %s (missing, linked, special, oversized or changed).\n",
                opts->path);
        return false;
    }

    json_object *object = physics_sim_job_json_parse(json, json_size);
    if (!object || !config_fields_valid(object)) {
        if (object) json_object_put(object);
        free(json);
        fprintf(stderr, "[config] JSON structure or known field contract held for %s.\n", opts->path);
        return false;
    }
    apply_json_overrides(object, cfg);
    json_object_put(object);
    fprintf(stderr, "[config] Loaded %s (%zu bytes).\n", opts->path, json_size);
    free(json);
    return true;
}

static void config_write_string(FILE *stream, const char *text) {
    json_object *string = json_object_new_string(text);
    if (!string) return;
    const char *encoded = json_object_to_json_string_ext(string, JSON_C_TO_STRING_PLAIN);
    if (encoded) fputs(encoded, stream);
    json_object_put(string);
}
static bool config_candidate_valid(PhysicsSimPersistence *save, FILE *stream) {
    struct stat st;
    if (ferror(stream) || fflush(stream) != 0 || fstat(fileno(stream), &st) != 0 ||
        st.st_size <= 0 || st.st_size > CONFIG_MAX_BYTES) return false;
    size_t size = (size_t)st.st_size, offset = 0;
    char *text = malloc(size);
    if (!text) return false;
    while (offset < size) {
        ssize_t count = pread(save->sidecar.pending_descriptor, text + offset, size - offset, (off_t)offset);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) break;
        offset += (size_t)count;
    }
    json_object *object = offset == size ? physics_sim_job_json_parse(text, size) : NULL;
    bool valid = object && config_fields_valid(object);
    if (object) json_object_put(object);
    free(text);
    return valid;
}

bool config_loader_save(const AppConfig *cfg, const char *path) {
    if (!cfg || !path) return false;
    if (!memchr(cfg->input_root, 0, sizeof(cfg->input_root)) ||
        !memchr(cfg->atmospheric_warm_start_path, 0, sizeof(cfg->atmospheric_warm_start_path)) ||
        !memchr(cfg->retained_runtime_scene_path, 0, sizeof(cfg->retained_runtime_scene_path)) ||
        !memchr(cfg->headless_output_dir, 0, sizeof(cfg->headless_output_dir))) return false;
    PhysicsSimPersistence save;
    FILE *f = physics_sim_persistence_begin(path, &save);
    if (!f) return false;

    fprintf(f, "{\n");
    fprintf(f, "  \"window\": {\n");
    fprintf(f, "    \"width\": %d,\n", cfg->window_w);
    fprintf(f, "    \"height\": %d\n", cfg->window_h);
    fprintf(f, "  },\n");
    fprintf(f, "  \"grid\": {\n");
    fprintf(f, "    \"width\": %d,\n", cfg->grid_w);
    fprintf(f, "    \"height\": %d,\n", cfg->grid_h);
    fprintf(f, "    \"depth\": %d\n", cfg->grid_d);
    fprintf(f, "  },\n");
    fprintf(f, "  \"simulation\": {\n");
    fprintf(f, "    \"mode\": %d,\n", cfg->sim_mode);
    fprintf(f, "    \"space_mode\": %d,\n", cfg->space_mode);
    fprintf(f, "    \"tunnel_inflow_speed\": %.9g,\n", cfg->tunnel_inflow_speed);
    fprintf(f, "    \"tunnel_inflow_density\": %.9g,\n", cfg->tunnel_inflow_density);
    fprintf(f, "    \"tunnel_viscosity_scale\": %.9g,\n", cfg->tunnel_viscosity_scale);
    fprintf(f, "    \"water_level\": %.9g\n", cfg->water_level);
    fprintf(f, "  },\n");
    fprintf(f, "  \"timing\": {\n");
    fprintf(f, "    \"min_dt\": %.17g,\n", cfg->min_dt);
    fprintf(f, "    \"max_dt\": %.17g,\n", cfg->max_dt);
    fprintf(f, "    \"substeps\": %d,\n", cfg->physics_substeps);
    fprintf(f, "    \"fixed_dt\": %.17g,\n", cfg->physics_fixed_dt);
    fprintf(f, "    \"max_steps_per_frame\": %d\n", cfg->max_physics_steps_per_frame);
    fprintf(f, "  },\n");
    fprintf(f, "  \"commands\": {\n");
    fprintf(f, "    \"max_per_frame\": %d\n", cfg->command_batch_limit);
    fprintf(f, "  },\n");
    fprintf(f, "  \"fluid\": {\n");
    fprintf(f, "    \"diffusion\": %.9g,\n", cfg->density_diffusion);
    fprintf(f, "    \"viscosity\": %.9g,\n", cfg->velocity_damping);
    fprintf(f, "    \"density_decay\": %.9g,\n", cfg->density_decay);
    fprintf(f, "    \"buoyancy\": %.9g,\n", cfg->fluid_buoyancy_force);
    fprintf(f, "    \"solver_iterations\": %d,\n", cfg->fluid_solver_iterations);
    fprintf(f, "    \"solver_region_cell_budget\": %d,\n", cfg->fluid_3d_solver_region_cell_budget);
    fprintf(f, "    \"max_velocity_displacement_cells\": %.9g\n",
            cfg->fluid_3d_max_velocity_displacement_cells);
    fprintf(f, "  },\n");
    fprintf(f, "  \"input\": {\n");
    fprintf(f, "    \"stroke_sample_rate\": %.17g,\n", cfg->stroke_sample_rate);
    fprintf(f, "    \"stroke_spacing\": %.9g\n", cfg->stroke_spacing);
    fprintf(f, "  },\n");
    fprintf(f, "  \"emitters\": {\n");
    fprintf(f, "    \"density_multiplier\": %.9g,\n", cfg->emitter_density_multiplier);
    fprintf(f, "    \"velocity_multiplier\": %.9g,\n", cfg->emitter_velocity_multiplier);
    fprintf(f, "    \"sink_multiplier\": %.9g\n", cfg->emitter_sink_multiplier);
    fprintf(f, "  },\n");
    fprintf(f, "  \"exports\": {\n");
    fprintf(f, "    \"save_volume_frames\": %s,\n", cfg->save_volume_frames ? "true" : "false");
    fprintf(f, "    \"save_render_frames\": %s\n", cfg->save_render_frames ? "true" : "false");
    fprintf(f, "  },\n");
    fprintf(f, "  \"render\": {\n");
    fprintf(f, "    \"blur_enabled\": %s,\n", cfg->enable_render_blur ? "true" : "false");
    fprintf(f, "    \"black_level\": %d\n", cfg->render_black_level);
    fprintf(f, "  },\n");
    fprintf(f, "  \"ui\": {\n");
    fprintf(f, "    \"text_zoom_step\": %d\n", app_config_text_zoom_step_clamp(cfg->text_zoom_step));
    fprintf(f, "  },\n");
    fprintf(f, "  \"paths\": {\n");
    fprintf(f, "    \"input_root\": ");
    config_write_string(f, cfg->input_root);
    fprintf(f, ",\n");
    fprintf(f, "    \"atmospheric_warm_start_path\": ");
    config_write_string(f, cfg->atmospheric_warm_start_path);
    fprintf(f, ",\n");
    fprintf(f, "    \"retained_runtime_scene_path\": ");
    config_write_string(f, cfg->retained_runtime_scene_path);
    fprintf(f, "\n");
    fprintf(f, "  },\n");
    fprintf(f, "  \"collider\": {\n");
    fprintf(f, "    \"max_loops\": %d,\n", cfg->collider_max_loops);
    fprintf(f, "    \"max_loop_vertices\": %d,\n", cfg->collider_max_loop_vertices);
    fprintf(f, "    \"max_parts\": %d,\n", cfg->collider_max_parts);
    fprintf(f, "    \"max_part_vertices\": %d,\n", cfg->collider_max_part_vertices);
    fprintf(f, "    \"simplify_epsilon\": %.9g,\n", cfg->collider_simplify_epsilon);
    fprintf(f, "    \"raster_padding\": %.9g\n", cfg->collider_raster_padding);
    fprintf(f, "  },\n");
    fprintf(f, "  \"broadphase\": {\n");
    fprintf(f, "    \"enabled\": %s,\n", cfg->physics_broadphase_enabled ? "true" : "false");
    fprintf(f, "    \"cell_size\": %.9g\n", cfg->physics_broadphase_cell_size);
    fprintf(f, "  },\n");
    fprintf(f, "  \"debug\": {\n");
    fprintf(f, "    \"collider_logs\": %d\n", cfg->collider_debug_logs ? 1 : 0);
    fprintf(f, "  },\n");
    fprintf(f, "  \"headless\": {\n");
    fprintf(f, "    \"enabled\": %s,\n", cfg->headless_enabled ? "true" : "false");
    fprintf(f, "    \"frame_count\": %d,\n", cfg->headless_frame_count);
    fprintf(f, "    \"custom_slot_index\": %d,\n", cfg->headless_custom_slot);
    fprintf(f, "    \"quality_index\": %d,\n", cfg->headless_quality_index);
    fprintf(f, "    \"skip_present\": %s,\n", cfg->headless_skip_present ? "true" : "false");
    fprintf(f, "    \"output_dir\": ");
    config_write_string(f, cfg->headless_output_dir);
    fprintf(f, "\n");
    fprintf(f, "  }\n");
    fprintf(f, "}\n");

    if (!config_candidate_valid(&save, f)) {
        physics_sim_persistence_abort(&save, f);
        return false;
    }
    return physics_sim_persistence_finish(&save, f);
}
