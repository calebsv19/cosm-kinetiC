#include "app/cfd_wall3d.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
int main(void) {
    for (int size = 8; size <= 32; size *= 2) {
        CfdMemoryBudget budget = {.limit_bytes = 512 * 1024 * 1024};
        CfdMemoryBudget *previous = cfd_memory_scope(&budget);
        CfdWall3d s;
        int n[3] = {2 * size, size, size};
        double L[3] = {4, 2, 2};
        assert(cfd_wall3d_init_open(&s, n, L, 1, .1, .005));
        double strain_work = 0, vector_work = 0;
        for (int k = 0; k < n[2]; k++)
            for (int j = 0; j < n[1]; j++)
                for (int i = 0; i < n[0]; i++) {
                    double d[3][3];
                    cfd_wall3d_derivatives(&s, i, j, k, d);
                    for (int a = 0; a < 3; a++)
                        for (int b = 0; b < 3; b++) {
                            double e = .5 * (d[a][b] + d[b][a]);
                            strain_work += 2 * s.mu * e * e * s.grid.volume;
                            vector_work += s.mu * d[a][b] * d[a][b] * s.grid.volume;
                        }
                }
        printf("{\"cross_section\":%d,\"reference_dissipation\":%.17g,"
               "\"exact_face_strain_dissipation_error\":%.17g,"
               "\"exact_face_vector_dissipation_error\":%.17g}\n", size, s.dissipation_base,
               fabs(strain_work / s.dissipation_base - 1),
               fabs(vector_work / s.dissipation_base - 1));
        cfd_wall3d_destroy(&s);
        assert(budget.live_bytes == 0);
        cfd_memory_scope(previous);
    }
    return 0;
}
