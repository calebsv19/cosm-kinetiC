#include "app/cfd_cartesian3d.h"
#include <assert.h>
int main(void){
    CfdCartesian3d g;double l[3]={1,1,.5};int n[3]={4,4,1024};
#ifdef PHYSICS_SIM_THIN_TRANSPORT_QUALIFIER
    assert(cfd_cartesian3d_init(&g,n,l));assert(g.count==16384);
#else
    assert(!cfd_cartesian3d_init(&g,n,l));
#endif
    n[1]=8;assert(!cfd_cartesian3d_init(&g,n,l));
    n[1]=4;n[2]=1025;assert(!cfd_cartesian3d_init(&g,n,l));
    n[2]=256;assert(cfd_cartesian3d_init(&g,n,l));
}
