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

static void apply_json_overrides(const char *json, AppConfig *cfg) {
    apply_window_settings(json, cfg);
    apply_grid_settings(json, cfg);
    apply_timing_settings(json, cfg);
    apply_command_settings(json, cfg);
    apply_fluid_settings(json, cfg);
    apply_input_settings(json, cfg);
    apply_emitter_settings(json, cfg);
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
