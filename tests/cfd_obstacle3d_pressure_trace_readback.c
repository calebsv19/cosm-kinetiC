/* Reconstruct the complete exported SI field and recheck original native equations. */
#include "../src/app/cfd_obstacle3d_mixed.c"
#include "../src/app/cfd_obstacle3d.c"
#include "cfd_obstacle3d_wall_pressure_candidate.h"
#include "app/cfd_obstacle3d_pressure_trace.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
int main(int argc, char **argv) {
    assert(argc==6);
    int n=atoi(argv[1]);double length=atof(argv[2]),pin=atof(argv[3]);
    assert((n==16||n==32||n==64||n==80)&&(length==4||length==8)&&isfinite(pin)&&pin>0);
    int dims[3]={(int)(length*n/2),n,n};double lengths[3]={length,2,2};
    CfdMemoryBudget budget={.limit_bytes=1024*1024*1024};CfdMemoryBudget *previous=cfd_memory_scope(&budget);
    CfdObstacle3d s;assert(cfd_obstacle3d_init(&s,dims,lengths,1,.1,.008,length/2));
    FILE *f=fopen(argv[4],"rb");assert(f);int header[3];assert(fread(header,sizeof(int),3,f)==3);
    assert(!memcmp(header,dims,sizeof(header)));int velocities=0,pressures=0;
    for(int k=0;k<n;k++)for(int j=0;j<n;j++)for(int i=0;i<dims[0];i++){
        double row[4];assert(fread(row,sizeof(double),4,f)==4);
        for(int a=0;a<3;a++){
            int q=cfd_obstacle_mixed3d_index(s.mixed,a,i,j,k);assert(isfinite(row[a]));
            if(q>=0){s.u[q]=row[a];velocities++;}else assert(row[a]==0);
        }
        int q=cfd_obstacle_mixed3d_cell(s.mixed,i,j,k);
        if(q>=0){assert(isfinite(row[3]));s.p[q]=row[3];pressures++;}else assert(isnan(row[3]));
    }
    for(int k=0;k<n;k++)for(int j=0;j<n;j++){
        double v;assert(fread(&v,sizeof(double),1,f)==1&&isfinite(v));
        int q=cfd_obstacle_mixed3d_index(s.mixed,0,dims[0],j,k);assert(q>=0);s.u[q]=v;velocities++;
    }
    assert(fgetc(f)==EOF&&fclose(f)==0&&velocities==s.count&&pressures==s.cells);
    s.inlet_pressure=pin;cfd_obstacle3d_measure(&s,s.u,s.p);
    double candidate[3]={0};
    for(int a=0;a<3;a++)for(int side=0;side<2;side++)
        for(int k=s.lo[2];k<s.hi[2];k++)for(int j=s.lo[1];j<s.hi[1];j++)for(int i=s.lo[0];i<s.hi[0];i++){
            int c[3]={i,j,k},direction=side?1:-1;if(c[a]!=s.lo[a])continue;
            c[a]=side?s.hi[a]:s.lo[a]-1;candidate_pressure_load(&s,s.p,a,direction,c,candidate);
        }
    CfdObstacle3dPressureTrace integrated;
    assert(cfd_obstacle3d_pressure_trace_diagnostic(&s,s.p,(size_t)s.cells,&integrated));
    for(int a=0;a<3;a++) assert(fabs(integrated.force_n[a]-candidate[a])<1e-13);
    assert(integrated.interval_depth_patches[0]==0&&integrated.interval_depth_patches[1]==0);
    assert(integrated.interval_depth_patches[2]==6*(size_t)(n/2)*(n/2));
    transpose(s.mixed,s.p,s.rhs);
    for(int k=0;k<n;k++)for(int j=0;j<n;j++){
        int q=cfd_obstacle_mixed3d_index(s.mixed,0,0,j,k);s.rhs[q]+=pin*s.grid.area[0];
    }
    double defect2=0,scale2=0,load2=(double)n*n*pow(pin*s.grid.area[0],2);
    for(int a=0;a<3;a++){
        int offset=s.mixed->offset[a];apply(&s.mixed->h[a],s.u+offset,s.candidate_u+offset);
        for(int q=0;q<s.mixed->h[a].n;q++){
            double error=s.candidate_u[offset+q]-s.rhs[offset+q];defect2+=error*error;scale2+=s.rhs[offset+q]*s.rhs[offset+q];
        }
    }
    double complete=sqrt(defect2/scale2),load_relative=sqrt(defect2/load2);
    double discrete=fabs(s.discrete_momentum_residual[0])/(4*pin);
    double original_closure=fabs(s.momentum_residual[0])/(4*pin);
    double candidate_closure=fabs(s.momentum_residual[0]-(candidate[0]-s.pressure_force[0]))/(4*pin);
    assert(complete<=1e-11&&s.max_divergence<1e-8&&s.flux_error<1e-9&&discrete<1e-9&&s.discrete_energy_imbalance<1e-9);
    FILE *out=fopen(argv[5],"wx");assert(out);
    fprintf(out,"{\"n\":%d,\"length\":%.17g,\"loaded_velocity_faces\":%d,\"loaded_pressure_cells\":%d,"
        "\"original_complete_scaled_momentum_residual\":%.17g,\"momentum_relative_to_inlet_load\":%.17g,"
        "\"maximum_divergence\":%.17g,\"flux_error\":%.17g,\"discrete_momentum_relative_residual\":%.17g,"
        "\"discrete_energy_imbalance\":%.17g,\"physical_energy_imbalance\":%.17g,"
        "\"current_streamwise_momentum_closure\":%.17g,\"candidate_streamwise_momentum_closure\":%.17g,"
        "\"pressure_force_n\":[%.17g,%.17g,%.17g],\"candidate_pressure_force_n\":[%.17g,%.17g,%.17g],"
        "\"viscous_force_n\":[%.17g,%.17g,%.17g],\"peak_owned_bytes\":%zu}\n",
        n,length,velocities,pressures,complete,load_relative,s.max_divergence,s.flux_error,discrete,s.discrete_energy_imbalance,
        s.physical_energy_imbalance,original_closure,candidate_closure,s.pressure_force[0],s.pressure_force[1],s.pressure_force[2],
        candidate[0],candidate[1],candidate[2],s.viscous_force[0],s.viscous_force[1],s.viscous_force[2],budget.peak_bytes);
    assert(fclose(out)==0);cfd_obstacle3d_destroy(&s);assert(budget.live_bytes==0);cfd_memory_scope(previous);
    puts("Complete exported SI field, original momentum/divergence, strict flux, discrete conservation and pressure observer readback verified");
}
