#define _DARWIN_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "physics_sim_joint_thermal.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
enum {WIRE_MAX=16384};
typedef struct {unsigned char bytes[WIRE_MAX];size_t at;bool ok;} Wire;
static uint64_t checksum(const unsigned char *p,size_t n){uint64_t h=UINT64_C(14695981039346656037);for(size_t i=0;i<n;i++){h^=p[i];h*=UINT64_C(1099511628211);}return h;}
static void integer(Wire *w,uint64_t v){if(w->at+8>WIRE_MAX){w->ok=false;return;}for(int i=0;i<8;i++)w->bytes[w->at++]=(unsigned char)(v>>(8*i));}
static uint64_t take(Wire *w){if(w->at+8>WIRE_MAX){w->ok=false;return 0;}uint64_t v=0;for(int i=0;i<8;i++)v|=(uint64_t)w->bytes[w->at++]<<(8*i);return v;}
static void number(Wire *w,double d){uint64_t bits;_Static_assert(sizeof(double)==8,"requires binary64");memcpy(&bits,&d,8);integer(w,bits);}
static double real(Wire *w){uint64_t bits=take(w);double d;memcpy(&d,&bits,8);return d;}
static void tag(Wire *w,const char *s,size_t n){if(w->at+n>WIRE_MAX){w->ok=false;return;}memcpy(w->bytes+w->at,s,n);w->at+=n;}
static bool match(Wire *w,const char *s,size_t n){if(w->at+n>WIRE_MAX||memcmp(w->bytes+w->at,s,n)){w->ok=false;return false;}w->at+=n;return true;}
static size_t encode(const PhysicsSimJointState *s,Wire *w) {
    *w=(Wire){.ok=true};tag(w,"PSJT0001",8);tag(w,JOINT_PROFILE,sizeof JOINT_PROFILE);
    tag(w,COUPLED_SOURCE_ID,64);tag(w,COUPLED_FIXTURE_ID,64);tag(w,COUPLED_SUPPORT_ID,64);
    integer(w,s->version);integer(w,s->mode);integer(w,s->generation);integer(w,(uint64_t)s->thermal.tick);number(w,s->thermal.source);number(w,s->last_q);
    for(int i=0;i<512;i++){number(w,s->thermal.v[i]);number(w,s->thermal.correction[i]);}
    for(int x=0;x<4;x++) {
        const ThermalReceiver *r=&s->thermal.r[x];const ThermalSimParticipantState *t=&r->solid;
        integer(w,r->binding);integer(w,r->windows);number(w,r->received_q_j);
        integer(w,t->native.count);integer(w,t->time.timebase.domain);integer(w,t->time.timebase.ticks_per_second);integer(w,t->time.ticks);integer(w,t->completed_steps);integer(w,t->last_input_hash);
        number(w,t->source_j);number(w,t->sink_j);
        for(int d=0;d<2;d++){number(w,t->native.energy_j[d]);number(w,t->cell_exchange_j[d]);}
        for(int d=0;d<3;d++)number(w,t->faces_j[d]);
    }
    uint64_t h=checksum(w->bytes,w->at);integer(w,h);return w->ok?w->at:0;
}
uint64_t physics_sim_joint_digest(const PhysicsSimJointState *s){Wire w;size_t n=encode(s,&w);return n?checksum(w.bytes,n-8):0;}
static bool decode(Wire *w,size_t n,const PhysicsSimJointRuntime *rt,PhysicsSimJointState *out) {
    if(n<8||n>WIRE_MAX)return false;
    w->at=n-8;w->ok=true;uint64_t expected=take(w);if(expected!=checksum(w->bytes,n-8))return false;
    w->at=0;
    if(!match(w,"PSJT0001",8)||!match(w,JOINT_PROFILE,sizeof JOINT_PROFILE)||
       !match(w,COUPLED_SOURCE_ID,64)||!match(w,COUPLED_FIXTURE_ID,64)||!match(w,COUPLED_SUPPORT_ID,64))return false;
    PhysicsSimJointState s;if(!physics_sim_joint_seed(rt->mode,&s))return false;
    uint64_t version=take(w),mode=take(w);if(version!=1||mode!=(uint64_t)rt->mode)return false;
    s.generation=take(w);uint64_t tick=take(w);if(tick>END)return false;s.thermal.tick=(int)tick;s.thermal.source=real(w);s.last_q=real(w);
    for(int i=0;i<512;i++){s.thermal.v[i]=real(w);s.thermal.correction[i]=real(w);}
    for(int x=0;x<4;x++) {
        ThermalReceiver *r=&s.thermal.r[x];ThermalSimParticipantState *t=&r->solid;
        if(take(w)!=r->binding)return false;r->windows=take(w);r->received_q_j=real(w);
        if(take(w)!=2||take(w)!=717||take(w)!=6400)return false;
        t->time.ticks=take(w);t->completed_steps=take(w);t->last_input_hash=take(w);t->source_j=real(w);t->sink_j=real(w);
        for(int d=0;d<2;d++){t->native.energy_j[d]=real(w);t->cell_exchange_j[d]=real(w);}
        for(int d=0;d<3;d++)t->faces_j[d]=real(w);
    }
    if(!w->ok||w->at!=n-8||!physics_sim_joint_valid(rt,&s))return false;
    *out=s;return true;
}
bool physics_sim_joint_load(const char *path,const PhysicsSimJointRuntime *rt,PhysicsSimJointState *out) {
    if(!path||!rt||!rt->initialized||!out)return false;
    int fd=open(path,O_RDONLY|O_NOFOLLOW);if(fd<0)return false;struct stat st;Wire w={0};
    bool ok=!fstat(fd,&st)&&S_ISREG(st.st_mode)&&st.st_size>0&&st.st_size<=WIRE_MAX;
    size_t used=0,n=ok?(size_t)st.st_size:0;
    while(ok&&used<n){ssize_t got=read(fd,w.bytes+used,n-used);if(got<0&&errno==EINTR)continue;if(got<=0){ok=false;break;}used+=(size_t)got;}
    if(close(fd))ok=false;
    return ok&&decode(&w,n,rt,out);
}
PhysicsSimJointStatus physics_sim_joint_publish(const char *path,const PhysicsSimJointRuntime *rt,
    const PhysicsSimJointState *expected,const PhysicsSimJointState *next,int fail) {
    if(!path||!physics_sim_joint_valid(rt,next)||(expected&&!physics_sim_joint_valid(rt,expected))||fail<0||fail>3)return JOINT_REFUSED;
    if(expected&&(next->generation<=expected->generation||next->mode!=expected->mode))return JOINT_REFUSED;
    if(!expected&&next->generation)return JOINT_REFUSED;
    char lock[4096],temp[4096],parent[4096];
    if(snprintf(lock,sizeof lock,"%s.lock",path)>=(int)sizeof lock||snprintf(temp,sizeof temp,"%s.next.%ld",path,(long)getpid())>=(int)sizeof temp||strlen(path)>=sizeof parent)return JOINT_REFUSED;
    strcpy(parent,path);char *slash=strrchr(parent,'/');if(slash){if(slash==parent)slash[1]=0;else *slash=0;}else strcpy(parent,".");
    int lockfd=open(lock,O_RDWR|O_CREAT|O_NOFOLLOW,0600);if(lockfd<0)return JOINT_IO;
    struct stat st;if(fstat(lockfd,&st)||!S_ISREG(st.st_mode)||flock(lockfd,LOCK_EX|LOCK_NB)){close(lockfd);return JOINT_REFUSED;}
    PhysicsSimJointStatus status=JOINT_REFUSED;PhysicsSimJointState disk;
    if(expected){if(!physics_sim_joint_load(path,rt,&disk)||physics_sim_joint_digest(&disk)!=physics_sim_joint_digest(expected))goto finish;}
    else {if(!lstat(path,&st)||errno!=ENOENT)goto finish;}
    Wire w;size_t n=encode(next,&w);if(!n)goto finish;
    if(fail==1){status=JOINT_IO;goto finish;}
    int fd=open(temp,O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW,0600);if(fd<0){status=JOINT_IO;goto finish;}
    bool ok=true;size_t written=0;
    while(written<n){ssize_t got=write(fd,w.bytes+written,n-written);if(got<0&&errno==EINTR)continue;if(got<=0){ok=false;break;}written+=(size_t)got;}
    if(fsync(fd))ok=false;if(close(fd))ok=false;
    if(!ok||fail==2){unlink(temp);status=JOINT_IO;goto finish;}
    if(rename(temp,path)){unlink(temp);status=JOINT_IO;goto finish;}
    int dir=open(parent,O_RDONLY);bool synced=dir>=0&&fsync(dir)==0;if(dir>=0)close(dir);
    status=(!synced||fail==3)?JOINT_PUBLISHED_RELOAD:JOINT_OK;
finish:
    flock(lockfd,LOCK_UN);close(lockfd);return status;
}
