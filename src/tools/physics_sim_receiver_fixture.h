#ifndef PHYSICS_SIM_RECEIVER_FIXTURE_H
#define PHYSICS_SIM_RECEIVER_FIXTURE_H
#include "receiver.h"
#include <stdbool.h>
#include <string.h>
/* Fixed qualified fixture. Shared privately by coordinator and process adapter. */
static inline bool physics_sim_receiver_fixture(int mode,ThermalReceiver out[4]) {
    if(mode<0||mode>2)return false;
    ThermalReceiverConfig c={0};c.version=c.quantity=c.unit=c.sign=1;c.cells=2;c.reference_k=300;c.max_k=321;c.area_m2=.0625/4;
    strcpy(c.fixture_sha256,COUPLED_FIXTURE_ID);strcpy(c.producer_sha256,COUPLED_SOURCE_ID);strcpy(c.support_sha256,COUPLED_SUPPORT_ID);
    c.face_hi_m[1]=.25;c.timebase=(CoreSimTimebase){717,6400};c.total_steps=32000;c.window_steps=1;
    for(int d=0;d<2;d++){c.width_m[d]=.005;c.cv_j_m3_k[d]=160000;c.conductivity_w_m_k[d]=.16;c.initial_k[d]=mode==2?320:300;}
    for(int x=0;x<4;x++) {
        c.face_lo_m[0]=x*.25/4;c.face_hi_m[0]=(x+1)*.25/4;
        if(!thermal_receiver_init(&c,&out[x]))return false;
    }
    return true;
}
#endif
