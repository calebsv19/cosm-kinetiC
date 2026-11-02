#include "../src/tools/physics_sim_heated_face.h"
#include "app/cfd_passive3d.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static void init(CfdOpenAtmosphere3d *s) {
    int n[3]={8,8,8},lo[3]={3,3,3},hi[3]={4,4,4};
    double l[3]={1,1,1},p[2]={0,0},t[2]={300,300};
    assert(cfd_open_atmosphere3d_init(s,n,l,1.2,.000018,1005,300,0,0,.0025,p,t,p,false,9.81,1./300,.1));
    s->ground=true;assert(cfd_open_atmosphere3d_lateral(s));
    assert(cfd_open_atmosphere3d_box(s,lo,hi));
}

int main(void) {
    CfdMemoryBudget budget={.limit_bytes=128*1024*1024};
    CfdMemoryBudget *prior=cfd_memory_scope(&budget);
    double maximum_error=0,maximum_balance=0;
    for(int a=0;a<3;a++)for(int sign=-1;sign<=1;sign+=2)for(int cooling=0;cooling<2;cooling++) {
        CfdOpenAtmosphere3d s;init(&s);
        int p[3]={3,3,3};p[a]=sign<0?2:4;int q=(p[2]*8+p[1])*8+p[0];
        double cg=1.2*1005/512.,cs=100.,tg=cooling?300:310,ts=cooling?310:300;
        s.energy[q]=cg*(tg-300);s.initial_j=s.energy[q];
        PhysicsSimHeatedFace face={a,sign,ts,cs,10,0,0};
        double equilibrium=(cg*tg+cs*ts)/(cg+cs);
        double expected=equilibrium+(ts-equilibrium)*exp(-10./64*(1/cg+1/cs)*3.2);
        for(int i=0;i<32;i++)assert(physics_sim_heated_face_exchange(&s,&face,.1));
        double error=fabs(face.temperature_k-expected);
        double balance=cfd_passive3d_total(s.energy,512)+face.gas_to_surface_j-s.initial_j;
        maximum_error=fmax(maximum_error,error);maximum_balance=fmax(maximum_balance,fabs(balance));
        assert(error<2e-11 && fabs(balance)<2e-12);
        assert(fabs(cs*(face.temperature_k-ts)-face.gas_to_surface_j)<2e-9);
        assert(s.input_j==0 && s.time==0 && s.steps==0);
        for(int j=0;j<512;j++)if(j!=q)assert(s.energy[j]==0);
        /* Save/restore accepted gas and face values: exact thermal replay. */
        double saved[512];memcpy(saved,s.energy,sizeof saved);PhysicsSimHeatedFace old=face;
        assert(physics_sim_heated_face_exchange(&s,&face,.07));
        double after[512];memcpy(after,s.energy,sizeof after);PhysicsSimHeatedFace done=face;
        memcpy(s.energy,saved,sizeof saved);face=old;
        assert(physics_sim_heated_face_exchange(&s,&face,.07));
        assert(!memcmp(after,s.energy,sizeof after) && !memcmp(&done,&face,sizeof face));
        /* Refusals preserve both participants, including allocation refusal. */
        memcpy(saved,s.energy,sizeof saved);old=face;
        assert(!physics_sim_heated_face_exchange(&s,&face,NAN));
        assert(!physics_sim_heated_face_exchange(&s,&face,-1));
        size_t limit=budget.limit_bytes;budget.limit_bytes=budget.live_bytes;
        assert(!physics_sim_heated_face_exchange(&s,&face,.1));budget.limit_bytes=limit;
        assert(!memcmp(saved,s.energy,sizeof saved) && !memcmp(&old,&face,sizeof face));
        face.temperature_k=10000;s.buoyancy=true;old=face;
        assert(!physics_sim_heated_face_exchange(&s,&face,100));
        assert(!memcmp(saved,s.energy,sizeof saved) && !memcmp(&old,&face,sizeof face));
        face=done;s.buoyancy=false;
        face.h_w_m2_k=0;old=face;
        assert(physics_sim_heated_face_exchange(&s,&face,1));
        assert(!memcmp(saved,s.energy,sizeof saved) && !memcmp(&old,&face,sizeof face));
        /* The next ordinary transport step accepts the debited gas state. */
        double zero[512]={0};assert(cfd_open_atmosphere3d_step(&s,zero,zero));
        double total=cfd_passive3d_total(s.energy,512);
        double in=cfd_passive3d_total(s.flux,s.boundary_faces);
        double out=cfd_passive3d_total(s.flux+s.boundary_faces,s.boundary_faces);
        assert(fabs(total+face.gas_to_surface_j-s.initial_j-in+out)<1e-10);
        cfd_open_atmosphere3d_destroy(&s);
    }
    /* Weak plume energy must not disappear into rounding at absolute 300 K. */
    CfdOpenAtmosphere3d weak;init(&weak);
    int q=(2*8+3)*8+3;weak.energy[q]=1e-9;
    PhysicsSimHeatedFace f={2,-1,300,100,10,0,0};
    for(int i=0;i<128;i++)assert(physics_sim_heated_face_exchange(&weak,&f,.025));
    double stored=100*((f.temperature_k-300)+f.temperature_correction_k);
    assert(f.gas_to_surface_j>0 && fabs(stored-f.gas_to_surface_j)<1e-20);
    assert(fabs(weak.energy[q]+stored-1e-9)<1e-20);
    cfd_open_atmosphere3d_destroy(&weak);
    assert(budget.live_bytes==0);cfd_memory_scope(prior);
    printf("{\"analytic_temperature_max_error_k\":%.17g,\"energy_max_residual_j\":%.17g,\"cases\":12,\"live_bytes\":0}\n",maximum_error,maximum_balance);
}
