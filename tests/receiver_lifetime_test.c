#define _DARWIN_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "physics_sim_receiver_client.h"
#include "physics_sim_receiver_fixture.h"
#include "physics_sim_receiver_wire.h"
#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <time.h>
#include <sys/resource.h>
#include <fcntl.h>
static void pause_ms(int ms){struct timespec t={ms/1000,(ms%1000)*1000000};nanosleep(&t,NULL);}
static double now(void){struct timespec t;assert(!clock_gettime(CLOCK_MONOTONIC,&t));return t.tv_sec+t.tv_nsec/1e9;}
static void worker(void){
 signal(SIGTERM,SIG_IGN);const char *mode=getenv("L3_RECEIVER_MODE");
 pid_t descendant=0;
 if(!strcmp(mode,"descendant")||!strcmp(mode,"eof")){descendant=fork();assert(descendant>=0);if(!descendant){close(0);close(1);for(;;)pause();}}
 FILE *f=fopen(getenv("L3_READY"),"w");assert(f);fprintf(f,"%ld",(long)descendant);fclose(f);
 if(!strcmp(mode,"eof"))_exit(0);
 if(!strcmp(mode,"malformed")){unsigned char bytes[8]={255,255};assert(write(1,bytes,8)==8);}
 for(;;)pause();
}
int main(int argc,char **argv){
 if(argc==2&&!strcmp(argv[1],"0")){worker();return 0;}
 assert(argc==4);const char *mode=argv[1];PhysicsSimReceiverClient c={0};
 if(!strcmp(mode,"wire")){
  int pipe_fd[2];assert(!pipe(pipe_fd));pid_t child=fork();assert(child>=0);
  if(!child){close(pipe_fd[0]);signal(SIGPIPE,SIG_IGN);for(int i=0;i<5;i++){pause_ms(45);if(write(pipe_fd[1],"x",1)!=1)break;}_exit(0);}
  close(pipe_fd[1]);char bytes[5];double start=now();bool ok=psrw_io_timeout(pipe_fd[0],bytes,5,false,70);double elapsed=now()-start;
  close(pipe_fd[0]);kill(child,SIGKILL);waitpid(child,0,0);
  fprintf(stderr,"wire ok=%d elapsed=%.3f\n",ok,elapsed);assert(!ok&&elapsed<.2);return 0;
 }
 if(!strcmp(mode,"admission")){
  struct rlimit saved,limited;assert(!getrlimit(RLIMIT_NOFILE,&saved));
  for(rlim_t limit=3;limit<9;limit++){
   limited=saved;limited.rlim_cur=limit;assert(!setrlimit(RLIMIT_NOFILE,&limited));
   bool accepted=physics_sim_receiver_client_open(&c,argv[2],0);assert(!setrlimit(RLIMIT_NOFILE,&saved));assert(!accepted&&!c.open);
  }
  assert(physics_sim_receiver_client_open(&c,argv[2],0));physics_sim_receiver_client_close(&c);return 0;
 }
 if(!strcmp(mode,"normal")){
  ThermalReceiver base[4],next,saved;ThermalReceiverReceipt receipt,expected_receipt;assert(physics_sim_receiver_fixture(0,base));
  CoreSimTimeInterval interval={base[0].solid.time,{base[0].config.timebase,1}};ThermalReceiverExchange packet;
  assert(thermal_receiver_exchange_make(&base[0],interval,1,.001,&packet));
  assert(thermal_receiver_stage(&base[0],&packet,interval.end,&saved,&expected_receipt));
  for(int i=0;i<3;i++){
   assert(physics_sim_receiver_client_open(&c,argv[2],0));pid_t pid=c.pid;
   PhysicsSimReceiverClient refused={0};assert(!physics_sim_receiver_client_open(&refused,argv[2],0));assert(!refused.open);
   assert(physics_sim_receiver_client_stage(&c,0,&base[0],&packet,interval.end,&next,&receipt));
   assert(!memcmp(&next,&saved,sizeof next));assert(!memcmp(&receipt,&expected_receipt,sizeof receipt));
   c.transactions=UINT64_MAX;assert(!physics_sim_receiver_client_stage(&c,0,&base[0],&packet,interval.end,&next,&receipt));assert(c.transactions==UINT64_MAX);
   physics_sim_receiver_client_close(&c);assert(!memcmp(&next,&saved,sizeof next));
   assert(waitpid(pid,0,WNOHANG)<0&&errno==ECHILD);physics_sim_receiver_client_close(&c);
  }
 }else{
  setenv("L3_RECEIVER_MODE",mode,1);setenv("L3_READY",argv[3],1);
  assert(physics_sim_receiver_client_open(&c,argv[0],0));pid_t pid=c.pid;FILE *f=NULL;
  for(int i=0;i<200&&!f;i++){f=fopen(argv[3],"r");if(!f)pause_ms(5);}assert(f);long descendant=0;assert(fscanf(f,"%ld",&descendant)==1);fclose(f);
  if(!strcmp(mode,"malformed")){
   ThermalReceiver base[4],next,saved;ThermalReceiverReceipt receipt,held;assert(physics_sim_receiver_fixture(0,base));
   CoreSimTimeInterval interval={base[0].solid.time,{base[0].config.timebase,1}};ThermalReceiverExchange packet;
   assert(thermal_receiver_exchange_make(&base[0],interval,1,.001,&packet));
   memset(&next,0x55,sizeof next);saved=next;memset(&receipt,0x66,sizeof receipt);held=receipt;
   assert(!physics_sim_receiver_client_stage(&c,0,&base[0],&packet,interval.end,&next,&receipt));
   assert(!memcmp(&next,&saved,sizeof next)&&!memcmp(&receipt,&held,sizeof receipt));assert(!c.healthy);
  }
  double start=now();physics_sim_receiver_client_close(&c);assert(now()-start<1.5);
  assert(waitpid(pid,0,WNOHANG)<0&&errno==ECHILD);physics_sim_receiver_client_close(&c);
  if(descendant){for(int i=0;i<200&&kill((pid_t)descendant,0)==0;i++)pause_ms(5);assert(kill((pid_t)descendant,0)<0&&errno==ESRCH);}
 }
 puts("PASS receiver direct reap, descendant stop, retained result and reopen");return 0;
}
