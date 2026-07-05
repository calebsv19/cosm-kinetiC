#include "export/wind_projection_frames.h"

#include "export/render_frames.h"

#include <errno.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifdef _WIN32
#include <direct.h>
#endif

typedef struct __attribute__((__packed__)) WindProjectionBmpFileHeader {
    uint16_t bfType;
    uint32_t bfSize;
    uint16_t bfReserved1;
    uint16_t bfReserved2;
    uint32_t bfOffBits;
} WindProjectionBmpFileHeader;

typedef struct __attribute__((__packed__)) WindProjectionBmpInfoHeader {
    uint32_t biSize;
    int32_t biWidth;
    int32_t biHeight;
    uint16_t biPlanes;
    uint16_t biBitCount;
    uint32_t biCompression;
    uint32_t biSizeImage;
    int32_t biXPelsPerMeter;
    int32_t biYPelsPerMeter;
    uint32_t biClrUsed;
    uint32_t biClrImportant;
} WindProjectionBmpInfoHeader;

static bool ensure_dir(const char *path) {
    int result = 0;
    if (!path || !path[0]) return false;
#ifdef _WIN32
    result = _mkdir(path);
#else
    result = mkdir(path, 0755);
#endif
    return result == 0 || errno == EEXIST;
}

static float abs_float(float v) {
    return v < 0.0f ? -v : v;
}

static uint8_t tone_log(float value, float max_value) {
    float normalized = 0.0f;
    if (!(max_value > 0.0f) || !(value > 0.0f)) return 0u;
    normalized = log1pf(8.0f * value / max_value) / log1pf(8.0f);
    if (normalized < 0.0f) normalized = 0.0f;
    if (normalized > 1.0f) normalized = 1.0f;
    return (uint8_t)(normalized * 255.0f + 0.5f);
}

static bool write_bmp32(const char *path,
                        const uint8_t *rgba_pixels,
                        int width,
                        int height) {
    FILE *f = NULL;
    WindProjectionBmpFileHeader file_header = {0};
    WindProjectionBmpInfoHeader info_header = {0};
    const uint32_t row_bytes = (uint32_t)(width * 4);
    const uint32_t pixel_data_size = row_bytes * (uint32_t)height;
    uint8_t *row = NULL;

    if (!path || !path[0] || !rgba_pixels || width <= 0 || height <= 0) return false;

    file_header.bfType = 0x4D42u;
    file_header.bfSize = (uint32_t)(sizeof(file_header) + sizeof(info_header)) + pixel_data_size;
    file_header.bfOffBits = (uint32_t)(sizeof(file_header) + sizeof(info_header));

    info_header.biSize = (uint32_t)sizeof(info_header);
    info_header.biWidth = width;
    info_header.biHeight = -height;
    info_header.biPlanes = 1u;
    info_header.biBitCount = 32u;
    info_header.biCompression = 0u;
    info_header.biSizeImage = pixel_data_size;

    f = fopen(path, "wb");
    if (!f) {
        fprintf(stderr, "[wind_projection] failed to open %s (%s)\n", path, strerror(errno));
        return false;
    }
    if (fwrite(&file_header, sizeof(file_header), 1u, f) != 1u ||
        fwrite(&info_header, sizeof(info_header), 1u, f) != 1u) {
        fclose(f);
        return false;
    }

    row = (uint8_t *)malloc((size_t)row_bytes);
    if (!row) {
        fclose(f);
        return false;
    }

    for (int y = 0; y < height; ++y) {
        const uint8_t *src = rgba_pixels + (size_t)y * (size_t)row_bytes;
        for (int x = 0; x < width; ++x) {
            const uint8_t *px = src + (size_t)x * 4u;
            uint8_t *dst = row + (size_t)x * 4u;
            dst[0] = px[2];
            dst[1] = px[1];
            dst[2] = px[0];
            dst[3] = px[3];
        }
        if (fwrite(row, 1u, (size_t)row_bytes, f) != (size_t)row_bytes) {
            free(row);
            fclose(f);
            return false;
        }
    }

    free(row);
    fclose(f);
    return true;
}

static bool build_wind_projection_pixels(const SceneState *scene,
                                         uint8_t **out_pixels,
                                         int *out_width,
                                         int *out_height) {
    SceneFluidVolumeExportView3D volume = {0};
    size_t pixel_count = 0u;
    float *max_density = NULL;
    float *max_speed = NULL;
    float *max_pressure = NULL;
    uint8_t *pixels = NULL;
    float density_peak = 0.0f;
    float speed_peak = 0.0f;
    float pressure_peak = 0.0f;
    bool ok = false;

    if (out_pixels) *out_pixels = NULL;
    if (out_width) *out_width = 0;
    if (out_height) *out_height = 0;
    if (!out_pixels || !out_width || !out_height) return false;
    if (!scene || !scene_backend_volume_export_view_3d(scene, &volume)) return false;
    if (volume.width <= 0 || volume.height <= 0 || volume.depth <= 0 || volume.cell_count == 0u) return false;
    if (!volume.density || !volume.velocity_x || !volume.velocity_y ||
        !volume.velocity_z || !volume.pressure) {
        return false;
    }

    pixel_count = (size_t)volume.width * (size_t)volume.height;
    max_density = (float *)calloc(pixel_count, sizeof(float));
    max_speed = (float *)calloc(pixel_count, sizeof(float));
    max_pressure = (float *)calloc(pixel_count, sizeof(float));
    pixels = (uint8_t *)calloc(pixel_count * 4u, sizeof(uint8_t));
    if (!max_density || !max_speed || !max_pressure || !pixels) goto cleanup;

    for (int z = 0; z < volume.depth; ++z) {
        const size_t z_base = (size_t)z * (size_t)volume.width * (size_t)volume.height;
        for (int y = 0; y < volume.height; ++y) {
            const size_t row_base = z_base + (size_t)y * (size_t)volume.width;
            const size_t pixel_row = (size_t)y * (size_t)volume.width;
            for (int x = 0; x < volume.width; ++x) {
                const size_t idx = row_base + (size_t)x;
                const size_t p = pixel_row + (size_t)x;
                const float density = abs_float(volume.density[idx]);
                const float vx = volume.velocity_x[idx];
                const float vy = volume.velocity_y[idx];
                const float vz = volume.velocity_z[idx];
                const float speed = sqrtf(vx * vx + vy * vy + vz * vz);
                const float pressure = abs_float(volume.pressure[idx]);
                if (density > max_density[p]) max_density[p] = density;
                if (speed > max_speed[p]) max_speed[p] = speed;
                if (pressure > max_pressure[p]) max_pressure[p] = pressure;
                if (density > density_peak) density_peak = density;
                if (speed > speed_peak) speed_peak = speed;
                if (pressure > pressure_peak) pressure_peak = pressure;
            }
        }
    }

    for (size_t i = 0u; i < pixel_count; ++i) {
        uint8_t *px = pixels + i * 4u;
        px[0] = tone_log(max_speed[i], speed_peak);
        px[1] = tone_log(max_density[i], density_peak);
        px[2] = tone_log(max_pressure[i], pressure_peak);
        px[3] = 255u;
    }

    *out_pixels = pixels;
    *out_width = volume.width;
    *out_height = volume.height;
    pixels = NULL;
    ok = true;

cleanup:
    free(max_density);
    free(max_speed);
    free(max_pressure);
    free(pixels);
    return ok;
}

static void write_rgba(uint8_t *pixels,
                       int width,
                       int height,
                       int x,
                       int y,
                       uint8_t r,
                       uint8_t g,
                       uint8_t b,
                       uint8_t a) {
    if (!pixels || x < 0 || y < 0 || x >= width || y >= height) return;
    uint8_t *px = pixels + ((size_t)y * (size_t)width + (size_t)x) * 4u;
    px[0] = r;
    px[1] = g;
    px[2] = b;
    px[3] = a;
}

