#ifndef PHYSICS_SIM_CFD_OPEN_ENERGY_H
#define PHYSICS_SIM_CFD_OPEN_ENERGY_H
#include "app/cfd_open_atmosphere3d.h"
/* Discrete MAC kinetic energy: fluid face dual volumes, half weight on
 * physical normal boundary faces. Blocked faces carry exactly zero velocity.
 * Entries are signed joules, not dissipation or thermodynamic heat receipts. */
enum { KE_INITIAL, KE_ADV_BOUNDARY, KE_ADV_INTERIOR, KE_VISCOSITY,
       KE_BUOYANCY, KE_EULER, KE_INLET, KE_PROJECTION, KE_CLOSURE, KE_ROUNDING };
static inline double cfd_open_face_mass(const CfdOpenAtmosphere3d *s,int a,int i,int j,int k) {
    int p[3]={i,j,k};
    double w=((a==2 || s->lateral_open) && (p[a]==0 || p[a]==s->grid.n[a]))?.5:1.;
    return s->rho*s->grid.volume*w;
}
static inline double cfd_open_kinetic(const CfdOpenAtmosphere3d *s,const double *v) {
    double total=0;
    for(int a=0;a<3;a++)for(int k=0;k<s->grid.n[2]+(a==2);k++)
    for(int j=0;j<s->grid.n[1]+(s->lateral_open && a==1);j++)
    for(int i=0;i<s->grid.n[0]+(s->lateral_open && a==0);i++) {
        double u=v[cfd_open_atmosphere3d_face(s,a,i,j,k)];
        total+=.5*cfd_open_face_mass(s,a,i,j,k)*u*u;
    }
    return total;
}
static inline bool cfd_open_dual_reservoir(const CfdOpenAtmosphere3d *s,int a,int b,int coordinate) {
    int upper=s->grid.n[b]-(a!=b);
    int side=coordinate<0?-1:coordinate>upper?1:0;
    return side && (s->lateral_open || b==2) &&
        !(b==2 && side<0 && s->ground) && !cfd_open_wind_neumann(s,b,side);
}
#endif
