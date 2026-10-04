#include "app/cfd_obstacle3d_box.h"
#include "app/cfd_obstacle3d_pressure_trace.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static bool cancel(void *unused) { (void)unused; return false; }
static void numerical(const CfdObstacle3d *s) {
    assert(s->solved && s->relative_residual <= 1e-11 && s->max_divergence < 1e-8);
    assert(s->flux_error < 1e-9 && s->discrete_energy_imbalance < 1e-9);
    for (int a=0;a<3;a++)
        assert(fabs(s->discrete_momentum_residual[a])/(4*s->inlet_pressure)<1e-9);
    assert(isfinite(s->physical_dissipation) && s->physical_dissipation>0);
}
static void symmetry(const CfdObstacle3d *a, const CfdObstacle3d *b, bool swap) {
    double error_u=0,norm_u=0,error_p=0,norm_p=0;
    for (int q=0;q<a->count;q++) {
        int axis,c[3];double x[3];
        cfd_obstacle_mixed3d_position(a->mixed,q,&axis,x,c);
        int target_axis=axis;
        double sign=1;
        if (swap) {
            int temp=c[1];c[1]=c[2];c[2]=temp;
            target_axis=axis==1?2:axis==2?1:0;
        } else {
            c[1]=a->grid.n[1]-c[1]-(axis==1?0:1);
            if(axis==1) sign=-1;
        }
        int target=cfd_obstacle_mixed3d_index(b->mixed,target_axis,c[0],c[1],c[2]);
        assert(target>=0);
        double expected=sign*a->u[q],d=b->u[target]-expected;
        error_u+=d*d;norm_u+=expected*expected;
    }
    for (int k=0;k<a->grid.n[2];k++) for(int j=0;j<a->grid.n[1];j++)
        for(int i=0;i<a->grid.n[0];i++) {
            int q=cfd_obstacle_mixed3d_cell(a->mixed,i,j,k);
            int jj=swap?k:a->grid.n[1]-1-j,kk=swap?j:k;
            int t=cfd_obstacle_mixed3d_cell(b->mixed,i,jj,kk);
            assert((q>=0)==(t>=0));
            if(q<0) continue;
            double d=b->p[t]-a->p[q];error_p+=d*d;norm_p+=a->p[q]*a->p[q];
        }
    double force_error=0,force_norm=0;
    for(int axis=0;axis<3;axis++) {
        int t=swap?(axis==1?2:axis==2?1:0):axis;
        double sign=(!swap&&axis==1)?-1:1;
        double dp=b->pressure_force[t]-sign*a->pressure_force[axis];
        double dv=b->viscous_force[t]-sign*a->viscous_force[axis];
        force_error+=dp*dp+dv*dv;
        force_norm+=a->pressure_force[axis]*a->pressure_force[axis]+a->viscous_force[axis]*a->viscous_force[axis];
    }
    double ur=sqrt(error_u/norm_u),pr=sqrt(error_p/norm_p),fr=sqrt(force_error/force_norm);
    double scalar=fmax(fabs(b->inlet_pressure/a->inlet_pressure-1),
                       fabs(b->physical_dissipation/a->physical_dissipation-1));
    assert(ur<1e-8 && pr<1e-8 && fr<1e-8 && scalar<1e-8);
    printf("{\"transform\":\"%s\",\"velocity_relative_error\":%.17g,\"pressure_relative_error\":%.17g,"
           "\"force_relative_error\":%.17g,\"scalar_relative_error\":%.17g}\n",swap?"YZ_exchange":"Y_reflection",ur,pr,fr,scalar);
}
int main(void) {
    int n[3]={32,16,16};double length[3]={4,2,2},lo[3]={1.5,.5,.5},hi[3]={2.5,1.5,1.5};
    CfdMemoryBudget budget={.limit_bytes=256*1024*1024};
    CfdMemoryBudget *previous=cfd_memory_scope(&budget);
    CfdObstacle3d original,box;
    assert(cfd_obstacle3d_init(&original,n,length,1,.1,.008,2));
    assert(cfd_obstacle3d_box_init(&box,n,length,1,.1,.008,lo,hi));
    size_t unit_owned=budget.live_bytes/2;
    assert(cfd_obstacle_mixed3d_verify(box.mixed));
    assert(cfd_obstacle3d_solve(&original) && cfd_obstacle3d_solve(&box));
    numerical(&box);
    assert(original.count==box.count && original.cells==box.cells);
    assert(!memcmp(original.u,box.u,(size_t)box.count*sizeof(double)));
    assert(!memcmp(original.p,box.p,(size_t)box.cells*sizeof(double)));
    assert(!memcmp(original.pressure_force,box.pressure_force,sizeof(box.pressure_force)));
    assert(!memcmp(original.viscous_force,box.viscous_force,sizeof(box.viscous_force)));
    assert(original.inlet_pressure==box.inlet_pressure && original.physical_dissipation==box.physical_dissipation);
    cfd_obstacle3d_destroy(&original);cfd_obstacle3d_destroy(&box);assert(budget.live_bytes==0);
    int anisotropic[3]={32,16,32},transposed[3]={32,32,16};
    double lower[3]={1.5,.375,.375},upper[3]={2.25,1.125,1.625};
    double reflected_lower[3]={1.5,.875,.375},reflected_upper[3]={2.25,1.625,1.625};
    double exchanged_lower[3]={1.5,.375,.375},exchanged_upper[3]={2.25,1.625,1.125};
    CfdObstacle3d reflected,exchanged;
    assert(cfd_obstacle3d_box_init(&box,anisotropic,length,1,.1,.008,lower,upper));
    assert(cfd_obstacle3d_box_init(&reflected,anisotropic,length,1,.1,.008,reflected_lower,reflected_upper));
    assert(cfd_obstacle3d_box_init(&exchanged,transposed,length,1,.1,.008,exchanged_lower,exchanged_upper));
    assert(cfd_obstacle3d_solve(&box) && cfd_obstacle3d_solve(&reflected) && cfd_obstacle3d_solve(&exchanged));
    numerical(&box);numerical(&reflected);numerical(&exchanged);
    symmetry(&box,&reflected,false);symmetry(&box,&exchanged,true);
    CfdObstacle3dPressureTrace trace;
    assert(cfd_obstacle3d_pressure_trace_diagnostic(&box,box.p,(size_t)box.cells,&trace));
    double pressure_before=box.inlet_pressure,force_before=box.pressure_force[0],value_before=box.u[0];
    cfd_obstacle_mixed3d_checkpoint(box.mixed,cancel,NULL);
    assert(!cfd_obstacle3d_solve(&box));
    assert(box.inlet_pressure==pressure_before && box.pressure_force[0]==force_before && box.u[0]==value_before);
    cfd_obstacle3d_destroy(&box);cfd_obstacle3d_destroy(&reflected);cfd_obstacle3d_destroy(&exchanged);
    assert(budget.live_bytes==0);
    int failures=0;
    for(int axis=0;axis<3;axis++) for(int kind=0;kind<5;kind++) {
        double l[3],h[3];memcpy(l,lo,sizeof(l));memcpy(h,hi,sizeof(h));
        if(kind==0) l[axis]=NAN;
        if(kind==1) h[axis]=INFINITY;
        if(kind==2) h[axis]=l[axis];
        if(kind==3) l[axis]+=.01;
        if(kind==4) l[axis]=0;
        assert(!cfd_obstacle3d_box_init(&box,n,length,1,.1,.008,l,h));
        cfd_obstacle3d_destroy(&box);assert(budget.live_bytes==0);failures++;
    }
    assert(!cfd_obstacle3d_box_init(&box,n,length,1,.1,1,lo,hi));
    cfd_obstacle3d_destroy(&box);failures++;
    for(size_t cap=1024;cap<unit_owned;cap*=2) {
        budget.limit_bytes=cap;
        assert(!cfd_obstacle3d_box_init(&box,n,length,1,.1,.008,lo,hi));
        cfd_obstacle3d_destroy(&box);assert(budget.live_bytes==0);failures++;
    }
    budget.limit_bytes=unit_owned;
    assert(cfd_obstacle3d_box_init(&box,n,length,1,.1,.008,lo,hi));
    cfd_obstacle3d_destroy(&box);assert(budget.live_bytes==0);
    budget.limit_bytes=unit_owned-1;
    assert(!cfd_obstacle3d_box_init(&box,n,length,1,.1,.008,lo,hi));
    cfd_obstacle3d_destroy(&box);assert(budget.live_bytes==0);failures++;
    cfd_memory_scope(previous);
    printf("{\"status\":\"passed_box_geometry_and_symmetry_contract\",\"cube_bitwise_parity\":true,"
           "\"failure_controls\":%d,\"peak_owned_bytes\":%zu,\"physical_absolute_force_certification\":false}\n",failures,budget.peak_bytes);
    return 0;
}
