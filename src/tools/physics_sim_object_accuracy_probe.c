/* Native authored support exchange against an independently evaluated gas/object
 * two-reservoir ODE. No transport or forced-temperature reservoir reset. */
#include "physics_sim_heated_face.h"
#include "app/cfd_passive3d.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
int main(int argc,char **argv) {
    if(argc!=4)return 2;
    int side=atoi(argv[1]),mode=atoi(argv[3]);double dt=strtod(argv[2],NULL);
    if((side!=16&&side!=32&&side!=64)||(dt!=.01&&dt!=.005)||mode<0||mode>3)return 2;
    int n[3]={side,side,side},lo[3]={3*side/8,3*side/8,side/16},hi[3]={5*side/8,5*side/8,5*side/16};
    double l[3]={1,1,1},p[2]={0},t[2]={300,300};CfdOpenAtmosphere3d s={0};
    CfdMemoryBudget mem={.limit_bytes=512*1024*1024};CfdMemoryBudget *prior=cfd_memory_scope(&mem);int status=1;
    if(!cfd_open_atmosphere3d_init(&s,n,l,1.2,1.8e-5,1005,300,.026,0,dt,p,t,p,false,9.81,1./300,.1))goto done;
    s.ground=true;if(!cfd_open_atmosphere3d_box(&s,lo,hi))goto done;
    int cooling=mode%2,nonuniform=mode>=2;double cg=1206*s.grid.volume;
    for(int q=0;q<s.grid.count;q++)if(!cfd_open_atmosphere3d_solid(&s,q)){
        double x=(q%side+.5)/side,y=(q/side%side+.5)/side,z=(q/(side*side)+.5)/side;
        double rise=cooling?0:5+(nonuniform?(x-.3125)+2*(y-.5)+3*(z-.1875):0);
        s.energy[q]=cg*rise;
    }
    PhysicsSimHeatedFace face={0,-1,cooling?305:300,10,2,0,0};
    double initial_gas=cfd_passive3d_total(s.energy,s.grid.count),initial_object=cooling?50:0,mean;
    if(!physics_sim_heated_face_sample(&s,&face,.125,&mean))goto done;
    printf("{\"side\":%d,\"mode\":%d,\"dt_s\":%.17g,\"support_gas_capacity_j_k\":%.17g,\"object_capacity_j_k\":10,\"conductance_w_k\":0.125,\"initial_gas_mean_rise_k\":%.17g,\"initial_object_rise_k\":%d,\"rows\":[",side,mode,dt,1206*.0625*.125,mean,cooling?5:0);
    int ticks=(int)llround(3.2/dt);double worst=0;
    for(int i=0;i<=ticks;i++) {
        if(i && !physics_sim_heated_face_exchange_depth(&s,&face,dt,.125))goto done;
        double rise=face.temperature_k-300+face.temperature_correction_k;
        double defect=cfd_passive3d_total(s.energy,s.grid.count)+10*rise-initial_gas-initial_object;worst=fmax(worst,fabs(defect));
        if(i%(ticks/16)==0 || i==ticks){
            if(!physics_sim_heated_face_sample(&s,&face,.125,&mean))goto done;
            printf("%s[%.17g,%.17g,%.17g,%.17g]",i?",":"",i*dt,rise,mean,face.gas_to_surface_j);
        }
    }
    printf("],\"max_heat_defect_j\":%.17g",worst);status=0;
done:
    cfd_open_atmosphere3d_destroy(&s);cfd_memory_scope(prior);
    if(!status)printf(",\"memory_peak_bytes\":%zu,\"memory_live_bytes\":%zu}\n",mem.peak_bytes,mem.live_bytes);
    return mem.live_bytes?4:status;
}
