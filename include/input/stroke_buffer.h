#ifndef STROKE_BUFFER_H
#define STROKE_BUFFER_H

#include <stdbool.h>
#include <stddef.h>

#include "input/input.h"

typedef struct StrokeSample {
    int x;
    int y;
    float vx;
    float vy;
    BrushMode mode;
} StrokeSample;

typedef struct StrokeBuffer {
    StrokeSample *samples;
    size_t capacity;
    size_t head;
    size_t tail;
    size_t count;
} StrokeBuffer;

void stroke_buffer_init(StrokeBuffer *buffer, size_t capacity);
void stroke_buffer_shutdown(StrokeBuffer *buffer);
void stroke_buffer_clear(StrokeBuffer *buffer);
bool stroke_buffer_push(StrokeBuffer *buffer, const StrokeSample *sample);
bool stroke_buffer_pop(StrokeBuffer *buffer, StrokeSample *out_sample);
size_t stroke_buffer_count(const StrokeBuffer *buffer);

#endif // STROKE_BUFFER_H
