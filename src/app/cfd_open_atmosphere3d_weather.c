/* Separate open-domain force substep; no inlet or pressure constraint. */
#include "app/cfd_open_atmosphere3d_weather.h"
#include "app/cfd_open_atmosphere3d.h"
#include "app/cfd_open_energy.h"
#include <math.h>
#include <string.h>
#include "cfd_open_atmosphere3d_weather_broad.inc"

static bool schedule_valid(const CfdOpenAtmosphereWeather *w) {
    if(!w || !w->enabled || w->count<1 || w->count>16 || w->step[0]!=0 ||
       !isfinite(w->response_s) || w->response_s<.001 || w->response_s>100 ||
       !isfinite(w->ramp_height_m))return false;
    for(int i=0;i<w->count;i++)if(w->step[i]<0 || w->step[i]>10000 ||
        (i && w->step[i]<=w->step[i-1]) || !isfinite(w->target_m_s[i]) ||
        fabs(w->target_m_s[i])>10 || !isfinite(w->strength[i]) ||
        w->strength[i]<0 || w->strength[i]>1)return false;
    return true;
}
bool cfd_open_weather_valid(const CfdOpenAtmosphere3d *s,
    const CfdOpenAtmosphereWeather *w) {
    return s && s->storage && s->ground && s->lateral_open && !s->wind.enabled &&
        schedule_valid(w) && (!w->broad_sweeping_shear || s->rig_mask) && w->ramp_height_m>=s->grid.h[2] &&
        w->ramp_height_m<=s->grid.length[2] && isfinite(s->dt) && s->dt>0;
}
bool cfd_open_weather_configure(const CfdOpenAtmosphere3d *s,
    CfdOpenAtmosphereWeather *out,double height,double response,bool linear,
    int count,const int *steps,const double *targets,const double *strengths) {
    if(!s || s->steps || !out || out->enabled || count<1 || count>16 ||
       !steps || !targets || !strengths)return false;
    CfdOpenAtmosphereWeather w={.enabled=true,.linear=linear,.count=count,
        .ramp_height_m=height,.response_s=response};
    memcpy(w.step,steps,(size_t)count*sizeof(int));
    memcpy(w.target_m_s,targets,(size_t)count*sizeof(double));
    memcpy(w.strength,strengths,(size_t)count*sizeof(double));
    if(!cfd_open_weather_valid(s,&w))return false;
    *out=w;return true;
}
bool cfd_open_weather_command(const CfdOpenAtmosphereWeather *w,int step,
    double *target,double *strength) {
    if(!schedule_valid(w) || step<0 || step>10000 || !target || !strength)return false;
    int i=0;while(i+1<w->count && step>=w->step[i+1])i++;
    double u=w->target_m_s[i],g=w->strength[i];
    if(w->linear && i+1<w->count) {
        double t=(double)(step-w->step[i])/(w->step[i+1]-w->step[i]);
        u+=(w->target_m_s[i+1]-u)*t;g+=(w->strength[i+1]-g)*t;
    }
    *target=u;*strength=g;return true;
}
bool cfd_open_weather_apply(CfdOpenAtmosphere3d *s,
    const CfdOpenAtmosphereWeather *w,double *work_j) {
    double target,strength;
    if(!work_j || !cfd_open_weather_valid(s,w) ||
       !cfd_open_weather_command(w,s->steps,&target,&strength))return false;
    double *v=s->candidate_velocity;
    for(int q=0;q<s->velocity_count;q++)if(!isfinite(v[q]))return false;
    double before=cfd_open_kinetic(s,v);
    if(!isfinite(before))return false;
    double alpha=-expm1(-strength*s->dt/w->response_s);
    if(!isfinite(alpha) || alpha<0 || alpha>1)return false;
    /* Exact release: do not rewrite even signed zeros or touch other axes. */
    if(strength==0){*work_j=0;return true;}
    /* Reproduce the MAC sum in its ordinary order before any write, so even
     * an overflow refusal leaves candidate bytes and the output work intact. */
    double after=0;
    for(int a=0;a<3;a++)for(int k=0;k<s->grid.n[2]+(a==2);k++)
    for(int j=0;j<s->grid.n[1]+(s->lateral_open && a==1);j++)
    for(int i=0;i<s->grid.n[0]+(s->lateral_open && a==0);i++) {
        int q=cfd_open_atmosphere3d_face(s,a,i,j,k);double u=v[q];
        if(a==0 && weather_drive(s,w,i,j,k)) {
            if(cfd_open_atmosphere3d_blocked(s,0,i,j,k)) {
                if(u!=0)return false;
            } else {
                double goal=weather_goal(s,w,target,j,k);
                u+=alpha*(goal-u);
            }
        }
        if(!isfinite(u))return false;
        after+=.5*cfd_open_face_mass(s,a,i,j,k)*u*u;
    }
    double work=after-before;
    if(!isfinite(after) || !isfinite(work))return false;
    for(int k=0;k<s->grid.n[2];k++)for(int j=0;j<s->grid.n[1];j++)
    for(int i=0;i<=s->grid.n[0];i++) {
        if(cfd_open_atmosphere3d_blocked(s,0,i,j,k) || !weather_drive(s,w,i,j,k))continue;
        int q=cfd_open_atmosphere3d_face(s,0,i,j,k);
        double goal=weather_goal(s,w,target,j,k);
        v[q]+=alpha*(goal-v[q]);
    }
    *work_j=work;return true;
}

