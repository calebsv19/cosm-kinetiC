#define _DARWIN_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "physics_sim_shear_checkpoint.h"
#include "physics_sim_receiver_wire.h"
#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
enum {CAP=8*1024*1024};
typedef struct {unsigned char *p;size_t at,n;int read,ok;} Buffer;
static uint64_t hash_more(uint64_t h,const unsigned char *p,size_t n){for(size_t i=0;i<n;i++){h^=p[i];h*=UINT64_C(1099511628211);}return h;}
static uint64_t hash(const unsigned char *p,size_t n){return hash_more(UINT64_C(14695981039346656037),p,n);}
static uint64_t u(Buffer *b,uint64_t v){if(!b->ok||b->at+8>b->n){b->ok=0;return 0;}if(b->read){v=0;for(int i=0;i<8;i++)v|=(uint64_t)b->p[b->at++]<<(8*i);}else for(int i=0;i<8;i++)b->p[b->at++]=(unsigned char)(v>>(8*i));return v;}
static double d(Buffer *b,double v){uint64_t bits;memcpy(&bits,&v,8);bits=u(b,bits);memcpy(&v,&bits,8);return v;}
static int tag(Buffer *b,const char *s,size_t n){if(!b->ok||b->at+n>b->n){b->ok=0;return 0;}if(b->read){if(memcmp(b->p+b->at,s,n)){b->ok=0;return 0;}}else memcpy(b->p+b->at,s,n);b->at+=n;return 1;}
static int engine_valid(const char *s){if(!s||strlen(s)!=64)return 0;for(int i=0;i<64;i++)if(!((s[i]>='0'&&s[i]<='9')||(s[i]>='a'&&s[i]<='f')))return 0;return 1;}
static int header(Buffer *b,const char *engine){return tag(b,"PSSP0001",8)&&tag(b,PHYSICS_SIM_SHEAR_CHECKPOINT_PROFILE,sizeof PHYSICS_SIM_SHEAR_CHECKPOINT_PROFILE)&&tag(b,COUPLED_SOURCE_ID,64)&&tag(b,COUPLED_FIXTURE_ID,64)&&tag(b,COUPLED_SUPPORT_ID,64)&&tag(b,engine,64);}
static int valid(PhysicsSimShearView *v,const PhysicsSimLocalizedState *local){
 CfdOpenAtmosphere3d *g=v->gas;if(!g||!v->receivers||!v->replay||g->grid.n[0]!=128||g->grid.n[1]!=4||g->grid.n[2]!=256||g->steps<0||g->steps>3200||g->steps%5||g->dt!=1./6400||g->time!=g->steps*g->dt||g->buoyancy||!g->ground||v->tested>3||v->refusals!=5*((v->tested&1)!=0)+5*((v->tested&2)!=0))return 0;
 if(g->initial_j!=0||g->initial_kg!=0||g->input_kg!=0||g->divergence!=0||g->residual!=0||g->scalar_work_cells!=0||!isfinite(g->input_j)||fabs(g->input_j-1206*.25*(1./32)*(1./64)*fmin(g->steps/6400.,.4))>1e-9)return 0;
 double energy=0,q=0;for(int i=0;i<g->grid.count;i++){if(!isfinite(g->energy[i])||g->energy[i]<0||g->pressure[i]!=0||g->smoke[i]!=0)return 0;energy+=g->energy[i];}
 for(int a=0;a<3;a++)for(int j=0;j<g->grid.count+(a==2?g->plane:0);j++)if(g->velocity[a*g->grid.count+j]!=(a==0?.05*(j/g->plane+.5)/256:0))return 0;
 for(int i=0;i<4*g->boundary_faces;i++)if(g->flux[i]!=0)return 0;
 for(int x=0;x<128;x++){ThermalReceiver *r=&v->receivers[x];ThermalReceiverObservation o;if(!thermal_receiver_observe(r,&o)||r->solid.time.ticks!=(uint64_t)g->steps||r->solid.time.timebase.domain!=716||r->solid.time.timebase.ticks_per_second!=6400||r->windows!=(uint64_t)g->steps/5||r->config.window_steps!=5||r->config.total_steps!=3200||!isfinite(v->replay[x])||fabs(o.balance_residual_j)>1e-9)return 0;q+=r->received_q_j;}
 return fabs(energy-g->input_j+q)<1e-8&&physics_sim_localized_state_valid(local,g);
}
static int read_file(const char *path,Buffer *b,uint64_t *digest){
 int fd=open(path,O_RDONLY|O_NOFOLLOW);if(fd<0)return 0;struct stat st;int ok=!fstat(fd,&st)&&S_ISREG(st.st_mode)&&st.st_size>8&&st.st_size<=CAP;
 if(!ok){close(fd);return 0;}*b=(Buffer){.p=malloc((size_t)st.st_size),.n=(size_t)st.st_size,.read=1,.ok=1};if(!b->p){close(fd);return 0;}
 size_t at=0;while(at<b->n){ssize_t got=read(fd,b->p+at,b->n-at);if(got<0&&errno==EINTR)continue;if(got<=0){ok=0;break;}at+=(size_t)got;}if(close(fd))ok=0;
 if(ok){b->at=b->n-8;*digest=u(b,0);ok=b->ok&&*digest==hash(b->p,b->n-8);b->n-=8;b->at=0;}
 if(!ok){free(b->p);b->p=NULL;}return ok;
}
static int encode(PhysicsSimShearView *v,const char *engine,int pending,uint64_t last,Buffer *b,uint64_t *operation){
 PhysicsSimLocalizedState s={0};if(!physics_sim_localized_capture(&s))return 0;
 if(!valid(v,&s)){physics_sim_localized_state_free(&s);return 0;}
 *b=(Buffer){.p=malloc(CAP),.n=CAP,.ok=1};if(!b->p){physics_sim_localized_state_free(&s);return 0;}
 header(b,engine);u(b,(uint64_t)pending);u(b,last);size_t op_at=b->at;u(b,0);size_t body=b->at;
 CfdOpenAtmosphere3d *g=v->gas;u(b,(uint64_t)g->steps);d(b,g->time);d(b,g->input_j);u(b,v->tested);u(b,v->refusals);
 for(int i=0;i<g->grid.count;i++)d(b,g->energy[i]);for(int i=0;i<128;i++)d(b,v->replay[i]);
 u(b,(uint64_t)s.n);u(b,(uint64_t)s.nz);u(b,(uint64_t)s.last_source_tick);u(b,(uint64_t)s.restriction_tick);u(b,s.work_cells);d(b,s.max_lift_balance);d(b,s.max_restriction);d(b,s.minimum);d(b,s.maximum);u(b,s.count);for(size_t i=0;i<s.count;i++)d(b,s.values[i]);
 for(int x=0;x<128;x++){PhysicsSimReceiverWire w=psrw_new();ThermalReceiver copy=v->receivers[x];psrw_state(&w,&copy);u(b,w.at);if(!w.ok||b->at+w.at>b->n){b->ok=0;break;}memcpy(b->p+b->at,w.bytes,w.at);b->at+=w.at;}
 *operation=pending?hash_more(hash(b->p,op_at),b->p+body,b->at-body):0;
 for(int i=0;i<8;i++)b->p[op_at+i]=(unsigned char)(*operation>>(8*i));uint64_t digest=hash(b->p,b->at);u(b,digest);physics_sim_localized_state_free(&s);
 if(!b->ok){free(b->p);b->p=NULL;return 0;}b->n=b->at;b->at=0;return 1;
}
int physics_sim_shear_load(const char *path,PhysicsSimShearView *v,const char *engine,uint64_t *digest){
 if(!path||!v||!engine_valid(engine)||!digest)return 0;Buffer b={0};uint64_t saved;if(!read_file(path,&b,&saved))return 0;
 int ok=header(&b,engine);uint64_t end=u(&b,0),last=u(&b,0);size_t op_at=b.at;uint64_t operation=u(&b,0);size_t body=b.at;
 CfdOpenAtmosphere3d gas=*v->gas;uint64_t tick=u(&b,0);gas.steps=tick<=3200?(int)tick:-1;gas.time=d(&b,0);gas.input_j=d(&b,0);
 PhysicsSimShearView next=*v;next.gas=&gas;uint64_t tested=u(&b,0),refusals=u(&b,0);next.tested=(unsigned)tested;next.refusals=(unsigned)refusals;
 double *energy=malloc((size_t)gas.grid.count*sizeof(double)),replay[128];ThermalReceiver *receivers=malloc(128*sizeof *receivers);PhysicsSimLocalizedState s={0};
 if(!energy||!receivers){ok=0;goto finish;}memcpy(receivers,v->receivers,128*sizeof *receivers);gas.energy=energy;next.receivers=receivers;next.replay=replay;
 for(int i=0;i<gas.grid.count;i++)energy[i]=d(&b,0);for(int i=0;i<128;i++)replay[i]=d(&b,0);
 uint64_t n=u(&b,0),nz=u(&b,0),source=u(&b,0),restriction=u(&b,0);s.work_cells=u(&b,0);s.max_lift_balance=d(&b,0);s.max_restriction=d(&b,0);s.minimum=d(&b,0);s.maximum=d(&b,0);uint64_t count=u(&b,0);
 if((n!=128&&n!=2112)||nz!=256||source>2560||restriction>3128||count!=n*nz||count>540672){ok=0;goto finish;}
 s.n=(int)n;s.nz=(int)nz;s.last_source_tick=(int)source;s.restriction_tick=(int)restriction;s.count=(size_t)count;s.values=malloc(s.count*sizeof(double));if(!s.values){ok=0;goto finish;}for(size_t i=0;i<s.count;i++)s.values[i]=d(&b,0);
 for(int x=0;x<128;x++){uint64_t bytes=u(&b,0);if(bytes>2048||b.at+bytes>b.n){ok=0;goto finish;}PhysicsSimReceiverWire w={.reading=true,.n=(size_t)bytes,.ok=true};memcpy(w.bytes,b.p+b.at,(size_t)bytes);b.at+=(size_t)bytes;uint64_t binding=receivers[x].binding;psrw_state(&w,&receivers[x]);if(!w.ok||w.at!=w.n||binding!=receivers[x].binding){ok=0;goto finish;}}
 if(!ok||!b.ok||b.at!=b.n||tested>3||refusals>10||end>3200||end%5||
   (end&&(end<=tick||operation!=hash_more(hash(b.p,op_at),b.p+body,b.n-body)))||(!end&&operation)||!valid(&next,&s)){ok=0;goto finish;}
 if(!physics_sim_localized_restore(&s,&gas)){ok=0;goto finish;}
 memcpy(v->gas->energy,energy,(size_t)gas.grid.count*sizeof(double));v->gas->steps=gas.steps;v->gas->time=gas.time;v->gas->input_j=gas.input_j;
 memcpy(v->receivers,receivers,128*sizeof *receivers);memcpy(v->replay,replay,sizeof replay);v->tested=next.tested;v->refusals=next.refusals;v->pending_end=(int)end;v->operation=operation;v->last_operation=last;*digest=saved;ok=1;
finish:
 free(energy);free(receivers);physics_sim_localized_state_free(&s);free(b.p);return ok;
}
PhysicsSimShearCheckpointStatus physics_sim_shear_save(const char *path,PhysicsSimShearView *v,const char *engine,uint64_t expected,int pending,uint64_t last,int fail,uint64_t *digest){
 if(!path||!v||!engine_valid(engine)||!digest||pending<0||pending>3200||pending%5||(pending&&pending<=v->gas->steps)||fail<0||fail>3)return SHEAR_CP_REFUSED;
 char lock[4096],temp[4096],parent[4096];if(snprintf(lock,sizeof lock,"%s.lock",path)>=(int)sizeof lock||snprintf(temp,sizeof temp,"%s.next.%ld",path,(long)getpid())>=(int)sizeof temp||strlen(path)>=sizeof parent)return SHEAR_CP_REFUSED;
 strcpy(parent,path);char *slash=strrchr(parent,'/');if(slash){if(slash==parent)slash[1]=0;else *slash=0;}else strcpy(parent,".");
 int fdlock=open(lock,O_RDWR|O_CREAT|O_NOFOLLOW,0600);if(fdlock<0)return SHEAR_CP_IO;struct stat st;if(fstat(fdlock,&st)||!S_ISREG(st.st_mode)||flock(fdlock,LOCK_EX|LOCK_NB)){close(fdlock);return SHEAR_CP_REFUSED;}
 PhysicsSimShearCheckpointStatus status=SHEAR_CP_REFUSED;Buffer old={0},next={0};uint64_t oldhash=0,operation=0;
 if(expected){if(!read_file(path,&old,&oldhash)||oldhash!=expected||!header(&old,engine))goto finish;uint64_t prior_end=u(&old,0);u(&old,0);uint64_t prior_operation=u(&old,0),prior_tick=u(&old,0);
  if(pending){if(prior_end||prior_tick!=(uint64_t)v->gas->steps)goto finish;}
  else if(!prior_end||prior_end!=(uint64_t)v->gas->steps||last!=prior_operation)goto finish;
 }else{if(v->gas->steps||pending||last||!lstat(path,&st)||errno!=ENOENT)goto finish;}
 if(!encode(v,engine,pending,last,&next,&operation))goto finish;
 if(fail==1){status=SHEAR_CP_IO;goto finish;}int fd=open(temp,O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW,0600);if(fd<0){status=SHEAR_CP_IO;goto finish;}
 size_t at=0;int ok=1;while(at<next.n){ssize_t got=write(fd,next.p+at,next.n-at);if(got<0&&errno==EINTR)continue;if(got<=0){ok=0;break;}at+=(size_t)got;}if(fsync(fd))ok=0;if(close(fd))ok=0;
 if(!ok||fail==2){unlink(temp);status=SHEAR_CP_IO;goto finish;}if(rename(temp,path)){unlink(temp);status=SHEAR_CP_IO;goto finish;}
 int dir=open(parent,O_RDONLY);int synced=dir>=0&&!fsync(dir);if(dir>=0)close(dir);
 *digest=hash(next.p,next.n-8);v->pending_end=pending;v->operation=operation;v->last_operation=last;status=(!synced||fail==3)?SHEAR_CP_RELOAD:SHEAR_CP_OK;
finish:
 free(old.p);free(next.p);flock(fdlock,LOCK_UN);close(fdlock);return status;
}
