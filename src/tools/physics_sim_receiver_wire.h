#ifndef PHYSICS_SIM_RECEIVER_WIRE_H
#define PHYSICS_SIM_RECEIVER_WIRE_H
#include "receiver.h"
#include <stdbool.h>
#include <errno.h>
#include <limits.h>
#include <poll.h>
#include <string.h>
#include <unistd.h>
/* Private trusted-local v1 IPC: explicit little-endian integer/binary64 fields.
 * Each bounded frame has a length, version, transaction identity and checksum.
 * No ABI struct, pointer, plan or matrix crosses the process boundary. */
typedef struct {unsigned char bytes[2048];size_t at,n;bool reading,ok;} PhysicsSimReceiverWire;
static inline uint64_t psrw_u(PhysicsSimReceiverWire *w,uint64_t value) {
    if(!w->ok||w->at+8>w->n){w->ok=false;return 0;}
    if(w->reading){value=0;for(int i=0;i<8;i++)value|=(uint64_t)w->bytes[w->at++]<<(8*i);}
    else for(int i=0;i<8;i++)w->bytes[w->at++]=(unsigned char)(value>>(8*i));return value;
}
static inline unsigned psrw_unsigned(PhysicsSimReceiverWire *w,unsigned value){uint64_t v=psrw_u(w,value);if(v>UINT_MAX)w->ok=false;return (unsigned)v;}
static inline double psrw_d(PhysicsSimReceiverWire *w,double value){uint64_t bits;memcpy(&bits,&value,8);bits=psrw_u(w,bits);memcpy(&value,&bits,8);return value;}
static inline void psrw_tag(PhysicsSimReceiverWire *w,char *s,size_t n){if(!w->ok||w->at+n>w->n){w->ok=false;return;}if(w->reading)memcpy(s,w->bytes+w->at,n);else memcpy(w->bytes+w->at,s,n);w->at+=n;}
static inline uint64_t psrw_hash(const unsigned char *b,size_t n){uint64_t h=UINT64_C(14695981039346656037);for(size_t i=0;i<n;i++){h^=b[i];h*=UINT64_C(1099511628211);}return h;}
static inline void psrw_time(PhysicsSimReceiverWire *w,CoreSimTimePoint *t){t->timebase.domain=psrw_u(w,t->timebase.domain);t->timebase.ticks_per_second=psrw_u(w,t->timebase.ticks_per_second);t->ticks=psrw_u(w,t->ticks);}
static inline void psrw_state(PhysicsSimReceiverWire *w,ThermalReceiver *r){
    r->binding=psrw_u(w,r->binding);r->windows=psrw_u(w,r->windows);r->received_q_j=psrw_d(w,r->received_q_j);
    r->solid.native.count=psrw_unsigned(w,r->solid.native.count);psrw_time(w,&r->solid.time);
    r->solid.completed_steps=psrw_u(w,r->solid.completed_steps);r->solid.last_input_hash=psrw_u(w,r->solid.last_input_hash);
    r->solid.source_j=psrw_d(w,r->solid.source_j);r->solid.sink_j=psrw_d(w,r->solid.sink_j);
    for(int d=0;d<2;d++){r->solid.native.energy_j[d]=psrw_d(w,r->solid.native.energy_j[d]);r->solid.cell_exchange_j[d]=psrw_d(w,r->solid.cell_exchange_j[d]);}
    for(int d=0;d<3;d++)r->solid.faces_j[d]=psrw_d(w,r->solid.faces_j[d]);
}
static inline void psrw_packet(PhysicsSimReceiverWire *w,ThermalReceiverExchange *p){
    psrw_tag(w,p->fixture_sha256,64);p->fixture_sha256[64]=0;psrw_tag(w,p->producer_sha256,64);p->producer_sha256[64]=0;psrw_tag(w,p->support_sha256,64);p->support_sha256[64]=0;
    p->binding=psrw_u(w,p->binding);p->sequence=psrw_u(w,p->sequence);p->source_checkpoint_ref=psrw_u(w,p->source_checkpoint_ref);p->payload_hash=psrw_u(w,p->payload_hash);
    p->quantity=psrw_unsigned(w,p->quantity);p->unit=psrw_unsigned(w,p->unit);p->sign=psrw_unsigned(w,p->sign);
    psrw_time(w,&p->interval.begin);psrw_time(w,&p->interval.end);p->q_j=psrw_d(w,p->q_j);
}
static inline bool psrw_io_timeout(int fd,void *bytes,size_t n,bool sending,int timeout_ms){
    unsigned char *p=bytes;while(n){struct pollfd event={fd,sending?POLLOUT:POLLIN,0};int rc;do{rc=poll(&event,1,timeout_ms);}while(rc<0&&errno==EINTR);if(rc<=0)return false;
        ssize_t got=sending?write(fd,p,n):read(fd,p,n);if(got<0&&errno==EINTR)continue;if(got<=0)return false;p+=got;n-=(size_t)got;}return true;
}
static inline bool psrw_io(int fd,void *bytes,size_t n,bool sending){return psrw_io_timeout(fd,bytes,n,sending,5000);}
static inline bool psrw_send(int fd,PhysicsSimReceiverWire *w){
    if(!w->ok||w->at>sizeof w->bytes-8)return false;uint64_t hash=psrw_hash(w->bytes,w->at);psrw_u(w,hash);
    unsigned char size[8];for(int i=0;i<8;i++)size[i]=(unsigned char)(w->at>>(8*i));return psrw_io(fd,size,8,true)&&psrw_io(fd,w->bytes,w->at,true);
}
static inline bool psrw_receive_wait(int fd,PhysicsSimReceiverWire *w,int idle_ms){
    unsigned char size[8];if(!psrw_io_timeout(fd,size,8,false,idle_ms))return false;uint64_t n=0;for(int i=0;i<8;i++)n|=(uint64_t)size[i]<<(8*i);
    if(n<8||n>sizeof w->bytes)return false;*w=(PhysicsSimReceiverWire){.n=(size_t)n,.reading=true,.ok=true};if(!psrw_io(fd,w->bytes,w->n,false))return false;
    w->at=w->n-8;uint64_t hash=psrw_u(w,0);w->at=0;w->n-=8;return hash==psrw_hash(w->bytes,w->n);
}
static inline bool psrw_receive(int fd,PhysicsSimReceiverWire *w){return psrw_receive_wait(fd,w,5000);}
static inline PhysicsSimReceiverWire psrw_new(void){return (PhysicsSimReceiverWire){.n=2048,.ok=true};}
#endif
