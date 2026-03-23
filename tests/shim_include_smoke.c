#include <stdbool.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <math.h>

static int sum_ints(int count, ...) {
    va_list ap;
    int total = 0;

    va_start(ap, count);
    for (int i = 0; i < count; ++i) {
        total += va_arg(ap, int);
    }
    va_end(ap);
    return total;
}

int main(void) {
    bool ok = true;
    uint32_t value = (uint32_t)sum_ints(3, 1, 2, 3);
    char text[32];
    time_t now = time(NULL);

    if (value != 6u) ok = false;
    (void)snprintf(text, sizeof(text), "%u", (unsigned)value);
    (void)strlen(text);
    (void)malloc(8);
    errno = 0;
    (void)fabs(1.0);
    (void)now;

    return ok ? 0 : 1;
}
