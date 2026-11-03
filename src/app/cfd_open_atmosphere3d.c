#include "app/cfd_open_atmosphere3d.h"
#include "app/cfd_passive3d.h"
#include "app/cfd_open_energy.h"
#include <math.h>
#include <string.h>
#include "cfd_open_atmosphere3d_momentum.inc"
#ifdef PHYSICS_SIM_OPEN_EDGE_CANDIDATE
#include "cfd_scalar_edge_candidate.h"
#endif
#ifdef PHYSICS_SIM_OPEN_MOMENTUM_DIAGNOSTIC
#include "app/cfd_open_momentum_diagnostics.h"
#endif
/* Only the contained full-grid numerical qualifier opts into this capacity.
 * Ordinary app/worker builds retain the existing admission limit. No solver
 * operator, tolerance or allocation layout depends on this build profile. */
#if defined(PHYSICS_SIM_OPEN_VERTICAL_REFERENCE) && !defined(PHYSICS_SIM_OPEN_FULL_GRID_QUALIFIER)
#error Vertical reference capacity requires the contained full-grid qualifier
#endif
#if defined(PHYSICS_SIM_OPEN_HORIZONTAL_REFERENCE) && !defined(PHYSICS_SIM_OPEN_FULL_GRID_QUALIFIER)
#error Horizontal capacity requires contained full-grid qualification
#endif
#ifdef PHYSICS_SIM_OPEN_HORIZONTAL_REFERENCE
#define OPEN_MAX_CELLS 8388608
#elif defined(PHYSICS_SIM_OPEN_VERTICAL_REFERENCE)
#define OPEN_MAX_CELLS 4194304
#elif defined(PHYSICS_SIM_OPEN_FULL_GRID_QUALIFIER)
#define OPEN_MAX_CELLS 2097152
#else
#define OPEN_MAX_CELLS 524288
#endif
static int wrap(int a,int n) { a%=n;return a<0?a+n:a; }
static double sample(const CfdOpenAtmosphere3d *s,const double *v,int a,int i,int j,int k) {
    const int *n=s->grid.n;
    if(s->lateral_open) {
        i=i<0?0:i>n[0]-(a!=0)?n[0]-(a!=0):i;
        j=j<0?0:j>n[1]-(a!=1)?n[1]-(a!=1):j;
    } else {i=wrap(i,n[0]);j=wrap(j,n[1]);}
    if(a==2) { if(k<0){if(s->ground)return -sample(s,v,a,i,j,-k);k=-k;}if(k>n[2])k=2*n[2]-k; }
    else { if(k<0){if(s->ground)return -sample(s,v,a,i,j,-k-1);k=0;}if(k>=n[2])k=n[2]-1; }
    return v[cfd_open_atmosphere3d_face(s,a,i,j,k)];
}
static double minmod(double a,double b) {
    return a*b>0?copysign(fmin(fabs(a),fabs(b)),a):0;
}
static double velocity_slope(const CfdOpenAtmosphere3d *s,int a,int b,int i,int j,int k) {
    int lo[3]={i,j,k},hi[3]={i,j,k};lo[b]--;hi[b]++;
    double c=sample(s,s->velocity,a,i,j,k);
    return minmod(c-sample(s,s->velocity,a,lo[0],lo[1],lo[2]),sample(s,s->velocity,a,hi[0],hi[1],hi[2])-c);
}
/* Upper face of the velocity-component dual cell. The same face flux is
 * evaluated by its two neighbours, so nonlinear advection telescopes rather
 * than creating momentum when limited slopes vary. Interpolating the MAC
 * normal velocity here preserves the dual-cell divergence constraint. */
