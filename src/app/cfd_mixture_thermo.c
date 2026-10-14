#include "app/cfd_mixture_thermo.h"
#include <math.h>
#include <string.h>
/* Atomic-consistent masses preserve CH3OH+1.5 O2 -> CO2+2 H2O mass.
 * Cp reuses prior continuous caloric model; GRI NASA7 h(298.15) fixes
 * formation anchors. N2/O2 elemental standard-state formation h is zero. */
const double cfd_mixture_mass[5]={.0280134,.0319988,.0440095,.01801528,.03204186};
static const double fuel[2][6]={
 {5.71539582,-.0152309129,6.52441155e-5,-7.10806889e-8,2.61352698e-11,-2.56427656e4},
 {1.78970791,.0140938292,-6.36500835e-6,1.38171085e-9,-1.1706022e-13,-2.53748747e4}};
static const double reference[3][6]={
 {2.35677352,8.98459677e-3,-7.12356269e-6,2.45919022e-9,-1.43699548e-13,-4.83719697e4},
 {4.19864056,-2.0364341e-3,6.52040211e-6,-5.48797062e-9,1.77197817e-12,-3.02937267e4},
 {5.71539582,-.0152309129,6.52441155e-5,-7.10806889e-8,2.61352698e-11,-2.56427656e4}};
static double cp_poly(const double a[6],double t) {
    return a[0]+t*(a[1]+t*(a[2]+t*(a[3]+t*a[4])))+a[5]/(t*t);
}
bool cfd_mixture_thermo_init(CfdMixtureThermo *out) {
    if (!out) return false;
    CfdMixtureThermo s={0};
    for (int i=0;i<4;++i) {
        double n[4]={0}; n[i]=1;
        if (!cfd_hotgas_caloric_init(&s.pure[i],n)) return false;
        if (i==3) { double scale=.0180153/cfd_mixture_mass[i];
            s.pure[i].gas_constant*=scale;
            for(int b=0;b<3;++b) for(int k=0;k<6;++k) s.pure[i].coefficients[b][k]*=scale;
        }
    }
    CfdHotGasCaloric *f=&s.pure[4];f->active=true;
    f->gas_constant=8.31446261815324/cfd_mixture_mass[4];
    for(int b=0;b<3;++b) for(int k=0;k<5;++k)
        f->coefficients[b][k]=f->gas_constant*fuel[b==2][k];
    f->coefficients[2][0]+=cp_poly(f->coefficients[1],1000)-cp_poly(f->coefficients[2],1000);
    for(int i=2;i<5;++i) {
        const double *a=reference[i-2];double t=298.15, h=a[5];
        for(int k=0;k<5;++k) h+=a[k]*pow(t,k+1)/(k+1);
        s.formation[i]=8.31446261815324*h/cfd_mixture_mass[i];
    }
    double e,cp;
    if(!cfd_hotgas_caloric_properties(f,200,&e,&cp) ||
       !cfd_hotgas_caloric_properties(f,1500,&e,&cp)) return false;
    *out=s;return true;
}
bool cfd_mixture_model(const CfdMixtureThermo *s,const double y[5],
    CfdHotGasCaloric *caloric,double *formation) {
    if(!s || !y || !caloric || !formation) return false;
    CfdHotGasCaloric c={.active=true};double sum=0,hf=0;
    for(int i=0;i<5;++i) {
        if(!isfinite(y[i]) || y[i]<0 || y[i]>1) return false;
        sum+=y[i];hf+=y[i]*s->formation[i];c.gas_constant+=y[i]*s->pure[i].gas_constant;
        for(int b=0;b<3;++b) for(int k=0;k<6;++k)
            c.coefficients[b][k]+=y[i]*s->pure[i].coefficients[b][k];
    }
    if(fabs(sum-1)>1e-11 || !isfinite(hf) || c.gas_constant<=0) return false;
    *caloric=c;*formation=hf;return true;
}
bool cfd_mixture_properties(const CfdMixtureThermo *s,const double y[5],double t,
    double *energy,double *cp,double *gas_constant) {
    if(!energy || !cp || !gas_constant) return false;
    CfdHotGasCaloric c;double hf,e,capacity;
    if(!cfd_mixture_model(s,y,&c,&hf) || !cfd_hotgas_caloric_properties(&c,t,&e,&capacity)) return false;
    *energy=e+hf;*cp=capacity;*gas_constant=c.gas_constant;return true;
}
bool cfd_mixture_primitive(const CfdMixtureThermo *s,const double u[10],
    double *temperature,double *pressure,double *sound,double *cv) {
    if(!u || !temperature || !pressure || !sound || !cv || !isfinite(u[0]) || u[0]<=0) return false;
    double y[5],sum=0;
    for(int i=0;i<10;++i) if(!isfinite(u[i])) return false;
    for(int i=0;i<5;++i) { if(u[i+5]<0) return false;y[i]=u[i+5]/u[0];sum+=u[i+5]; }
    if(fabs(sum-u[0])>1e-11*u[0]) return false;
    CfdHotGasCaloric c;double hf,t,e,cp;
    double kinetic=(u[1]*u[1]+u[2]*u[2]+u[3]*u[3])/(2*u[0]);
    if(!cfd_mixture_model(s,y,&c,&hf) ||
       !cfd_hotgas_caloric_temperature(&c,(u[4]-kinetic)/u[0]-hf,&t) ||
       !cfd_hotgas_caloric_properties(&c,t,&e,&cp)) return false;
    double p=u[0]*c.gas_constant*t,capacity=cp-c.gas_constant;
    double speed=sqrt(cp/capacity*c.gas_constant*t);
    if(!isfinite(p) || !isfinite(speed) || p<=0) return false;
    *temperature=t;*pressure=p;*sound=speed;*cv=capacity;return true;
}
bool cfd_mixture_enthalpies(const CfdMixtureThermo *s,double t,double h[5]) {
    if(!s || !h) return false;
    double out[5];
    for(int i=0;i<5;++i) {
        double e,cp;if(!cfd_hotgas_caloric_properties(&s->pure[i],t,&e,&cp)) return false;
        out[i]=e+s->pure[i].gas_constant*t+s->formation[i];
    }
    memcpy(h,out,sizeof(out));return true;
}
