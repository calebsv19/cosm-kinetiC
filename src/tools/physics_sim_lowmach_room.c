#include "app/cfd_lowmach_room.h"
#include "app/cfd_memory.h"
#include "physics_sim_lowmach_room_io.h"
#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

static bool number(const char *arg,double *v) {
    char *end;errno=0;*v=strtod(arg,&end);return !errno&&*end=='\0'&&isfinite(*v);
}
int main(int argc,char **argv) {
    if(argc!=7 && argc!=8) {
        fprintf(stderr,"usage: %s cold|sealed|vent|plume|nogravity n dt steps checkpoint history [restart]\n",argv[0]);return 2;
    }
    const char *mode=argv[1];bool cold=!strcmp(mode,"cold"),sealed=!strcmp(mode,"sealed"),
        vent=!strcmp(mode,"vent"),plume=!strcmp(mode,"plume"),nog=!strcmp(mode,"nogravity");
    double nn,dt,st;
    if(!(cold||sealed||vent||plume||nog)||!number(argv[2],&nn)||!number(argv[3],&dt)||!number(argv[4],&st)||
       (nn!=16&&nn!=32)||st<0||st>4000||st!=floor(st)||dt<=0||dt>.01)return 2;
    CfdMemoryBudget budget={.limit_bytes=128*1024*1024};cfd_memory_scope(&budget);
    CfdLmRoom s={0};double gravity=cold||plume?-9.81:0;
    bool ok=argc==8?lm_load(&s,argv[7]):cfd_lm_create(&s,(int)nn,!sealed,true,gravity);
    if(!ok || s.n!=(int)nn || s.vent==sealed || !s.obstacle || s.gravity!=gravity ||
       s.time+dt*st>.200000000001) {cfd_lm_destroy(&s);return 3;}
    double *heat=cfd_memory_calloc((size_t)s.cells,sizeof(double));if(!heat){cfd_lm_destroy(&s);return 3;}
    if(plume||nog)cfd_lm_heater(&s,1e6,heat);
    if(sealed||vent)for(int q=0;q<s.cells;q++)if(!s.solid[q])heat[q]=1e5;
    FILE *history=fopen(argv[6],"wx");if(!history){cfd_memory_free(heat);cfd_lm_destroy(&s);return 3;}
    fprintf(history,"step,time_s,p0_pa,heat_j,outward_mass_kg,outward_enthalpy_j,pressure_work_j,mass_residual_kg,enthalpy_residual_j,eos_relative_error,divergence_error,courant,max_mach,iterations\n");
    for(int i=0;i<(int)st;i++) {
        CfdLmReceipt r;
        if(!cfd_lm_step(&s,dt,heat,&r)) {fprintf(stderr,"step refused at %" PRIu64 " time %.17g\n",s.steps,s.time);ok=false;break;}
        fprintf(history,"%" PRIu64 ",%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%d\n",
            s.steps,s.time,s.p0,r.heat_j,r.outward_mass_kg,r.outward_enthalpy_j,r.pressure_work_j,
            r.mass_residual_kg,r.enthalpy_residual_j,r.eos_relative_error,r.divergence_error,r.advective_courant,r.max_mach,r.pressure_iterations);
    }
    ok=!ferror(history)&&ok;if(fclose(history))ok=false;
    if(ok)ok=lm_save(&s,argv[5]);
    printf("{\"accepted\":%s,\"steps\":%" PRIu64 ",\"time_s\":%.17g,\"peak_native_bytes\":%zu}\n",ok?"true":"false",s.steps,s.time,budget.peak_bytes);
    cfd_memory_free(heat);cfd_lm_destroy(&s);
    if(budget.live_bytes)return 5;
    cfd_memory_scope(NULL);return ok?0:4;
}
