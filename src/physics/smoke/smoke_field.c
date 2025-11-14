#include "physics/smoke/smoke_field.h"

#include <stdlib.h>
#include <string.h>

SmokeField *smoke_field_create(int width, int height) {
    if (width <= 0 || height <= 0) return NULL;
    SmokeField *field = (SmokeField *)malloc(sizeof(SmokeField));
    if (!field) return NULL;
    field->width = width;
    field->height = height;
    size_t count = (size_t)width * (size_t)height;
    field->density = (float *)calloc(count, sizeof(float));
    if (!field->density) {
        free(field);
        return NULL;
    }
    return field;
}

void smoke_field_destroy(SmokeField *field) {
    if (!field) return;
    free(field->density);
    free(field);
}

void smoke_field_clear(SmokeField *field) {
    if (!field || !field->density) return;
    size_t count = (size_t)field->width * (size_t)field->height;
    memset(field->density, 0, count * sizeof(float));
}
