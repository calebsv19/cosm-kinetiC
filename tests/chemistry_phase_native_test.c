#include "physics/chemistry/phase.h"
#include <assert.h>
#include <math.h>
#include <string.h>
int main(void) {
    ChemPhaseModel m={298.15,1.2562194750835444e-5,-9591.795293192527,5e-7,5e-7,1e-4,1e-9};
    ChemState s={0};s.amount[0]=5000*m.gas_volume/(8.31446261815324*m.temperature);double E=0,Q;
    double initial=s.amount[0],a=m.area*m.permeance*8.31446261815324*m.temperature/m.gas_volume,b=m.area*m.permeance/(m.henry*m.liquid_volume);
    assert(chem_phase_step(&m,&s,&E,100,&Q));double exact=initial*a/(a+b)*(1-exp(-(a+b)*100));assert(fabs(s.amount[1]-exact)<1e-20);assert(E<0 && Q==E);
    ChemState before=s;double Eb=E;Q=123;assert(!chem_phase_step(&m,&s,&E,1e7,&Q));assert(!memcmp(&s,&before,sizeof s) && E==Eb && Q==123);
    ChemGrid gas={{4,4,4},{0,0,0},{.01,.01,.01},{1e-5,0,0},1e-8},liquid=gas;liquid.velocity[0]=-1e-5;liquid.diffusion=2e-9;
    ChemState cells[64]={0},saved[64];double energy[64]={0},heat[64],saved_energy[64],sumQ=0;
    for(int q=0;q<64;q++) cells[q].amount[0]=initial*(1+.1*sin((q%4+.5)*2*3.141592653589793/4));
    for(int i=0;i<100;i++) {assert(chem_phase_spatial_step(&m,&gas,&liquid,cells,energy,1,heat));for(int q=0;q<64;q++) sumQ+=heat[q];}
    double sumE=0,total=0;for(int q=0;q<64;q++) {sumE+=energy[q];total+=cells[q].amount[0]+cells[q].amount[1];}assert(fabs(sumE-sumQ)<1e-14);assert(fabs(total-initial*64)<1e-16);
    memcpy(saved,cells,sizeof cells);memcpy(saved_energy,energy,sizeof energy);for(int q=0;q<64;q++) heat[q]=123;gas.diffusion=1;
    assert(!chem_phase_spatial_step(&m,&gas,&liquid,cells,energy,1,heat));assert(!memcmp(cells,saved,sizeof cells) && !memcmp(energy,saved_energy,sizeof energy));for(int q=0;q<64;q++) assert(heat[q]==123);
    return 0;
}
