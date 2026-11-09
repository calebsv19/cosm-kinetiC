#ifndef PHYSICS_CHEMISTRY_REACTOR_H
#define PHYSICS_CHEMISTRY_REACTOR_H
#include <stdbool.h>
#define CHEM_S 16
#define CHEM_R 32
#define CHEM_E 4
/* Integer stoichiometry, elementary mass action, mol/m3 and seconds. */
typedef struct { int species,reactions,elements; char id[CHEM_S][32];
 int atoms[CHEM_S][CHEM_E],charge[CHEM_S]; double atomic_mass[CHEM_E];
 int reactant[CHEM_R][CHEM_S],product[CHEM_R][CHEM_S],order[CHEM_R];
 double k[CHEM_R]; } ChemMechanism;
typedef struct { double amount[CHEM_S],extent[CHEM_R]; } ChemState;
bool chem_valid(const ChemMechanism *m);
bool chem_rates(const ChemMechanism *m,const ChemState *s,double volume,double *rates);
bool chem_step(const ChemMechanism *m,ChemState *s,double volume,double dt);
#endif
