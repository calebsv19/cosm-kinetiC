/* Reuse the independent whole-field material/flow scaling fixture on a
 * displaced .75 x .75 x 1.25 m box, without changing the original cube fixture. */
#include "app/cfd_obstacle3d_box.h"
static bool rectangular_init(CfdObstacle3d *s,const int n[3],const double length[3],
                              double rho,double mu,double flow,double center) {
    const double lo[3]={center-.375,.375,.375},hi[3]={center+.375,1.125,1.625};
    return cfd_obstacle3d_box_init(s,n,length,rho,mu,flow,lo,hi);
}
#define cfd_obstacle3d_init rectangular_init
#include "cfd_obstacle3d_material_scaling_test.c"
