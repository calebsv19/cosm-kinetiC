#include "../src/tools/physics_sim_flow_source.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
int main(void) {
    const int n=4,m=8,c=n*n*n,f=m*m*m;
    double parent[128],same[128],fine[1024];
    for (int a=0;a<2;a++) for (int q=0;q<c;q++) parent[a*c+q]=(a+1)*(q+1)/128.;
    assert(physics_sim_flow_source(parent,n,same,n));
    for(int q=0;q<2*c;q++) assert(parent[q]==same[q]);
    assert(physics_sim_flow_source(parent,n,fine,m));
    for(int a=0;a<2;a++) for(int k=0;k<n;k++) for(int j=0;j<n;j++) for(int i=0;i<n;i++) {
        double sum=0;
        for(int z=0;z<2;z++) for(int y=0;y<2;y++) for(int x=0;x<2;x++)
            sum+=fine[a*f+((2*k+z)*m+2*j+y)*m+2*i+x];
        assert(sum==parent[a*c+(k*n+j)*n+i]);
    }
    assert(!physics_sim_flow_source(parent,n,fine,7));
    assert(!physics_sim_flow_source(parent,n,parent,n));
    parent[0]=NAN;assert(!physics_sim_flow_source(parent,n,fine,m));
    parent[0]=-1;assert(!physics_sim_flow_source(parent,n,fine,m));
    /* Exercise actual capacity and corner indexing without expensive flow. */
    double *large=calloc(2*64*64*64,sizeof(double)),*expanded=calloc(2*128*128*128,sizeof(double));
    assert(large && expanded);large[2*64*64*64-1]=8;
    assert(physics_sim_flow_source(large,64,expanded,128));
    assert(expanded[2*128*128*128-1]==1);
    /* The real fine source uses its own mapped lower support, not parent/8. */
    large[0]=4;large[64*64*64]=3;
    assert(physics_sim_flow_support(large,128,expanded));
    assert(expanded[0]==4 && expanded[128*128*128]==3);
    assert(expanded[128*128*128-1]==0 && expanded[2*128*128*128-1]==0);
    large[0]=NAN;assert(!physics_sim_flow_support(large,128,expanded));
    free(expanded);free(large);puts("extensive nested-source parity/conservation/admission/capacity passed");
}
