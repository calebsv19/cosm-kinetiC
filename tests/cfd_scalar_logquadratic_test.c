/* Independent analytic Gaussian/exponential cell integrals qualify the fit. */
#include "../src/tools/physics_sim_scalar_logquadratic.h"
#include <assert.h>
#include <stdio.h>
static double integral(double b,double c,double lo) {
    if(c==0)return b==0?1:exp(b*lo)*expm1(b)/b;
    double s=sqrt(-c),a=s*(lo+b/(2*c)),z=a+s,diff;
    if(a>=0)diff=erfc(a)-erfc(z);
    else if(z<=0)diff=erfc(-z)-erfc(-a);
    else diff=erf(z)-erf(a);
    return exp(-b*b/(4*c))*sqrt(3.14159265358979323846)/(2*s)*diff;
}
int main(void) {
    int checks=0;double worst=0;
    for(int sign=-1;sign<=1;sign+=2)for(int curvature=0;curvature<4;curvature++)
    for(int slope=0;slope<3;slope++)for(int scale=0;scale<3;scale++) {
        double cs[4]={0,-.025,-.1,-.25},bs[3]={1,2,4};
        double b=sign*bs[slope],c=cs[curvature],amp=scale==0?1:scale==1?1e-100:1e100;
        double v[4];for(int j=0;j<4;j++)v[j]=amp*integral(b,c,j-2);
        double face,gradient;assert(physics_sim_logquadratic_fit(v[0],v[1],v[2],v[3],.002,&face,&gradient));
        double error=fmax(fabs(face/amp-1),fabs(gradient/(amp*b/.002)-1));worst=fmax(worst,error);assert(error<2e-7);
        double reflected,rg;assert(physics_sim_logquadratic_fit(v[3],v[2],v[1],v[0],.002,&reflected,&rg));
        assert(fabs(reflected/face-1)<2e-10 && fabs(rg/gradient+1)<2e-10);checks++;
    }
    double cases[][4]={{1,.002,0,0},{1,2,1,.5},{-2,-1,1,2},{1,2,4,16}};
    for(int j=0;j<4;j++) {
        double *v=cases[j],f,g,oldf,oldg;
        assert(!physics_sim_logquadratic_fit(v[0],v[1],v[2],v[3],.002,&f,&g));
        physics_sim_scalar_logquadratic(v[0],v[1],v[2],v[3],.01,.002,&f,&g);
        physics_sim_scalar_front_target(v[0],v[1],v[2],v[3],.01,.002,&oldf,&oldg);
        assert(f==oldf && g==oldg);checks++;
    }
    printf("{\"analytic_fit_checks\":%d,\"maximum_relative_error\":%.17g}\n",checks,worst);
}
