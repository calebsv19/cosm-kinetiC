#include "import/shape_import.h"

#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "app/physics_sim_job_json.h"

#include "ShapeLib/shape_flatten.h"
#include "ShapeLib/shape_json.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static void mask_set(uint8_t *mask, int w, int h, int x, int y) {
    if (!mask) return;
    if (x < 0 || x >= w || y < 0 || y >= h) return;
    mask[(size_t)y * (size_t)w + (size_t)x] = 1;
}

static bool shape_geometry_admitted(const Shape *shape, float tolerance);

static bool shape_keys_admitted(json_object *object, const char *const *known, size_t count) {
    if (!object || !json_object_is_type(object, json_type_object)) return false;
    json_object_object_foreach(object, key, value) {
        (void)value;
        for (size_t i = 0; i < count; ++i)
            if (strcasecmp(key, known[i]) == 0 && strcmp(key, known[i]) != 0) return false;
    }
    return true;
}

static json_object *shape_field(json_object *object, const char *name) {
    json_object *value = NULL;
    json_object_object_get_ex(object, name, &value);
    return value;
}

static bool shape_point_admitted(json_object *point) {
    if (!point || !json_object_is_type(point, json_type_array) || json_object_array_length(point) != 2) return false;
    for (size_t i = 0; i < 2; ++i) {
        json_object *number = json_object_array_get_idx(point, i);
        if (!number || !(json_object_is_type(number, json_type_int) || json_object_is_type(number, json_type_double))) return false;
        double coordinate = json_object_get_double(number);
        if (!isfinite(coordinate) || coordinate < -FLT_MAX || coordinate > FLT_MAX) return false;
    }
    return true;
}

static bool shape_document_admitted(json_object *root) {
    static const char *const root_keys[] = {"version", "shapes"};
    static const char *const shape_keys[] = {"name", "paths"};
    static const char *const path_keys[] = {"closed", "segments"};
    static const char *const segment_keys[] = {"type", "p0", "p1", "c1", "c2"};
    if (!shape_keys_admitted(root, root_keys, 2)) return false;
    json_object *version = NULL;
    if (json_object_object_get_ex(root, "version", &version) &&
        (!version || !json_object_is_type(version, json_type_int) || json_object_get_int64(version) != 1)) return false;
    json_object *shapes = shape_field(root, "shapes");
    if (!shapes || !json_object_is_type(shapes, json_type_array) || json_object_array_length(shapes) > 1024) return false;
    size_t remaining_paths = 10000, remaining_segments = 10000;
    for (size_t i = 0; i < json_object_array_length(shapes); ++i) {
        json_object *shape = json_object_array_get_idx(shapes, i), *name = NULL;
        if (!shape_keys_admitted(shape, shape_keys, 2)) return false;
        if (json_object_object_get_ex(shape, "name", &name) && (!name || !json_object_is_type(name, json_type_string))) return false;
        json_object *paths = shape_field(shape, "paths");
        if (!paths || !json_object_is_type(paths, json_type_array) || json_object_array_length(paths) > remaining_paths) return false;
        remaining_paths -= json_object_array_length(paths);
        for (size_t j = 0; j < json_object_array_length(paths); ++j) {
            json_object *path = json_object_array_get_idx(paths, j), *closed = NULL;
            if (!shape_keys_admitted(path, path_keys, 2)) return false;
            if (json_object_object_get_ex(path, "closed", &closed) && (!closed || !json_object_is_type(closed, json_type_boolean))) return false;
            json_object *segments = shape_field(path, "segments");
            if (!segments || !json_object_is_type(segments, json_type_array) || json_object_array_length(segments) > remaining_segments) return false;
            remaining_segments -= json_object_array_length(segments);
            for (size_t k = 0; k < json_object_array_length(segments); ++k) {
                json_object *segment = json_object_array_get_idx(segments, k);
                if (!shape_keys_admitted(segment, segment_keys, 5)) return false;
                json_object *type = shape_field(segment, "type");
                if (!type || !json_object_is_type(type, json_type_string)) return false;
                const char *kind = json_object_get_string(type);
                if (strcmp(kind, "line") != 0 && strcmp(kind, "cubic") != 0) return false;
                if (!shape_point_admitted(shape_field(segment, "p0")) || !shape_point_admitted(shape_field(segment, "p1"))) return false;
                for (size_t n = 3; n < 5; ++n) {
                    json_object *control = NULL;
                    bool present = json_object_object_get_ex(segment, segment_keys[n], &control);
                    if ((present || strcmp(kind, "cubic") == 0) && !shape_point_admitted(control)) return false;
                }
            }
        }
    }
    return true;
}

