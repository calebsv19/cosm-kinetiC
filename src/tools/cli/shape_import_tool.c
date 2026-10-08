#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app/physics_sim_persistence.h"
#include "ShapeLib/shape_core.h"
#include "ShapeLib/shape_json.h"
#include "import/shape_import.h"

typedef struct {
    const char *input_path;
    const char *output_path;
    int grid_w;
    int grid_h;
    float margin;
    float stroke;
    float max_error;
    float pos_x;
    float pos_y;
    float rotation_deg;
    float scale;
    bool center_fit;
} Args;

static void usage(const char *exe) {
    fprintf(stderr,
            "Usage: %s [--grid W H (positive, <=67108864 cells)] [--margin cells] [--stroke cells] [--max-error e] [--pos x y] [--rot deg] [--scale s] [--no-fit] [--out file.pgm] <shape.json>\n",
            exe);
}

static int parse_int(const char *s, int *out) {
    if (!s || !out) return 0;
    char *end = NULL;
    errno = 0;
    long v = strtol(s, &end, 10);
    if (errno || end == s || *end != '\0' || v < INT_MIN || v > INT_MAX) return 0;
    *out = (int)v;
    return 1;
}

static int parse_float(const char *s, float *out) {
    if (!s || !out) return 0;
    char *end = NULL;
    errno = 0;
    float v = strtof(s, &end);
    if (errno || end == s || *end != '\0' || !isfinite(v)) return 0;
    *out = v;
    return 1;
}

static int parse_args(int argc, char **argv, Args *out) {
    Args a = {
        .grid_w = 256,
        .grid_h = 256,
        .margin = 2.0f,
        .stroke = 1.0f,
        .max_error = 0.5f,
        .pos_x = 0.5f,
        .pos_y = 0.5f,
        .rotation_deg = 0.0f,
        .scale = 1.0f,
        .center_fit = true,
    };
    for (int i = 1; i < argc; ++i) {
        const char *arg = argv[i];
        if (strcmp(arg, "--grid") == 0) {
            if (i + 2 >= argc || !parse_int(argv[i + 1], &a.grid_w) || !parse_int(argv[i + 2], &a.grid_h)) {
                fprintf(stderr, "Invalid --grid values\n");
                return 0;
            }
            i += 2;
        } else if (strcmp(arg, "--margin") == 0) {
            if (i + 1 >= argc || !parse_float(argv[i + 1], &a.margin)) {
                fprintf(stderr, "Invalid --margin value\n");
                return 0;
            }
            ++i;
        } else if (strcmp(arg, "--stroke") == 0) {
            if (i + 1 >= argc || !parse_float(argv[i + 1], &a.stroke)) {
                fprintf(stderr, "Invalid --stroke value\n");
                return 0;
            }
            ++i;
        } else if (strcmp(arg, "--max-error") == 0) {
            if (i + 1 >= argc || !parse_float(argv[i + 1], &a.max_error)) {
                fprintf(stderr, "Invalid --max-error value\n");
                return 0;
            }
            ++i;
        } else if (strcmp(arg, "--pos") == 0) {
            if (i + 2 >= argc || !parse_float(argv[i + 1], &a.pos_x) || !parse_float(argv[i + 2], &a.pos_y)) {
                fprintf(stderr, "Invalid --pos values\n");
                return 0;
            }
            i += 2;
        } else if (strcmp(arg, "--rot") == 0) {
            if (i + 1 >= argc || !parse_float(argv[i + 1], &a.rotation_deg)) {
                fprintf(stderr, "Invalid --rot value\n");
                return 0;
            }
            ++i;
        } else if (strcmp(arg, "--scale") == 0) {
            if (i + 1 >= argc || !parse_float(argv[i + 1], &a.scale)) {
                fprintf(stderr, "Invalid --scale value\n");
                return 0;
            }
            ++i;
        } else if (strcmp(arg, "--no-fit") == 0) {
            a.center_fit = false;
        } else if (strcmp(arg, "--out") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Missing --out path\n");
                return 0;
            }
            a.output_path = argv[++i];
        } else if (arg[0] == '-') {
            fprintf(stderr, "Unknown option: %s\n", arg);
            return 0;
        } else {
            a.input_path = arg;
        }
    }
    if (!a.input_path) {
        return 0;
    }
    /* Bound the one-byte-per-cell CLI mask before input I/O or allocation. */
    const size_t max_mask_cells = 64u * 1024u * 1024u;
    if (a.grid_w <= 0 || a.grid_h <= 0 ||
        (size_t)a.grid_w > max_mask_cells / (size_t)a.grid_h) {
        fprintf(stderr, "Invalid --grid: positive dimensions and at most 67108864 cells required\n");
        return 0;
    }
    if (a.margin < 0.0f || a.stroke <= 0.0f || a.max_error <= 0.0f ||
        a.scale <= 0.0f || a.pos_x < 0.0f || a.pos_x > 1.0f ||
        a.pos_y < 0.0f || a.pos_y > 1.0f) {
        fprintf(stderr, "Invalid shape options: margin >= 0; stroke, max-error, scale > 0; pos in [0,1] required\n");
        return 0;
    }
    *out = a;
    return 1;
}

