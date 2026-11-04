#include "app/cfd_single_window_room.h"
#include "app/cfd_open_atmosphere3d.h"
bool cfd_single_window_room(CfdOpenAtmosphere3d *s,bool large_window) {
    if(!s || !s->storage || s->steps || s->solid_box || !s->ground ||
       !s->lateral_open || s->wind.enabled || s->weather.broad_sweeping_shear)return false;
    int n=s->grid.n[0];
    if((n!=16 && n!=32) || s->grid.n[1]!=n || s->grid.n[2]!=n ||
       s->grid.length[0]!=1 || s->grid.length[1]!=1 || s->grid.length[2]!=1)return false;
    unsigned char *mask=cfd_memory_calloc((size_t)s->grid.count,1);if(!mask)return false;
    int lo=large_window?n/4:3*n/8,hi=large_window?3*n/4:5*n/8;
    for(int k=0;k<n;k++)for(int j=0;j<n;j++)for(int i=0;i<n;i++) {
        bool opening=j>=lo && j<hi && k>=3*n/8 && k<7*n/8;
        bool solid=i<n/16 || j<n/16 || j>=15*n/16 || k>=15*n/16 ||
            (i>=15*n/16 && !opening);
        mask[(k*n+j)*n+i]=(unsigned char)solid;
    }
    s->rig_mask=mask;s->solid_box=true;return true;
}