bool shape_import_load(const char *path, ShapeDocument *out_doc) {
    if (!path || !out_doc) return false;
    memset(out_doc, 0, sizeof(*out_doc));
    json_object *root = physics_sim_job_json_read(path);
    if (!root) return false;
    ShapeDocument candidate = {0};
    bool valid = shape_document_admitted(root) && ShapeDocument_LoadFromJsonText(
        json_object_to_json_string_ext(root, JSON_C_TO_STRING_PLAIN), &candidate);
    if (valid) {
        for (size_t i = 0; i < candidate.shapeCount; ++i)
            if (candidate.shapes[i].pathCount && !shape_geometry_admitted(&candidate.shapes[i], 0.01f)) { valid = false; break; }
    }
    json_object_put(root);
    if (!valid) { ShapeDocument_Free(&candidate); return false; }
    *out_doc = candidate;
    return true;
}

static ShapeVec2 rotate_about(ShapeVec2 p, float cx, float cy, float cos_a, float sin_a) {
    float dx = p.x - cx;
    float dy = p.y - cy;
    ShapeVec2 r;
    r.x = dx * cos_a - dy * sin_a + cx;
    r.y = dx * sin_a + dy * cos_a + cy;
    return r;
}

/* Local admission protects the shared flattener's integer sample conversion.
 * Bound estimated flatten storage as well as later raster work. */
static bool shape_geometry_admitted(const Shape *shape, float tolerance) {
    if (!shape || !shape->paths || shape->pathCount == 0 || shape->pathCount > 10000) return false;
    size_t remaining = 1048576;
    const double coordinate_limit = sqrt((double)FLT_MAX) / 32.0;
    for (size_t i = 0; i < shape->pathCount; ++i) {
        const ShapePath *path = &shape->paths[i];
        if ((path->segmentCount && !path->segments) || path->segmentCount > remaining) return false;
        if (remaining < 2) return false;
        remaining -= 2; /* initial and possible closing points */
        for (size_t j = 0; j < path->segmentCount; ++j) {
            const ShapeSegment *seg = &path->segments[j];
            bool cubic = seg->type == SHAPE_SEGMENT_CUBIC_BEZIER;
            if (!cubic && seg->type != SHAPE_SEGMENT_LINE) return false;
            size_t points = cubic ? 96 : 1;
            if (points > remaining) return false;
            remaining -= points;
            ShapeVec2 controls[] = {seg->p0, seg->c1, seg->c2, seg->p1};
            for (size_t k = 0; k < 4; ++k) {
                if (!cubic && k != 0 && k != 3) continue;
                if (!isfinite(controls[k].x) || !isfinite(controls[k].y) ||
                    fabs((double)controls[k].x) > coordinate_limit ||
                    fabs((double)controls[k].y) > coordinate_limit) return false;
            }
            if (cubic) {
                double length = 0.0;
                for (size_t k = 1; k < 4; ++k)
                    length += hypot((double)controls[k].x - controls[k-1].x,
                                    (double)controls[k].y - controls[k-1].y);
                double coordinate = 0.0;
                for (size_t k = 0; k < 4; ++k)
                    coordinate = fmax(coordinate, fmax(fabs((double)controls[k].x), fabs((double)controls[k].y)));
                /* Cover float interpolation error for curves far from origin. */
                double upper_length = 2.0 * length + 128.0 * FLT_EPSILON * coordinate;
                double spacing = fmax((double)tolerance, 0.01);
                if (!isfinite(upper_length) || upper_length / spacing > (double)INT_MAX / 16.0) return false;
            }
        }
    }
    return true;
}

