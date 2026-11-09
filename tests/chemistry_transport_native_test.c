#include "physics/chemistry/transport.h"
#include <assert.h>
#include <math.h>
#include <string.h>
int main(void) {
 ChemMechanism m={0};m.species=2;m.reactions=1;m.elements=1;
 strcpy(m.id[0],"A");strcpy(m.id[1],"B");m.atoms[0][0]=m.atoms[1][0]=1;
 m.atomic_mass[0]=.01;m.reactant[0][0]=1;m.product[0][1]=1;m.order[0]=1;m.k[0]=.1;
 ChemGrid g={{4,3,2},{0,0,0},{.25,.3,.5},{.1,-.1,.05},.01};
 ChemState cells[24]={0},before[24];for(int i=0;i<24;i++) cells[i].amount[0]=.01*(i+1);
 for(int i=0;i<100;i++) assert(chem_spatial_step(&m,&g,cells,.01,true));
 double total=0;for(int i=0;i<24;i++) total+=cells[i].amount[0]+cells[i].amount[1];assert(fabs(total-3)<1e-12);
 memcpy(before,cells,sizeof cells);assert(!chem_spatial_step(&m,&g,cells,100,true));assert(!memcmp(before,cells,sizeof cells));
 assert(chem_spatial_step(&m,&g,cells,0,true));assert(!memcmp(before,cells,sizeof cells));
 g.n[0]=4097;assert(!chem_grid_valid(&g));g.n[0]=4;g.boundary[0]=1;assert(!chem_grid_valid(&g));
 return 0;
}