static void blend_rgba(uint8_t *pixels,
                       int width,
                       int height,
                       int x,
                       int y,
                       uint8_t r,
                       uint8_t g,
                       uint8_t b,
                       uint8_t a) {
    if (!pixels || x < 0 || y < 0 || x >= width || y >= height) return;
    uint8_t *px = pixels + ((size_t)y * (size_t)width + (size_t)x) * 4u;
    const unsigned int inv_a = 255u - (unsigned int)a;
    px[0] = (uint8_t)(((unsigned int)r * (unsigned int)a + (unsigned int)px[0] * inv_a) / 255u);
    px[1] = (uint8_t)(((unsigned int)g * (unsigned int)a + (unsigned int)px[1] * inv_a) / 255u);
    px[2] = (uint8_t)(((unsigned int)b * (unsigned int)a + (unsigned int)px[2] * inv_a) / 255u);
    px[3] = 255u;
}

static void draw_rect(uint8_t *pixels,
                      int width,
                      int height,
                      int x,
                      int y,
                      int w,
                      int h,
                      uint8_t r,
                      uint8_t g,
                      uint8_t b,
                      uint8_t a) {
    for (int yy = 0; yy < h; ++yy) {
        for (int xx = 0; xx < w; ++xx) {
            blend_rgba(pixels, width, height, x + xx, y + yy, r, g, b, a);
        }
    }
}

static void draw_line(uint8_t *pixels,
                      int width,
                      int height,
                      int x0,
                      int y0,
                      int x1,
                      int y1,
                      uint8_t r,
                      uint8_t g,
                      uint8_t b,
                      uint8_t a) {
    int dx = abs(x1 - x0);
    int sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0);
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        blend_rgba(pixels, width, height, x0, y0, r, g, b, a);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