static void print_shape_summary(const ShapeDocument *doc) {
    if (!doc) return;
    printf("Shapes: %zu\n", doc->shapeCount);
    for (size_t si = 0; si < doc->shapeCount; ++si) {
        const Shape *s = &doc->shapes[si];
        printf("  [%zu] name=\"%s\" paths=%zu\n", si, s->name ? s->name : "(unnamed)", s->pathCount);
    }
}

static bool write_pgm(const char *path, const uint8_t *mask, int w, int h) {
    if (!path || !mask || w <= 0 || h <= 0 ||
        (size_t)w > (64u * 1024u * 1024u) / (size_t)h) return false;
    PhysicsSimPersistence save;
    FILE *stream = physics_sim_persistence_begin_bounded(path,
        UINT64_C(64) * 1024 * 1024 + 64, &save);
    if (!stream) return false;
    size_t cells = (size_t)w * (size_t)h;
    if (fprintf(stream, "P5\n%d %d\n255\n", w, h) < 0 ||
        fwrite(mask, 1, cells, stream) != cells) {
        physics_sim_persistence_abort(&save, stream);
        return false;
    }
    return physics_sim_persistence_finish(&save, stream);
}

int main(int argc, char **argv) {
    Args args;
    if (!parse_args(argc, argv, &args)) {
        usage(argv[0]);
        return 1;
    }

    ShapeDocument doc;
    if (!shape_import_load(args.input_path, &doc)) {
        fprintf(stderr, "Failed to load shape JSON: %s\n", args.input_path);
        return 1;
    }

    print_shape_summary(&doc);

    if (doc.shapeCount == 0) {
        ShapeDocument_Free(&doc);
        fprintf(stderr, "No shapes found.\n");
        return 1;
    }

    const Shape *shape = &doc.shapes[0];
    uint8_t *mask = (uint8_t *)calloc((size_t)args.grid_w * (size_t)args.grid_h, sizeof(uint8_t));
    if (!mask) {
        fprintf(stderr, "Out of memory for mask\n");
        ShapeDocument_Free(&doc);
        return 1;
    }

    ShapeRasterOptions ropts = {
        .margin_cells = args.margin,
        .stroke = args.stroke,
        .max_error = args.max_error,
        .position_x_norm = args.pos_x,
        .position_y_norm = args.pos_y,
        .rotation_deg = args.rotation_deg,
        .scale = args.scale,
        .center_fit = args.center_fit,
    };
    if (!shape_import_rasterize(shape, args.grid_w, args.grid_h, &ropts, mask)) {
        fprintf(stderr, "Rasterization failed\n");
        free(mask);
        ShapeDocument_Free(&doc);
        return 1;
    }

    printf("Rasterized to %dx%d mask.\n", args.grid_w, args.grid_h);

    int result = 0;
    if (args.output_path) {
        struct stat input, output;
        bool overlaps_input = stat(args.input_path, &input) == 0 &&
            lstat(args.output_path, &output) == 0 &&
            input.st_dev == output.st_dev && input.st_ino == output.st_ino;
        if (overlaps_input) {
            fprintf(stderr, "Failed to write %s: output aliases shape input\n", args.output_path);
            result = 1;
        } else if (write_pgm(args.output_path, mask, args.grid_w, args.grid_h)) {
            printf("Wrote %s\n", args.output_path);
        } else {
            fprintf(stderr, "Failed to write %s\n", args.output_path);
            result = 1;
        }
    }

    free(mask);
    ShapeDocument_Free(&doc);
    return result;
}
