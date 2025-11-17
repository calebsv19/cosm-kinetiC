#include "export/render_frames.h"

#include "export/export_paths.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct __attribute__((__packed__)) BmpFileHeader {
    uint16_t bfType;
    uint32_t bfSize;
    uint16_t bfReserved1;
    uint16_t bfReserved2;
    uint32_t bfOffBits;
} BmpFileHeader;

typedef struct __attribute__((__packed__)) BmpInfoHeader {
    uint32_t biSize;
    int32_t  biWidth;
    int32_t  biHeight;
    uint16_t biPlanes;
    uint16_t biBitCount;
    uint32_t biCompression;
    uint32_t biSizeImage;
    int32_t  biXPelsPerMeter;
    int32_t  biYPelsPerMeter;
    uint32_t biClrUsed;
    uint32_t biClrImportant;
} BmpInfoHeader;

bool render_frames_write_bmp(const uint8_t *rgba_pixels,
                             int width,
                             int height,
                             int pitch,
                             uint64_t frame_index) {
    if (!rgba_pixels || width <= 0 || height <= 0) return false;
    if (!export_paths_init()) return false;
    const char *dir = export_render_dir();
    if (!dir) return false;

    char path[512];
    snprintf(path, sizeof(path), "%s/frame_%06llu.bmp",
             dir, (unsigned long long)frame_index);

    int row_bytes = width * 4;
    uint32_t pixel_data_size = (uint32_t)(row_bytes * height);

    BmpFileHeader file_header = {
        .bfType = 0x4D42, // 'BM'
        .bfSize = sizeof(BmpFileHeader) + sizeof(BmpInfoHeader) + pixel_data_size,
        .bfReserved1 = 0,
        .bfReserved2 = 0,
        .bfOffBits = sizeof(BmpFileHeader) + sizeof(BmpInfoHeader)
    };

    BmpInfoHeader info_header = {
        .biSize = sizeof(BmpInfoHeader),
        .biWidth = width,
        .biHeight = -height, // top-down bitmap
        .biPlanes = 1,
        .biBitCount = 32,
        .biCompression = 0,
        .biSizeImage = pixel_data_size,
        .biXPelsPerMeter = 0,
        .biYPelsPerMeter = 0,
        .biClrUsed = 0,
        .biClrImportant = 0
    };

    FILE *f = fopen(path, "wb");
    if (!f) {
        fprintf(stderr, "[export] Failed to open %s (%s)\n", path, strerror(errno));
        return false;
    }

    if (fwrite(&file_header, sizeof(file_header), 1, f) != 1 ||
        fwrite(&info_header, sizeof(info_header), 1, f) != 1) {
        fprintf(stderr, "[export] Failed to write BMP headers to %s\n", path);
        fclose(f);
        return false;
    }

    uint8_t *row = (uint8_t *)malloc((size_t)width * 4);
    if (!row) {
        fclose(f);
        return false;
    }

    for (int y = 0; y < height; ++y) {
  	  const uint8_t *src = rgba_pixels + y * pitch;
  	  for (int x = 0; x < width; ++x) {
  	      const uint8_t *px = src + x * 4;
	
	        // Assume source is RGBA
	        uint8_t r = px[0];
	        uint8_t g = px[1];
	        uint8_t b = px[2];
	        uint8_t a = px[3];
	
	        uint8_t *dst = row + x * 4;
	
	        // BMP 32-bit expects BGRA
	        dst[0] = b;
	        dst[1] = g;
	        dst[2] = r;
	        dst[3] = a;
	    }
	
	    if (fwrite(row, 1, width * 4, f) != (size_t)(width * 4)) {
	        fprintf(stderr, "[export] Failed to write BMP row to %s\n", path);
	        free(row);
	        fclose(f);
	        return false;
	    }
    }


    free(row);

    fclose(f);
    return true;
}
