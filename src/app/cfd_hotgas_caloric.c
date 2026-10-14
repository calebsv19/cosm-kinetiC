#include "app/cfd_hotgas_caloric.h"
#include <float.h>
#include <math.h>

/* N2: NIST Shomate 100..500,500..2000 K. O2/CO2/H2O: GRI-Mech
 * NASA7 Cp low/high 200..1000..3500 K, Cantera gri30.yaml. Keep only
 * Cp coefficients: integrate across joins, not rounded enthalpy offsets. */
static const double mass[4] = {.0280134, .0319988, .0440095, .0180153};
static const double nitrogen[2][5] = {{28.98641,1.853978,-9.647459,16.63537,.000117},
                                    {19.50583,19.88705,-8.598535,1.369784,.527601}};
static const double nasa[3][2][5] = {
    {{3.78245636,-2.99673416e-3,9.84730201e-6,-9.68129509e-9,3.24372837e-12},
     {3.28253784,1.48308754e-3,-7.57966669e-7,2.09470555e-10,-2.16717794e-14}},
    {{2.35677352,8.98459677e-3,-7.12356269e-6,2.45919022e-9,-1.43699548e-13},
     {3.85746029,4.41437026e-3,-2.21481404e-6,5.23490188e-10,-4.72084164e-14}},
    {{4.19864056,-2.0364341e-3,6.52040211e-6,-5.48797062e-9,1.77197817e-12},
     {3.03399249,2.17691804e-3,-1.64072518e-7,-9.7041987e-11,1.68200992e-14}}
};
static const double edges[4] = {200,500,1000,1500};
static double polynomial(const double a[6], double t) {
    return a[0]+t*(a[1]+t*(a[2]+t*(a[3]+t*a[4])))+a[5]/(t*t);
}
static double integral(const double a[6], double lo, double hi) {
    /* Factored power differences avoid cancellation for nearby temperatures. */
    double sum = a[0]+a[5]/(lo*hi), lp[5]={1}, hp[5]={1};
    for (int i=1; i<=4; ++i) { lp[i]=lp[i-1]*lo; hp[i]=hp[i-1]*hi; }
    for (int i=1; i<=4; ++i) {
        double divided=0;
        for (int k=0; k<=i; ++k) divided += lp[k]*hp[i-k];
        sum += a[i]*divided/(i+1);
    }
    return (hi-lo)*sum;
}
bool cfd_hotgas_caloric_init(CfdHotGasCaloric *out, const double moles[4]) {
    if (!out || !moles) return false;
    CfdHotGasCaloric candidate = {0}; double total=0, weight=0;
    for (int i=0; i<4; ++i) {
        if (!isfinite(moles[i]) || moles[i]<0) return false;
        total += moles[i]; weight += moles[i]*mass[i];
    }
    if (!isfinite(total) || !isfinite(weight) || total<=0 || weight<=0) return false;
    candidate.gas_constant=8.31446261815324*total/weight;
    for (int band=0; band<3; ++band) {
        double *a=candidate.coefficients[band]; const double *n=nitrogen[band>0];
        for (int k=0; k<4; ++k) a[k]=moles[0]*n[k]/pow(1000,k)/weight;
        a[5]=moles[0]*n[4]*1e6/weight;
        for (int species=1; species<4; ++species)
            for (int k=0; k<5; ++k)
                a[k]+=moles[species]*8.31446261815324*nasa[species-1][band==2][k]/weight;
    }
    /* Match Cp at each join by its rounded-fit residual. This deterministic
     * constant correction is not experimental fitting; h/e then integrate the
     * continuous Cp. Preserve the underlying data and bound corrections in tests. */
    for (int b=1; b<3; ++b)
        candidate.coefficients[b][0] += polynomial(candidate.coefficients[b-1],edges[b]) -
                                       polynomial(candidate.coefficients[b],edges[b]);
    if (!isfinite(candidate.gas_constant) || candidate.gas_constant<=0) return false;
    for (int b=0; b<3; ++b)
        for (int k=0; k<6; ++k)
            if (!isfinite(candidate.coefficients[b][k])) return false;
    candidate.active=true;
    double e, cp;
    if (!cfd_hotgas_caloric_properties(&candidate,200,&e,&cp) ||
        !cfd_hotgas_caloric_properties(&candidate,1500,&e,&cp)) return false;
    *out=candidate; return true;
}
bool cfd_hotgas_caloric_properties(const CfdHotGasCaloric *s, double t, double *energy, double *cp) {
    if (!s || !s->active || !energy || !cp || !isfinite(t) || t<200 || t>1500) return false;
    double heat=0, lower=fmin(t,298.15), upper=fmax(t,298.15);
    for (int b=0; b<3; ++b) {
        double lo=fmax(lower,edges[b]), hi=fmin(upper,edges[b+1]);
        if (hi>lo) heat+=integral(s->coefficients[b],lo,hi);
    }
    double capacity=polynomial(s->coefficients[t<500 ? 0 : t<1000 ? 1 : 2],t);
    double e=(t<298.15 ? -heat : heat)-s->gas_constant*t;
    if (!isfinite(e) || !isfinite(capacity) || capacity<=s->gas_constant) return false;
    *energy=e; *cp=capacity; return true;
}
bool cfd_hotgas_caloric_temperature(const CfdHotGasCaloric *s, double energy, double *out) {
    double lo=200,hi=1500,el,eh,cp;
    if (!out || !isfinite(energy) || !cfd_hotgas_caloric_properties(s,lo,&el,&cp) ||
        !cfd_hotgas_caloric_properties(s,hi,&eh,&cp)) return false;
    double tolerance=32*DBL_EPSILON*fmax(1,fmax(fabs(el),fabs(eh)));
    if (energy<el-tolerance || energy>eh+tolerance) return false;
    if (energy<=el) { *out=lo; return true; }
    if (energy>=eh) { *out=hi; return true; }
    double t=lo+(hi-lo)*(energy-el)/(eh-el);
    for (int k=0; k<40; ++k) {
        double e;
        if (!cfd_hotgas_caloric_properties(s,t,&e,&cp)) return false;
        double error=e-energy;
        if (fabs(error)<=tolerance) { *out=t; return true; }
        if (error<0) lo=t; else hi=t;
        double next=t-error/(cp-s->gas_constant);
        t=next>lo && next<hi ? next : .5*(lo+hi);
    }
    return false;
}
