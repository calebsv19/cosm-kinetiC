/* Independent rectangular-grid 90-degree XY rotation through evolving buoyancy,
 * momentum, pressure, reservoir faces and MUSCL scalar transport. */
#include "app/cfd_open_atmosphere3d.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    CfdMemoryBudget budget={.limit_bytes=128*1024*1024};CfdMemoryBudget *prior=cfd_memory_scope(&budget);
    CfdOpenAtmosphere3d s[2];double l[3]={1,1,1},p[2]={0,0},t[2]={300,300};
    int grids[2][3]={{8,12,16},{12,8,16}},count=8*12*16;
    double *source[2];
    for(int a=0;a<2;a++) {
        assert(cfd_open_atmosphere3d_init(s+a,grids[a],l,1.2,.000018,1005,300,.026,.00002,.0025,p,t,p,true,9.81,1./300,.1));
        s[a].ground=true;s[a].muscl=true;s[a].cache_projection=true;
        assert(cfd_open_atmosphere3d_lateral(s+a));
        source[a]=calloc(2*count,sizeof(double));assert(source[a]);
    }
    for(int k=0;k<16;k++)for(int j=0;j<12;j++)for(int i=0;i<8;i++) {
        int q=(k*12+j)*8+i,r=(k*8+i)*12+11-j;
        double x=(i+.5)/8.,y=(j+.5)/12.,z=(k+.5)/16.;
        double heat=.003*exp(-50*((x-.4)*(x-.4)+2*(y-.55)*(y-.55)+z*z));
        source[0][q]=source[1][r]=heat;
        source[0][count+q]=source[1][count+r]=heat*1e-6;
    }
    for(int step=0;step<80;step++)for(int a=0;a<2;a++)
        assert(cfd_open_atmosphere3d_step(s+a,source[a],source[a]+count));
    double heat=0,scale=0,velocity=0;
    for(int k=0;k<16;k++)for(int j=0;j<12;j++)for(int i=0;i<8;i++) {
        int q=(k*12+j)*8+i,r=(k*8+i)*12+11-j;
        heat+=fabs(s[0].energy[q]-s[1].energy[r]);scale+=fabs(s[0].energy[q]);
    }
    for(int a=0;a<3;a++)for(int k=0;k<16+(a==2);k++)for(int j=0;j<12+(a==1);j++)for(int i=0;i<8+(a==0);i++) {
        int q=cfd_open_atmosphere3d_face(s,a,i,j,k);
        int r=cfd_open_atmosphere3d_face(s+1,a==2?2:1-a,(a==1?12:11)-j,i,k);
        velocity=fmax(velocity,fabs(s[0].velocity[q]-(a==1?-1:1)*s[1].velocity[r]));
    }
    printf("{\"xy_rotated_heat_relative_l1\":%.17g,\"xy_rotated_velocity_max_m_s\":%.17g,\"steps\":80}\n",heat/scale,velocity);
    assert(heat/scale<1e-9 && velocity<1e-9);
    for(int a=0;a<2;a++){free(source[a]);cfd_open_atmosphere3d_destroy(s+a);}
    assert(budget.live_bytes==0);cfd_memory_scope(prior);
}