bool shape_import_bounds(const Shape *shape, ShapeBounds *out_bounds) {
    if (!out_bounds || !shape_geometry_admitted(shape, 0.5f)) return false;
    PolylineSet set;
    if (!Shape_FlattenToPolylines(shape, 0.5f, &set)) {
        memset(out_bounds, 0, sizeof(*out_bounds));
        out_bounds->valid = false;
        return false;
    }
    float min_x = FLT_MAX, min_y = FLT_MAX;
    float max_x = -FLT_MAX, max_y = -FLT_MAX;
    bool has_points = false;
    for (size_t i = 0; i < set.count; ++i) {
        const Polyline *line = &set.lines[i];
        for (size_t j = 0; j < line->count; ++j) {
            ShapeVec2 p = line->points[j];
            if (!isfinite(p.x) || !isfinite(p.y)) {
                PolylineSet_Free(&set);
                return false;
            }
            if (p.x < min_x) min_x = p.x;
            if (p.x > max_x) max_x = p.x;
            if (p.y < min_y) min_y = p.y;
            if (p.y > max_y) max_y = p.y;
            has_points = true;
        }
    }
    ShapeBounds b = {0};
    if (has_points) {
        b.min_x = min_x;
        b.min_y = min_y;
        b.max_x = max_x;
        b.max_y = max_y;
        b.valid = true;
    }
    *out_bounds = b;
    PolylineSet_Free(&set);
    return has_points;
}

static void fit_transform(const ShapeBounds *b,
                          int grid_w,
                          int grid_h,
                          float margin,
                          float *out_scale,
                          float *out_offset_x,
                          float *out_offset_y,
                          float *out_center_x,
                          float *out_center_y) {
    float width = b->max_x - b->min_x;
    float height = b->max_y - b->min_y;
    float avail_w = (float)(grid_w - 1) - margin * 2.0f;
    float avail_h = (float)(grid_h - 1) - margin * 2.0f;
    if (avail_w <= 0.0f) avail_w = (float)(grid_w - 1);
    if (avail_h <= 0.0f) avail_h = (float)(grid_h - 1);

    float scale_x = width > 1e-5f ? (avail_w / width) : avail_w;
    float scale_y = height > 1e-5f ? (avail_h / height) : avail_h;
    float scale = fminf(scale_x, scale_y);
    if (scale <= 0.0f) scale = 1.0f;

    float center_x = 0.5f * (b->min_x + b->max_x);
    float center_y = 0.5f * (b->min_y + b->max_y);
    float offset_x = 0.5f * (float)(grid_w - 1) - center_x * scale;
    float offset_y = 0.5f * (float)(grid_h - 1) - center_y * scale;

    if (out_scale) *out_scale = scale;
    if (out_offset_x) *out_offset_x = offset_x;
    if (out_offset_y) *out_offset_y = offset_y;
    if (out_center_x) *out_center_x = center_x;
    if (out_center_y) *out_center_y = center_y;
}

static bool world_to_grid(float px, float py,
                          float scale, float ox, float oy,
                          int *out_x, int *out_y) {
    float gx = px * scale + ox;
    float gy = py * scale + oy;
    /* Leave headroom for interpolation, subtraction and stroke expansion. */
    const float limit = (float)(INT_MAX / 8);
    if (!isfinite(gx) || !isfinite(gy) || fabsf(gx) > limit || fabsf(gy) > limit) return false;
    if (out_x) *out_x = (int)lroundf(gx);
    if (out_y) *out_y = (int)lroundf(gy);
    return true;
}

static bool charge_work(size_t *remaining, size_t a, size_t b, size_t c) {
    if (!a || !b || !c) return true;
    if (a > *remaining / b) return false;
    size_t ab = a * b;
    if (ab > *remaining / c) return false;
    *remaining -= ab * c;
    return true;
}

static bool admit_segment(ShapeVec2 a, ShapeVec2 b,
                          float center_x, float center_y, float cos_a, float sin_a,
                          float scale, float ox, float oy, int half, size_t *remaining) {
    a = rotate_about(a, center_x, center_y, cos_a, sin_a);
    b = rotate_about(b, center_x, center_y, cos_a, sin_a);
    int ax, ay, bx, by;
    if (!world_to_grid(a.x, a.y, scale, ox, oy, &ax, &ay) ||
        !world_to_grid(b.x, b.y, scale, ox, oy, &bx, &by)) return false;
    float steps = ceilf(fmaxf(fabsf((float)(bx - ax)), fabsf((float)(by - ay))));
    if (steps < 1.0f) steps = 1.0f;
    size_t side = (size_t)half * 2 + 1;
    return charge_work(remaining, (size_t)steps + 1, side, side);
}

