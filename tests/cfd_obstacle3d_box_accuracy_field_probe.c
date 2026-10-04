/* Complete physical field solve/readback for an explicit rectangular obstacle.
 * Original mixed equations are checked in SI after scaling/export/reload. */
#include "../src/app/cfd_obstacle3d_mixed.c"
#include "app/cfd_obstacle3d_box.h"
#include "app/cfd_obstacle3d_pressure_trace.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static double started;
static double now(void) {
    struct timespec t;timespec_get(&t,TIME_UTC);return t.tv_sec+t.tv_nsec*1e-9;
}
static bool bounded(void *unused) { (void)unused;return now()-started<1800; }
static void write_field(const CfdObstacle3d *s, const char *path) {
    FILE *f=fopen(path,"wx");assert(f);
    int header[11];
    for(int a=0;a<3;a++){header[a]=s->grid.n[a];header[3+a]=s->lo[a];header[6+a]=s->hi[a];}
    header[9]=s->count;header[10]=s->cells;
    double parameters[7]={s->grid.length[0],s->grid.length[1],s->grid.length[2],s->rho,s->mu,s->requested_flow,s->inlet_pressure};
    assert(fwrite("C3DBOX1",1,8,f)==8);
    assert(fwrite(header,sizeof(int),11,f)==11);
    assert(fwrite(parameters,sizeof(double),7,f)==7);
    assert(fwrite(s->u,sizeof(double),(size_t)s->count,f)==(size_t)s->count);
    assert(fwrite(s->p,sizeof(double),(size_t)s->cells,f)==(size_t)s->cells);
    assert(fclose(f)==0);
}
static void read_field(CfdObstacle3d *s,const char *path) {
    FILE *f=fopen(path,"rb");assert(f);char magic[8];int header[11];double parameters[7];
    assert(fread(magic,1,8,f)==8 && !memcmp(magic,"C3DBOX1",8));
    assert(fread(header,sizeof(int),11,f)==11 && fread(parameters,sizeof(double),7,f)==7);
    CfdCartesian3d grid;assert(cfd_cartesian3d_init(&grid,header,parameters));
    double lo[3],hi[3];
    for(int a=0;a<3;a++){lo[a]=header[3+a]*grid.h[a];hi[a]=header[6+a]*grid.h[a];}
    assert(cfd_obstacle3d_box_init(s,header,parameters,parameters[3],parameters[4],parameters[5],lo,hi));
    assert(s->count==header[9] && s->cells==header[10] && parameters[6]>0 && isfinite(parameters[6]));
    assert(fread(s->u,sizeof(double),(size_t)s->count,f)==(size_t)s->count);
    assert(fread(s->p,sizeof(double),(size_t)s->cells,f)==(size_t)s->cells);
    assert(fgetc(f)==EOF && fclose(f)==0);
    for(int q=0;q<s->count;q++)assert(isfinite(s->u[q]));
    for(int q=0;q<s->cells;q++)assert(isfinite(s->p[q]));
    s->inlet_pressure=parameters[6];cfd_obstacle3d_measure(s,s->u,s->p);s->solved=true;
}
static void report(CfdObstacle3d *s,CfdMemoryBudget *budget,bool readback) {
    transpose(s->mixed,s->p,s->rhs);
    double load2=0;
    for(int k=0;k<s->grid.n[2];k++)for(int j=0;j<s->grid.n[1];j++) {
        int q=cfd_obstacle_mixed3d_index(s->mixed,0,0,j,k);
        double load=s->inlet_pressure*s->grid.area[0];s->rhs[q]+=load;load2+=load*load;
    }
    double defect2=0,scale2=0;
    for(int a=0;a<3;a++) {
        int offset=s->mixed->offset[a];
        apply(&s->mixed->h[a],s->u+offset,s->candidate_u+offset);
        for(int q=0;q<s->mixed->h[a].n;q++) {
            double r=s->candidate_u[offset+q]-s->rhs[offset+q];
            defect2+=r*r;scale2+=s->rhs[offset+q]*s->rhs[offset+q];
        }
    }
    double complete=sqrt(defect2/scale2),load_relative=sqrt(defect2/load2),discrete=0,physical=0;
    for(int a=0;a<3;a++) {
        discrete=fmax(discrete,fabs(s->discrete_momentum_residual[a])/(4*s->inlet_pressure));
        physical=fmax(physical,fabs(s->momentum_residual[a])/(4*s->inlet_pressure));
    }
    assert(complete<=1e-11 && s->max_divergence<1e-8 && s->flux_error<1e-9 &&
           discrete<1e-9 && s->discrete_energy_imbalance<1e-9);
    CfdObstacle3dPressureTrace trace;
    assert(cfd_obstacle3d_pressure_trace_diagnostic(s,s->p,(size_t)s->cells,&trace));
    double lo[3],hi[3];for(int a=0;a<3;a++){lo[a]=s->lo[a]*s->grid.h[a];hi[a]=s->hi[a]*s->grid.h[a];}
    printf("{\"readback\":%s,\"grid\":[%d,%d,%d],\"length_m\":%.17g,\"lower_m\":[%.17g,%.17g,%.17g],\"upper_m\":[%.17g,%.17g,%.17g],"
           "\"faces\":%d,\"fluid_cells\":%d,\"pressure_drop_pa\":%.17g,\"pressure_force_n\":[%.17g,%.17g,%.17g],"
           "\"viscous_force_n\":[%.17g,%.17g,%.17g],\"diagnostic_pressure_force_n\":[%.17g,%.17g,%.17g],"
           "\"physical_dissipation_w\":%.17g,\"physical_boundary_power_w\":%.17g,\"physical_energy_imbalance\":%.17g,"
           "\"physical_momentum_relative_max\":%.17g,\"discrete_momentum_relative_max\":%.17g,\"discrete_energy_imbalance\":%.17g,"
           "\"complete_scaled_momentum_residual\":%.17g,\"momentum_relative_to_load\":%.17g,\"divergence\":%.17g,\"flux_error\":%.17g,"
           "\"owner_peak_bytes\":%zu,\"wall_s\":%.17g,\"physical_absolute_force_certification\":false}\n",
           readback?"true":"false",s->grid.n[0],s->grid.n[1],s->grid.n[2],s->grid.length[0],lo[0],lo[1],lo[2],hi[0],hi[1],hi[2],
           s->count,s->cells,s->inlet_pressure,s->pressure_force[0],s->pressure_force[1],s->pressure_force[2],
           s->viscous_force[0],s->viscous_force[1],s->viscous_force[2],trace.force_n[0],trace.force_n[1],trace.force_n[2],
           s->physical_dissipation,s->physical_power,s->physical_energy_imbalance,physical,discrete,s->discrete_energy_imbalance,
           complete,load_relative,s->max_divergence,s->flux_error,budget->peak_bytes,now()-started);
}
int main(int argc,char **argv) {
    started=now();assert(argc==12 || (argc==3 && !strcmp(argv[1],"--readback")));
    CfdMemoryBudget budget={.limit_bytes=1024*1024*1024};CfdMemoryBudget *previous=cfd_memory_scope(&budget);
    CfdObstacle3d s;bool readback=argc==3;
    if(readback) read_field(&s,argv[2]);
    else {
        int n[3];double lo[3],hi[3];
        for(int a=0;a<3;a++){n[a]=atoi(argv[1+a]);lo[a]=atof(argv[5+a]);hi[a]=atof(argv[8+a]);}
        double length[3]={atof(argv[4]),2,2};
        assert(cfd_obstacle3d_box_init(&s,n,length,1,.1,.008,lo,hi));
        assert(cfd_obstacle_mixed3d_verify(s.mixed));
        cfd_obstacle_mixed3d_checkpoint(s.mixed,bounded,NULL);
        if(!cfd_obstacle3d_solve(&s)){fprintf(stderr,"%s\n",s.error);return 1;}
        write_field(&s,argv[11]);
    }
    report(&s,&budget,readback);cfd_obstacle3d_destroy(&s);
    assert(budget.live_bytes==0);cfd_memory_scope(previous);return 0;
}
