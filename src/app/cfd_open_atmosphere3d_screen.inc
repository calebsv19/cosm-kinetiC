/* One-dimensional pressure/through-flow screen proof. No legacy admission edit. */
static bool open_screen_admit(const CfdOpenAtmosphere3d *s,const double *ej,
    const double *kg,const double *j,size_t count) {
    if(!s || !ej || !kg || !j || !s->storage || !cfd_open_atmosphere3d_valid(s) ||
       count!=(size_t)s->velocity_count || s->ground || s->lateral_open ||
       s->solid_box || s->rig_mask || s->wind.enabled || s->wind.quiescent_backflow ||
       s->weather.enabled || s->buoyancy || s->muscl || s->scalar_mc || s->scalar_rk2 ||
       s->steps>=20 || s->dt!=.005 || s->rho!=1.2 || s->mu!=1.8e-5 ||
       s->cp!=1005 || s->reference_k!=293.15 || s->conductivity!=0 || s->diffusivity!=0 ||
       s->pressure_boundary[1]!=0 || !isfinite(s->pressure_boundary[0]) ||
       fabs(s->pressure_boundary[0])>.4)return false;
    for(int a=0;a<2;a++)if(s->ambient_k[a]!=293.15 || s->ambient_smoke[a]!=0)return false;
    int n=s->grid.n[0];
    if((n!=8 && n!=16) || s->grid.n[1]!=n || s->grid.n[2]!=n)return false;
    for(int a=0;a<3;a++)if(s->grid.length[a]!=1)return false;
    double flow=s->velocity[2*s->grid.count],absolute=0,work=0;
    if(!isfinite(flow) || fabs(flow)>2)return false;
    for(int q=0;q<s->grid.count;q++) {
        if(ej[q]!=0 || kg[q]!=0 || s->energy[q]!=0 || s->smoke[q]!=0 ||
           fabs(s->velocity[q])>1e-10 || fabs(s->velocity[s->grid.count+q])>1e-10 ||
           j[q]!=0 || j[s->grid.count+q]!=0)return false;
    }
    for(int k=0;k<=n;k++) {
        double plane_j=j[2*s->grid.count+k*s->plane];
        if(!isfinite(plane_j) || ((k==0 || k==n) && plane_j!=0) ||
           plane_j*flow>0 || (flow==0 && plane_j!=0))return false;
        for(int q=0;q<s->plane;q++) {
            int face=2*s->grid.count+k*s->plane+q;
            if(!isfinite(s->velocity[face]) || fabs(s->velocity[face]-flow)>1e-10 ||
               j[face]!=plane_j)return false;
            double mass=cfd_open_face_mass(s,2,q%n,q/n,k);
            absolute+=fabs(j[face]);work+=j[face]*(s->velocity[face]+.5*j[face]/mass);
        }
    }
    return absolute<=.002+1e-14 && isfinite(work) && work<=1e-16;
}
bool cfd_open_atmosphere3d_step_screen_momentum(CfdOpenAtmosphere3d *s,
    const double *ej,const double *kg,const double *impulse,size_t count,
    CfdOpenMomentumReceipt *receipt) {
    if(!receipt || !open_screen_admit(s,ej,kg,impulse,count))return false;
    CfdOpenMomentumReceipt trial={0};
    if(!open_step(s,ej,kg,impulse,&trial))return false;
    open_momentum_vector(s,s->velocity,trial.projected_momentum_ns);
    trial.projection_work_j=cfd_open_kinetic(s,s->velocity)-trial.forced_kinetic_j;
    *receipt=trial;return true;
}
