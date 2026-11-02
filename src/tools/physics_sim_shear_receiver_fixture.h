#ifndef PHYSICS_SIM_SHEAR_RECEIVER_FIXTURE_H
#define PHYSICS_SIM_SHEAR_RECEIVER_FIXTURE_H
#include "receiver.h"
#include <stdbool.h>
#include <string.h>
/* Existing selected shear fixture: no remapping to the warm four-strip case. */
static inline int physics_sim_shear_receiver_steps(const char *profile){
    return !strcmp(profile,"shear256-160")?160:!strcmp(profile,"shear256-320")?320:!strcmp(profile,"shear256-640")?640:!strcmp(profile,"shear256-3200")?3200:0;
}
static inline bool physics_sim_shear_receiver_init(int column,int steps,ThermalReceiver *out){
    if(column<0||column>=128||(steps!=160&&steps!=320&&steps!=640&&steps!=3200)||!out)return false;
    ThermalReceiverConfig c={0};c.version=c.quantity=c.unit=c.sign=1;c.cells=2;
    c.area_m2=.0625/128;c.reference_k=300;c.max_k=500;
    strcpy(c.fixture_sha256,COUPLED_FIXTURE_ID);strcpy(c.producer_sha256,COUPLED_SOURCE_ID);strcpy(c.support_sha256,COUPLED_SUPPORT_ID);
    c.face_lo_m[0]=column*.25/128;c.face_hi_m[0]=(column+1)*.25/128;c.face_hi_m[1]=.25;
    c.timebase=(CoreSimTimebase){716,6400};c.total_steps=(uint64_t)steps;c.window_steps=5;
    for(int i=0;i<2;i++){c.width_m[i]=.005;c.cv_j_m3_k[i]=160000;c.conductivity_w_m_k[i]=.16;c.initial_k[i]=300;}
    return thermal_receiver_init(&c,out)!=0;
}
#endif