static void rasterize_segment(uint8_t *mask,
                              int w, int h,
                              ShapeVec2 a,
                              ShapeVec2 b,
                              float center_x, float center_y,
                              float cos_a, float sin_a,
                              float scale, float ox, float oy,
                              float stroke) {
    ShapeVec2 ra = rotate_about(a, center_x, center_y, cos_a, sin_a);
    ShapeVec2 rb = rotate_about(b, center_x, center_y, cos_a, sin_a);
    int ax, ay, bx, by;
    world_to_grid(ra.x, ra.y, scale, ox, oy, &ax, &ay);
    world_to_grid(rb.x, rb.y, scale, ox, oy, &bx, &by);
    int dx = bx - ax;
    int dy = by - ay;
    int steps = (int)ceilf(fmaxf(fabsf((float)dx), fabsf((float)dy)));
    if (steps < 1) steps = 1;
    for (int i = 0; i <= steps; ++i) {
        float t = (float)i / (float)steps;
        float gx = (1.0f - t) * (float)ax + t * (float)bx;
        float gy = (1.0f - t) * (float)ay + t * (float)by;
        int cx = (int)lroundf(gx);
        int cy = (int)lroundf(gy);
        int half = (int)ceilf(fmaxf(stroke * 0.5f, 0.5f));
        for (int yy = cy - half; yy <= cy + half; ++yy) {
            for (int xx = cx - half; xx <= cx + half; ++xx) {
                mask_set(mask, w, h, xx, yy);
            }
        }
    }
}

static bool point_in_polygon(const ShapeVec2 *pts, size_t count, float x, float y) {
    bool inside = false;
    if (!pts || count < 3) return false;
    for (size_t i = 0, j = count - 1; i < count; j = i++) {
        float xi = pts[i].x, yi = pts[i].y;
        float xj = pts[j].x, yj = pts[j].y;
        bool intersect = ((yi > y) != (yj > y)) &&
                         (x < (xj - xi) * (y - yi) / ((yj - yi) + 1e-6f) + xi);
        if (intersect) inside = !inside;
    }
    return inside;
}

static void fill_polygon(uint8_t *mask,
                         int w, int h,
                         const Polyline *line,
                         float center_x, float center_y,
                         float cos_a, float sin_a,
                         float scale, float ox, float oy) {
    if (!line || !line->closed || line->count < 3) return;
    float inv_scale = (scale > 1e-6f) ? (1.0f / scale) : 1.0f;
    const ShapeVec2 *pts = line->points;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            float wx = ((float)x - ox) * inv_scale;
            float wy = ((float)y - oy) * inv_scale;
            float dx = wx - center_x;
            float dy = wy - center_y;
            float lx = dx * cos_a + dy * sin_a + center_x;
            float ly = -dx * sin_a + dy * cos_a + center_y;
            if (point_in_polygon(pts, line->count, lx, ly)) {
                mask_set(mask, w, h, x, y);
            }
        }
    }
}

