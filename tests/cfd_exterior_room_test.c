#include "app/cfd_exterior_room.h"
#include "app/cfd_open_atmosphere3d.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
static void trial(bool far, bool large, bool fine) {
    int n = far ? (fine ? 64 : 32) : (fine ? 48 : 24);
    double l = far ? 2 : 1.5, o = (l-1)/2;
    CfdOpenAtmosphere3d s={0}; int grid[3]={n,n,n};
    double length[3]={l,l,l},p[2]={0},t[2]={300,300},m[2]={0};
    assert(cfd_open_atmosphere3d_init(&s,grid,length,1.2,1.8e-5,1005,300,.026,0,.005,p,t,m,true,9.81,1./300,.1));
    s.ground=true; assert(cfd_open_atmosphere3d_lateral(&s));
    assert(cfd_exterior_room(&s,large,far));
    int ix=(int)round(1/s.grid.h[0]), openings=0;
    for(int k=0;k<n;k++) for(int j=0;j<n;j++) {
        double y=(j+.5)*s.grid.h[1]-o,z=(k+.5)*s.grid.h[2];
        bool inwall=y>=0 && y<1 && z<1;
        bool aperture=y>=(large?.25:.375) && y<(large?.75:.625) && z>=.375 && z<.875;
        bool blocked=cfd_open_atmosphere3d_blocked(&s,0,ix,j,k);
        if(inwall) {assert(blocked==!aperture); if(aperture)openings++;}
        else assert(!blocked);
        /* The distant east boundary is open everywhere, with air between it and the room. */
        assert(!cfd_open_atmosphere3d_blocked(&s,0,n,j,k));
    }
    assert(fabs(openings*s.grid.h[1]*s.grid.h[2]-(large?.25:.125))<1e-12);
    assert(!cfd_exterior_room(&s,large,far)); cfd_open_atmosphere3d_destroy(&s);
}
int main(void) {
    for(int far=0;far<2;far++)for(int large=0;large<2;large++)for(int fine=0;fine<2;fine++)trial(far,large,fine);
    puts("exterior room window/closed shell/distant ambient and refinement geometry passed");return 0;
}
