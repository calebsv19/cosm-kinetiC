#include "input/stroke_buffer.h"

#include <stdlib.h>
#include <string.h>

void stroke_buffer_init(StrokeBuffer *buffer, size_t capacity) {
    if (!buffer) return;
    if (capacity == 0) capacity = 1024;
    buffer->samples = (StrokeSample *)calloc(capacity, sizeof(StrokeSample));
    buffer->capacity = buffer->samples ? capacity : 0;
    buffer->head = buffer->tail = buffer->count = 0;
}

void stroke_buffer_shutdown(StrokeBuffer *buffer) {
    if (!buffer) return;
    free(buffer->samples);
    buffer->samples = NULL;
    buffer->capacity = buffer->head = buffer->tail = buffer->count = 0;
}

void stroke_buffer_clear(StrokeBuffer *buffer) {
    if (!buffer) return;
    buffer->head = buffer->tail = buffer->count = 0;
    if (buffer->samples) {
        memset(buffer->samples, 0, buffer->capacity * sizeof(StrokeSample));
    }
}

bool stroke_buffer_push(StrokeBuffer *buffer, const StrokeSample *sample) {
    if (!buffer || !sample || buffer->capacity == 0) return false;
    if (buffer->count >= buffer->capacity) {
        // grow by doubling
        size_t new_capacity = buffer->capacity * 2;
        StrokeSample *new_samples =
            (StrokeSample *)calloc(new_capacity, sizeof(StrokeSample));
        if (!new_samples) {
            return false;
        }
        for (size_t i = 0; i < buffer->count; ++i) {
            size_t src = (buffer->head + i) % buffer->capacity;
            new_samples[i] = buffer->samples[src];
        }
        free(buffer->samples);
        buffer->samples = new_samples;
        buffer->capacity = new_capacity;
        buffer->head = 0;
        buffer->tail = buffer->count;
    }
    buffer->samples[buffer->tail] = *sample;
    buffer->tail = (buffer->tail + 1) % buffer->capacity;
    buffer->count++;
    return true;
}

bool stroke_buffer_pop(StrokeBuffer *buffer, StrokeSample *out_sample) {
    if (!buffer || !out_sample || buffer->count == 0) return false;
    *out_sample = buffer->samples[buffer->head];
    buffer->head = (buffer->head + 1) % buffer->capacity;
    buffer->count--;
    return true;
}

size_t stroke_buffer_count(const StrokeBuffer *buffer) {
    return buffer ? buffer->count : 0;
}
