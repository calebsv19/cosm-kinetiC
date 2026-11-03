/* Role-separated opt-in inlet, included by the native momentum owner so
 * existing explicit-source tool builders retain their ordinary link contract. */
static bool open_momentum_admit(const CfdOpenAtmosphere3d *s,const double *j,size_t count) {
    if(!s || !j || !s->storage || count!=(size_t)s->velocity_count || !s->ground ||
       s->lateral_open || s->solid_box || s->rig_mask || s->wind.enabled ||
       s->wind.quiescent_backflow || s->weather.enabled)return false;
    for(int q=0;q<s->velocity_count;q++) {
        if(!isfinite(j[q]))return false;
        if(j[q]==0)continue;
        if(q>=3*s->grid.count)return false;
        int cell=q%s->grid.count,i=cell%s->grid.n[0],
            k=cell/s->plane,y=(cell/s->grid.n[0])%s->grid.n[1];
        if(i<1 || i>=s->grid.n[0]-1 || y<1 || y>=s->grid.n[1]-1 ||
           k<1 || k>=s->grid.n[2]-1)return false;
    }
    return true;
}
static void open_momentum_vector(const CfdOpenAtmosphere3d *s,const double *v,double p[3]) {
    p[0]=p[1]=p[2]=0;
    for(int a=0;a<3;a++)for(int k=0;k<s->grid.n[2]+(a==2);k++)
    for(int y=0;y<s->grid.n[1];y++)for(int i=0;i<s->grid.n[0];i++) {
        int q=cfd_open_atmosphere3d_face(s,a,i,y,k);
        p[a]+=cfd_open_face_mass(s,a,i,y,k)*v[q];
    }
}
static bool open_momentum_apply(CfdOpenAtmosphere3d *s,const double *j,CfdOpenMomentumReceipt *r) {
    s->step_rejection="external momentum impulse/CFL";
    open_momentum_vector(s,s->candidate_velocity,r->predictor_momentum_ns);
    for(int a=0;a<3;a++)for(int k=0;k<s->grid.n[2]+(a==2);k++)
    for(int y=0;y<s->grid.n[1];y++)for(int i=0;i<s->grid.n[0];i++) {
        int q=cfd_open_atmosphere3d_face(s,a,i,y,k);
        double mass=cfd_open_face_mass(s,a,i,y,k),dv=j[q]/mass,old=s->candidate_velocity[q];
        if(!isfinite(dv) || !isfinite(old+dv))return false;
        r->impulse_ns[a]+=j[q];r->work_j+=j[q]*(old+.5*dv);
        r->quadratic_j+=.5*j[q]*dv;s->candidate_velocity[q]=old+dv;
    }
    double maximum=0;
    for(int cell=0;cell<s->grid.count;cell++) {
        int i=cell%s->grid.n[0],y=(cell/s->grid.n[0])%s->grid.n[1],k=cell/s->plane;
        double rate=0;
        for(int a=0;a<3;a++) {
            double lo=s->candidate_velocity[cfd_open_atmosphere3d_face(s,a,i,y,k)];
            int hi[3]={i,y,k};hi[a]++;
            if(a<2)hi[a]%=s->grid.n[a];
            double upper=s->candidate_velocity[cfd_open_atmosphere3d_face(s,a,hi[0],hi[1],hi[2])];
            rate+=fmax(fabs(lo),fabs(upper))/s->grid.h[a]+2*s->mu/s->rho/(s->grid.h[a]*s->grid.h[a]);
        }
        maximum=fmax(maximum,rate);
    }
    r->forced_courant=s->dt*maximum;
    if(!isfinite(r->work_j) || !isfinite(r->quadratic_j) ||
       !isfinite(r->forced_courant) || r->forced_courant>(s->muscl?.4:.8))return false;
    open_momentum_vector(s,s->candidate_velocity,r->forced_momentum_ns);
    r->forced_kinetic_j=cfd_open_kinetic(s,s->candidate_velocity);
    return isfinite(r->forced_kinetic_j);
}
