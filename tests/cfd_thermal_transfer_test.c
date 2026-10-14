#include "../src/tools/physics_sim_prescribed_flow.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
static void init(CfdOpenAtmosphere3d *s,int side) {
    int n[3]={side,side,side};double l[3]={1,1,1},z[2]={0,0},t[2]={300,300};
    assert(cfd_open_atmosphere3d_init(s,n,l,1.2,1.8e-5,1005,300,.026,.0005,.005,z,t,z,true,9.81,1./300,.1));
}
static double mean(double x,double h) {
    const double pi=3.14159265358979323846;
    return (sin(2*pi*(x+h/2))-sin(2*pi*(x-h/2)))/(2*pi*h);
}
static double run(int n,int mode) {
    CfdOpenAtmosphere3d c,f;init(&c,n);init(&f,2*n);
    double *v=cfd_memory_calloc(f.grid.count,sizeof(double));assert(v);
    double h=1./n;
    for(int k=0;k<n;k++)for(int j=0;j<n;j++)for(int i=0;i<n;i++) {
        double a=mode==0?2:mode==1?2+(.5+i)*h+2*(.5+j)*h+3*(.5+k)*h:
                 mode==2?(i<n/2 && j<n/2 && k<n/2?1:0):mode==4?ldexp(1,-((k*n+j)*n+i)%960):2+.2*mean((i+.5)*h,h)+.1*mean((j+.5)*h,h)+.05*mean((k+.5)*h,h);
        c.energy[(k*n+j)*n+i]=a;
    }
    assert(prescribed_flow_reconstruct(&c,&f,v));double error=0;
    for(int k=0;k<n;k++)for(int j=0;j<n;j++)for(int i=0;i<n;i++) {
        int q=(k*n+j)*n+i;double parent=c.energy[q],lo=parent,hi=parent,sum=0;
        int pos[3]={i,j,k},stride[3]={1,n,n*n};
        for(int a=0;a<3;a++) {
            if(pos[a]>0){lo=fmin(lo,c.energy[q-stride[a]]);hi=fmax(hi,c.energy[q-stride[a]]);}
            if(pos[a]+1<n){lo=fmin(lo,c.energy[q+stride[a]]);hi=fmax(hi,c.energy[q+stride[a]]);}
        }
        for(int z=0;z<2;z++)for(int y=0;y<2;y++)for(int x=0;x<2;x++) {
            double value=8*v[(((2*k+z)*2*n+2*j+y)*2*n+2*i+x)];sum+=value/8;
            assert(value>=lo-2e-15 && value<=hi+2e-15 && value>=0);
            if(mode==0)assert(value==2);
            if(mode==2)assert(value==parent);
            if(i>0 && j>0 && k>0 && i<n-1 && j<n-1 && k<n-1) {
                if(mode==1)assert(fabs(value-(2+(2*i+x+.5)*h/2+2*(2*j+y+.5)*h/2+3*(2*k+z+.5)*h/2))<4e-15);
                if(mode==3) {
                    double exact=2+.2*mean((2*i+x+.5)*h/2,h/2)+.1*mean((2*j+y+.5)*h/2,h/2)+.05*mean((2*k+z+.5)*h/2,h/2);
                    error+=fabs(value-exact)*h*h*h/8;
                }
            }
        }
        assert(fabs(sum-parent)<=3e-15*fmax(parent,1e-300));
    }
    c.energy[c.grid.count-1]=NAN;v[0]=123;
    assert(!prescribed_flow_reconstruct(&c,&f,v));assert(v[0]==123);
    assert(!prescribed_flow_reconstruct(&c,&c,v));
    cfd_memory_free(v);cfd_open_atmosphere3d_destroy(&c);cfd_open_atmosphere3d_destroy(&f);return error;
}
int main(void) {
    CfdMemoryBudget b={.limit_bytes=100000000};CfdMemoryBudget *prior=cfd_memory_scope(&b);
    for(int m=0;m<3;m++)run(8,m);run(8,4);
    double e[3];for(int a=0;a<3;a++)e[a]=run(8<<a,3);
    printf("smooth cell-average L1 %.17g %.17g %.17g ratios %.8g %.8g\n",e[0],e[1],e[2],e[0]/e[1],e[1]/e[2]);
    assert(e[0]/e[1]>2.5 && e[1]/e[2]>2.5);
    cfd_memory_scope(prior);assert(!b.live_bytes && !b.rejected_allocations);
    puts("conservative bounded transfer: constants, affine interior, sharp edges, smooth refinement, atomic input refusal, memory closure passed");
}
