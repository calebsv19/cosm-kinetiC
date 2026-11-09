#include "physics/chemistry/flow.h"
#include <assert.h>
#include <math.h>
#include <string.h>
int main(void) {
 ChemMechanism m={0};m.species=2;m.reactions=1;m.elements=1;strcpy(m.id[0],"A");strcpy(m.id[1],"B");m.atoms[0][0]=m.atoms[1][0]=1;m.atomic_mass[0]=.01;m.reactant[0][0]=1;m.product[0][1]=1;m.order[0]=1;m.k[0]=.1;
 ChemGrid g={{4,4,4},{0,0,0},{.25,.25,.25},{0,0,0},.01};ChemFlowModel f={0};f.rho=1000;f.nu=.1;f.gravity[1]=-1;f.beta[0]=.001;f.reference[0]=1;f.feedback=1;
 ChemState c[64]={0},before[64];double v[192]={0},p[64]={0},vb[192],pb[64];ChemFlowMetrics metrics={0};
 for(int q=0;q<64;q++) c[q].amount[0]=(1+.2*sin(2*3.141592653589793*(q%4+.5)/4))/64;
 for(int t=0;t<10;t++) assert(chem_flow_step(&m,&g,&f,c,v,p,.002,true,&metrics));assert(metrics.reynolds<.1);assert(metrics.divergence<1e-8);
 memcpy(before,c,sizeof c);memcpy(vb,v,sizeof v);memcpy(pb,p,sizeof p);f.beta[0]=1;
 assert(!chem_flow_step(&m,&g,&f,c,v,p,.002,true,&metrics));assert(!memcmp(before,c,sizeof c)&&!memcmp(vb,v,sizeof v)&&!memcmp(pb,p,sizeof p));
 f.beta[0]=.001;g.diffusion=100;assert(!chem_flow_step(&m,&g,&f,c,v,p,.002,true,&metrics));assert(!memcmp(before,c,sizeof c)&&!memcmp(vb,v,sizeof v)&&!memcmp(pb,p,sizeof p));
 return 0;
}
