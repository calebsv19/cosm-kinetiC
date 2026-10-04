#define main wall_shear_main
#include "cfd_obstacle3d_wall_shear_probe.c"
#undef main
int main(void) {
    printf("[");
    int count = 0;
    for (int a = 0; a < 3; a++) {
        double intervals[][2] = {{low[a]-.2,low[a]-.01}, {low[a]-.1,low[a]+.2},
            {low[a]+.1,high[a]-.1}, {high[a]-.2,high[a]+.1},
            {high[a]+.01,high[a]+.2}, {low[a]+.0001,low[a]+.001},
            {high[a]-.001,high[a]}};
        for (int k = 0; k < 7; k++)
            for (int d = 0; d < 4; d++) {
                if (count++) printf(",");
                printf("[%d,%.17g,%.17g,%d,%.17g]",a,intervals[k][0],intervals[k][1],d,mean(a,intervals[k][0],intervals[k][1],d));
            }
    }
    printf("]\n");
    return 0;
}
