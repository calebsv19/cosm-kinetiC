#include "physics_sim_horizontal_source.h"
#include <math.h>
#include <stddef.h>
bool physics_sim_horizontal_source(const double *support, int parent, double *full) {
    if (!support || !full || support == full || parent < 8 || parent > 128 || parent % 8) return false;
    size_t supported=(size_t)parent*parent*(parent/8), n=2*parent, count=n*parent*n;
    for(size_t q=0;q<2*supported;q++)
        if(!isfinite(support[q]) || support[q]<0 || (support[q]>0 && support[q]/4==0)) return false;
    for(size_t a=0;a<2;a++)for(size_t k=0;k<n;k++)for(size_t j=0;j<(size_t)parent;j++)for(size_t i=0;i<n;i++)
        full[a*count+(k*parent+j)*n+i]=k<n/8 ? support[a*supported+((k/2)*parent+j)*parent+i/2]/4 : 0;
    return true;
}
