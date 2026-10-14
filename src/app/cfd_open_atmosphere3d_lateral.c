/* Optional non-periodic lateral ambient reservoirs. Legacy storage/layout is unchanged. */
#include "app/cfd_open_atmosphere3d.h"
#include <string.h>

int cfd_open_atmosphere3d_face(const CfdOpenAtmosphere3d *s,int a,int i,int j,int k) {
    const int *n=s->grid.n;int count=s->grid.count;
    if(s->lateral_open && a==0 && i==n[0])return 3*count+s->plane+k*n[1]+j;
    if(s->lateral_open && a==1 && j==n[1])return 3*count+s->plane+n[1]*n[2]+k*n[0]+i;
    return a*count+(k*n[1]+j)*n[0]+i;
}

bool cfd_open_atmosphere3d_lateral(CfdOpenAtmosphere3d *s) {
    if(!s || !s->storage || s->steps || s->lateral_open || s->solid_box || !s->ground)return false;
    int n=s->grid.count,extra=s->grid.n[1]*s->grid.n[2]+s->grid.n[0]*s->grid.n[2];
    int velocities=3*n+s->plane+extra,faces=2*(s->plane+extra);
    double *storage=cfd_memory_calloc((size_t)2*velocities+9*n+8*faces,sizeof(double));
    if(!storage)return false;
    double *work=cfd_memory_calloc((size_t)5*n,sizeof(double));
    if(!work){cfd_memory_free(storage);return false;}
    cfd_memory_free(s->storage);s->storage=storage;s->solid_work=work;
    s->velocity_count=velocities;s->boundary_faces=faces;s->lateral_open=true;
    s->velocity=storage;s->pressure=s->velocity+velocities;s->energy=s->pressure+n;s->smoke=s->energy+n;
    s->candidate_velocity=s->smoke+n;s->candidate_pressure=s->candidate_velocity+velocities;s->rhs=s->candidate_pressure+n;
    s->scalar_candidate=s->rhs+n;s->scalar_work=s->scalar_candidate+2*n;s->flux=s->scalar_work+2*n;s->candidate_flux=s->flux+4*faces;
    return true;
}
