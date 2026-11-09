#include "physics/chemistry/thermal.h"
#include <assert.h>
#include <math.h>
#include <string.h>
int main(void) {
 ChemMechanism m={0};m.species=2;m.reactions=1;m.elements=1;strcpy(m.id[0],"A");strcpy(m.id[1],"B");m.atoms[0][0]=m.atoms[1][0]=1;m.atomic_mass[0]=.01;m.reactant[0][0]=1;m.product[0][1]=1;m.order[0]=1;m.k[0]=.4;
 ChemThermalModel t={0};t.reference_t=300;t.min_t=250;t.max_t=400;t.carrier_cv=1000;t.cv[0]=t.cv[1]=10;t.formation[0]=3000;t.bath_t=300;t.expansion=.001;t.activation[0]=12000;
 ChemState local={0};local.amount[0]=1;double energy=3000,heat,T;
 for(int i=0;i<200;i++) assert(chem_thermal_react(&m,&t,&local,&energy,1,.02,&heat));
 assert(energy==3000 && heat==0);assert(chem_thermal_temperature(&m,&t,&local,1,energy,&T));assert(T>302);
 ChemGrid g={{4,4,4},{0,0,0},{.25,.25,.25},{0,0,0},.01};ChemFlowModel f={0};f.rho=1000;f.nu=.1;f.gravity[1]=-1;f.beta[0]=.001;f.feedback=1;
 ChemState c[64]={0},before[64];double v[192]={0},p[64]={0},E[64],vb[192],pb[64],Eb[64],visc;ChemFlowMetrics metrics={0};
 for(int q=0;q<64;q++) {c[q].amount[0]=1./64;T=300+.2*sin(2*3.141592653589793*(q%4+.5)/4);assert(chem_thermal_energy(&m,&t,c+q,1./64,T,E+q));}
 for(int i=0;i<100;i++) assert(chem_thermal_flow_step(&m,&g,&f,&t,c,v,p,E,.002,true,&metrics,&heat,&visc));
 assert(metrics.reynolds<.1 && metrics.divergence<1e-8);
 memcpy(before,c,sizeof c);memcpy(vb,v,sizeof v);memcpy(pb,p,sizeof p);memcpy(Eb,E,sizeof E);t.power=1e12;metrics.work=123;heat=456;visc=789;
 assert(!chem_thermal_flow_step(&m,&g,&f,&t,c,v,p,E,.002,true,&metrics,&heat,&visc));
 assert(!memcmp(before,c,sizeof c)&&!memcmp(vb,v,sizeof v)&&!memcmp(pb,p,sizeof p)&&!memcmp(Eb,E,sizeof E));assert(metrics.work==123 && heat==456 && visc==789);
 return 0;
}
