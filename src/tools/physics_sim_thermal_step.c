#include "physics_sim_thermal_step.h"
#include "physics_sim_normal_conservative.h"
#include <float.h>
#include <math.h>
double physics_sim_thermal_solid_energy(const PhysicsSimThermalState *s) {
    double e=0; for(int x=0;x<4;x++)for(int d=0;d<2;d++)e+=s->r[x].solid.native.energy_j[d]; return e;
}
/* Center the normal block around the wall step in the refined profile.
 * Each half uses the original scalar timestep, including exact source-off. */
static bool advance_normal(PhysicsSimThermalState *next,PhysicsSimLocalMatrix *matrix,
                           int begin,int end,int scalar_factor,int normal_cells) {
    for(int now=begin;now<end;now++)for(int sub=0;sub<scalar_factor;sub++) {
        if(normal_cells==512) {
            double dt=1./(TPS*scalar_factor);
            if(!physics_sim_normal_conservative_step(matrix,next->v,next->correction,
                normal_cells,.026/1206,dt,.0625/normal_cells,now<TPS?dt:0,.5))return false;
        } else {
            if(now<TPS)for(int k=0;k<normal_cells;k++)next->v[k]+=1./(TPS*scalar_factor);
            if(!physics_sim_local_matrix_step(matrix,next->v))return false;
        }
        for(int k=0;k<normal_cells;k++)if(!isfinite(next->v[k])||next->v[k]<0||next->v[k]>21+64*DBL_EPSILON*21)return false;
    }
    return true;
}
/* All outputs are local until every wall, packet, scalar and bound check passes.
 * fail=1/2 injects refusal after wall/receiver or scalar work, respectively. */
bool physics_sim_thermal_step_with_receiver(const PhysicsSimThermalState *s, PhysicsSimThermalState *out, CfdOpenAtmosphere3d *gas,
                  PhysicsSimLocalMatrix *matrix, int ticks, int scalar_factor, int mode, int normal_cells, int fail, PhysicsSimThermalTrace *trace,PhysicsSimReceiverStage stage,void *context) {
    if(!s||!out||out==s||!gas||!matrix||!trace||ticks<8||ticks>40||ticks%8||scalar_factor<1||scalar_factor>4||(normal_cells!=256&&normal_cells!=512)||mode<0||mode>2)return false;
    PhysicsSimThermalState next=*s;PhysicsSimThermalTrace tr={0};tr.micro=ticks/MICRO;
    const double cg=1206*gas->grid.volume,dz=.0625/normal_cells;
    if(normal_cells==512&&!advance_normal(&next,matrix,s->tick,s->tick+ticks/2,scalar_factor,normal_cells))return false;
    for(int k=0;k<normal_cells;k++)for(int j=0;j<16;j++)gas->energy[k*16+j]=next.v[k]*cg;
    double before=0;for(int j=0;j<16;j++)before+=gas->energy[j];
    PhysicsSimHeatedFace face[4];
    for(int x=0;x<4;x++) {
        ThermalReceiverObservation o;if(!thermal_receiver_observe(&s->r[x],&o))return false;
        face[x]=(PhysicsSimHeatedFace){2,-1,o.temperature_k[0],o.capacity_j_k[0],
            1/(dz/(2*.026)+.1+.005/(2*.16)),0,o.temperature_rise_k[0]-(o.temperature_k[0]-300)};
    }
    if(mode!=1&&!physics_sim_heated_columns_exchange(gas,face,4,ticks/(double)TPS))return false;
    double after=0;for(int j=0;j<16;j++)after+=gas->energy[j];tr.debit=before-after;
    next.v[0]=after/(16*cg);next.correction[0]=0;
    for(int x=0;x<4;x++) {
        double amounts[100];
        /* Event-truncated host batches need not align to the inert helper
         * fixed-window calendar. Each actual receiver packet still has its
         * native one-step overlap validation. Preserve the exact Q remainder. */
        double allocated=0;
        for(int m=0;m<tr.micro;m++){amounts[m]=m+1==tr.micro?face[x].gas_to_surface_j-allocated:face[x].gas_to_surface_j/tr.micro;allocated+=amounts[m];}
        tr.q+=face[x].gas_to_surface_j;
        for(int m=0;m<tr.micro;m++) {
            ThermalReceiver *r=&next.r[x],candidate;ThermalReceiverExchange packet;ThermalReceiverReceipt receipt;
            CoreSimTimeInterval interval={r->solid.time,{r->config.timebase,r->solid.time.ticks+1}};
            if(!thermal_receiver_exchange_make(r,interval,(uint64_t)s->tick+1,amounts[m],&packet)||
               !(stage?stage(context,x,r,&packet,interval.end,&candidate,&receipt):thermal_receiver_stage(r,&packet,interval.end,&candidate,&receipt)))return false;
            tr.amounts[m]+=amounts[m];tr.allocation+=receipt.allocated_q_j;
            for(int d=0;d<2;d++)tr.energy[m][d]+=candidate.solid.native.energy_j[d];
            *r=candidate;
        }
    }
    tr.solid=physics_sim_thermal_solid_energy(&next)-physics_sim_thermal_solid_energy(s);
    if(fail==1)return false;
    int first=s->tick+(normal_cells==512?ticks/2:0);
    if(!advance_normal(&next,matrix,first,s->tick+ticks,scalar_factor,normal_cells))return false;
    next.source=s->source+4.7109375*(fmin(TPS,s->tick+ticks)-fmin(TPS,s->tick))/(double)TPS;
    next.tick+=ticks;
    if(fail==2)return false;
    *out=next;*trace=tr;return true;
}

bool physics_sim_thermal_step(const PhysicsSimThermalState *s,PhysicsSimThermalState *out,CfdOpenAtmosphere3d *gas,
    PhysicsSimLocalMatrix *matrix,int ticks,int factor,int mode,int cells,int fail,PhysicsSimThermalTrace *trace) {
    return physics_sim_thermal_step_with_receiver(s,out,gas,matrix,ticks,factor,mode,cells,fail,trace,NULL,NULL);
}
