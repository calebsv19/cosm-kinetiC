/* Exact accepted patch-6 packets, written only after joint admission. */
#ifndef PHYSICS_SIM_PATCH6_TRACE_H
#define PHYSICS_SIM_PATCH6_TRACE_H
static FILE *patch6_trace;
static void patch6_open(const char *path,int nx,int nz,int window,int steps) {
    assert(nx==128);patch6_trace=fopen(path,"wx");assert(patch6_trace);
    assert(fprintf(patch6_trace,"patch6_packets_v1 %d %d %d %d 716 1600 %s %s %s\n",
        nx,nz,window,steps,COUPLED_SOURCE_ID,COUPLED_FIXTURE_ID,COUPLED_SUPPORT_ID)>0);
}
static void patch6_commit(const ThermalReceiver *before,const ThermalReceiver *after,
    const PhysicsSimHeatedFace *face,const double *debit) {
    assert(patch6_trace);
    assert(fprintf(patch6_trace,"%llu %llu %llu",
        (unsigned long long)before[96].solid.time.ticks,
        (unsigned long long)after[96].solid.time.ticks,
        (unsigned long long)before[96].solid.time.ticks+1)>0);
    for(int x=96;x<112;x++) {
        double amounts[100];
        assert(thermal_inert_rebin(&before[x].plan,before[x].solid.completed_steps,
            before[x].config.window_steps,face[x].gas_to_surface_j,amounts));
        assert(fprintf(patch6_trace," %a %a %a",face[x].gas_to_surface_j,
            debit[x-96],after[x].received_q_j-before[x].received_q_j)>0);
        for(unsigned j=0;j<before[x].config.window_steps;j++)assert(fprintf(patch6_trace," %a",amounts[j])>0);
    }
    assert(fputc('\n',patch6_trace)!=EOF);
}
#endif
