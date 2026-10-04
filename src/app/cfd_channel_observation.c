#include "app/cfd_channel_observation.h"
#include <math.h>
#include <string.h>
static void number(struct json_object *o,const char *k,double v) { json_object_object_add(o,k,json_object_new_double(v)); }
static void integer(struct json_object *o,const char *k,int v) { json_object_object_add(o,k,json_object_new_int(v)); }
static void text(struct json_object *o,const char *k,const char *v) { json_object_object_add(o,k,json_object_new_string(v)); }
static struct json_object *get(struct json_object *o,const char *k) { struct json_object *v=NULL;json_object_object_get_ex(o,k,&v);return v; }
static struct json_object *array(const double *v,int n) { struct json_object *a=json_object_new_array();for(int i=0;i<n;i++)json_object_array_add(a,json_object_new_double(v[i]));return a; }
static void values(const CfdChannel *c,const double p[3],double v[11]) {
    double u=cfd_channel_velocity(c,p[1]),tau=cfd_channel_shear(c,p[1]);
    double a[11]={fabs(u),0,0,u,0,0,0,0,fabs(tau/c->mu),c->gradient*(c->length-p[0]),tau};
    memcpy(v,a,sizeof(a));
}
struct json_object *cfd_channel_sample(const CfdChannel *c,struct json_object *req) {
    const char *fields[]={"speed","dye","solid","vx","vy","vz","pressure_proxy","divergence","vorticity","pressure_pa","shear_stress_pa"};
    const char *units[]={"m/s","unused","mask","m/s","m/s","m/s","unavailable","1/s","1/s","Pa","Pa"};
    const char *plane=req?json_object_get_string(get(req,"plane")):"XY";
    if(!plane)plane="XY";
    int u=0,v=1,n=2;
    if(!strcmp(plane,"XZ")){v=2;n=1;}else if(!strcmp(plane,"YZ")){u=1;v=2;n=0;}
    int res=req?json_object_get_int(get(req,"resolution")):32;
    if(res<4)res=4;if(res>64)res=64;
    double position=req?json_object_get_double(get(req,"position")):.5;
    double dims[3]={c->length,c->height,c->width},bounds[6]={0,0,0,c->length,c->height,c->width};
    struct json_object *o=json_object_new_object(),*samples=json_object_new_array(),*stats=json_object_new_object();
    double low[11],high[11],sum[11]={0};for(int k=0;k<11;k++){low[k]=INFINITY;high[k]=-INFINITY;}
    for(int j=0;j<res;j++)for(int i=0;i<res;i++) {
        double p[3]={0},val[11];p[n]=position*dims[n];p[u]=(i+.5)/res*dims[u];p[v]=(j+.5)/res*dims[v];
        values(c,p,val);
        struct json_object *cell=array(val,req?11:3);
        if(req)json_object_array_put_idx(cell,6,NULL);
        json_object_array_add(samples,cell);
        for(int k=0;k<11;k++){low[k]=fmin(low[k],val[k]);high[k]=fmax(high[k],val[k]);sum[k]+=val[k];}
    }
    struct json_object *names=json_object_new_array(),*unit=json_object_new_array();
    for(int k=0;k<(req?11:3);k++) {
        json_object_array_add(names,json_object_new_string(fields[k]));json_object_array_add(unit,json_object_new_string(units[k]));
        struct json_object *s=json_object_new_object();number(s,"min",low[k]);number(s,"max",high[k]);number(s,"mean",sum[k]/(res*res));number(s,"finite_samples",res*res);
        json_object_object_add(stats,fields[k],k==6?NULL:s);if(k==6)json_object_put(s);
    }
    json_object_object_add(o,"fields",names);json_object_object_add(o,"units",unit);json_object_object_add(o,"statistics",stats);
    json_object_object_add(o,"samples",samples);integer(o,"width",res);integer(o,"height",res);
    text(o,"plane",plane);integer(o,"normal_axis",n);integer(o,"u_axis",u);integer(o,"v_axis",v);
    number(o,"slice_world_m",position*dims[n]);number(o,"extent_u_m",dims[u]);number(o,"extent_v_m",dims[v]);
    number(o,"speed_max",high[0]);integer(o,"sampled_nonfinite_cells",0);
    text(o,"sampling","reduced fully developed field; linear y reconstruction; affine imposed pressure, not a 3D pressure solve");
    json_object_object_add(o,"world_bounds_m",array(bounds,6));
    struct json_object *grid=json_object_new_array();
    json_object_array_add(grid,json_object_new_int(1));json_object_array_add(grid,json_object_new_int(c->n));json_object_array_add(grid,json_object_new_int(1));
    json_object_object_add(o,"grid",grid);
    struct json_object *probes=json_object_new_array(),*points=req?get(req,"points"):NULL;
    for(size_t i=0;i<(points?json_object_array_length(points):0);i++) {
        struct json_object *point=json_object_array_get_idx(points,i),*probe=json_object_new_object();double p[3],val[11];bool inside=true;
        for(int a=0;a<3;a++){p[a]=json_object_get_double(json_object_array_get_idx(point,a));inside &= p[a]>=0&&p[a]<=dims[a];}
        json_object_object_add(probe,"requested_world_m",array(p,3));json_object_object_add(probe,"inside",json_object_new_boolean(inside));text(probe,"status",inside?"ok":"outside_domain");
        if(inside){values(c,p,val);struct json_object *a=array(val,11);json_object_array_put_idx(a,6,NULL);json_object_object_add(probe,"values",a);}
        json_object_array_add(probes,probe);
    }
    json_object_object_add(o,"probes",probes);return o;
}
void cfd_channel_snapshot(const CfdChannel *c,struct json_object *out) {
    text(out,"model","incompressible_channel_fv_v1");
    text(out,"model_limitations","Reduced fully developed laminar u(y,t); periodic x/z velocity, imposed affine pressure. No inlet development, obstacles, turbulence or general 3D pressure-velocity solve.");
    struct json_object *grid=json_object_new_array();
    json_object_array_add(grid,json_object_new_int(1));json_object_array_add(grid,json_object_new_int(c->n));json_object_array_add(grid,json_object_new_int(1));
    json_object_object_add(out,"effective_grid",grid);
    number(out,"voxel_size_m",c->height/c->n);integer(out,"estimated_dense_bytes",sizeof(*c));
    json_object_object_add(out,"geometry",json_object_new_array());
    struct json_object *p=json_object_new_object(),*h=json_object_new_object();
    double dimensions[3]={c->length,c->height,c->width};
    json_object_object_add(p,"dimensions_m",array(dimensions,3));
    text(p,"spatial_discretization","one wall-normal finite-volume column; shared streamwise face flux repeated in invariant x/z directions");
    number(p,"density_kg_m3",c->rho);number(p,"dynamic_viscosity_pa_s",c->mu);number(p,"kinematic_viscosity_m2_s",c->mu/c->rho);
    number(p,"characteristic_reynolds_number",c->rho*c->height*(fmax(fabs(c->wall_bottom),fabs(c->wall_top))+fabs(c->gradient)*c->height*c->height/(8*c->mu))/c->mu);
    number(p,"pressure_gradient_pa_m",c->gradient);number(p,"pressure_drop_pa",c->gradient*c->length);
    text(p,"pressure_semantics","G=-dp/dx prescribed; p(L)=0 gauge; pressure drop is an input, not solved by projection");
    text(p,"boundary_model","no-slip walls at y=0,H; prescribed tangential wall velocities; fully developed periodic x/z with imposed mean pressure gradient");
    text(p,"viscosity_operator","finite-volume shared shear fluxes; half-cell wall distance; backward Euler tridiagonal solve");
    number(p,"wall_bottom_velocity_m_s",c->wall_bottom);number(p,"wall_top_velocity_m_s",c->wall_top);
    number(p,"wall_on_fluid_bottom_shear_pa",-c->tau[0]);number(p,"wall_on_fluid_top_shear_pa",c->tau[c->n]);
    text(p,"wall_shear_sign","streamwise traction exerted by wall on fluid; fluid-on-wall force has opposite sign");
    json_object_object_add(p,"drag_coefficient",NULL);text(p,"drag_status","not applicable to this parallel-wall model");
    struct json_object *profile=json_object_new_array(),*faces=json_object_new_array();double dy=c->height/c->n;
    for(int j=0;j<c->n;j++){double a[2]={(j+.5)*dy,c->u[j]};json_object_array_add(profile,array(a,2));}
    for(int j=0;j<=c->n;j++){double a[2]={j*dy,c->tau[j]};json_object_array_add(faces,array(a,2));}
    json_object_object_add(p,"profile_y_m_vx_m_s",profile);json_object_object_add(p,"face_y_m_tau_xy_pa",faces);
    number(h,"volume_flux_m3_s",c->volume_flux);number(h,"mass_flux_kg_s",c->rho*c->volume_flux);
    double fluxes[6]={-c->volume_flux,c->volume_flux,0,0,0,0};json_object_object_add(h,"outward_face_volume_flux_m3_s",array(fluxes,6));
    number(h,"mass_imbalance_kg_s",0);text(h,"continuity_scope","identically satisfied by fully developed reduction, not a pressure-solver convergence result");
    number(h,"max_acceleration_m_s2",c->max_acceleration);number(h,"momentum_balance_residual_n",c->momentum_residual);
    number(h,"local_momentum_residual_pa_m",c->local_residual);
    if(c->time>0)number(h,"pressure_drop_from_momentum_pa",c->recovered_pressure_drop);else json_object_object_add(h,"pressure_drop_from_momentum_pa",NULL);
    number(h,"kinetic_energy_j",c->kinetic_energy);number(h,"pressure_power_w",c->pressure_power);number(h,"wall_power_w",c->wall_power);
    number(h,"viscous_dissipation_w",c->dissipation);number(h,"backward_euler_dissipation_w",c->temporal_dissipation);number(h,"energy_balance_residual_w",c->energy_residual);
    text(h,"steady_status",c->time==0?"not_started":"not_certified; inspect acceleration and time-window criteria");
    json_object_object_add(out,"physics",p);json_object_object_add(out,"health",h);
}
