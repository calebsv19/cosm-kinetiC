#include "app/cfd_refined_diffusion.h"
#include <stdio.h>
#include <stdlib.h>
static bool emit(int row, int column, double value, void *context) {
    int *first = context;
    printf("%s[%d,%d,%.17g]", *first ? "" : ",", row, column, value);
    *first = 0;
    return true;
}
int main(int argc, char **argv) {
    int n = argc > 1 ? atoi(argv[1]) : 8;
    if (n < 4 || n > 64 || n % 4)
        return 2;
    CfdRefinedMesh m;
    if (!cfd_refined_mesh_init(&m, 2 * n, n, 4, 2, n / 2, n / 4, 3 * n / 2, 3 * n / 4))
        return 2;
    CfdRefinedDiffusion *d = cfd_refined_diffusion_create(&m, NULL);
    if (!d)
        return 2;
    printf("{\"cells\":[");
    for (int c = 0; c < m.cell_count; c++)
        printf("%s[%.17g,%.17g,%.17g]", c ? "," : "", m.cells[c].cx, m.cells[c].cy,
               m.cells[c].volume);
    printf("],\"faces\":[");
    for (int f = 0; f < m.face_count; f++) {
        CfdRefinedFace *p = &m.faces[f];
        printf("%s[%d,%d,%d,%.17g,%.17g,%.17g]", f ? "," : "", p->lo, p->hi, p->axis, p->cx, p->cy,
               p->area);
    }
    printf("],\"entries\":[");
    int first = 1;
    bool ok = cfd_refined_diffusion_matrix_visit(d, emit, &first);
    printf("]}\n");
    cfd_refined_diffusion_destroy(d);
    cfd_refined_mesh_destroy(&m);
    return ok ? 0 : 2;
}
