#include "app/cfd_mixture3d.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static void state(const CfdMixtureThermo *s,const double y[5],double rho,double t,double u[10]) {
    double e,cp,r;assert(cfd_mixture_properties(s,y,t,&e,&cp,&r));
    u[0]=rho;u[1]=u[2]=u[3]=0;u[4]=rho*e;
    for(int i=0;i<5;++i) u[5+i]=rho*y[i];
}
int main(void) {
    CfdMixtureThermo thermo;assert(cfd_mixture_thermo_init(&thermo));
    double temperatures[]={200,298.15,300,500,999.999999,1000,1000.000001,1300,1500};
    puts("[");bool comma=false;
    for(int species=0;species<5;++species) for(int j=0;j<9;++j) {
        double y[5]={0},u[10],t,p,c,cv,e,cp,r;y[species]=1;
        state(&thermo,y,1,temperatures[j],u);
        assert(cfd_mixture_primitive(&thermo,u,&t,&p,&c,&cv));
        assert(fabs(t-temperatures[j])<1e-8);
        assert(cfd_mixture_properties(&thermo,y,t,&e,&cp,&r));
        printf("%s{\"species\":%d,\"t\":%.17g,\"e\":%.17g,\"cp\":%.17g,\"formation\":%.17g}",comma?",":"",species,t,e,cp,thermo.formation[species]);comma=true;
    }
    puts("]");
    int n[3]={4,4,4};double length[3]={1,1,1};CfdMixture3d s;
    assert(cfd_mixture3d_init(&s,n,length,0,0));
    double y[5]={.7,.2,0,0,.1},u[10];state(&thermo,y,1,500,u);
    for(int q=0;q<64;++q) memcpy(s.state+10*q,u,sizeof(u));
    double source[640]={0},before[640];memcpy(before,s.state,sizeof(before));
    source[5]=-.8;assert(!cfd_mixture3d_step(&s,source,1e-6));
    assert(!memcmp(before,s.state,sizeof(before)));memset(source,0,sizeof(source));
    source[0]=.001;assert(!cfd_mixture3d_step(&s,source,1e-6));
    assert(!memcmp(before,s.state,sizeof(before)));memset(source,0,sizeof(source));
    source[4]=1e9;assert(!cfd_mixture3d_step(&s,source,1e-6));
    assert(!memcmp(before,s.state,sizeof(before)));memset(source,0,sizeof(source));
    assert(!cfd_mixture3d_step(&s,source,1));assert(!memcmp(before,s.state,sizeof(before)));
    assert(cfd_mixture3d_step(&s,source,1e-6));
    assert(!memcmp(before,s.state,sizeof(before)));
    /* Pure fuel injection includes formation and sensible internal energy once. */
    double fuel[5]={0,0,0,0,1},e,cp,r;
    assert(cfd_mixture_properties(&thermo,fuel,400,&e,&cp,&r));
    for(int q=0;q<64;++q) {source[10*q]=.001;source[10*q+4]=.001*e;source[10*q+9]=.001;}
    assert(cfd_mixture3d_step(&s,source,1e-6));
    for(int q=0;q<64;++q) {assert(fabs(s.state[10*q]-1.001)<1e-14);assert(fabs(s.state[10*q+4]-(u[4]+.001*e))<1e-8);}
    cfd_mixture3d_destroy(&s);
    double bad[5]={1,0,0,0,-.001};CfdHotGasCaloric c={0};double f=7;
    assert(!cfd_mixture_model(&thermo,bad,&c,&f));assert(!c.active && f==7);
    double l[10],right[10],flux[10],diff[10],speed;
    state(&thermo,y,1,500,l);state(&thermo,y,1,600,right);
    for(int i=0;i<10;++i) flux[i]=diff[i]=123;
    speed=123;
    assert(!cfd_mixture_flux(&thermo,l,right,3,.01,.1,.1,flux,diff,&speed));
    for(int i=0;i<10;++i) assert(flux[i]==123 && diff[i]==123);
    assert(speed==123);return 0;
}