static double momentum_flux(const CfdOpenAtmosphere3d *s,int a,int b,const int p[3],double *speed) {
    int hi[3]={p[0],p[1],p[2]};hi[b]++;
    double u;
    if(a==b)u=.5*(sample(s,s->velocity,b,p[0],p[1],p[2])+sample(s,s->velocity,b,hi[0],hi[1],hi[2]));
    else {
        int lo[3]={hi[0],hi[1],hi[2]};lo[a]--;
        u=.5*(sample(s,s->velocity,b,hi[0],hi[1],hi[2])+sample(s,s->velocity,b,lo[0],lo[1],lo[2]));
    }
    const int *donor=u>=0?p:hi;
    double value=sample(s,s->velocity,a,donor[0],donor[1],donor[2]);
    if(s->muscl)value+=(u>=0?.5:-.5)*velocity_slope(s,a,b,donor[0],donor[1],donor[2]);
    if(s->wind.quiescent_backflow && cfd_open_dual_reservoir(s,a,b,donor[b]))value=0;
    *speed=u;return u*value;
}
static double scalar_slope(const CfdOpenAtmosphere3d *s,const double *v,int q,int a) {
    const CfdCartesian3d *g=&s->grid;
    if(a==2 && (q<s->plane || q>=(g->n[2]-1)*s->plane))return 0;
    int stride=(int[]){1,g->n[0],s->plane}[a], coord=(q/stride)%g->n[a];
    if(s->lateral_open && a<2 && (coord==0 || coord==g->n[a]-1))return 0;
    int lo=a==2?q-s->plane:cfd_cartesian3d_neighbor(g,q,a,-1);
    int hi=a==2?q+s->plane:cfd_cartesian3d_neighbor(g,q,a,1);
    if(cfd_open_atmosphere3d_solid(s,lo) || cfd_open_atmosphere3d_solid(s,hi))return 0;
    double left=v[q]-v[lo],right=v[hi]-v[q];
    /* MC retains the centered slope on smooth data while limiting interface
     * states between neighbouring values. Opt-in: do not silently invalidate
     * retained plume references or change momentum reconstruction. */
    if(s->scalar_mc)return minmod(.5*(left+right),minmod(2*left,2*right));
    return minmod(left,right);
}
static double divergence(const CfdOpenAtmosphere3d *s,const double *v,int q) {
    const CfdCartesian3d *g=&s->grid;int i=q%g->n[0],j=(q/g->n[0])%g->n[1],k=q/s->plane;
    return (sample(s,v,0,i+1,j,k)-sample(s,v,0,i,j,k))/g->h[0]+
        (sample(s,v,1,i,j+1,k)-sample(s,v,1,i,j,k))/g->h[1]+
        (sample(s,v,2,i,j,k+1)-sample(s,v,2,i,j,k))/g->h[2];
}
void cfd_open_atmosphere3d_destroy(CfdOpenAtmosphere3d *s) { if(s){cfd_cartesian3d_linear_destroy(s->projection);s->projection=NULL;cfd_sparse_mg_destroy(s->solid_projection);s->solid_projection=NULL;cfd_memory_free(s->solid_row);s->solid_row=NULL;cfd_memory_free(s->solid_col);s->solid_col=NULL;cfd_memory_free(s->solid_values);s->solid_values=NULL;cfd_memory_free(s->storage);s->storage=NULL;cfd_memory_free(s->solid_work);s->solid_work=NULL;cfd_memory_free(s->rig_mask);s->rig_mask=NULL;} }
bool cfd_open_atmosphere3d_init(CfdOpenAtmosphere3d *s,const int n[3],const double length[3],
    double rho,double mu,double cp,double ref,double conductivity,double diffusivity,double dt,
    const double pressure[2],const double ambient[2],const double smoke[2],bool buoyancy,double gravity,double beta,double contrast) {
    if(!s || !pressure || !ambient || !smoke)return false;
    memset(s,0,sizeof(*s));
    if(!cfd_cartesian3d_init(&s->grid,n,length) || s->grid.count>OPEN_MAX_CELLS ||
        !isfinite(rho) || rho<=0 || !isfinite(mu) || mu<=0 || !isfinite(cp) || cp<=0 ||
        !isfinite(ref) || ref<=0 || !isfinite(conductivity) || conductivity<0 || !isfinite(diffusivity) || diffusivity<0 ||
        !isfinite(dt) || dt<=0 || dt>.1 || !isfinite(gravity) || gravity<0 || gravity>100 ||
        !isfinite(beta) || beta<0 || beta>1/ref || !isfinite(contrast) || contrast<=0 || contrast>.1)return false;
    for(int a=0;a<2;a++)if(!isfinite(pressure[a]) || !isfinite(ambient[a]) || ambient[a]<ref ||
        !isfinite(smoke[a]) || smoke[a]<0 || (buoyancy && (ambient[a]-ref)/ref>contrast))return false;
    s->rho=rho;s->mu=mu;s->cp=cp;s->reference_k=ref;s->conductivity=conductivity;s->diffusivity=diffusivity;s->dt=dt;
    s->max_scalar_work_cells=100000000;
    s->max_scalar_cycles=4096;
    s->buoyancy=buoyancy;s->gravity=gravity;s->beta=beta;s->max_contrast=contrast;
    memcpy(s->pressure_boundary,pressure,2*sizeof(double));memcpy(s->ambient_k,ambient,2*sizeof(double));memcpy(s->ambient_smoke,smoke,2*sizeof(double));
    s->plane=n[0]*n[1];s->boundary_faces=2*s->plane;s->velocity_count=3*s->grid.count+s->plane;
    int count=s->grid.count,faces=2*s->plane;
    s->storage=cfd_memory_calloc((size_t)2*s->velocity_count+9*count+8*faces,sizeof(double));if(!s->storage)return false;
    s->velocity=s->storage;s->pressure=s->velocity+s->velocity_count;s->energy=s->pressure+count;s->smoke=s->energy+count;
    s->candidate_velocity=s->smoke+count;s->candidate_pressure=s->candidate_velocity+s->velocity_count;s->rhs=s->candidate_pressure+count;
    s->scalar_candidate=s->rhs+count;s->scalar_work=s->scalar_candidate+2*count;s->flux=s->scalar_work+2*count;s->candidate_flux=s->flux+4*faces;
    return true;
}
bool cfd_open_atmosphere3d_valid(const CfdOpenAtmosphere3d *s) {
    if(!s || !s->storage || s->steps<0 || s->steps>10000 || !isfinite(s->time) ||
        fabs(s->time-s->steps*s->dt)>1e-12*fmax(1,s->time) || s->scalar_work_cells>s->max_scalar_work_cells)return false;
    if(s->wind.enabled) {
        if(!isfinite(s->wind_air_in) || !isfinite(s->wind_air_out) || !isfinite(s->wind_inlet_volume) ||
           s->wind_air_in<0 || s->wind_air_out<0 || s->wind_inlet_volume<0)return false;
        if(!cfd_open_wind_state_valid(s))return false;
    }
    if(s->wind.quiescent_backflow)for(int z=0;z<10;z++)if(!isfinite(s->kinetic_ledger[z]))return false;
    if(s->weather.enabled && !cfd_open_weather_state_valid(s,&s->weather,&s->weather_state))return false;
    if(s->ground) {
        for(int q=0;q<s->plane;q++)if(s->velocity[2*s->grid.count+q]!=0)return false;
        for(int b=0;b<4;b++)for(int q=0;q<s->plane;q++)if(s->flux[b*s->boundary_faces+q]!=0)return false;
    }
    if(s->solid_box) {
        for(int q=0;q<s->grid.count;q++)if(cfd_open_atmosphere3d_solid(s,q) && (s->energy[q]!=0 || s->smoke[q]!=0 || s->pressure[q]!=0))return false;
        for(int a=0;a<3;a++)for(int k=0;k<s->grid.n[2]+(a==2);k++)for(int j=0;j<s->grid.n[1];j++)for(int i=0;i<s->grid.n[0];i++)
            if(cfd_open_atmosphere3d_blocked(s,a,i,j,k) && s->velocity[a*s->grid.count+(k*s->grid.n[1]+j)*s->grid.n[0]+i]!=0)return false;
    }
    if(!cfd_open_atmosphere3d_rig_valid(s))return false;
    double capacity=s->rho*s->cp*s->grid.volume;
    if(!isfinite(capacity) || capacity<=0)return false;
    for(int q=0;q<s->velocity_count;q++)if(!isfinite(s->velocity[q]))return false;
    for(int q=0;q<s->grid.count;q++)if(!isfinite(s->pressure[q]) || !isfinite(s->energy[q]) || s->energy[q]<0 ||
        !isfinite(s->smoke[q]) || s->smoke[q]<0 || !isfinite(divergence(s,s->velocity,q)) || fabs(divergence(s,s->velocity,q))>=1e-8 ||
        (s->buoyancy && s->energy[q]/capacity/s->reference_k>s->max_contrast))return false;
    for(int q=0;q<4*s->boundary_faces;q++)if(!isfinite(s->flux[q]) || s->flux[q]<0)return false;
    return true;
}
static bool momentum_forced(CfdOpenAtmosphere3d *s,double *maxdiv,double *residual,
    const double *impulse,CfdOpenMomentumReceipt *receipt) {
    s->step_rejection="momentum predictor finite values";
#ifdef PHYSICS_SIM_OPEN_MOMENTUM_DIAGNOSTIC
    cfd_open_momentum_diagnostics_begin();
    *residual=NAN;
#endif
    const CfdCartesian3d *g=&s->grid;int n=g->count;
    double maximum=0,nu=s->mu/s->rho;
    bool account=s->wind.quiescent_backflow;
    double *ledger=s->candidate_kinetic_ledger;
    if(s->weather.enabled)s->candidate_weather_work_j=s->weather_state.external_work_j;
    if(account) {
        memcpy(ledger,s->kinetic_ledger,sizeof(s->kinetic_ledger));
        if(s->steps==0)ledger[KE_INITIAL]=cfd_open_kinetic(s,s->velocity);
    }
    double before=account?cfd_open_kinetic(s,s->velocity):0;
    double work_before=0;
    if(account)for(int z=1;z<=KE_EULER;z++)work_before+=ledger[z];
    for(int a=0;a<3;a++)for(int k=0;k<g->n[2]+(a==2);k++)for(int j=0;j<g->n[1]+(s->lateral_open && a==1);j++)for(int i=0;i<g->n[0]+(s->lateral_open && a==0);i++) {
        int p[3]={i,j,k},q=cfd_open_atmosphere3d_face(s,a,i,j,k);
        if(cfd_open_atmosphere3d_blocked(s,a,i,j,k) || (s->ground && a==2 && k==0)){s->candidate_velocity[q]=0;continue;}
        double old=s->velocity[q],change=0,rate=0;
        for(int b=0;b<3;b++) {
            int lo[3]={p[0],p[1],p[2]},hi[3]={p[0],p[1],p[2]};lo[b]--;hi[b]++;
            double left=sample(s,s->velocity,a,lo[0],lo[1],lo[2]),right=sample(s,s->velocity,a,hi[0],hi[1],hi[2]),h=g->h[b];
            double lower_speed,upper_speed;
            double lower_flux=momentum_flux(s,a,b,lo,&lower_speed),upper_flux=momentum_flux(s,a,b,p,&upper_speed);
            /* Tangential solid ghosts impose no slip at the half-cell wall;
             * normal face unknowns are exactly zero. No scalar crosses this wall. */
            double dl=left,dr=right;
            if(a!=b && cfd_open_atmosphere3d_blocked(s,a,lo[0],lo[1],lo[2]))dl=-old;
            if(a!=b && cfd_open_atmosphere3d_blocked(s,a,hi[0],hi[1],hi[2]))dr=-old;
            change+=nu*(dl-2*old+dr)/(h*h)-(upper_flux-lower_flux)/h;
            if(account) {
                double factor=cfd_open_face_mass(s,a,i,j,k)*old*s->dt;
                double boundary=(cfd_open_dual_reservoir(s,a,b,hi[b])?-upper_flux/h:0)+
                    (cfd_open_dual_reservoir(s,a,b,lo[b])?lower_flux/h:0);
                ledger[KE_ADV_BOUNDARY]+=factor*boundary;
                ledger[KE_ADV_INTERIOR]+=factor*(-(upper_flux-lower_flux)/h-boundary);
                ledger[KE_VISCOSITY]+=factor*nu*(dl-2*old+dr)/(h*h);
            }
            rate+=(fmax(upper_speed,0)+fmax(-lower_speed,0))/h+2*nu/(h*h);
        }
        if(s->buoyancy && a==2) {
            int kl=k==0?0:k-1,kh=k==g->n[2]?k-1:k;
            int low=(kl*g->n[1]+j)*g->n[0]+i,high=(kh*g->n[1]+j)*g->n[0]+i;
            double delta=.5*(s->energy[low]+s->energy[high])/(s->rho*s->cp*g->volume);
            change+=s->gravity*s->beta*delta;
            if(account)ledger[KE_BUOYANCY]+=cfd_open_face_mass(s,a,i,j,k)*old*s->dt*s->gravity*s->beta*delta;
        }
#ifdef PHYSICS_SIM_OPEN_MOMENTUM_DIAGNOSTIC
        if(rate>maximum) {
            cfd_open_momentum_diagnostics.face_axis=a;
            cfd_open_momentum_diagnostics.face_i=i;
            cfd_open_momentum_diagnostics.face_j=j;
            cfd_open_momentum_diagnostics.face_k=k;
        }
        cfd_open_momentum_diagnostics.check="predictor_nonfinite";
#endif
        maximum=fmax(maximum,rate);s->candidate_velocity[q]=old+s->dt*change;
        if(account)ledger[KE_EULER]+=.5*cfd_open_face_mass(s,a,i,j,k)*(s->dt*change)*(s->dt*change);
        if(!isfinite(s->candidate_velocity[q]))return false;
    }
    if(s->wind.enabled)maximum=fmax(maximum,2*fabs(cfd_open_wind_command(s,s->steps))/g->h[0]);
    if(s->weather.enabled) {
        double target,strength;
        if(!cfd_open_weather_command(&s->weather,s->steps,&target,&strength))return false;
        if(strength>0)maximum=fmax(maximum,2*fabs(target)/g->h[0]);
    }
    s->momentum_courant=s->dt*maximum;
    s->step_rejection="momentum CFL";
#ifdef PHYSICS_SIM_OPEN_MOMENTUM_DIAGNOSTIC
    cfd_open_momentum_diagnostics.check="momentum_cfl";
    cfd_open_momentum_diagnostics.courant=s->momentum_courant;
    cfd_open_momentum_diagnostics.courant_limit=s->muscl?.4:.8;
#endif
    if(!isfinite(maximum) || s->dt*maximum>(s->muscl?.4:.8))return false;
    double predictor=account?cfd_open_kinetic(s,s->candidate_velocity):0;
    if(account) {
        double work=0;for(int z=1;z<=KE_EULER;z++)work+=ledger[z];
        ledger[KE_ROUNDING]+=predictor-before-(work-work_before);
    }
    cfd_open_wind_predictor(s);
    double imposed=account?cfd_open_kinetic(s,s->candidate_velocity):0;
    if(account)ledger[KE_INLET]+=imposed-predictor;
    if(s->weather.enabled) {
        double work;
        s->step_rejection="open weather forcing";
        if(!cfd_open_weather_apply(s,&s->weather,&work))return false;
        s->candidate_weather_work_j=s->weather_state.external_work_j+work;
        if(!isfinite(s->candidate_weather_work_j))return false;
        imposed=cfd_open_kinetic(s,s->candidate_velocity);
    }
    if(impulse && !open_momentum_apply(s,impulse,receipt))return false;
    for(int q=0;q<n;q++) {
        if(cfd_open_atmosphere3d_solid(s,q)){s->rhs[q]=0;continue;}
        int k=q/s->plane;s->rhs[q]=-s->rho/s->dt*divergence(s,s->candidate_velocity,q);
        if(k==0 && !s->ground)s->rhs[q]+=2*s->pressure_boundary[0]/(g->h[2]*g->h[2]);
        if(k==g->n[2]-1 && !cfd_open_wind_neumann(s,2,1))s->rhs[q]+=2*s->pressure_boundary[1]/(g->h[2]*g->h[2]);
    }
    if(s->wind.confined || s->wind.balanced)memset(s->candidate_pressure,0,(size_t)n*sizeof(double));
    else memcpy(s->candidate_pressure,s->pressure,(size_t)n*sizeof(double));
#ifdef PHYSICS_SIM_OPEN_MOMENTUM_DIAGNOSTIC
    cfd_open_momentum_diagnostics.pressure_attempted=1;
    cfd_open_momentum_diagnostics.check="solid_pressure_solve";
#endif
    if(s->solid_box) {
        s->step_rejection="solid pressure solve";
        if(!cfd_open_atmosphere3d_box_pressure(s,residual))return false;
        if((s->wind.confined || s->wind.balanced) && !cfd_open_wind_pressure_refine(s,NULL,residual))return false;
    } else {
    bool periodic[3]={!s->lateral_open,!s->lateral_open,false};bool neumann[6]={false,false,false,false,s->ground,false};
    for(int axis=0;axis<3;axis++)for(int side=0;side<2;side++)
        if(cfd_open_wind_neumann(s,axis,side?1:-1))neumann[2*axis+side]=true;
    if(s->projection && !s->cache_projection){cfd_cartesian3d_linear_destroy(s->projection);s->projection=NULL;}
#ifdef PHYSICS_SIM_OPEN_MOMENTUM_DIAGNOSTIC
    cfd_open_momentum_diagnostics.check="pressure_cache_boundary";
#endif
    if(s->projection && s->projection_ground!=s->ground)return false;
    s->step_rejection="pressure operator allocation";
    CfdCartesian3dLinear *solver=s->projection;
    if(!solver)solver=cfd_cartesian3d_linear_create_mixed(g,periodic,neumann,0,1,false);
    if(s->cache_projection && solver){s->projection=solver;s->projection_ground=s->ground;}
#ifdef PHYSICS_SIM_OPEN_MOMENTUM_DIAGNOSTIC
    cfd_open_momentum_diagnostics.check="pressure_solver_allocation";
#endif
    if(!solver)return false;
    if(s->wind.confined || s->wind.balanced)memset(s->candidate_pressure,0,(size_t)n*sizeof(double));
    else memcpy(s->candidate_pressure,s->pressure,(size_t)n*sizeof(double));int iterations;
#ifdef PHYSICS_SIM_OPEN_MOMENTUM_DIAGNOSTIC
    iterations=-1;
    cfd_open_momentum_diagnostics.check="pressure_solve";
#endif
    s->step_rejection="pressure linear solve";
    bool solved=cfd_cartesian3d_linear_solve(solver,s->rhs,s->candidate_pressure,&iterations,residual);
    if(solved && (s->wind.confined || s->wind.balanced))solved=cfd_open_wind_pressure_refine(s,solver,residual);
    if(!s->cache_projection)cfd_cartesian3d_linear_destroy(solver);
#ifdef PHYSICS_SIM_OPEN_MOMENTUM_DIAGNOSTIC
    cfd_open_momentum_diagnostics.pressure_iterations=iterations;
    cfd_open_momentum_diagnostics.pressure_residual=*residual;
    if(solved)cfd_open_momentum_diagnostics.linear_failure="none";
#endif
    if(!solved)return false;
    }
    for(int a=0;a<2;a++)for(int q=0;q<n;q++) {
        if(cfd_open_atmosphere3d_blocked(s,a,q%g->n[0],(q/g->n[0])%g->n[1],q/s->plane)){s->candidate_velocity[a*n+q]=0;continue;}
        int coord=(q/(a==0?1:g->n[0]))%g->n[a];
        if(coord==0 && cfd_open_wind_neumann(s,a,-1))continue;
        double grad=s->lateral_open && coord==0?2*s->candidate_pressure[q]/g->h[a]:
            (s->candidate_pressure[q]-s->candidate_pressure[cfd_cartesian3d_neighbor(g,q,a,-1)])/g->h[a];
        s->candidate_velocity[a*n+q]-=s->dt/s->rho*grad;
    }
    if(s->lateral_open) for(int a=0;a<2;a++)for(int q=0;q<n;q++) {
        int stride=a==0?1:g->n[0];
        if((q/stride)%g->n[a]!=g->n[a]-1 || cfd_open_wind_neumann(s,a,1))continue;
        int i=q%g->n[0],j=(q/g->n[0])%g->n[1],k=q/s->plane;
        int face=cfd_open_atmosphere3d_face(s,a,i+(a==0),j+(a==1),k);
        s->candidate_velocity[face]+=s->dt/s->rho*2*s->candidate_pressure[q]/g->h[a];
    }
    for(int k=0;k<=g->n[2];k++)for(int q=0;q<s->plane;q++) {
        if(cfd_open_atmosphere3d_blocked(s,2,q%g->n[0],q/g->n[0],k) || (s->ground && k==0)){s->candidate_velocity[2*n+k*s->plane+q]=0;continue;}
        if((k==0 && cfd_open_wind_neumann(s,2,-1)) || (k==g->n[2] && cfd_open_wind_neumann(s,2,1)))continue;
        double grad=k==0?2*(s->candidate_pressure[q]-s->pressure_boundary[0])/g->h[2]:
            k==g->n[2]?2*(s->pressure_boundary[1]-s->candidate_pressure[(k-1)*s->plane+q])/g->h[2]:
            (s->candidate_pressure[k*s->plane+q]-s->candidate_pressure[(k-1)*s->plane+q])/g->h[2];
        s->candidate_velocity[2*n+k*s->plane+q]-=s->dt/s->rho*grad;
    }
    double projected=account?cfd_open_kinetic(s,s->candidate_velocity):0;
    if(account)ledger[KE_PROJECTION]+=projected-imposed;
    s->step_rejection="boundary roundoff closure";
    if(!cfd_open_wind_roundoff_balance(s))return false;
    if(account)ledger[KE_CLOSURE]+=cfd_open_kinetic(s,s->candidate_velocity)-projected;
    *maxdiv=0;
    s->step_rejection="projected velocity divergence";
    for(int q=0;q<n;q++) { double d=divergence(s,s->candidate_velocity,q);
#ifdef PHYSICS_SIM_OPEN_MOMENTUM_DIAGNOSTIC
        cfd_open_momentum_diagnostics.check="projection_nonfinite_divergence";
        if(!isfinite(d) || fabs(d)>*maxdiv)cfd_open_momentum_diagnostics.divergence_cell=q;
#endif
        if(!isfinite(d))return false;*maxdiv=fmax(*maxdiv,fabs(d)); }
#ifdef PHYSICS_SIM_OPEN_MOMENTUM_DIAGNOSTIC
    cfd_open_momentum_diagnostics.check=*maxdiv<1e-8?"accepted_momentum":"projection_divergence";
    cfd_open_momentum_diagnostics.maximum_divergence=*maxdiv;
#endif
    return *maxdiv<1e-8;
}
static bool scalar_euler(CfdOpenAtmosphere3d *s,const double *old,double *out,const double *source,
    double dt,double fraction,double diffusivity,int quantity,double ledger_weight) {
    const CfdCartesian3d *g=&s->grid;int n=g->count,faces=s->boundary_faces;
    for(int q=0;q<n;q++)out[q]=old[q]+fraction*source[q];
    for(int a=0;a<3;a++)for(int q=0;q<n;q++) {
        if(a==2 && q<s->plane)continue;
        if(s->lateral_open && a<2 && (q/(a==0?1:g->n[0]))%g->n[a]==0)continue;
        int left=a==2?q-s->plane:cfd_cartesian3d_neighbor(g,q,a,-1);
        if(cfd_open_atmosphere3d_solid(s,q) || cfd_open_atmosphere3d_solid(s,left))continue;
        double u=s->candidate_velocity[a*n+q],h=g->h[a];
        double donor=u>=0?old[left]:old[q];
        if(s->muscl || s->scalar_mc)donor+=u>=0?.5*scalar_slope(s,old,left,a):-.5*scalar_slope(s,old,q,a);
        double flux=dt*(u*donor/h-diffusivity*(old[q]-old[left])/(h*h));
#ifdef PHYSICS_SIM_OPEN_EDGE_CANDIDATE
        if(a==0 && !s->lateral_open && !s->solid_box) {
            double gradient;
            cfd_scalar_edge_candidate(old[cfd_cartesian3d_neighbor(g,left,0,-1)],
                old[left],old[q],old[cfd_cartesian3d_neighbor(g,q,0,1)],u,h,&donor,&gradient);
            flux=dt*(u*donor-diffusivity*gradient)/h;
        }
#endif
        out[q]+=flux;out[left]-=flux;
    }
    for(int boundary=0;boundary<2;boundary++)for(int q=0;q<s->plane;q++) {
        if(s->ground && boundary==0)continue;
        int cell=boundary?(g->n[2]-1)*s->plane+q:q,face=boundary*g->n[2]*s->plane+q;
        double outward=(boundary?1:-1)*s->candidate_velocity[2*n+face];
        double ambient=quantity?s->ambient_smoke[boundary]*g->volume:s->rho*s->cp*(s->ambient_k[boundary]-s->reference_k)*g->volume;
        double amount=dt*fabs(outward)*(outward>=0?old[cell]:ambient)/g->h[2];
        if(outward>=0){out[cell]-=amount;s->candidate_flux[(2*quantity+1)*faces+boundary*s->plane+q]+=ledger_weight*amount;}
        else {out[cell]+=amount;s->candidate_flux[2*quantity*faces+boundary*s->plane+q]+=ledger_weight*amount;}
    }
    if(s->lateral_open) for(int a=0;a<2;a++)for(int side=0;side<2;side++)for(int q=0;q<n;q++) {
        int stride=a==0?1:g->n[0],coord=(q/stride)%g->n[a];
        if(coord!=(side?g->n[a]-1:0))continue;
        int i=q%g->n[0],j=(q/g->n[0])%g->n[1],k=q/s->plane;
        int face=cfd_open_atmosphere3d_face(s,a,i+(side && a==0),j+(side && a==1),k);
        double outward=(side?1:-1)*s->candidate_velocity[face];
        /* Lateral reservoir is reference-temperature clean air: zero excess scalars. */
        if(outward>=0) {
            double amount=dt*outward*old[q]/g->h[a];out[q]-=amount;
            int face_count=g->count/g->n[a];
            int offset=2*s->plane+(a==1?2*g->n[1]*g->n[2]:0)+side*face_count;
            int index=a==0?k*g->n[1]+j:k*g->n[0]+i;
            s->candidate_flux[(2*quantity+1)*faces+offset+index]+=ledger_weight*amount;
        }
    }
    for(int q=0;q<n;q++)if(!isfinite(out[q]) || out[q]<0)return false;
    return true;
}
/* Pressure RHS is disposable after projection. Reuse it for the second
 * Euler stage; accepted pressure/fields and both scalar input arrays remain
 * untouched. Stage boundary fluxes receive the same 1/2 weights as the state. */