bool shape_import_rasterize(const Shape *shape,
                            int grid_w,
                            int grid_h,
                            const ShapeRasterOptions *opts,
                            uint8_t *mask_out) {
    if (!shape || grid_w <= 0 || grid_h <= 0 || !mask_out) return false;
    const size_t max_cells = 64u * 1024u * 1024u;
    if ((size_t)grid_w > max_cells / (size_t)grid_h) return false;
    if (opts && (!isfinite(opts->max_error) || !isfinite(opts->margin_cells) ||
                 !isfinite(opts->stroke) || !isfinite(opts->position_x_norm) ||
                 !isfinite(opts->position_y_norm) || !isfinite(opts->rotation_deg) ||
                 !isfinite(opts->scale))) return false;

    float max_error = 0.5f;
    float margin = 2.0f;
    float stroke = 1.0f;
    float pos_x = 0.5f;
    float pos_y = 0.5f;
    float rotation_deg = 0.0f;
    float user_scale = 1.0f;
    bool center_fit = true;
    if (opts) {
        if (opts->max_error > 0.0f) max_error = opts->max_error;
        if (opts->margin_cells >= 0.0f) margin = opts->margin_cells;
        if (opts->stroke > 0.0f) stroke = opts->stroke;
        if (opts->position_x_norm >= 0.0f && opts->position_x_norm <= 1.0f) pos_x = opts->position_x_norm;
        if (opts->position_y_norm >= 0.0f && opts->position_y_norm <= 1.0f) pos_y = opts->position_y_norm;
        rotation_deg = opts->rotation_deg;
        if (opts->scale > 0.0f) user_scale = opts->scale;
        center_fit = opts->center_fit;
    }

    if (!shape_geometry_admitted(shape, fminf(max_error, 0.5f))) return false;

    PolylineSet set;
    if (!Shape_FlattenToPolylines(shape, max_error, &set)) {
        return false;
    }

    ShapeBounds bounds = {0};
    if (!shape_import_bounds(shape, &bounds) || !bounds.valid) {
        PolylineSet_Free(&set);
        return false;
    }

    float scale = 1.0f, ox = 0.0f, oy = 0.0f;
    float center_x = 0.5f * (bounds.min_x + bounds.max_x);
    float center_y = 0.5f * (bounds.min_y + bounds.max_y);

    if (center_fit) {
        fit_transform(&bounds, grid_w, grid_h, margin, &scale, &ox, &oy, &center_x, &center_y);
    } else {
        scale = user_scale;
        float target_x = pos_x * (float)(grid_w - 1);
        float target_y = pos_y * (float)(grid_h - 1);
        ox = target_x - center_x * scale;
        oy = target_y - center_y * scale;
    }

    float radians = rotation_deg * (float)M_PI / 180.0f;
    float cos_a = cosf(radians);
    float sin_a = sinf(radians);

    float half_float = ceilf(fmaxf(stroke * 0.5f, 0.5f));
    if (!isfinite(scale) || scale <= 0.0f || !isfinite(ox) || !isfinite(oy) ||
        !isfinite(center_x) || !isfinite(center_y) || !isfinite(cos_a) || !isfinite(sin_a) ||
        !isfinite(half_float) || half_float > (float)(INT_MAX / 8)) {
        PolylineSet_Free(&set);
        return false;
    }
    int half = (int)half_float;
    size_t remaining = 64u * 1024u * 1024u;
    /* Admit the whole draw before changing the caller's mask. This bounds the
     * actual segment square visits plus polygon edge tests, including off-grid work. */
    for (size_t i = 0; i < set.count; ++i) {
        const Polyline *line = &set.lines[i];
        if (line->count < 2) continue;
        for (size_t j = 1; j < line->count; ++j) {
            if (!admit_segment(line->points[j-1], line->points[j], center_x, center_y,
                               cos_a, sin_a, scale, ox, oy, half, &remaining)) goto refused;
        }
        if (line->closed) {
            if (!admit_segment(line->points[line->count-1], line->points[0], center_x, center_y,
                               cos_a, sin_a, scale, ox, oy, half, &remaining)) goto refused;
            if (line->count >= 3 && !charge_work(&remaining, (size_t)grid_w,
                                               (size_t)grid_h, line->count)) goto refused;
        }
    }
    memset(mask_out, 0, (size_t)grid_w * (size_t)grid_h);

    for (size_t i = 0; i < set.count; ++i) {
        Polyline *line = &set.lines[i];
        if (!line || line->count < 2) continue;
        for (size_t j = 1; j < line->count; ++j) {
            ShapeVec2 a = line->points[j - 1];
            ShapeVec2 b = line->points[j];
            rasterize_segment(mask_out, grid_w, grid_h, a, b,
                              center_x, center_y, cos_a, sin_a,
                              scale, ox, oy, stroke);
        }
        if (line->closed) {
            ShapeVec2 a = line->points[line->count - 1];
            ShapeVec2 b = line->points[0];
            rasterize_segment(mask_out, grid_w, grid_h, a, b,
                              center_x, center_y, cos_a, sin_a,
                              scale, ox, oy, stroke);
            fill_polygon(mask_out, grid_w, grid_h, line,
                         center_x, center_y, cos_a, sin_a,
                         scale, ox, oy);
        }
    }

    PolylineSet_Free(&set);
    return true;

refused:
    PolylineSet_Free(&set);
    return false;
}
