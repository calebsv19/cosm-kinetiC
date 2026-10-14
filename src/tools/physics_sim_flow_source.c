#include "physics_sim_flow_source.h"
#include <math.h>
#include <stddef.h>
bool physics_sim_flow_source(const double *parent, int side, double *fine, int fine_side) {
    if (!parent || !fine || parent == fine || side < 1 || side > 64 ||
        (fine_side != side && fine_side != 2*side)) return false;
    int ratio=fine_side/side, count=side*side*side, fine_count=fine_side*fine_side*fine_side;
    double divisor=(double)(ratio*ratio*ratio);
    for (int q=0;q<2*count;q++)
        if (!isfinite(parent[q]) || parent[q]<0) return false;
    for (int a=0;a<2;a++) for (int k=0;k<fine_side;k++)
        for (int j=0;j<fine_side;j++) for (int i=0;i<fine_side;i++) {
            size_t q=(size_t)a*fine_count+(k*fine_side+j)*fine_side+i;
            int p=a*count+((k/ratio)*side+j/ratio)*side+i/ratio;
            fine[q]=parent[p]/divisor;
        }
    return true;
}
/* Compact original receiver mapping occupies the lower 1/8 domain only. */
bool physics_sim_flow_support(const double *support, int side, double *full) {
    if (!support || !full || support==full || side<8 || side>128 || side%8) return false;
    int count=side*side*side, supported=count/8;
    for(int q=0;q<2*supported;q++)
        if(!isfinite(support[q]) || support[q]<0) return false;
    for(int a=0;a<2;a++) for(int q=0;q<count;q++)
        full[a*count+q]=q<supported ? support[a*supported+q] : 0.;
    return true;
}
