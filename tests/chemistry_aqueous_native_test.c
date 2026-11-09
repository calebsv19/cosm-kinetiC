#include "physics/chemistry/transport.h"
#include <assert.h>
#include <math.h>
#include <string.h>
int main(void) {
 ChemMechanism m={0};m.species=2;m.reactions=2;m.elements=3;
 strcpy(m.id[0],"alpha-D-glucopyranose");strcpy(m.id[1],"beta-D-glucopyranose");
 m.atomic_mass[0]=.012011;m.atomic_mass[1]=.001008;m.atomic_mass[2]=.015999;
 for(int i=0;i<2;i++) {m.atoms[i][0]=6;m.atoms[i][1]=12;m.atoms[i][2]=6;m.order[i]=1;}
 m.reactant[0][0]=m.product[0][1]=m.reactant[1][1]=m.product[1][0]=1;
 double lambda=3.204234121092262e-5,K=1.883076923076923;
 m.k[0]=lambda*K/(1+K);m.k[1]=lambda/(1+K);assert(chem_valid(&m));
 ChemState s={0};s.amount[0]=.01;
 for(int i=0;i<200;i++) assert(chem_step(&m,&s,1e-4,30));
 assert(fabs(s.amount[0]-.01*(1/(1+K)+K/(1+K)*exp(-lambda*6000)))<1e-14);
 assert(fabs(s.amount[0]+s.amount[1]-.01)<1e-15);
 ChemState before=s;assert(!chem_step(&m,&s,1e-4,1e8));assert(!memcmp(&s,&before,sizeof s));
 ChemGrid g={{8,4,3},{0,0,0},{.01,.01,.01},{1e-6,0,0},1e-8};ChemState cells[96]={0};
 for(int q=0;q<96;q++) {double a=.5+.2*sin(2*3.141592653589793*(q%8+.5)/8);cells[q].amount[0]=.0002*a;cells[q].amount[1]=.0002*(1-a);}
 for(int t=0;t<100;t++) assert(chem_spatial_step(&m,&g,cells,30,true));
 double sum=0;for(int q=0;q<96;q++) {assert(cells[q].amount[0]>=0 && cells[q].amount[1]>=0);sum+=cells[q].amount[0]+cells[q].amount[1];}assert(fabs(sum-.0192)<1e-14);
 return 0;
}
