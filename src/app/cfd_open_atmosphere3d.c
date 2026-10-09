#include "app/cfd_open_atmosphere3d.h"
#include "app/cfd_passive3d.h"
#include <math.h>
#include <string.h>
static int wrap(int a,int n) { a%=n;return a<0?a+n:a; }
static double sample(const CfdOpenAtmosphere3d *s,const double *v,int a,int i,int j,int k) {
    const int *n=s->grid.n;i=wrap(i,n[0]);j=wrap(j,n[1]);
    if(a==2) { if(k<0){if(s->ground)return -sample(s,v,a,i,j,-k);k=-k;}if(k>n[2])k=2*n[2]-k; }
    else { if(k<0){if(s->ground)return -sample(s,v,a,i,j,-k-1);k=0;}if(k>=n[2])k=n[2]-1; }
    return v[a*s->grid.count+(k*n[1]+j)*n[0]+i];
}
static double advector(const CfdOpenAtmosphere3d *s,int a,int b,int i,int j,int k) {
    if(a==b)return sample(s,s->velocity,b,i,j,k);
    int p[3]={i,j,k},m[3]={i,j,k};m[a]--;p[b]++;
    int mp[3]={m[0],m[1],m[2]};mp[b]++;
    return .25*(sample(s,s->velocity,b,i,j,k)+sample(s,s->velocity,b,p[0],p[1],p[2])+
        sample(s,s->velocity,b,m[0],m[1],m[2])+sample(s,s->velocity,b,mp[0],mp[1],mp[2]));
}
static double minmod(double a,double b) {
    return a*b>0?copysign(fmin(fabs(a),fabs(b)),a):0;
}
static double velocity_slope(const CfdOpenAtmosphere3d *s,int a,int b,int i,int j,int k) {
    int lo[3]={i,j,k},hi[3]={i,j,k};lo[b]--;hi[b]++;
    double c=sample(s,s->velocity,a,i,j,k);
    return minmod(c-sample(s,s->velocity,a,lo[0],lo[1],lo[2]),sample(s,s->velocity,a,hi[0],hi[1],hi[2])-c);
}
static double scalar_slope(const CfdOpenAtmosphere3d *s,const double *v,int q,int a) {
    const CfdCartesian3d *g=&s->grid;
    if(a==2 && (q<s->plane || q>=(g->n[2]-1)*s->plane))return 0;
    int lo=a==2?q-s->plane:cfd_cartesian3d_neighbor(g,q,a,-1);
    int hi=a==2?q+s->plane:cfd_cartesian3d_neighbor(g,q,a,1);
    return minmod(v[q]-v[lo],v[hi]-v[q]);
}
static double divergence(const CfdOpenAtmosphere3d *s,const double *v,int q) {
    const CfdCartesian3d *g=&s->grid;int i=q%g->n[0],j=(q/g->n[0])%g->n[1],k=q/s->plane;
    return (sample(s,v,0,i+1,j,k)-sample(s,v,0,i,j,k))/g->h[0]+
        (sample(s,v,1,i,j+1,k)-sample(s,v,1,i,j,k))/g->h[1]+
        (sample(s,v,2,i,j,k+1)-sample(s,v,2,i,j,k))/g->h[2];
}
void cfd_open_atmosphere3d_destroy(CfdOpenAtmosphere3d *s) { if(s){cfd_cartesian3d_linear_destroy(s->projection);s->projection=NULL;cfd_memory_free(s->storage);s->storage=NULL;} }
bool cfd_open_atmosphere3d_init(CfdOpenAtmosphere3d *s,const int n[3],const double length[3],
    double rho,double mu,double cp,double ref,double conductivity,double diffusivity,double dt,
    const double pressure[2],const double ambient[2],const double smoke[2],bool buoyancy,double gravity,double beta,double contrast) {
    if(!s || !pressure || !ambient || !smoke)return false;
    memset(s,0,sizeof(*s));
    if(!cfd_cartesian3d_init(&s->grid,n,length) || s->grid.count>524288 ||
        !isfinite(rho) || rho<=0 || !isfinite(mu) || mu<=0 || !isfinite(cp) || cp<=0 ||
        !isfinite(ref) || ref<=0 || !isfinite(conductivity) || conductivity<0 || !isfinite(diffusivity) || diffusivity<0 ||
        !isfinite(dt) || dt<=0 || dt>.1 || !isfinite(gravity) || gravity<0 || gravity>100 ||
        !isfinite(beta) || beta<0 || beta>1/ref || !isfinite(contrast) || contrast<=0 || contrast>.1)return false;
    for(int a=0;a<2;a++)if(!isfinite(pressure[a]) || !isfinite(ambient[a]) || ambient[a]<ref ||
        !isfinite(smoke[a]) || smoke[a]<0 || (buoyancy && (ambient[a]-ref)/ref>contrast))return false;
    s->rho=rho;s->mu=mu;s->cp=cp;s->reference_k=ref;s->conductivity=conductivity;s->diffusivity=diffusivity;s->dt=dt;
    s->max_scalar_work_cells=100000000;
    s->buoyancy=buoyancy;s->gravity=gravity;s->beta=beta;s->max_contrast=contrast;
    memcpy(s->pressure_boundary,pressure,2*sizeof(double));memcpy(s->ambient_k,ambient,2*sizeof(double));memcpy(s->ambient_smoke,smoke,2*sizeof(double));
    s->plane=n[0]*n[1];s->velocity_count=3*s->grid.count+s->plane;
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
    if(s->ground) {
        for(int q=0;q<s->plane;q++)if(s->velocity[2*s->grid.count+q]!=0)return false;
        for(int b=0;b<4;b++)for(int q=0;q<s->plane;q++)if(s->flux[b*2*s->plane+q]!=0)return false;
    }
    double capacity=s->rho*s->cp*s->grid.volume;
    if(!isfinite(capacity) || capacity<=0)return false;
    for(int q=0;q<s->velocity_count;q++)if(!isfinite(s->velocity[q]))return false;
    for(int q=0;q<s->grid.count;q++)if(!isfinite(s->pressure[q]) || !isfinite(s->energy[q]) || s->energy[q]<0 ||
        !isfinite(s->smoke[q]) || s->smoke[q]<0 || !isfinite(divergence(s,s->velocity,q)) || fabs(divergence(s,s->velocity,q))>=1e-8 ||
        (s->buoyancy && s->energy[q]/capacity/s->reference_k>s->max_contrast))return false;
    for(int q=0;q<8*s->plane;q++)if(!isfinite(s->flux[q]) || s->flux[q]<0)return false;
    return true;
}
static bool momentum(CfdOpenAtmosphere3d *s,double *maxdiv,double *residual) {
    const CfdCartesian3d *g=&s->grid;int n=g->count;
    double maximum=0,nu=s->mu/s->rho;
    for(int a=0;a<3;a++)for(int k=0;k<g->n[2]+(a==2);k++)for(int j=0;j<g->n[1];j++)for(int i=0;i<g->n[0];i++) {
        int p[3]={i,j,k},q=a*n+(k*g->n[1]+j)*g->n[0]+i;
        if(s->ground && a==2 && k==0){s->candidate_velocity[q]=0;continue;}
        double old=s->velocity[q],change=0,rate=0;
        for(int b=0;b<3;b++) {
            int lo[3]={p[0],p[1],p[2]},hi[3]={p[0],p[1],p[2]};lo[b]--;hi[b]++;
            double left=sample(s,s->velocity,a,lo[0],lo[1],lo[2]),right=sample(s,s->velocity,a,hi[0],hi[1],hi[2]),u=advector(s,a,b,i,j,k),h=g->h[b];
            double derivative=u>=0?(old-left):(right-old);
            if(s->muscl) {
                double center_slope=velocity_slope(s,a,b,i,j,k);
                derivative=u>=0?old+.5*center_slope-left-.5*velocity_slope(s,a,b,lo[0],lo[1],lo[2]):
                    right-.5*velocity_slope(s,a,b,hi[0],hi[1],hi[2])-old+.5*center_slope;
            }
            change+=nu*(left-2*old+right)/(h*h)-u*derivative/h;
            rate+=fabs(u)/h+2*nu/(h*h);
        }
        if(s->buoyancy && a==2) {
            int kl=k==0?0:k-1,kh=k==g->n[2]?k-1:k;
            int low=(kl*g->n[1]+j)*g->n[0]+i,high=(kh*g->n[1]+j)*g->n[0]+i;
            double delta=.5*(s->energy[low]+s->energy[high])/(s->rho*s->cp*g->volume);
            change+=s->gravity*s->beta*delta;
        }
        maximum=fmax(maximum,rate);s->candidate_velocity[q]=old+s->dt*change;
        if(!isfinite(s->candidate_velocity[q]))return false;
    }
    if(!isfinite(maximum) || s->dt*maximum>(s->muscl?.4:.8))return false;
    for(int q=0;q<n;q++) {
        int k=q/s->plane;s->rhs[q]=-s->rho/s->dt*divergence(s,s->candidate_velocity,q);
        if(k==0 && !s->ground)s->rhs[q]+=2*s->pressure_boundary[0]/(g->h[2]*g->h[2]);
        if(k==g->n[2]-1)s->rhs[q]+=2*s->pressure_boundary[1]/(g->h[2]*g->h[2]);
    }
    bool periodic[3]={true,true,false};bool neumann[6]={false,false,false,false,s->ground,false};
    if(s->projection && !s->cache_projection){cfd_cartesian3d_linear_destroy(s->projection);s->projection=NULL;}
    if(s->projection && s->projection_ground!=s->ground)return false;
    CfdCartesian3dLinear *solver=s->projection;
    if(!solver)solver=cfd_cartesian3d_linear_create_mixed(g,periodic,neumann,0,1,false);
    if(s->cache_projection && solver){s->projection=solver;s->projection_ground=s->ground;}
    if(!solver)return false;
    memcpy(s->candidate_pressure,s->pressure,(size_t)n*sizeof(double));int iterations;
    bool solved=cfd_cartesian3d_linear_solve(solver,s->rhs,s->candidate_pressure,&iterations,residual);if(!s->cache_projection)cfd_cartesian3d_linear_destroy(solver);
    if(!solved)return false;
    for(int a=0;a<2;a++)for(int q=0;q<n;q++) {
        double grad=(s->candidate_pressure[q]-s->candidate_pressure[cfd_cartesian3d_neighbor(g,q,a,-1)])/g->h[a];
        s->candidate_velocity[a*n+q]-=s->dt/s->rho*grad;
    }
    for(int k=0;k<=g->n[2];k++)for(int q=0;q<s->plane;q++) {
        if(s->ground && k==0){s->candidate_velocity[2*n+q]=0;continue;}
        double grad=k==0?2*(s->candidate_pressure[q]-s->pressure_boundary[0])/g->h[2]:
            k==g->n[2]?2*(s->pressure_boundary[1]-s->candidate_pressure[(k-1)*s->plane+q])/g->h[2]:
            (s->candidate_pressure[k*s->plane+q]-s->candidate_pressure[(k-1)*s->plane+q])/g->h[2];
        s->candidate_velocity[2*n+k*s->plane+q]-=s->dt/s->rho*grad;
    }
    *maxdiv=0;
    for(int q=0;q<n;q++) { double d=divergence(s,s->candidate_velocity,q);if(!isfinite(d))return false;*maxdiv=fmax(*maxdiv,fabs(d)); }
    return *maxdiv<1e-8;
}
static bool scalar(CfdOpenAtmosphere3d *s,const double *old,double *out,const double *source,
    double dt,double fraction,double diffusivity,int quantity) {
    const CfdCartesian3d *g=&s->grid;int n=g->count,faces=2*s->plane;
    for(int q=0;q<n;q++)out[q]=old[q]+fraction*source[q];
    for(int a=0;a<3;a++)for(int q=0;q<n;q++) {
        if(a==2 && q<s->plane)continue;
        int left=a==2?q-s->plane:cfd_cartesian3d_neighbor(g,q,a,-1);
        double u=s->candidate_velocity[a*n+q],h=g->h[a];
        double donor=u>=0?old[left]:old[q];
        if(s->muscl)donor+=u>=0?.5*scalar_slope(s,old,left,a):-.5*scalar_slope(s,old,q,a);
        double flux=dt*(u*donor/h-diffusivity*(old[q]-old[left])/(h*h));out[q]+=flux;out[left]-=flux;
    }
    for(int boundary=0;boundary<2;boundary++)for(int q=0;q<s->plane;q++) {
        if(s->ground && boundary==0)continue;
        int cell=boundary?(g->n[2]-1)*s->plane+q:q,face=boundary*g->n[2]*s->plane+q;
        double outward=(boundary?1:-1)*s->candidate_velocity[2*n+face];
        double ambient=quantity?s->ambient_smoke[boundary]*g->volume:s->rho*s->cp*(s->ambient_k[boundary]-s->reference_k)*g->volume;
        double amount=dt*fabs(outward)*(outward>=0?old[cell]:ambient)/g->h[2];
        if(outward>=0){out[cell]-=amount;s->candidate_flux[(2*quantity+1)*faces+boundary*s->plane+q]+=amount;}
        else {out[cell]+=amount;s->candidate_flux[2*quantity*faces+boundary*s->plane+q]+=amount;}
    }
    for(int q=0;q<n;q++)if(!isfinite(out[q]) || out[q]<0)return false;
    return true;
}
bool cfd_open_atmosphere3d_step(CfdOpenAtmosphere3d *s,const double *source_j,const double *source_kg) {
    if(!cfd_open_atmosphere3d_valid(s) || !source_j || !source_kg || s->steps>=10000)return false;
    int n=s->grid.count,faces=2*s->plane;
    for(int q=0;q<n;q++)if(!isfinite(source_j[q]) || source_j[q]<0 || !isfinite(source_kg[q]) || source_kg[q]<0)return false;
    double maxdiv,residual;if(!momentum(s,&maxdiv,&residual))return false;
    double maximum=0,alpha=s->conductivity/(s->rho*s->cp),diff=fmax(alpha,s->diffusivity);
    for(int q=0;q<n;q++) {
        double rate=0;
        for(int a=0;a<3;a++) {
            int upper=a==2?q+s->plane:cfd_cartesian3d_neighbor(&s->grid,q,a,1);
            double lo=s->candidate_velocity[a*n+q],hi=s->candidate_velocity[a*n+upper],h=s->grid.h[a];
            int adjacent=a==2?((q>=s->plane)+(q<(s->grid.n[2]-1)*s->plane)):2;
            rate+=(fmax(hi,0)+fmax(-lo,0))/h+adjacent*diff/(h*h);
        }
        maximum=fmax(maximum,rate);
    }
    double needed=ceil(s->dt*maximum/(s->muscl?.4:.8));if(!isfinite(needed) || needed>4096)return false;
    unsigned cycles=(unsigned)fmax(1,needed);unsigned long long work=(unsigned long long)cycles*n;
    if(work>s->max_scalar_work_cells-s->scalar_work_cells)return false;
    memcpy(s->scalar_candidate,s->energy,(size_t)n*sizeof(double));memcpy(s->scalar_candidate+n,s->smoke,(size_t)n*sizeof(double));memcpy(s->candidate_flux,s->flux,(size_t)4*faces*sizeof(double));
    for(unsigned i=0;i<cycles;i++) {
        if(!scalar(s,s->scalar_candidate,s->scalar_work,source_j,s->dt/cycles,1./cycles,alpha,0) ||
            !scalar(s,s->scalar_candidate+n,s->scalar_work+n,source_kg,s->dt/cycles,1./cycles,s->diffusivity,1))return false;
        memcpy(s->scalar_candidate,s->scalar_work,(size_t)2*n*sizeof(double));
    }
    double added[2]={cfd_passive3d_total(source_j,n),cfd_passive3d_total(source_kg,n)};
    for(int a=0;a<2;a++) {
        double before=cfd_passive3d_total(a?s->smoke:s->energy,n),after=cfd_passive3d_total(s->scalar_candidate+a*n,n);
        double incoming=cfd_passive3d_total(s->candidate_flux+2*a*faces,faces)-cfd_passive3d_total(s->flux+2*a*faces,faces);
        double outgoing=cfd_passive3d_total(s->candidate_flux+(2*a+1)*faces,faces)-cfd_passive3d_total(s->flux+(2*a+1)*faces,faces);
        double balance=after-before-added[a]-incoming+outgoing;
        if(!isfinite(balance) || fabs(balance)>1e-11*fmax(before+added[a]+incoming+outgoing,1e-12))return false;
    }
    double capacity=s->rho*s->cp*s->grid.volume;
    for(int q=0;q<n;q++)if(!isfinite(s->reference_k+s->scalar_candidate[q]/capacity) ||
        (s->buoyancy && s->scalar_candidate[q]/capacity/s->reference_k>s->max_contrast))return false;
    if(!isfinite(s->input_j+added[0]) || !isfinite(s->input_kg+added[1]))return false;
    memcpy(s->velocity,s->candidate_velocity,(size_t)s->velocity_count*sizeof(double));memcpy(s->pressure,s->candidate_pressure,(size_t)n*sizeof(double));
    memcpy(s->energy,s->scalar_candidate,(size_t)2*n*sizeof(double));memcpy(s->flux,s->candidate_flux,(size_t)4*faces*sizeof(double));
    s->steps++;s->time=s->steps*s->dt;s->input_j+=added[0];s->input_kg+=added[1];s->scalar_work_cells+=work;s->divergence=maxdiv;s->residual=residual;return true;
}
