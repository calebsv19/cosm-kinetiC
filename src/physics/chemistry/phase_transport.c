#include "physics/chemistry/phase.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
static bool same_geometry(const ChemGrid *a,const ChemGrid *b) {
    for(int i=0;i<3;i++) if(a->n[i]!=b->n[i] || a->boundary[i]!=b->boundary[i] || a->spacing[i]!=b->spacing[i]) return false;
    return true;
}
bool chem_phase_spatial_step(const ChemPhaseModel *m,const ChemGrid *gas,const ChemGrid *liquid,ChemState *cells,double *energy,double dt,double *heat) {
    if(!cells || !energy || !heat || !chem_phase_valid(m) || !chem_grid_valid(gas) || !chem_grid_valid(liquid) || !same_geometry(gas,liquid) || !isfinite(dt) || dt<0 || dt>1e6) return false;
    double volume=gas->spacing[0]*gas->spacing[1]*gas->spacing[2];
    if(fabs(volume-m->gas_volume-m->liquid_volume)>1e-12*volume) return false;
    int n=gas->n[0]*gas->n[1]*gas->n[2];size_t bytes=(size_t)n*sizeof(ChemState);
    ChemState *c=malloc(bytes),*field=calloc((size_t)n,sizeof(ChemState));double *data=calloc((size_t)3*n,sizeof(double));bool ok=c && field && data;
    if(!ok) goto done;
    memcpy(c,cells,bytes);memcpy(data,energy,(size_t)n*sizeof(double));double N0=0,N1=0,E0=0,E1=0,Q=0;
    for(int q=0;q<n;q++) {
        N0+=cells[q].amount[0]+cells[q].amount[1];E0+=energy[q];
        if(!chem_phase_step(m,c+q,data+q,dt*.5,data+n+q)) {ok=false;goto done;}
    }
    for(int q=0;q<n;q++) data[2*n+q]=c[q].amount[1];
    ChemMechanism identity={0};identity.species=identity.reactions=identity.elements=1;strcpy(identity.id[0],"oxygen");identity.atoms[0][0]=2;identity.atomic_mass[0]=.015999;identity.reactant[0][0]=identity.product[0][0]=identity.order[0]=1;
    for(int phase=0;phase<2;phase++) {
        memset(field,0,bytes);for(int q=0;q<n;q++) field[q].amount[0]=c[q].amount[phase];
        if(!chem_spatial_step(&identity,phase==0?gas:liquid,field,dt,false)) {ok=false;goto done;}
        for(int q=0;q<n;q++) c[q].amount[phase]=field[q].amount[0];
    }
    for(int q=0;q<n;q++) {
        data[q]+=m->delta_u*(c[q].amount[1]-data[2*n+q]);double extra;
        if(!chem_phase_step(m,c+q,data+q,dt*.5,&extra)) {ok=false;goto done;}
        data[n+q]+=extra;Q+=data[n+q];N1+=c[q].amount[0]+c[q].amount[1];E1+=data[q];
    }
    if(!isfinite(N1) || !isfinite(Q) || fabs(N1-N0)>1e-11*fmax(1e-12,N0) || fabs(E1-E0-Q)>1e-10*fmax(1e-12,fmax(fabs(E0),fabs(E1)))) {ok=false;goto done;}
    memcpy(cells,c,bytes);memcpy(energy,data,(size_t)n*sizeof(double));memcpy(heat,data+n,(size_t)n*sizeof(double));
    done:free(c);free(field);free(data);return ok;
}
