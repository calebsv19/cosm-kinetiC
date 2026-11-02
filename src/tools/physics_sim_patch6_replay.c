/* Native receiver replay of exact captured packets. No gas evolution. */
#include "receiver.h"
#include "inert.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static ThermalReceiver r[16];
int main(void) {
    char tag[32],producer[65],fixture[65],support[65];
    int nx,nz,window,steps,domain,tps;
    assert(scanf("%31s %d %d %d %d %d %d %64s %64s %64s",tag,&nx,&nz,&window,&steps,&domain,&tps,producer,fixture,support)==10);
    assert(!strcmp(tag,"patch6_packets_v1")&&nx==128&&nz==64&&window==5&&
        (steps==80||steps==5120)&&domain==716&&tps==1600);
    ThermalReceiverConfig c={0};c.version=c.quantity=c.unit=c.sign=1;c.cells=2;
    c.reference_k=300;c.max_k=500;c.timebase=(CoreSimTimebase){(uint64_t)domain,(uint64_t)tps};
    c.total_steps=steps;c.window_steps=window;c.face_hi_m[1]=.25;
    strcpy(c.producer_sha256,producer);strcpy(c.fixture_sha256,fixture);strcpy(c.support_sha256,support);
    for(int d=0;d<2;d++){c.width_m[d]=.005;c.cv_j_m3_k[d]=160000;c.conductivity_w_m_k[d]=.16;c.initial_k[d]=300;}
    for(int j=0;j<16;j++) {
        int x=j+96;c.face_lo_m[0]=x*.25/nx;c.face_hi_m[0]=(x+1)*.25/nx;c.area_m2=.0625/nx;
        assert(thermal_receiver_init(&c,&r[j]));
    }
    unsigned refusals=0,packets=0;
    for(int tick=0;tick<steps;tick+=window) {
        unsigned long long begin,end,checkpoint;
        assert(scanf("%llu %llu %llu",&begin,&end,&checkpoint)==3);
        assert(begin==(unsigned)tick&&end==(unsigned)(tick+window)&&checkpoint==begin+1);
        double q[16],debit[16],old_received[16],recorded[16][5];
        for(int j=0;j<16;j++) {
            assert(scanf("%la %la %la",&q[j],&debit[j],&old_received[j])==3);
            assert(isfinite(q[j])&&isfinite(debit[j])&&isfinite(old_received[j]));
            for(int k=0;k<5;k++)assert(scanf("%la",&recorded[j][k])==1&&isfinite(recorded[j][k]));
        }
        double allocated[16],residual[16],received_delta[16];
        for(int j=0;j<16;j++) {
            double amounts[100];assert(thermal_inert_rebin(&r[j].plan,tick,window,q[j],amounts));
            assert(!memcmp(amounts,recorded[j],sizeof recorded[j]));
            CoreSimTimeInterval interval={r[j].solid.time,{r[j].config.timebase,end}};
            ThermalReceiverExchange packet;ThermalReceiver next;ThermalReceiverReceipt receipt;
            assert(thermal_receiver_exchange_make(&r[j],interval,checkpoint,q[j],&packet));
            if(j==0&&(tick==0||tick==40)) {
                for(int fail=0;fail<3;fail++) {
                    ThermalReceiver saved=r[j],sentinel;memset(&next,0x5a,sizeof next);sentinel=next;
                    ThermalReceiverExchange bad=packet;CoreSimTimePoint available=interval.end;
                    if(fail==0)bad.q_j+=1;
                    if(fail==1)bad.binding++;
                    if(fail==2)available.ticks--;
                    assert(!thermal_receiver_stage(&r[j],&bad,available,&next,&receipt));
                    assert(!memcmp(&r[j],&saved,sizeof saved)&&!memcmp(&next,&sentinel,sizeof next));refusals++;
                }
            }
            assert(thermal_receiver_stage(&r[j],&packet,interval.end,&next,&receipt));
            assert(next.solid.completed_steps==end&&receipt.interval.begin.ticks==begin&&receipt.interval.end.ticks==end);
            received_delta[j]=next.received_q_j-r[j].received_q_j;
            allocated[j]=receipt.allocated_q_j;residual[j]=receipt.accepted_candidate.balance_residual_j;
            r[j]=next;packets++;
        }
        printf("{\"tick\":%llu,\"energy_j\":[",end);
        for(int j=0;j<16;j++)printf("%s[%.17g,%.17g]",j?",":"",r[j].solid.native.energy_j[0],r[j].solid.native.energy_j[1]);
        printf("],\"received_j\":[");for(int j=0;j<16;j++)printf("%s%.17g",j?",":"",r[j].received_q_j);
        printf("],\"received_delta_j\":[");for(int j=0;j<16;j++)printf("%s%.17g",j?",":"",received_delta[j]);
        printf("],\"allocated_j\":[");for(int j=0;j<16;j++)printf("%s%.17g",j?",":"",allocated[j]);
        printf("],\"balance_j\":[");for(int j=0;j<16;j++)printf("%s%.17g",j?",":"",residual[j]);
        puts("]}");
    }
    char extra;assert(scanf(" %c",&extra)==EOF);
    printf("{\"terminal\":true,\"steps\":%d,\"packets\":%u,\"refusal_checks\":%u,\"timebase_domain\":716,\"ticks_per_second\":1600}\n",steps,packets,refusals);
    return 0;
}
