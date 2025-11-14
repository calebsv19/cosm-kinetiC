#ifndef SMOKE_FIELD_H
#define SMOKE_FIELD_H

#include <stddef.h>

typedef struct SmokeField {
    int    width;
    int    height;
    float *density;
} SmokeField;

SmokeField *smoke_field_create(int width, int height);
void        smoke_field_destroy(SmokeField *field);
void        smoke_field_clear(SmokeField *field);

#endif // SMOKE_FIELD_H
