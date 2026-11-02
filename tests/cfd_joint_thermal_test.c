#define _DARWIN_C_SOURCE
#include "physics_sim_joint_thermal.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
static unsigned checks;
#define CHECK(x) do{checks++;assert(x);}while(0)
int main(int argc,char **argv){
    assert(argc==2);char path[4096],lock[4096];snprintf(path,sizeof path,"%s/state",argv[1]);snprintf(lock,sizeof lock,"%s/state.lock",argv[1]);
    CfdMemoryBudget b={.limit_bytes=16*1024*1024};CfdMemoryBudget *prior=cfd_memory_scope(&b);
    PhysicsSimJointRuntime rt={0};PhysicsSimJointState s,old,loaded;PhysicsSimJointCandidate c,untouched;
    CHECK(physics_sim_joint_init(&rt,0,&s));old=s;memset(&c,0x55,sizeof c);untouched=c;
    CHECK(!physics_sim_joint_prepare(&rt,&s,8,16,0,&c));CHECK(!memcmp(&c,&untouched,sizeof c));
    CHECK(!physics_sim_joint_prepare(&rt,&s,0,16,0,&c));
    for(int fail=1;fail<=2;fail++){CHECK(!physics_sim_joint_prepare(&rt,&s,0,8,fail,&c));CHECK(!memcmp(&s,&old,sizeof s));CHECK(!memcmp(&c,&untouched,sizeof c));}
    CHECK(physics_sim_joint_publish(path,&rt,NULL,&s,0)==JOINT_OK);
    for(int i=0;i<128;i++){CHECK(physics_sim_joint_prepare(&rt,&s,s.thermal.tick,s.thermal.tick+8,0,&c));CHECK(physics_sim_joint_accept(&rt,&s,&c));}
    CHECK(!physics_sim_joint_accept(&rt,&s,&c));CHECK(physics_sim_joint_prepare(&rt,&s,s.thermal.tick,s.thermal.tick+8,0,&c));
    c.state.thermal.v[0]+=.01;CHECK(!physics_sim_joint_accept(&rt,&s,&c));
    for(int fail=1;fail<=2;fail++){CHECK(physics_sim_joint_publish(path,&rt,&old,&s,fail)==JOINT_IO);CHECK(physics_sim_joint_load(path,&rt,&loaded));CHECK(physics_sim_joint_digest(&loaded)==physics_sim_joint_digest(&old));}
    int fd=open(lock,O_RDWR);CHECK(fd>=0);CHECK(!flock(fd,LOCK_EX|LOCK_NB));CHECK(physics_sim_joint_publish(path,&rt,&old,&s,0)==JOINT_REFUSED);flock(fd,LOCK_UN);close(fd);
    CHECK(physics_sim_joint_publish(path,&rt,&old,&s,3)==JOINT_PUBLISHED_RELOAD);
    CHECK(physics_sim_joint_load(path,&rt,&loaded));CHECK(physics_sim_joint_digest(&s)==physics_sim_joint_digest(&loaded));
    CHECK(!memcmp(s.thermal.correction,loaded.thermal.correction,sizeof s.thermal.correction));int nonzero=0;for(int i=0;i<512;i++)nonzero+=s.thermal.correction[i]!=0;CHECK(nonzero>0);
    CHECK(physics_sim_joint_publish(path,&rt,&old,&s,0)==JOINT_REFUSED);
    CHECK(physics_sim_joint_prepare(&rt,&loaded,loaded.thermal.tick,loaded.thermal.tick+8,0,&c));CHECK(physics_sim_joint_accept(&rt,&loaded,&c));
    CHECK(physics_sim_joint_prepare(&rt,&s,s.thermal.tick,s.thermal.tick+8,0,&c));CHECK(physics_sim_joint_accept(&rt,&s,&c));CHECK(physics_sim_joint_digest(&s)==physics_sim_joint_digest(&loaded));
    unsigned char bytes[16384];FILE *f=fopen(path,"rb");assert(f);size_t n=fread(bytes,1,sizeof bytes,f);fclose(f);old=loaded;
    for(size_t j=0;j<n;j+=127){bytes[j]^=1;f=fopen(path,"wb");assert(f);assert(fwrite(bytes,1,n,f)==n);fclose(f);CHECK(!physics_sim_joint_load(path,&rt,&loaded));CHECK(!memcmp(&old,&loaded,sizeof old));bytes[j]^=1;}
    /* Recompute the integrity checksum: these exercise semantic admission,
       rather than merely detecting a damaged byte. */
    size_t header=8+sizeof JOINT_PROFILE+192;
    size_t offsets[]={8,8+sizeof JOINT_PROFILE,header,header+8,header+16,header+24,header+39,header+47,header+55,header+48+8192+24+24};
    for(size_t j=0;j<sizeof offsets/sizeof *offsets;j++){
        size_t at=offsets[j];unsigned char saved=bytes[at];bytes[at]^=0x80;
        uint64_t h=UINT64_C(14695981039346656037);for(size_t k=0;k<n-8;k++){h^=bytes[k];h*=UINT64_C(1099511628211);}for(int k=0;k<8;k++)bytes[n-8+k]=(unsigned char)(h>>(8*k));
        f=fopen(path,"wb");assert(f);assert(fwrite(bytes,1,n,f)==n);fclose(f);CHECK(!physics_sim_joint_load(path,&rt,&loaded));CHECK(!memcmp(&old,&loaded,sizeof old));bytes[at]=saved;
    }
    f=fopen(path,"wb");assert(f);assert(fwrite(bytes,1,n-1,f)==n-1);fclose(f);CHECK(!physics_sim_joint_load(path,&rt,&loaded));
    s.thermal.r[0].config.area_m2*=2;CHECK(!physics_sim_joint_valid(&rt,&s));
    physics_sim_joint_destroy(&rt);cfd_memory_scope(prior);CHECK(b.live_bytes==0);printf("joint runtime: %u checks; peak %zu bytes\n",checks,b.peak_bytes);return 0;
}
