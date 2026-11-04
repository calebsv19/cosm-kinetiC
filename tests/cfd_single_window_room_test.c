#include "app/cfd_single_window_room.h"
#include "app/cfd_open_atmosphere3d.h"
#include <assert.h>
#include <stdio.h>
static void trial(int n,bool large) {
    CfdOpenAtmosphere3d s={0};int grid[3]={n,n,n};double length[3]={1,1,1},p[2]={0},t[2]={300,300},m[2]={0};
    assert(cfd_open_atmosphere3d_init(&s,grid,length,1.2,1.8e-5,1005,300,.026,0,.005,p,t,m,true,9.81,1./300,.1));
    s.ground=true;assert(cfd_open_atmosphere3d_lateral(&s));assert(cfd_single_window_room(&s,large));
    int openings=0,lo=large?n/4:3*n/8,hi=large?3*n/4:5*n/8;
    for(int k=0;k<n;k++)for(int j=0;j<n;j++) {
        assert(cfd_open_atmosphere3d_blocked(&s,0,0,j,k));
        assert(cfd_open_atmosphere3d_blocked(&s,1,j,0,k));
        assert(cfd_open_atmosphere3d_blocked(&s,1,j,n,k));
        assert(cfd_open_atmosphere3d_blocked(&s,2,j,k,n));
        bool open=!cfd_open_atmosphere3d_blocked(&s,0,n,j,k);
        assert(open==(j>=lo && j<hi && k>=3*n/8 && k<7*n/8));openings+=open;
    }
    assert(openings==(hi-lo)*n/2);assert(!cfd_single_window_room(&s,large));
    cfd_open_atmosphere3d_destroy(&s);
}
int main(void){trial(16,false);trial(16,true);trial(32,false);trial(32,true);puts("single-window room exact opening, closed remaining boundaries and refusal passed");return 0;}
