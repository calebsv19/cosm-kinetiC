/* Quadratic reconstruction of tangential, face-averaged velocity at a wall.
 * With wall coordinate zero and fluid layers [0,h], [h,2h], integrating a
 * quadratic gives ghost = 3*wall - 2.5*nearest + 0.5*next. Linear profiles
 * are reproduced too. Only the viscous stencil consumes this reconstruction.
 */
static bool viscous_wall_sample(const CfdOpenAtmosphere3d *s,int component,
    int axis,const int p[3],int side,double *value,bool *curved) {
    *curved=false;
    if(!s->viscous_wall_curvature || component==axis)return true;
    int neighbor[3]={p[0],p[1],p[2]};neighbor[axis]+=side;
    bool wall=cfd_open_atmosphere3d_blocked(s,component,neighbor[0],neighbor[1],neighbor[2]);
    double wall_velocity=0;
    if(axis==2 && side<0 && p[2]==0 && s->ground)wall=true;
#ifdef PHYSICS_SIM_OPEN_NO_SLIP_CEILING_REFERENCE
    if(axis==2 && side>0 && p[2]==s->grid.n[2]-1) {
        wall=true;wall_velocity=physics_sim_gap_ceiling_velocity(component);
    }
#endif
    if(!wall)return true;
    int inward[3]={p[0],p[1],p[2]};inward[axis]-=side;
    if(inward[axis]<0 || inward[axis]>=s->grid.n[axis] ||
       cfd_open_atmosphere3d_blocked(s,component,inward[0],inward[1],inward[2])) {
        return false;
    }
    double nearest=sample(s,s->velocity,component,p[0],p[1],p[2]);
    double next=sample(s,s->velocity,component,inward[0],inward[1],inward[2]);
    *value=3*wall_velocity-2.5*nearest+.5*next;
    *curved=true;
    return isfinite(*value);
}