bool cfd_open_weather_state_valid(const CfdOpenAtmosphere3d *s,
    const CfdOpenAtmosphereWeather *w,const CfdOpenWeatherState *state) {
    if(!cfd_open_weather_valid(s,w) || !s->wind.quiescent_backflow || !state ||
       !isfinite(state->external_work_j) || !isfinite(state->air_in_m3) ||
       !isfinite(state->air_out_m3) || state->air_in_m3<0 || state->air_out_m3<0)return false;
    double bound=s->steps*s->dt*s->grid.volume*s->grid.count*1e-8+
        1e-12*(state->air_in_m3+state->air_out_m3);
    if(fabs(state->air_in_m3-state->air_out_m3)>bound)return false;
    if(s->steps==0 && (state->external_work_j!=0 || state->air_in_m3!=0 || state->air_out_m3!=0))return false;
    double total=state->external_work_j,scale=1+fabs(total);
    for(int q=0;q<10;q++) {
        if(!isfinite(s->kinetic_ledger[q]))return false;
        total+=s->kinetic_ledger[q];scale+=fabs(s->kinetic_ledger[q]);
    }
    return s->kinetic_ledger[KE_EULER]>=0 && s->kinetic_ledger[KE_INLET]==0 &&
        isfinite(total) && fabs(total-cfd_open_kinetic(s,s->velocity))<=1e-10*scale;
}
bool cfd_open_weather_volume(const CfdOpenAtmosphere3d *s,
    const CfdOpenWeatherState *accepted,double totals[2]) {
    if(!s || !accepted || !totals || !s->storage || !s->ground || !s->lateral_open ||
       s->wind.enabled || !isfinite(accepted->air_in_m3) || !isfinite(accepted->air_out_m3) ||
       accepted->air_in_m3<0 || accepted->air_out_m3<0)return false;
    double incoming=0,outgoing=0;const int *n=s->grid.n;
    for(int a=0;a<3;a++)for(int side=0;side<2;side++) {
        if(a==2 && side==0)continue;
        int b=(a+1)%3,c=(a+2)%3;
        for(int u=0;u<n[b];u++)for(int v=0;v<n[c];v++) {
            int p[3]={0};p[a]=side?n[a]:0;p[b]=u;p[c]=v;
            double amount=(side?1:-1)*s->candidate_velocity[
                cfd_open_atmosphere3d_face(s,a,p[0],p[1],p[2])]*s->dt*s->grid.h[b]*s->grid.h[c];
            if(!isfinite(amount))return false;
            incoming+=fmax(-amount,0);outgoing+=fmax(amount,0);
        }
    }
    double bound=s->dt*s->grid.volume*s->grid.count*1e-8+1e-12*(incoming+outgoing);
    double in=accepted->air_in_m3+incoming,out=accepted->air_out_m3+outgoing;
    if(fabs(incoming-outgoing)>bound || !isfinite(in) || !isfinite(out))return false;
    totals[0]=in;totals[1]=out;return true;
}
