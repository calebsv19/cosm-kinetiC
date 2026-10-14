#include "app/cfd_hotgas3d.h"
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
int main(int argc, char **argv) {
    if (argc != 11)
        return 2;
    double v[10];
    for (int i = 0; i < 10; ++i) {
        char *end = NULL;
        errno = 0;
        v[i] = strtod(argv[i + 1], &end);
        if (errno || end == argv[i + 1] || *end || !isfinite(v[i]))
            return 2;
    }
    for (int i = 0; i < 3; ++i)
        if (v[i] != floor(v[i]) || v[i] < 4 || v[i] > 256)
            return 2;
    if (v[9] != floor(v[9]) || v[9] < 0 || v[9] > 100000 || v[8] <= 0)
        return 2;
    int n[3] = {(int)v[0], (int)v[1], (int)v[2]};
    double length[3] = {v[3], v[4], v[5]};
    CfdHotGas3d s;
    if (!cfd_hotgas3d_init(&s, n, length, v[6], v[7]))
        return 3;
    int count = 5 * s.grid.count, code = 3;
    double *source = cfd_memory_calloc((size_t)count, sizeof(double));
    if (!source)
        goto done;
    for (int i = 0; i < count; ++i)
        if (scanf("%lf", s.state + i) != 1)
            goto done;
    for (int i = 0; i < count; ++i)
        if (scanf("%lf", source + i) != 1)
            goto done;
    char tail;
    if (scanf(" %c", &tail) == 1)
        goto done;
    for (int q = 0; q < s.grid.count; ++q) {
        double t, p, c;
        if (!cfd_hotgas3d_primitive(&s, s.state + 5 * q, &t, &p, &c))
            goto done;
    }
    for (int step = 0; step < (int)v[9]; ++step)
        if (!cfd_hotgas3d_step(&s, source, v[8]))
            goto done;
    printf("{\"conserved\":[");
    for (int i = 0; i < count; ++i)
        printf("%s%.17g", i ? "," : "", s.state[i]);
    printf("]}\n");
    code = 0;
done:
    cfd_memory_free(source);
    cfd_hotgas3d_destroy(&s);
    return code;
}
