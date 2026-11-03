/* Opt-in fixed-rig spatial gust. Kept separate from the ordinary port driver.
 * The configuration digest binds flow_profile; native restore binds the worker.
 * This is distributed upstream X momentum forcing, not a boundary inlet or a
 * turbulence model. Pressure projection/solid faces remain ordinary authority. */
static bool weather_drive(const CfdOpenAtmosphere3d *s,
    const CfdOpenAtmosphereWeather *w,int i,int j,int k) {
    if(!w->broad_sweeping_shear)return cfd_open_atmosphere3d_rig_drive(s,i,j,k);
    if(!s->rig_mask)return false;
    double x=i*s->grid.h[0],y=(j+.5)*s->grid.h[1],z=(k+.5)*s->grid.h[2];
    return x<=.4375 && y>=.0625 && y<.9375 && z>=.0625 && z<.9375;
}
static double weather_goal(const CfdOpenAtmosphere3d *s,
    const CfdOpenAtmosphereWeather *w,double target,int j,int k) {
    double goal=target*fmin(1,(k+.5)*s->grid.h[2]/w->ramp_height_m);
    if(!w->broad_sweeping_shear)return goal;
    const double tau=6.283185307179586476925286766559;
    double t=s->steps*s->dt,y=(j+.5)*s->grid.h[1],z=(k+.5)*s->grid.h[2];
    double yc=.5+.18*sin(tau*t/3),zc=.5+.12*sin(tau*t/4);
    /* Broad positive envelope retains commanded direction everywhere, while
     * moving lateral/vertical gradients produce changing projected shear. */
    return goal*(.65+.35*cos(tau*(y-yc))*cos(tau*(z-zc)));
}
