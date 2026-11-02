/* Fixed unit chamber: insulated no-slip shell, partition and optional window.
 * Every connected fluid region must touch a pressure reservoir. This profile
 * does not admit an isolated sealed pressure component or moving topology. */
#include "app/cfd_open_atmosphere3d.h"
#include <math.h>

bool cfd_open_atmosphere3d_rig(CfdOpenAtmosphere3d *s,bool window_open) {
    if(!s || !s->storage || s->steps || s->solid_box || !s->ground || !s->lateral_open || s->wind.enabled)return false;
    int n=s->grid.n[0];
    if((n!=16 && n!=32) || s->grid.n[1]!=n || s->grid.n[2]!=n ||
       s->grid.length[0]!=1 || s->grid.length[1]!=1 || s->grid.length[2]!=1)return false;
    unsigned char *mask=cfd_memory_calloc((size_t)s->grid.count,1);
    if(!mask)return false;
    for(int k=0;k<n;k++)for(int j=0;j<n;j++)for(int i=0;i<n;i++) {
        bool port=j>=n/4 && j<3*n/4 && k>=n/4 && k<3*n/4;
        bool shell=j<n/16 || j>=15*n/16 || k>=15*n/16 ||
                   ((i<n/16 || i>=15*n/16) && !port);
        bool opening=window_open && j>=3*n/8 && j<5*n/8 && k>=n/4 && k<3*n/4;
        bool partition=i>=n/2 && i<5*n/8 && !opening;
        mask[(k*n+j)*n+i]=(unsigned char)(shell || partition);
    }
    /* The admitted fixed topology has one connected region with an open window,
     * or two regions each anchored by its own external port when closed. */
    s->rig_mask=mask;s->solid_box=true;return true;
}

bool cfd_open_atmosphere3d_rig_blocked(const CfdOpenAtmosphere3d *s,int a,int i,int j,int k) {
    int p[3]={i,j,k},lo[3]={i,j,k};lo[a]--;
    for(int side=0;side<2;side++) {
        const int *v=side?p:lo;
        if(v[0]>=0 && v[0]<s->grid.n[0] && v[1]>=0 && v[1]<s->grid.n[1] && v[2]>=0 && v[2]<s->grid.n[2] &&
           s->rig_mask[(v[2]*s->grid.n[1]+v[1])*s->grid.n[0]+v[0]])return true;
    }
    return false;
}

bool cfd_open_atmosphere3d_rig_drive(const CfdOpenAtmosphere3d *s,int i,int j,int k) {
    if(!s->rig_mask)return true;
    int n=s->grid.n[0];
    return i<=n/8 && j>=n/4 && j<3*n/4 && k>=n/4 && k<3*n/4;
}

bool cfd_open_atmosphere3d_rig_valid(const CfdOpenAtmosphere3d *s) {
    if(!s->rig_mask)return true;
    const int *n=s->grid.n;
    for(int a=0;a<3;a++)for(int k=0;k<n[2]+(a==2);k++)
    for(int j=0;j<n[1]+(a==1);j++)for(int i=0;i<n[0]+(a==0);i++)
        if(cfd_open_atmosphere3d_rig_blocked(s,a,i,j,k) &&
           s->velocity[cfd_open_atmosphere3d_face(s,a,i,j,k)]!=0)return false;
    return true;
}