static bool scalar(CfdOpenAtmosphere3d *s,const double *old,double *out,const double *source,
    double dt,double fraction,double diffusivity,int quantity) {
    if(!s->scalar_rk2)return scalar_euler(s,old,out,source,dt,fraction,diffusivity,quantity,1);
    if(!scalar_euler(s,old,out,source,dt,fraction,diffusivity,quantity,.5) ||
       !scalar_euler(s,out,s->rhs,source,dt,fraction,diffusivity,quantity,.5))return false;
    for(int q=0;q<s->grid.count;q++)out[q]=.5*old[q]+.5*s->rhs[q];
    return true;
}

static bool momentum(CfdOpenAtmosphere3d *s,double *maxdiv,double *residual) {
    return momentum_forced(s,maxdiv,residual,NULL,NULL);
}
static bool open_step(CfdOpenAtmosphere3d *s,const double *source_j,const double *source_kg,
    const double *impulse,CfdOpenMomentumReceipt *receipt) {
    if(s){s->step_rejection="state/source admission";s->scalar_work_requested=0;s->scalar_cycles_requested=0;s->momentum_courant=0;}
    if(!cfd_open_atmosphere3d_valid(s) || !source_j || !source_kg || s->steps>=10000)return false;
    int n=s->grid.count,faces=s->boundary_faces;
    for(int q=0;q<n;q++)if(!isfinite(source_j[q]) || source_j[q]<0 || !isfinite(source_kg[q]) || source_kg[q]<0)return false;
    for(int q=0;q<n;q++)if(cfd_open_atmosphere3d_solid(s,q) && (source_j[q]!=0 || source_kg[q]!=0))return false;
    s->step_rejection="momentum CFL/projection/divergence";
    double maxdiv=NAN,residual=NAN;bool momentum_ok;
    s->attempted_projection_residual=NAN;s->attempted_projection_divergence=NAN;
#ifdef PHYSICS_SIM_OPEN_PRESCRIBED_THERMAL_QUALIFIER
    /* Contained diagnostic wrapper supplies only momentum's thermal input.
     * Accepted scalar state and the ordinary app/worker operator are unchanged. */
    if(impulse)return false;
    momentum_ok=physics_sim_prescribed_momentum(s,&maxdiv,&residual);
#else
    momentum_ok=impulse?momentum_forced(s,&maxdiv,&residual,impulse,receipt):momentum(s,&maxdiv,&residual);
#endif
    s->attempted_projection_residual=residual;s->attempted_projection_divergence=maxdiv;
    if(!momentum_ok)return false;
    s->step_rejection="scalar CFL subdivision bound";
    double maximum=0,alpha=s->conductivity/(s->rho*s->cp),diff=fmax(alpha,s->diffusivity);
    for(int q=0;q<n;q++) {
        double rate=0;
        for(int a=0;a<3;a++) {
            int upper=a==2?q+s->plane:cfd_cartesian3d_neighbor(&s->grid,q,a,1);
            int i=q%s->grid.n[0],j=(q/s->grid.n[0])%s->grid.n[1],k=q/s->plane;
            int upper_face=s->lateral_open?cfd_open_atmosphere3d_face(s,a,i+(a==0),j+(a==1),k+(a==2)):a*n+upper;
            double lo=s->candidate_velocity[a*n+q],hi=s->candidate_velocity[upper_face],h=s->grid.h[a];
            int adjacent=a==2?((q>=s->plane)+(q<(s->grid.n[2]-1)*s->plane)):2;
            rate+=(fmax(hi,0)+fmax(-lo,0))/h+adjacent*diff/(h*h);
        }
        maximum=fmax(maximum,rate);
    }
    /* Scalar MC is independent of momentum reconstruction; retain its stricter CFL. */
    double needed=ceil(s->dt*maximum/((s->muscl || s->scalar_mc)?.4:.8));if(!isfinite(needed) || needed>4096)return false;
    unsigned cycles=(unsigned)fmax(1,needed);
#ifdef PHYSICS_SIM_OPEN_EXPLICIT_SCALAR_DT
#ifndef PHYSICS_SIM_OPEN_FULL_GRID_QUALIFIER
#error Explicit scalar timing is restricted to the local numerical qualifier
#endif
    /* Hold scalar integration fixed while independently refining flow time.
     * Refuse unsafe/nonintegral timing rather than silently adapting it. The
     * ordinary app/worker build contains none of this qualification hook. */
    double fixed_dt=PHYSICS_SIM_OPEN_EXPLICIT_SCALAR_DT;
    double fixed_cycles=s->dt/fixed_dt;
    if(!isfinite(fixed_dt) || fixed_dt<=0 || !isfinite(fixed_cycles) ||
       fixed_cycles<1 || fixed_cycles>4096 || fabs(fixed_cycles-round(fixed_cycles))>1e-9 ||
       round(fixed_cycles)<cycles)return false;
    cycles=(unsigned)round(fixed_cycles);
#endif
    s->scalar_cycles_requested=cycles;
    s->step_rejection="selected scalar cycle capacity";
    if(cycles>s->max_scalar_cycles)return false;
    unsigned long long work=(unsigned long long)cycles*n*(s->scalar_rk2?2:1);
    s->scalar_work_requested=work;
    s->step_rejection="scalar cumulative work budget";
    if(work>s->max_scalar_work_cells-s->scalar_work_cells)return false;
    memcpy(s->scalar_candidate,s->energy,(size_t)n*sizeof(double));memcpy(s->scalar_candidate+n,s->smoke,(size_t)n*sizeof(double));memcpy(s->candidate_flux,s->flux,(size_t)4*faces*sizeof(double));
    s->step_rejection="scalar positivity/finite transport";
    for(unsigned i=0;i<cycles;i++) {
        if(!scalar(s,s->scalar_candidate,s->scalar_work,source_j,s->dt/cycles,1./cycles,alpha,0) ||
            !scalar(s,s->scalar_candidate+n,s->scalar_work+n,source_kg,s->dt/cycles,1./cycles,s->diffusivity,1))return false;
        memcpy(s->scalar_candidate,s->scalar_work,(size_t)2*n*sizeof(double));
    }
    double added[2]={cfd_passive3d_total(source_j,n),cfd_passive3d_total(source_kg,n)};
    s->step_rejection="scalar conservation";
    for(int a=0;a<2;a++) {
        double before=cfd_passive3d_total(a?s->smoke:s->energy,n),after=cfd_passive3d_total(s->scalar_candidate+a*n,n);
        double incoming=cfd_passive3d_total(s->candidate_flux+2*a*faces,faces)-cfd_passive3d_total(s->flux+2*a*faces,faces);
        double outgoing=cfd_passive3d_total(s->candidate_flux+(2*a+1)*faces,faces)-cfd_passive3d_total(s->flux+(2*a+1)*faces,faces);
        double balance=after-before-added[a]-incoming+outgoing;
        if(!isfinite(balance) || fabs(balance)>1e-11*fmax(before+added[a]+incoming+outgoing,1e-12))return false;
    }
    double capacity=s->rho*s->cp*s->grid.volume;
    s->step_rejection="temperature contrast applicability";
    for(int q=0;q<n;q++)if(!isfinite(s->reference_k+s->scalar_candidate[q]/capacity) ||
        (s->buoyancy && s->scalar_candidate[q]/capacity/s->reference_k>s->max_contrast))return false;
    if(!isfinite(s->input_j+added[0]) || !isfinite(s->input_kg+added[1]))return false;
    s->step_rejection="wind air volume balance";
    double wind_totals[3];
    if(!cfd_open_wind_volume(s,wind_totals))return false;
    double weather_totals[2]={0};
    if(s->weather.enabled && !cfd_open_weather_volume(s,&s->weather_state,weather_totals))return false;
    if(s->wind.quiescent_backflow) {
        double total=s->weather.enabled?s->candidate_weather_work_j:0,scale=1+fabs(total);
        s->step_rejection="discrete kinetic accounting";
        for(int z=0;z<10;z++) {
            if(!isfinite(s->candidate_kinetic_ledger[z]))return false;
            total+=s->candidate_kinetic_ledger[z];scale+=fabs(s->candidate_kinetic_ledger[z]);
        }
        if(!isfinite(total) || fabs(total-cfd_open_kinetic(s,s->candidate_velocity))>1e-10*scale)return false;
        memcpy(s->kinetic_ledger,s->candidate_kinetic_ledger,sizeof(s->kinetic_ledger));
    }
    if(s->weather.enabled)s->weather_state=(CfdOpenWeatherState){
        s->candidate_weather_work_j,weather_totals[0],weather_totals[1]};
    s->wind_air_in=wind_totals[0];s->wind_air_out=wind_totals[1];s->wind_inlet_volume=wind_totals[2];
    memcpy(s->velocity,s->candidate_velocity,(size_t)s->velocity_count*sizeof(double));memcpy(s->pressure,s->candidate_pressure,(size_t)n*sizeof(double));
    memcpy(s->energy,s->scalar_candidate,(size_t)2*n*sizeof(double));memcpy(s->flux,s->candidate_flux,(size_t)4*faces*sizeof(double));
    s->steps++;s->time=s->steps*s->dt;s->input_j+=added[0];s->input_kg+=added[1];s->scalar_work_cells+=work;s->divergence=maxdiv;s->residual=residual;s->step_rejection=NULL;return true;
}

/* Separate opt-in topology implementation in this existing translation unit.
 * Legacy contained tool builders compile this TU through explicit source lists. */
#include "cfd_open_atmosphere3d_rig.inc"

bool cfd_open_atmosphere3d_step(CfdOpenAtmosphere3d *s,const double *source_j,const double *source_kg) {
    return open_step(s,source_j,source_kg,NULL,NULL);
}
bool cfd_open_atmosphere3d_step_momentum(CfdOpenAtmosphere3d *s,
    const double *source_j,const double *source_kg,const double *impulse,
    size_t count,CfdOpenMomentumReceipt *receipt) {
    if(!receipt || !open_momentum_admit(s,impulse,count))return false;
    CfdOpenMomentumReceipt trial={0};
    if(!open_step(s,source_j,source_kg,impulse,&trial))return false;
    open_momentum_vector(s,s->velocity,trial.projected_momentum_ns);
    trial.projection_work_j=cfd_open_kinetic(s,s->velocity)-trial.forced_kinetic_j;
    *receipt=trial;return true;
}