static int clamp_int(int value, int min_value, int max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static int wind_render_world_to_cell(double world_value,
                                     float origin,
                                     float voxel_size,
                                     int extent) {
    int cell = 0;
    if (extent <= 1 || !(voxel_size > 0.0f)) return 0;
    cell = (int)lround((world_value - (double)origin) / (double)voxel_size);
    return clamp_int(cell, 0, extent - 1);
}

static void wind_render_project_cell(int x,
                                     int y,
                                     int z,
                                     int volume_height,
                                     int volume_depth,
                                     int scale_x,
                                     int scale_y,
                                     int depth_x,
                                     int depth_y,
                                     int margin,
                                     int *out_x,
                                     int *out_y) {
    if (out_x) *out_x = margin + x * scale_x + z * depth_x;
    if (out_y) {
        *out_y = margin + (volume_depth - 1 - z) * depth_y +
                 (volume_height - 1 - y) * scale_y;
    }
}

static void draw_wind_authored_object_box(uint8_t *pixels,
                                          int canvas_w,
                                          int canvas_h,
                                          const SceneFluidVolumeExportView3D *volume,
                                          int scale_x,
                                          int scale_y,
                                          int depth_x,
                                          int depth_y,
                                          int margin,
                                          double center_x,
                                          double center_y,
                                          double center_z,
                                          double size_x,
                                          double size_y,
                                          double size_z) {
    const double half_x = size_x > 0.0 ? size_x * 0.5 : (double)volume->voxel_size;
    const double half_y = size_y > 0.0 ? size_y * 0.5 : (double)volume->voxel_size;
    const double half_z = size_z > 0.0 ? size_z * 0.5 : (double)volume->voxel_size;
    const int min_x = wind_render_world_to_cell(center_x - half_x, volume->origin_x,
                                                volume->voxel_size, volume->width);
    const int max_x = wind_render_world_to_cell(center_x + half_x, volume->origin_x,
                                                volume->voxel_size, volume->width);
    const int min_y = wind_render_world_to_cell(center_y - half_y, volume->origin_y,
                                                volume->voxel_size, volume->height);
    const int max_y = wind_render_world_to_cell(center_y + half_y, volume->origin_y,
                                                volume->voxel_size, volume->height);
    const int min_z = wind_render_world_to_cell(center_z - half_z, volume->origin_z,
                                                volume->voxel_size, volume->depth);
    const int max_z = wind_render_world_to_cell(center_z + half_z, volume->origin_z,
                                                volume->voxel_size, volume->depth);
    int sx0 = 0;
    int sy0 = 0;
    int sx1 = 0;
    int sy1 = 0;
    int sx2 = 0;
    int sy2 = 0;
    int sx3 = 0;
    int sy3 = 0;
    int scx = 0;
    int scy = 0;
    int corner_x[8] = {0};
    int corner_y[8] = {0};

    if (!pixels || !volume) return;
    wind_render_project_cell(min_x, max_y, max_z, volume->height, volume->depth,
                             scale_x, scale_y, depth_x, depth_y, margin, &sx0, &sy0);
    wind_render_project_cell(max_x, max_y, max_z, volume->height, volume->depth,
                             scale_x, scale_y, depth_x, depth_y, margin, &sx1, &sy1);
    wind_render_project_cell(min_x, min_y, min_z, volume->height, volume->depth,
                             scale_x, scale_y, depth_x, depth_y, margin, &sx2, &sy2);
    wind_render_project_cell(max_x, min_y, min_z, volume->height, volume->depth,
                             scale_x, scale_y, depth_x, depth_y, margin, &sx3, &sy3);

    draw_rect(pixels, canvas_w, canvas_h, sx0 - 2, sy0 - 2,
              (sx3 - sx0) + 5, (sy3 - sy0) + 5, 255u, 212u, 76u, 210u);
    draw_rect(pixels, canvas_w, canvas_h, sx0 - 1, sy0 - 1,
              (sx3 - sx0) + 3, (sy3 - sy0) + 3, 25u, 31u, 38u, 230u);
    draw_line(pixels, canvas_w, canvas_h, sx0 - 2, sy0 - 2, sx1 + 2, sy1 - 2,
              255u, 222u, 96u, 240u);
    draw_line(pixels, canvas_w, canvas_h, sx1 + 2, sy1 - 2, sx3 + 2, sy3 + 2,
              255u, 222u, 96u, 240u);
    draw_line(pixels, canvas_w, canvas_h, sx3 + 2, sy3 + 2, sx2 - 2, sy2 + 2,
              255u, 222u, 96u, 240u);
    draw_line(pixels, canvas_w, canvas_h, sx2 - 2, sy2 + 2, sx0 - 2, sy0 - 2,
              255u, 222u, 96u, 240u);
    wind_render_project_cell(min_x, min_y, min_z, volume->height, volume->depth,
                             scale_x, scale_y, depth_x, depth_y, margin, &corner_x[0], &corner_y[0]);
    wind_render_project_cell(max_x, min_y, min_z, volume->height, volume->depth,
                             scale_x, scale_y, depth_x, depth_y, margin, &corner_x[1], &corner_y[1]);
    wind_render_project_cell(max_x, max_y, min_z, volume->height, volume->depth,
                             scale_x, scale_y, depth_x, depth_y, margin, &corner_x[2], &corner_y[2]);
    wind_render_project_cell(min_x, max_y, min_z, volume->height, volume->depth,
                             scale_x, scale_y, depth_x, depth_y, margin, &corner_x[3], &corner_y[3]);
    wind_render_project_cell(min_x, min_y, max_z, volume->height, volume->depth,
                             scale_x, scale_y, depth_x, depth_y, margin, &corner_x[4], &corner_y[4]);
    wind_render_project_cell(max_x, min_y, max_z, volume->height, volume->depth,
                             scale_x, scale_y, depth_x, depth_y, margin, &corner_x[5], &corner_y[5]);
    wind_render_project_cell(max_x, max_y, max_z, volume->height, volume->depth,
                             scale_x, scale_y, depth_x, depth_y, margin, &corner_x[6], &corner_y[6]);
    wind_render_project_cell(min_x, max_y, max_z, volume->height, volume->depth,
                             scale_x, scale_y, depth_x, depth_y, margin, &corner_x[7], &corner_y[7]);
    for (int edge = 0; edge < 4; ++edge) {
        const int next = (edge + 1) % 4;
        draw_line(pixels, canvas_w, canvas_h,
                  corner_x[edge], corner_y[edge], corner_x[next], corner_y[next],
                  255u, 236u, 80u, 255u);
        draw_line(pixels, canvas_w, canvas_h,
                  corner_x[edge + 4], corner_y[edge + 4], corner_x[next + 4], corner_y[next + 4],
                  255u, 236u, 80u, 255u);
        draw_line(pixels, canvas_w, canvas_h,
                  corner_x[edge], corner_y[edge], corner_x[edge + 4], corner_y[edge + 4],
                  255u, 236u, 80u, 255u);
    }
    wind_render_project_cell((min_x + max_x) / 2,
                             (min_y + max_y) / 2,
                             (min_z + max_z) / 2,
                             volume->height, volume->depth,
                             scale_x, scale_y, depth_x, depth_y, margin, &scx, &scy);
    draw_rect(pixels, canvas_w, canvas_h, scx - 4, scy - 4, 8, 8,
              18u, 20u, 24u, 235u);
    draw_line(pixels, canvas_w, canvas_h, scx - 9, scy, scx + 9, scy,
              255u, 255u, 255u, 255u);
    draw_line(pixels, canvas_w, canvas_h, scx, scy - 9, scx, scy + 9,
              255u, 255u, 255u, 255u);
}

static bool draw_wind_authored_object_overlays(uint8_t *pixels,
                                               int canvas_w,
                                               int canvas_h,
                                               const SceneState *scene,
                                               const SceneFluidVolumeExportView3D *volume,
                                               int scale_x,
                                               int scale_y,
                                               int depth_x,
                                               int depth_y,
                                               int margin) {
    const PhysicsSimRetainedRuntimeScene *retained = NULL;
    int count = 0;
    if (!pixels || !scene || !volume || !scene->runtime_visual.valid) return false;
    retained = &scene->runtime_visual.retained_scene;
    count = retained->retained_object_count;
    if (count <= 0) return false;
    if (count > PHYSICS_SIM_RUNTIME_SCENE_MAX_OBJECTS) count = PHYSICS_SIM_RUNTIME_SCENE_MAX_OBJECTS;
    for (int i = 0; i < count; ++i) {
        const CoreSceneObjectContract *object = &retained->objects[i];
        double center_x = object->object.transform.position.x;
        double center_y = object->object.transform.position.y;
        double center_z = object->object.transform.position.z;
        double size_x = object->object.transform.scale.x;
        double size_y = object->object.transform.scale.y;
        double size_z = object->object.transform.scale.z;
        if (!object->object.flags.locked) continue;
        if (object->has_rect_prism_primitive) {
            center_x = object->rect_prism_primitive.frame.origin.x;
            center_y = object->rect_prism_primitive.frame.origin.y;
            center_z = object->rect_prism_primitive.frame.origin.z;
            size_x = object->rect_prism_primitive.width;
            size_y = object->rect_prism_primitive.height;
            size_z = object->rect_prism_primitive.depth;
        }
        draw_wind_authored_object_box(pixels, canvas_w, canvas_h, volume,
                                      scale_x, scale_y, depth_x, depth_y, margin,
                                      center_x, center_y, center_z,
                                      size_x, size_y, size_z);
    }
    return true;
}

static size_t wind_volume_index3(const SceneFluidVolumeExportView3D *volume,
                                 int x,
                                 int y,
                                 int z) {
    return ((size_t)z * (size_t)volume->height + (size_t)y) * (size_t)volume->width +
           (size_t)x;
}

static float wind_cell_speed(const SceneFluidVolumeExportView3D *volume, size_t idx) {
    const float vx = volume->velocity_x[idx];
    const float vy = volume->velocity_y[idx];
    const float vz = volume->velocity_z[idx];
    return sqrtf(vx * vx + vy * vy + vz * vz);
}

static float wind_velocity_component(const float *field,
                                     const SceneFluidVolumeExportView3D *volume,
                                     int x,
                                     int y,
                                     int z) {
    x = clamp_int(x, 0, volume->width - 1);
    y = clamp_int(y, 0, volume->height - 1);
    z = clamp_int(z, 0, volume->depth - 1);
    return field[wind_volume_index3(volume, x, y, z)];
}

static float wind_cell_vorticity(const SceneFluidVolumeExportView3D *volume,
                                 int x,
                                 int y,
                                 int z) {
    const float scale = volume->voxel_size > 0.0f ? 0.5f / volume->voxel_size : 0.5f;
    const float d_vz_dy =
        (wind_velocity_component(volume->velocity_z, volume, x, y + 1, z) -
         wind_velocity_component(volume->velocity_z, volume, x, y - 1, z)) * scale;
    const float d_vy_dz =
        (wind_velocity_component(volume->velocity_y, volume, x, y, z + 1) -
         wind_velocity_component(volume->velocity_y, volume, x, y, z - 1)) * scale;
    const float d_vx_dz =
        (wind_velocity_component(volume->velocity_x, volume, x, y, z + 1) -
         wind_velocity_component(volume->velocity_x, volume, x, y, z - 1)) * scale;
    const float d_vz_dx =
        (wind_velocity_component(volume->velocity_z, volume, x + 1, y, z) -
         wind_velocity_component(volume->velocity_z, volume, x - 1, y, z)) * scale;
    const float d_vy_dx =
        (wind_velocity_component(volume->velocity_y, volume, x + 1, y, z) -
         wind_velocity_component(volume->velocity_y, volume, x - 1, y, z)) * scale;
    const float d_vx_dy =
        (wind_velocity_component(volume->velocity_x, volume, x, y + 1, z) -
         wind_velocity_component(volume->velocity_x, volume, x, y - 1, z)) * scale;
    const float wx = d_vz_dy - d_vy_dz;
    const float wy = d_vx_dz - d_vz_dx;
    const float wz = d_vy_dx - d_vx_dy;
    return sqrtf(wx * wx + wy * wy + wz * wz);
}

static void wind_heat_color(float t, uint8_t *out_r, uint8_t *out_g, uint8_t *out_b) {
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    if (t < 0.25f) {
        const float u = t / 0.25f;
        r = 20.0f;
        g = 80.0f + 150.0f * u;
        b = 190.0f + 45.0f * u;
    } else if (t < 0.5f) {
        const float u = (t - 0.25f) / 0.25f;
        r = 20.0f + 40.0f * u;
        g = 230.0f;
        b = 235.0f - 180.0f * u;
    } else if (t < 0.75f) {
        const float u = (t - 0.5f) / 0.25f;
        r = 60.0f + 190.0f * u;
        g = 230.0f;
        b = 55.0f - 45.0f * u;
    } else {
        const float u = (t - 0.75f) / 0.25f;
        r = 250.0f;
        g = 230.0f - 170.0f * u;
        b = 10.0f + 30.0f * u;
    }
    if (out_r) *out_r = (uint8_t)(r + 0.5f);
    if (out_g) *out_g = (uint8_t)(g + 0.5f);
    if (out_b) *out_b = (uint8_t)(b + 0.5f);
}

static void draw_wind_authored_object_slice_overlays(uint8_t *pixels,
                                                     int canvas_w,
                                                     int canvas_h,
                                                     const SceneState *scene,
                                                     const SceneFluidVolumeExportView3D *volume,
                                                     int scale,
                                                     int margin) {
    const PhysicsSimRetainedRuntimeScene *retained = NULL;
    int count = 0;
    if (!pixels || !scene || !volume || !scene->runtime_visual.valid) return;
    retained = &scene->runtime_visual.retained_scene;
    count = retained->retained_object_count;
    if (count <= 0) return;
    if (count > PHYSICS_SIM_RUNTIME_SCENE_MAX_OBJECTS) count = PHYSICS_SIM_RUNTIME_SCENE_MAX_OBJECTS;
    for (int i = 0; i < count; ++i) {
        const CoreSceneObjectContract *object = &retained->objects[i];
        double center_x = object->object.transform.position.x;
        double center_y = object->object.transform.position.y;
        double size_x = object->object.transform.scale.x;
        double size_y = object->object.transform.scale.y;
        int min_x = 0;
        int max_x = 0;
        int min_y = 0;
        int max_y = 0;
        int sx = 0;
        int sy = 0;
        int sw = 0;
        int sh = 0;
        if (!object->object.flags.locked) continue;
        if (object->has_rect_prism_primitive) {
            center_x = object->rect_prism_primitive.frame.origin.x;
            center_y = object->rect_prism_primitive.frame.origin.y;
            size_x = object->rect_prism_primitive.width;
            size_y = object->rect_prism_primitive.height;
        }
        min_x = wind_render_world_to_cell(center_x - size_x * 0.5, volume->origin_x,
                                          volume->voxel_size, volume->width);
        max_x = wind_render_world_to_cell(center_x + size_x * 0.5, volume->origin_x,
                                          volume->voxel_size, volume->width);
        min_y = wind_render_world_to_cell(center_y - size_y * 0.5, volume->origin_y,
                                          volume->voxel_size, volume->height);
        max_y = wind_render_world_to_cell(center_y + size_y * 0.5, volume->origin_y,
                                          volume->voxel_size, volume->height);
        sx = margin + min_x * scale;
        sy = margin + (volume->height - 1 - max_y) * scale;
        sw = (max_x - min_x + 1) * scale;
        sh = (max_y - min_y + 1) * scale;
        draw_rect(pixels, canvas_w, canvas_h, sx - 2, sy - 2, sw + 4, sh + 4,
                  255u, 236u, 80u, 230u);
        draw_rect(pixels, canvas_w, canvas_h, sx, sy, sw, sh,
                  20u, 24u, 30u, 245u);
    }
}

static bool wind_cell_is_interior_solid_any_depth(const SceneFluidVolumeExportView3D *volume,
                                                  int x,
                                                  int y) {
    if (!volume || !volume->solid_mask) return false;
    if (x <= 1 || y <= 1 || x >= volume->width - 2 || y >= volume->height - 2) return false;
    for (int z = 2; z < volume->depth - 2; ++z) {
        if (volume->solid_mask[wind_volume_index3(volume, x, y, z)] != 0u) return true;
    }
    return false;
}

static float wind_sample_slice_motion(const SceneFluidVolumeExportView3D *volume,
                                      int x,
                                      int y,
                                      int z,
                                      float inflow) {
    size_t idx = 0u;
    if (!volume || volume->width <= 0 || volume->height <= 0 || volume->depth <= 0) return 0.0f;
    x = clamp_int(x, 0, volume->width - 1);
    y = clamp_int(y, 0, volume->height - 1);
    z = clamp_int(z, 0, volume->depth - 1);
    idx = wind_volume_index3(volume, x, y, z);
    if (volume->solid_mask && volume->solid_mask[idx] != 0u) return 0.0f;
    {
        const float deficit = inflow > 0.0f ? fmaxf(0.0f, inflow - wind_cell_speed(volume, idx)) : 0.0f;
        const float vorticity = wind_cell_vorticity(volume, x, y, z);
        return deficit * 0.25f + vorticity * 0.010f;
    }
}

static void draw_wind_slice_inlet_dye(uint8_t *pixels,
                                      int canvas_w,
                                      int canvas_h,
                                      const SceneFluidVolumeExportView3D *volume,
                                      int scale,
                                      int margin,
                                      uint64_t frame_index) {
    const float phase = (float)(frame_index % 100000u) * 0.65f;
    if (!pixels || !volume) return;
    for (int band = 0; band < 4; ++band) {
        const float offset = fmodf(phase + (float)band * 7.0f, (float)(volume->width + 18)) - 18.0f;
        for (int step = 0; step < 18; ++step) {
            int x = (int)lroundf(offset + (float)step);
            if (x < 0 || x >= volume->width) continue;
            const float t = (float)step / 17.0f;
            const uint8_t alpha = (uint8_t)(105.0f * (1.0f - t));
            const int y0 = 2 + band * (volume->height - 4) / 4;
            const int y1 = 2 + (band + 1) * (volume->height - 4) / 4;
            for (int y = y0; y < y1; ++y) {
                const size_t idx = wind_volume_index3(volume, x, y, volume->depth / 2);
                if (volume->solid_mask && volume->solid_mask[idx] != 0u) continue;
                draw_rect(pixels,
                          canvas_w,
                          canvas_h,
                          margin + x * scale,
                          margin + (volume->height - 1 - y) * scale,
                          scale,
                          scale,
                          82u,
                          196u,
                          255u,
                          alpha);
            }
        }
    }
}

static void draw_wind_slice_tracers(uint8_t *pixels,
                                    int canvas_w,
                                    int canvas_h,
                                    const SceneFluidVolumeExportView3D *volume,
                                    int scale,
                                    int margin,
                                    uint64_t frame_index,
                                    float inflow) {
    const int particle_count = 44;
    const int trail_segments = 9;
    const int slice_z = volume ? volume->depth / 2 : 0;
    const float frame_phase = (float)(frame_index % 100000u);
    const float speed_cells = inflow > 0.0f ? fmaxf(0.8f, inflow * 0.18f) : 2.0f;
    if (!pixels || !volume || volume->width <= 2 || volume->height <= 2) return;

    for (int i = 0; i < particle_count; ++i) {
        const float lane_t = ((float)((i * 37) % 100) + 0.5f) / 100.0f;
        const float base_y = 2.0f + lane_t * (float)(volume->height - 5);
        const float phase = frame_phase * speed_cells + (float)(i * 11);
        const float head_x = fmodf(phase, (float)(volume->width + trail_segments * 3)) -
                             (float)(trail_segments * 3);
        int prev_sx = 0;
        int prev_sy = 0;
        bool have_prev = false;
        for (int j = trail_segments; j >= 0; --j) {
            const float trail_t = (float)j / (float)trail_segments;
            float x = head_x - (float)j * 2.4f;
            float y = base_y + sinf((x + (float)i * 3.1f + frame_phase * 0.45f) * 0.18f) * 0.35f;
            int cx = 0;
            int cy = 0;
            size_t idx = 0u;
            int sx = 0;
            int sy = 0;
            uint8_t alpha = 0u;
            if (x < 1.0f || x >= (float)(volume->width - 1)) {
                have_prev = false;
                continue;
            }
            cx = clamp_int((int)lroundf(x), 1, volume->width - 2);
            cy = clamp_int((int)lroundf(y), 1, volume->height - 2);
            y += sinf(frame_phase * 0.28f + x * 0.21f + (float)i) *
                 fminf(2.2f, wind_sample_slice_motion(volume, cx, cy, slice_z, inflow) * 0.08f);
            cy = clamp_int((int)lroundf(y), 1, volume->height - 2);
            idx = wind_volume_index3(volume, cx, cy, slice_z);
            if (volume->solid_mask && volume->solid_mask[idx] != 0u) {
                have_prev = false;
                continue;
            }
            sx = margin + cx * scale + scale / 2;
            sy = margin + (volume->height - 1 - cy) * scale + scale / 2;
            alpha = (uint8_t)(42.0f + 150.0f * (1.0f - trail_t));
            if (have_prev) {
                draw_line(pixels, canvas_w, canvas_h, prev_sx, prev_sy, sx, sy,
                          (uint8_t)(120.0f + 80.0f * (1.0f - trail_t)),
                          (uint8_t)(210.0f + 35.0f * (1.0f - trail_t)),
                          255u,
                          alpha);
            }
            if (j == 0) {
                draw_rect(pixels, canvas_w, canvas_h, sx - 1, sy - 1, 3, 3,
                          235u, 255u, 255u, 210u);
            }
            prev_sx = sx;
            prev_sy = sy;
            have_prev = true;
        }
    }
}

static bool wind_visual_mode_is_volume_diagnostic(WindVisualMode mode) {
    return mode == WIND_VISUAL_MODE_FLOW ||
           mode == WIND_VISUAL_MODE_VOLUME_SPEED_DEFICIT ||
           mode == WIND_VISUAL_MODE_VOLUME_VORTICITY;
}

static float wind_sample_volume_motion(const SceneFluidVolumeExportView3D *volume,
                                       int x,
                                       int y,
                                       int z,
                                       float inflow) {
    size_t idx = 0u;
    if (!volume || volume->width <= 0 || volume->height <= 0 || volume->depth <= 0) return 0.0f;
    x = clamp_int(x, 0, volume->width - 1);
    y = clamp_int(y, 0, volume->height - 1);
    z = clamp_int(z, 0, volume->depth - 1);
    idx = wind_volume_index3(volume, x, y, z);
    if (volume->solid_mask && volume->solid_mask[idx] != 0u) return 0.0f;
    {
        const float deficit = inflow > 0.0f ? fmaxf(0.0f, inflow - wind_cell_speed(volume, idx)) : 0.0f;
        const float vorticity = wind_cell_vorticity(volume, x, y, z);
        return deficit * 0.18f + vorticity * 0.006f;
    }
}

static void draw_wind_volume_inlet_dye(uint8_t *pixels,
                                       int canvas_w,
                                       int canvas_h,
                                       const SceneFluidVolumeExportView3D *volume,
                                       int scale_x,
                                       int scale_y,
                                       int depth_x,
                                       int depth_y,
                                       int margin,
                                       uint64_t frame_index) {
    const float phase = (float)(frame_index % 100000u) * 0.72f;
    if (!pixels || !volume) return;
    for (int sheet = 0; sheet < 5; ++sheet) {
        const float offset = fmodf(phase + (float)sheet * 8.0f,
                                   (float)(volume->width + 20)) - 20.0f;
        for (int step = 0; step < 18; ++step) {
            const int x = (int)lroundf(offset + (float)step);
            const float fade = 1.0f - (float)step / 17.0f;
            if (x < 0 || x >= volume->width) continue;
            for (int z = 2; z < volume->depth - 2; z += 3) {
                for (int y = 2; y < volume->height - 2; y += 3) {
                    const size_t idx = wind_volume_index3(volume, x, y, z);
                    int sx = 0;
                    int sy = 0;
                    if (volume->solid_mask && volume->solid_mask[idx] != 0u) continue;
                    wind_render_project_cell(x, y, z, volume->height, volume->depth,
                                             scale_x, scale_y, depth_x, depth_y,
                                             margin, &sx, &sy);
                    draw_rect(pixels, canvas_w, canvas_h, sx, sy, 3, 3,
                              78u, 190u, 255u, (uint8_t)(70.0f * fade));
                }
            }
        }
    }
}

static void draw_wind_volume_tracers(uint8_t *pixels,
                                     int canvas_w,
                                     int canvas_h,
                                     const SceneFluidVolumeExportView3D *volume,
                                     int scale_x,
                                     int scale_y,
                                     int depth_x,
                                     int depth_y,
                                     int margin,
                                     uint64_t frame_index,
                                     float inflow) {
    const int particle_count = 96;
    const int trail_segments = 12;
    const float frame_phase = (float)(frame_index % 100000u);
    const float speed_cells = inflow > 0.0f ? fmaxf(1.0f, inflow * 0.20f) : 2.4f;
    const float inv_inflow = inflow > 0.001f ? 1.0f / inflow : 0.0f;
    const float crossflow_visual_gain = 2.2f;
    bool obstacle_found = false;
    int obstacle_min_y = 0;
    int obstacle_max_y = 0;
    int obstacle_min_z = 0;
    int obstacle_max_z = 0;
    float obstacle_center_y = 0.0f;
    float obstacle_center_z = 0.0f;
    float obstacle_radius = 0.0f;
    if (!pixels || !volume || volume->width <= 4 || volume->height <= 4 || volume->depth <= 4) return;

    if (volume->solid_mask) {
        for (int sz = 1; sz < volume->depth - 1; ++sz) {
            for (int sy = 1; sy < volume->height - 1; ++sy) {
                for (int sx = 1; sx < volume->width - 1; ++sx) {
                    const size_t idx = wind_volume_index3(volume, sx, sy, sz);
                    if (volume->solid_mask[idx] == 0u) continue;
                    if (!obstacle_found) {
                        obstacle_min_y = obstacle_max_y = sy;
                        obstacle_min_z = obstacle_max_z = sz;
                        obstacle_found = true;
                    } else {
                        if (sy < obstacle_min_y) obstacle_min_y = sy;
                        if (sy > obstacle_max_y) obstacle_max_y = sy;
                        if (sz < obstacle_min_z) obstacle_min_z = sz;
                        if (sz > obstacle_max_z) obstacle_max_z = sz;
                    }
                }
            }
        }
        if (obstacle_found) {
            obstacle_center_y = ((float)obstacle_min_y + (float)obstacle_max_y) * 0.5f;
            obstacle_center_z = ((float)obstacle_min_z + (float)obstacle_max_z) * 0.5f;
            obstacle_radius = 0.5f * (float)((obstacle_max_y - obstacle_min_y) > (obstacle_max_z - obstacle_min_z)
                                             ? (obstacle_max_y - obstacle_min_y)
                                             : (obstacle_max_z - obstacle_min_z));
            if (obstacle_radius < 2.0f) obstacle_radius = 2.0f;
        }
    }

    for (int i = 0; i < particle_count; ++i) {
        const float lane_y = ((float)((i * 37) % 100) + 0.5f) / 100.0f;
        const float lane_z = ((float)((i * 61 + 17) % 100) + 0.5f) / 100.0f;
        float base_y = 2.0f + lane_y * (float)(volume->height - 5);
        float base_z = 2.0f + lane_z * (float)(volume->depth - 5);
        const float phase = frame_phase * speed_cells + (float)(i * 9);
        float x = fmodf(phase, (float)(volume->width + trail_segments * 3)) -
                  (float)(trail_segments * 3);
        float y = base_y + sinf(((float)i * 2.7f + frame_phase * 0.37f) * 0.16f) * 0.45f;
        float z = base_z + cosf(((float)i * 1.9f + frame_phase * 0.29f) * 0.14f) * 0.65f;
        int point_sx[16] = {0};
        int point_sy[16] = {0};
        bool point_valid[16] = {false};
        if (obstacle_found && i < particle_count / 2) {
            const float angle = (float)(i % 24) * 0.2617994f + (float)(i / 24) * 0.33f;
            const float ring = obstacle_radius + 2.0f + (float)((i * 5) % 11) * 0.45f;
            base_y = obstacle_center_y + sinf(angle) * ring;
            base_z = obstacle_center_z + cosf(angle) * ring;
            y = fmaxf(2.0f, fminf((float)(volume->height - 3), base_y));
            z = fmaxf(2.0f, fminf((float)(volume->depth - 3), base_z));
        }
        for (int j = 0; j <= trail_segments; ++j) {
            int cx = 0;
            int cy = 0;
            int cz = 0;
            size_t idx = 0u;
            int sx = 0;
            int sy = 0;
            float vx = inflow;
            float vy = 0.0f;
            float vz = 0.0f;
            float speed = 0.0f;
            float inv_speed = 0.0f;
            float step = 2.0f;
            if (x < 1.0f || x >= (float)(volume->width - 1)) {
                x -= 2.0f;
                continue;
            }
            cx = clamp_int((int)lroundf(x), 1, volume->width - 2);
            cy = clamp_int((int)lroundf(y), 1, volume->height - 2);
            cz = clamp_int((int)lroundf(z), 1, volume->depth - 2);
            idx = wind_volume_index3(volume, cx, cy, cz);
            if (volume->solid_mask && volume->solid_mask[idx] != 0u) {
                x -= 2.0f;
                continue;
            }
            wind_render_project_cell(cx, cy, cz, volume->height, volume->depth,
                                     scale_x, scale_y, depth_x, depth_y, margin, &sx, &sy);
            sx += scale_x / 2;
            sy += scale_y / 2;
            point_sx[j] = sx;
            point_sy[j] = sy;
            point_valid[j] = true;
            vx = volume->velocity_x ? volume->velocity_x[idx] : inflow;
            vy = volume->velocity_y ? volume->velocity_y[idx] : 0.0f;
            vz = volume->velocity_z ? volume->velocity_z[idx] : 0.0f;
            speed = sqrtf(vx * vx + vy * vy + vz * vz);
            if (speed < 0.001f) {
                vx = inflow > 0.001f ? inflow : 1.0f;
                vy = 0.0f;
                vz = 0.0f;
                speed = fabsf(vx);
            }
            inv_speed = speed > 0.001f ? 1.0f / speed : 0.0f;
            step = fmaxf(1.4f, fminf(3.2f, speed * inv_inflow * 2.2f));
            if (j < trail_segments) {
                const float motion = wind_sample_volume_motion(volume, cx, cy, cz, inflow);
                const float procedural = fminf(0.35f, motion * 0.010f);
                x -= vx * inv_speed * step;
                y -= vy * inv_speed * step * crossflow_visual_gain;
                z -= vz * inv_speed * step * crossflow_visual_gain;
                y += sinf(frame_phase * 0.19f + x * 0.13f + (float)i) * procedural;
                z += cosf(frame_phase * 0.17f + x * 0.11f + (float)i * 0.7f) * procedural;
            }
        }
        for (int j = trail_segments - 1; j >= 0; --j) {
            const float trail_t = (float)j / (float)trail_segments;
            const uint8_t alpha = (uint8_t)(42.0f + 170.0f * (1.0f - trail_t));
            if (!point_valid[j] || !point_valid[j + 1]) continue;
            draw_line(pixels, canvas_w, canvas_h,
                      point_sx[j + 1],
                      point_sy[j + 1],
                      point_sx[j],
                      point_sy[j],
                      (uint8_t)(125.0f + 95.0f * trail_t),
                      (uint8_t)(205.0f + 42.0f * trail_t),
                      255u,
                      alpha);
        }
        if (point_valid[0]) {
            draw_rect(pixels, canvas_w, canvas_h, point_sx[0] - 1, point_sy[0] - 1, 4, 4,
                      235u, 255u, 255u, 230u);
        }
    }
}

static void draw_wind_volume_solid_mask_overlay(uint8_t *pixels,
                                                int canvas_w,
                                                int canvas_h,
                                                const SceneFluidVolumeExportView3D *volume,
                                                int scale_x,
                                                int scale_y,
                                                int depth_x,
                                                int depth_y,
                                                int margin) {
    if (!pixels || !volume || !volume->solid_mask) return;
    for (int z = 2; z < volume->depth - 2; ++z) {
        for (int y = 2; y < volume->height - 2; ++y) {
            for (int x = 2; x < volume->width - 2; ++x) {
                const size_t idx = wind_volume_index3(volume, x, y, z);
                int sx = 0;
                int sy = 0;
                if (volume->solid_mask[idx] == 0u) continue;
                wind_render_project_cell(x, y, z, volume->height, volume->depth,
                                         scale_x, scale_y, depth_x, depth_y, margin, &sx, &sy);
                draw_rect(pixels, canvas_w, canvas_h, sx - 1, sy - 1, 5, 5,
                          255u, 225u, 70u, 210u);
                draw_rect(pixels, canvas_w, canvas_h, sx, sy, 3, 3,
                          18u, 20u, 24u, 245u);
            }
        }
    }
}

static bool build_wind_render_slice_pixels(const SceneState *scene,
                                           const SceneFluidVolumeExportView3D *volume,
                                           WindVisualMode mode,
                                           uint64_t frame_index,
                                           uint8_t **out_pixels,
                                           int *out_width,
                                           int *out_height) {
    const int scale = 5;
    const int margin = 10;
    const int slice_z = volume ? volume->depth / 2 : 0;
    const float inflow = scene && scene->config && scene->config->tunnel_inflow_speed > 0.0f
                             ? scene->config->tunnel_inflow_speed
                             : 0.0f;
    int canvas_w = 0;
    int canvas_h = 0;
    uint8_t *pixels = NULL;
    float peak = 0.0f;
    if (out_pixels) *out_pixels = NULL;
    if (out_width) *out_width = 0;
    if (out_height) *out_height = 0;
    if (!out_pixels || !out_width || !out_height || !volume) return false;

    canvas_w = margin * 2 + volume->width * scale;
    canvas_h = margin * 2 + volume->height * scale;
    pixels = (uint8_t *)calloc((size_t)canvas_w * (size_t)canvas_h * 4u, sizeof(uint8_t));
    if (!pixels) return false;
    for (int y = 0; y < canvas_h; ++y) {
        for (int x = 0; x < canvas_w; ++x) {
            write_rgba(pixels, canvas_w, canvas_h, x, y, 4u, 6u, 10u, 255u);
        }
    }

    for (int y = 0; y < volume->height; ++y) {
        for (int x = 0; x < volume->width; ++x) {
            const size_t idx = wind_volume_index3(volume, x, y, slice_z);
            float value = 0.0f;
            if (mode == WIND_VISUAL_MODE_SLICE_VORTICITY) {
                value = wind_cell_vorticity(volume, x, y, slice_z);
            } else if (mode == WIND_VISUAL_MODE_SLICE_SPEED_DEFICIT) {
                value = inflow > 0.0f ? fmaxf(0.0f, inflow - wind_cell_speed(volume, idx))
                                      : wind_cell_speed(volume, idx);
            }
            if (value > peak) peak = value;
        }
    }
    if (mode == WIND_VISUAL_MODE_SLICE_SPEED_DEFICIT && inflow > peak) peak = inflow;

    draw_rect(pixels, canvas_w, canvas_h, margin - 1, margin - 1,
              volume->width * scale + 2, volume->height * scale + 2,
              40u, 90u, 70u, 150u);
    for (int y = 0; y < volume->height; ++y) {
        for (int x = 0; x < volume->width; ++x) {
            const int sx = margin + x * scale;
            const int sy = margin + (volume->height - 1 - y) * scale;
            const size_t idx = wind_volume_index3(volume, x, y, slice_z);
            uint8_t r = 0u;
            uint8_t g = 0u;
            uint8_t b = 0u;
            if (mode == WIND_VISUAL_MODE_OBJECT_MASK) {
                if (!wind_cell_is_interior_solid_any_depth(volume, x, y)) continue;
                draw_rect(pixels, canvas_w, canvas_h, sx, sy, scale, scale,
                          255u, 70u, 55u, 240u);
                continue;
            }
            if (volume->solid_mask && volume->solid_mask[idx] != 0u) continue;
            if (mode == WIND_VISUAL_MODE_SLICE_VORTICITY) {
                wind_heat_color(peak > 0.0f ? wind_cell_vorticity(volume, x, y, slice_z) / peak : 0.0f,
                                &r, &g, &b);
            } else {
                const float deficit = inflow > 0.0f ? fmaxf(0.0f, inflow - wind_cell_speed(volume, idx))
                                                    : wind_cell_speed(volume, idx);
                wind_heat_color(peak > 0.0f ? deficit / peak : 0.0f, &r, &g, &b);
            }
            draw_rect(pixels, canvas_w, canvas_h, sx, sy, scale, scale, r, g, b, 220u);
        }
    }
    draw_line(pixels, canvas_w, canvas_h, margin - 3, margin,
              margin - 3, margin + volume->height * scale, 80u, 255u, 140u, 230u);
    draw_line(pixels, canvas_w, canvas_h, margin + volume->width * scale + 2, margin,
              margin + volume->width * scale + 2, margin + volume->height * scale,
              255u, 110u, 90u, 230u);
    if (mode != WIND_VISUAL_MODE_OBJECT_MASK) {
        draw_wind_slice_inlet_dye(pixels, canvas_w, canvas_h, volume, scale, margin, frame_index);
        draw_wind_slice_tracers(pixels, canvas_w, canvas_h, volume, scale, margin, frame_index, inflow);
    }
    draw_wind_authored_object_slice_overlays(pixels, canvas_w, canvas_h, scene, volume, scale, margin);
    *out_pixels = pixels;
    *out_width = canvas_w;
    *out_height = canvas_h;
    return true;
}

static bool build_wind_render_fallback_pixels(const SceneState *scene,
                                              uint64_t frame_index,
                                              uint8_t **out_pixels,
                                              int *out_width,
                                              int *out_height) {
    SceneFluidVolumeExportView3D volume = {0};
    uint8_t *pixels = NULL;
    float speed_peak = 0.0f;
    float density_peak = 0.0f;
    float pressure_peak = 0.0f;
    float deficit_peak = 0.0f;
    float vorticity_peak = 0.0f;
    float inflow_speed = 0.0f;
    WindVisualMode mode = WIND_VISUAL_MODE_FLOW;
    const int scale_x = 3;
    const int scale_y = 3;
    const int depth_x = 2;
    const int depth_y = 1;
    const int margin = 10;
    int canvas_w = 0;
    int canvas_h = 0;
    bool object_bounds_found = false;
    int object_min_x = 0;
    int object_max_x = 0;
    int object_min_y = 0;
    int object_max_y = 0;
    int object_min_z = 0;
    int object_max_z = 0;
    float object_center_y = 0.0f;
    float object_center_z = 0.0f;
    float object_radius = 0.0f;
    bool ok = false;

    if (out_pixels) *out_pixels = NULL;
    if (out_width) *out_width = 0;
    if (out_height) *out_height = 0;
    if (!out_pixels || !out_width || !out_height) return false;
    if (!scene || !scene_backend_volume_export_view_3d(scene, &volume)) return false;
    if (volume.width <= 0 || volume.height <= 0 || volume.depth <= 0 || volume.cell_count == 0u) return false;
    if (!volume.density || !volume.velocity_x || !volume.velocity_y ||
        !volume.velocity_z || !volume.pressure) {
        return false;
    }
    if (scene && scene->config) {
        mode = scene->config->wind_visual_mode;
        inflow_speed = scene->config->tunnel_inflow_speed;
    }
    if (mode == WIND_VISUAL_MODE_OBJECT_MASK ||
        mode == WIND_VISUAL_MODE_SLICE_SPEED_DEFICIT ||
        mode == WIND_VISUAL_MODE_SLICE_VORTICITY) {
        return build_wind_render_slice_pixels(scene,
                                              &volume,
                                              mode,
                                              frame_index,
                                              out_pixels,
                                              out_width,
                                              out_height);
    }

    canvas_w = margin * 2 + volume.width * scale_x + volume.depth * depth_x + 2;
    canvas_h = margin * 2 + volume.height * scale_y + volume.depth * depth_y + 2;
    pixels = (uint8_t *)calloc((size_t)canvas_w * (size_t)canvas_h * 4u, sizeof(uint8_t));
    if (!pixels) return false;

    for (int y = 0; y < canvas_h; ++y) {
        for (int x = 0; x < canvas_w; ++x) {
            write_rgba(pixels, canvas_w, canvas_h, x, y, 6u, 8u, 12u, 255u);
        }
    }

    for (int z = 0; z < volume.depth; ++z) {
        const size_t z_base = (size_t)z * (size_t)volume.width * (size_t)volume.height;
        for (int y = 0; y < volume.height; ++y) {
            const size_t row_base = z_base + (size_t)y * (size_t)volume.width;
            for (int x = 0; x < volume.width; ++x) {
                const size_t idx = row_base + (size_t)x;
                const float density = abs_float(volume.density[idx]);
                const float vx = volume.velocity_x[idx];
                const float vy = volume.velocity_y[idx];
                const float vz = volume.velocity_z[idx];
                const float speed = sqrtf(vx * vx + vy * vy + vz * vz);
                const float pressure = abs_float(volume.pressure[idx]);
                if (speed > speed_peak) speed_peak = speed;
                if (density > density_peak) density_peak = density;
                if (pressure > pressure_peak) pressure_peak = pressure;
                if (inflow_speed > 0.0f && inflow_speed - speed > deficit_peak) {
                    deficit_peak = inflow_speed - speed;
                }
            }
        }
    }
    if (mode == WIND_VISUAL_MODE_VORTICITY ||
        mode == WIND_VISUAL_MODE_VOLUME_VORTICITY) {
        for (int z = 1; z < volume.depth - 1; ++z) {
            for (int y = 1; y < volume.height - 1; ++y) {
                for (int x = 1; x < volume.width - 1; ++x) {
                    const float vorticity = wind_cell_vorticity(&volume, x, y, z);
                    if (vorticity > vorticity_peak) vorticity_peak = vorticity;
                }
            }
        }
    }
    if ((mode == WIND_VISUAL_MODE_SPEED_DEFICIT ||
         mode == WIND_VISUAL_MODE_VOLUME_SPEED_DEFICIT) &&
        !(deficit_peak > 0.0f)) {
        deficit_peak = inflow_speed > 0.0f ? inflow_speed : speed_peak;
    }
    if (volume.solid_mask) {
        for (int z = 2; z < volume.depth - 2; ++z) {
            for (int y = 2; y < volume.height - 2; ++y) {
                for (int x = 2; x < volume.width - 2; ++x) {
                    const size_t idx = wind_volume_index3(&volume, x, y, z);
                    if (volume.solid_mask[idx] == 0u) continue;
                    if (!object_bounds_found) {
                        object_min_x = object_max_x = x;
                        object_min_y = object_max_y = y;
                        object_min_z = object_max_z = z;
                        object_bounds_found = true;
                    } else {
                        if (x < object_min_x) object_min_x = x;
                        if (x > object_max_x) object_max_x = x;
                        if (y < object_min_y) object_min_y = y;
                        if (y > object_max_y) object_max_y = y;
                        if (z < object_min_z) object_min_z = z;
                        if (z > object_max_z) object_max_z = z;
                    }
                }
            }
        }
        if (object_bounds_found) {
            object_center_y = ((float)object_min_y + (float)object_max_y) * 0.5f;
            object_center_z = ((float)object_min_z + (float)object_max_z) * 0.5f;
            object_radius = 0.5f *
                            (float)((object_max_y - object_min_y) > (object_max_z - object_min_z)
                                        ? (object_max_y - object_min_y)
                                        : (object_max_z - object_min_z));
            if (object_radius < 2.0f) object_radius = 2.0f;
        }
    }

    const int x0 = margin;
    const int y0 = margin + volume.depth * depth_y;
    const int x1 = x0 + (volume.width - 1) * scale_x;
    const int y1 = y0;
    const int x2 = x0 + volume.depth * depth_x;
    const int y2 = margin;
    const int x3 = x1 + volume.depth * depth_x;
    const int y3 = margin;
    const int bottom = y0 + (volume.height - 1) * scale_y;
    draw_line(pixels, canvas_w, canvas_h, x0, y0, x1, y1, 64u, 94u, 116u, 170u);
    draw_line(pixels, canvas_w, canvas_h, x2, y2, x3, y3, 64u, 94u, 116u, 170u);
    draw_line(pixels, canvas_w, canvas_h, x0, y0, x2, y2, 64u, 94u, 116u, 170u);
    draw_line(pixels, canvas_w, canvas_h, x1, y1, x3, y3, 64u, 94u, 116u, 170u);
    draw_line(pixels, canvas_w, canvas_h, x0, bottom, x1, bottom, 40u, 62u, 78u, 150u);
    draw_line(pixels, canvas_w, canvas_h, x2, y2 + (volume.height - 1) * scale_y,
              x3, y3 + (volume.height - 1) * scale_y, 40u, 62u, 78u, 150u);

    draw_line(pixels, canvas_w, canvas_h, x0 - 2, y0, x0 - 2, bottom, 80u, 255u, 140u, 210u);
    draw_line(pixels, canvas_w, canvas_h, x1 + 2, y1, x1 + 2, bottom, 255u, 110u, 90u, 210u);

    for (int z = volume.depth - 1; z >= 0; --z) {
        const size_t z_base = (size_t)z * (size_t)volume.width * (size_t)volume.height;
        for (int y = 0; y < volume.height; ++y) {
            const size_t row_base = z_base + (size_t)y * (size_t)volume.width;
            for (int x = 0; x < volume.width; ++x) {
                const size_t idx = row_base + (size_t)x;
                const float density = abs_float(volume.density[idx]);
                const float vx = volume.velocity_x[idx];
                const float vy = volume.velocity_y[idx];
                const float vz = volume.velocity_z[idx];
                const float speed = sqrtf(vx * vx + vy * vy + vz * vz);
                const float pressure = abs_float(volume.pressure[idx]);
                const bool boundary =
                    x <= 1 || y <= 1 || z <= 1 ||
                    x >= volume.width - 2 ||
                    y >= volume.height - 2 ||
                    z >= volume.depth - 2;
                const bool solid = volume.solid_mask && volume.solid_mask[idx] != 0u && !boundary;
                const uint8_t speed_tone = tone_log(speed, speed_peak);
                const uint8_t density_tone = tone_log(density, density_peak);
                const uint8_t pressure_tone = tone_log(pressure, pressure_peak);
                const float deficit = inflow_speed > 0.0f ? fmaxf(0.0f, inflow_speed - speed) : 0.0f;
                const bool active_flow = speed_tone || density_tone || pressure_tone;
                const bool active_deficit =
                    (mode == WIND_VISUAL_MODE_SPEED_DEFICIT ||
                     mode == WIND_VISUAL_MODE_VOLUME_SPEED_DEFICIT) &&
                    deficit > 0.0f;
                const bool active_vorticity =
                    (mode == WIND_VISUAL_MODE_VORTICITY ||
                     mode == WIND_VISUAL_MODE_VOLUME_VORTICITY) &&
                    wind_cell_vorticity(&volume, x, y, z) > 0.0f;
                const int sx = margin + x * scale_x + z * depth_x;
                const int sy = margin + (volume.depth - 1 - z) * depth_y +
                               (volume.height - 1 - y) * scale_y;
                uint8_t draw_r = 0u;
                uint8_t draw_g = 0u;
                uint8_t draw_b = 0u;
                uint8_t alpha = (uint8_t)(48u + (unsigned int)speed_tone / 3u);
                int mark_size = 3;
                if (solid) {
                    draw_rect(pixels, canvas_w, canvas_h, sx - 1, sy - 1, 5, 5,
                              255u, 220u, 72u, 155u);
                    draw_rect(pixels, canvas_w, canvas_h, sx, sy, 3, 3,
                              20u, 24u, 30u, 225u);
                    continue;
                }
                if (!active_flow && !active_deficit && !active_vorticity) continue;
                if (mode == WIND_VISUAL_MODE_SPEED) {
                    wind_heat_color(speed_peak > 0.0f ? speed / speed_peak : 0.0f,
                                    &draw_r, &draw_g, &draw_b);
                } else if (mode == WIND_VISUAL_MODE_SPEED_DEFICIT ||
                           mode == WIND_VISUAL_MODE_VOLUME_SPEED_DEFICIT) {
                    bool in_object_wake = true;
                    const float deficit_denom =
                        mode == WIND_VISUAL_MODE_VOLUME_SPEED_DEFICIT && inflow_speed > 0.0f
                            ? inflow_speed
                            : deficit_peak;
                    const float deficit_t = deficit_denom > 0.0f ? deficit / deficit_denom : 0.0f;
                    if (mode == WIND_VISUAL_MODE_VOLUME_SPEED_DEFICIT && object_bounds_found) {
                        const float downstream = (float)(x - object_max_x);
                        const float wake_len = (float)(volume.width - object_max_x);
                        const float radial_yz =
                            sqrtf(((float)y - object_center_y) * ((float)y - object_center_y) +
                                  ((float)z - object_center_z) * ((float)z - object_center_z));
                        const float wake_radius =
                            object_radius + 2.5f +
                            fmaxf(0.0f, downstream) *
                                (float)(volume.height < volume.depth ? volume.height : volume.depth) *
                                0.010f;
                        in_object_wake = downstream > 0.0f &&
                                         downstream < wake_len * 0.82f &&
                                         radial_yz <= wake_radius;
                    }
                    if (mode == WIND_VISUAL_MODE_VOLUME_SPEED_DEFICIT && !in_object_wake) {
                        continue;
                    }
                    wind_heat_color(deficit_t, &draw_r, &draw_g, &draw_b);
                    if (mode == WIND_VISUAL_MODE_VOLUME_SPEED_DEFICIT) {
                        const float boosted_t = sqrtf(deficit_t < 0.0f ? 0.0f :
                                                      (deficit_t > 1.0f ? 1.0f : deficit_t));
                        if (deficit_t < 0.035f) continue;
                        alpha = (uint8_t)(28.0f + boosted_t * 132.0f);
                        mark_size = boosted_t > 0.42f ? 4 : 3;
                    }
                } else if (mode == WIND_VISUAL_MODE_VORTICITY ||
                           mode == WIND_VISUAL_MODE_VOLUME_VORTICITY) {
                    const float vorticity = wind_cell_vorticity(&volume, x, y, z);
                    wind_heat_color(vorticity_peak > 0.0f ? vorticity / vorticity_peak : 0.0f,
                                    &draw_r, &draw_g, &draw_b);
                } else {
                    const unsigned int flow_r = 18u + (unsigned int)speed_tone * 5u / 8u +
                                                (unsigned int)pressure_tone / 5u;
                    const unsigned int flow_g = 32u + (unsigned int)density_tone * 5u / 12u;
                    const unsigned int flow_b = 58u + (unsigned int)pressure_tone * 5u / 8u;
                    draw_r = (uint8_t)(flow_r > 230u ? 230u : flow_r);
                    draw_g = (uint8_t)(flow_g > 190u ? 190u : flow_g);
                    draw_b = (uint8_t)(flow_b > 235u ? 235u : flow_b);
                }
                draw_rect(pixels,
                          canvas_w,
                          canvas_h,
                          sx,
                          sy,
                          mark_size,
                          mark_size,
                          draw_r,
                          draw_g,
                          draw_b,
                          wind_visual_mode_is_volume_diagnostic(mode) &&
                                  mode != WIND_VISUAL_MODE_VOLUME_SPEED_DEFICIT
                              ? (uint8_t)(alpha / 2u + 18u)
                              : alpha);
            }
        }
    }

    if (wind_visual_mode_is_volume_diagnostic(mode)) {
        draw_wind_volume_inlet_dye(pixels, canvas_w, canvas_h, &volume,
                                   scale_x, scale_y, depth_x, depth_y, margin, frame_index);
        draw_wind_volume_tracers(pixels, canvas_w, canvas_h, &volume,
                                 scale_x, scale_y, depth_x, depth_y, margin,
                                 frame_index, inflow_speed);
        draw_wind_volume_solid_mask_overlay(pixels, canvas_w, canvas_h, &volume,
                                            scale_x, scale_y, depth_x, depth_y, margin);
    }

    (void)draw_wind_authored_object_overlays(pixels, canvas_w, canvas_h, scene, &volume,
                                             scale_x, scale_y, depth_x, depth_y, margin);

    *out_pixels = pixels;
    *out_width = canvas_w;
    *out_height = canvas_h;
    pixels = NULL;
    ok = true;
    free(pixels);
    return ok;
}

bool wind_projection_frames_write_bmp(const SceneState *scene, uint64_t frame_index) {
    const char *root = NULL;
    char dir[512];
    char path[640];
    uint8_t *pixels = NULL;
    int width = 0;
    int height = 0;
    bool ok = false;

    root = (scene && scene->config && scene->config->headless_output_dir[0])
               ? scene->config->headless_output_dir
               : "export";
    if (snprintf(dir, sizeof(dir), "%s/wind_projection_frames", root) >= (int)sizeof(dir)) return false;
    if (!ensure_dir(root) || !ensure_dir(dir)) return false;
    if (snprintf(path, sizeof(path), "%s/frame_%06llu.bmp",
                 dir,
                 (unsigned long long)frame_index) >= (int)sizeof(path)) {
        return false;
    }

    if (build_wind_projection_pixels(scene, &pixels, &width, &height)) {
        ok = write_bmp32(path, pixels, width, height);
    }
    free(pixels);
    return ok;
}

bool wind_projection_frames_write_render_fallback_bmp(const SceneState *scene,
                                                      uint64_t frame_index) {
    uint8_t *pixels = NULL;
    int width = 0;
    int height = 0;
    bool ok = false;

    if (build_wind_render_fallback_pixels(scene, frame_index, &pixels, &width, &height)) {
        ok = render_frames_write_bmp(pixels, width, height, width * 4, frame_index);
    }
    free(pixels);
    return ok;
}
