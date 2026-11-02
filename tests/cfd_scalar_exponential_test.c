#include "../src/tools/physics_sim_scalar_exponential.h"
#include <assert.h>
#include <stdio.h>
int main(void){
    int checks=0;
    for(int sign=-1;sign<=1;sign+=2)for(int j=9;j<=40;j++)for(int scale=0;scale<3;scale++){
        double k=sign*j*.125,amp=scale==0?1:scale==1?1e-100:1e100;
        double mean=amp*expm1(k)/k;
        double ll=mean*exp(-2*k),l=mean*exp(-k),r=mean,rr=mean*exp(k),f,g;
        physics_sim_scalar_exponential(ll,l,r,rr,.01,.002,&f,&g);
        assert(f>0 && fabs(f/amp-1)<1e-12 && fabs(g/(k*amp/.002)-1)<1e-12);
        assert(fabs((f*(-expm1(-k))/k)/l-1)<1e-12);
        assert(fabs((f*expm1(k)/k)/r-1)<1e-12);
        double reverse,rg;physics_sim_scalar_exponential(rr,r,l,ll,-.01,.002,&reverse,&rg);
        assert(f==reverse && g==-rg);checks++;
    }
    double cases[][4]={{0,0,1,1},{1,2,1,.5},{-2,-1,1,2},{1,1,1,1},{1,1.1,1.2,1.3}};
    for(int i=0;i<5;i++){double a,b,c,d;double *v=cases[i];
        physics_sim_scalar_exponential(v[0],v[1],v[2],v[3],.01,.002,&a,&b);
        cfd_scalar_edge_candidate(v[0],v[1],v[2],v[3],.01,.002,&c,&d);
        assert(a==c && b==d);checks++;
    }
    printf("{\"reconstruction_checks\":%d}\n",checks);
}
