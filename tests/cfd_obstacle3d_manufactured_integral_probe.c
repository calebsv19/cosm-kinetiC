#define main manufactured_stokes_main
#include "cfd_obstacle3d_manufactured_stokes_probe.c"
#undef main
int main(void) {
    double intervals[][2] = {{-.2,.2},{.1,.6},{.4,.9},{1.,1.4},{1.3,1.9},{.2501,.251},{1.749,1.7499}};
    printf("[");
    int count = 0;
    for (int a = 0; a < 3; a++)
        for (int k = 0; k < 7; k++)
            for (int d = 0; d < 4; d++) {
                if (count++)
                    printf(",");
                printf("[%d,%.17g,%.17g,%d,%.17g]",a,intervals[k][0],intervals[k][1],d,mean(a,intervals[k][0],intervals[k][1],d));
            }
    printf("]\n");
    return 0;
}
