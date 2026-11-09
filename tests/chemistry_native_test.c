#include "physics/chemistry/reactor.h"
#include <assert.h>
#include <math.h>
#include <string.h>
int main(void) {
 ChemMechanism m={0};m.species=2;m.reactions=1;m.elements=1;
 strcpy(m.id[0],"A");strcpy(m.id[1],"B");m.atoms[0][0]=m.atoms[1][0]=1;
 m.atomic_mass[0]=.01;m.reactant[0][0]=1;m.product[0][1]=1;m.order[0]=1;m.k[0]=.1;
 ChemState s={0};s.amount[0]=1;
 assert(chem_valid(&m));for(int i=0;i<1000;i++) assert(chem_step(&m,&s,1,.01));
 assert(fabs(s.amount[0]-exp(-1))<1e-13);assert(fabs(s.amount[0]+s.amount[1]-1)<1e-13);
 ChemState before=s;assert(!chem_step(&m,&s,1,1000));assert(!memcmp(&before,&s,sizeof s));
 m.atoms[1][0]=2;assert(!chem_valid(&m));assert(!chem_step(&m,&s,1,.01));assert(!memcmp(&before,&s,sizeof s));
 m.species=17;assert(!chem_valid(&m));assert(!chem_valid(0));return 0;
}
