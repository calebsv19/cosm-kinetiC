#ifndef PHYSICS_SIM_LOW_MACH_ROOM_IO_H
#define PHYSICS_SIM_LOW_MACH_ROOM_IO_H
#include "app/cfd_lowmach_room.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

/* Trusted-local, versioned hex-float checkpoint. Geometry is reconstructed
 * from its typed configuration, never accepted as unchecked pointer/index data.
 * Exact bytes are bound by the Python assessment's SHA-256 manifest. */
static bool lm_save(const CfdLmRoom *s,const char *path) {
    FILE *f=fopen(path,"wx");if(!f)return false;
    fprintf(f,"CP2C1 %d %d %d %a %a %a %" PRIu64 " %d %d\n",s->n,s->vent,s->obstacle,
            s->gravity,s->p0,s->time,s->steps,s->cells,s->faces);
    for(int q=0;q<s->cells;q++)fprintf(f,"%a %a %a\n",s->rho[q],s->enthalpy[q],s->pressure[q]);
    for(int q=0;q<s->faces;q++)fprintf(f,"%a\n",s->velocity[q]);
    bool ok=!ferror(f);return fclose(f)==0&&ok;
}
static bool lm_load(CfdLmRoom *s,const char *path) {
    FILE *f=fopen(path,"r");if(!f)return false;
    CfdLmRoom t={0};char magic[8];int n,v,o,cells,faces;uint64_t steps;
    double gravity,p0,time;
    bool ok=fscanf(f,"%7s %d %d %d %la %la %la %" SCNu64 " %d %d",magic,&n,&v,&o,&gravity,&p0,&time,&steps,&cells,&faces)==10;
    ok=ok&&!strcmp(magic,"CP2C1")&&(v==0||v==1)&&(o==0||o==1)&&cfd_lm_create(&t,n,v!=0,o!=0,gravity);
    ok=ok&&cells==t.cells&&faces==t.faces;
    if(ok) {
        t.p0=p0;t.time=time;t.steps=steps;
        for(int q=0;q<t.cells&&ok;q++)ok=fscanf(f,"%la %la %la",&t.rho[q],&t.enthalpy[q],&t.pressure[q])==3;
        for(int q=0;q<t.faces&&ok;q++)ok=fscanf(f,"%la",&t.velocity[q])==1;
        char extra;ok=ok&&fscanf(f," %c",&extra)==EOF&&cfd_lm_valid(&t);
    }
    fclose(f);
    if(ok) {*s=t;return true;}cfd_lm_destroy(&t);return false;
}
#endif
