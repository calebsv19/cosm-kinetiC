/* Codec/publication tests reuse the exact fixed benchmark initializer. */
#define main shear_benchmark_main
#include "../src/tools/physics_sim_thermal_columns_probe.c"
#undef main
#include <sys/file.h>
#include <fcntl.h>
#include <unistd.h>
static unsigned checks;
#define CHECK(x) do {checks++;assert(x);} while(0)
static uint64_t test_hash(const unsigned char *p,size_t n){uint64_t h=UINT64_C(14695981039346656037);for(size_t i=0;i<n;i++){h^=p[i];h*=UINT64_C(1099511628211);}return h;}
static void put(unsigned char *p,uint64_t n){for(int i=0;i<8;i++)p[i]=(unsigned char)(n>>(8*i));}
static void output(const char *p,unsigned char *b,size_t n){FILE *f=fopen(p,"wb");assert(f);assert(fwrite(b,1,n,f)==n);assert(!fclose(f));}
int main(int argc,char **argv){
 assert(argc==2);char path[4096],bad[4096],lock[4096];assert(snprintf(path,sizeof path,"%s/state",argv[1])<(int)sizeof path);assert(snprintf(bad,sizeof bad,"%s/bad",argv[1])<(int)sizeof bad);assert(snprintf(lock,sizeof lock,"%s.lock",path)<(int)sizeof lock);
 const char *engine="aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
 CfdOpenAtmosphere3d gas={0};CHECK(init(&gas,128,256));CHECK(physics_sim_localized_init(&gas));ThermalReceiver r[128];for(int x=0;x<128;x++)CHECK(physics_sim_shear_receiver_init(x,3200,&r[x]));double replay[128]={0};PhysicsSimShearView v={.gas=&gas,.receivers=r,.replay=replay};uint64_t digest=0,next=0;
 CHECK(physics_sim_shear_save(path,&v,engine,0,0,0,0,&digest)==SHEAR_CP_OK);
 CHECK(physics_sim_shear_load(path,&v,engine,&next)&&next==digest);
 CHECK(!physics_sim_shear_load(path,&v,"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",&next));
 CHECK(physics_sim_shear_save(path,&v,engine,0,0,0,0,&next)==SHEAR_CP_REFUSED);
 CHECK(physics_sim_shear_save(path,&v,engine,digest^1,5,0,0,&next)==SHEAR_CP_REFUSED);
 CHECK(physics_sim_shear_save(path,&v,engine,digest,0,0,0,&next)==SHEAR_CP_REFUSED);
 int fd=open(lock,O_RDWR);CHECK(fd>=0&&!flock(fd,LOCK_EX|LOCK_NB));CHECK(physics_sim_shear_save(path,&v,engine,digest,5,0,0,&next)==SHEAR_CP_REFUSED);CHECK(!flock(fd,LOCK_UN));close(fd);
 for(int fail=1;fail<=2;fail++){CHECK(physics_sim_shear_save(path,&v,engine,digest,5,0,fail,&next)==SHEAR_CP_IO);CHECK(physics_sim_shear_load(path,&v,engine,&next)&&next==digest&&!v.pending_end);}
 FILE *f=fopen(path,"rb");assert(f);assert(!fseek(f,0,SEEK_END));size_t n=(size_t)ftell(f);rewind(f);unsigned char *original=malloc(n+1),*bytes=malloc(n+1);assert(original&&bytes&&fread(original,1,n,f)==n);fclose(f);
 const size_t head=8+sizeof PHYSICS_SIM_SHEAR_CHECKPOINT_PROFILE+4*64,body=head+24;
 size_t local=body+40+131072*8+128*8;
 size_t offsets[]={0,8,8+sizeof PHYSICS_SIM_SHEAR_CHECKPOINT_PROFILE,head,head+16,body,body+8,body+16,body+24,body+32,body+40,local,local+8,local+16,local+24,local+40,local+72};
 uint64_t before=physics_sim_localized_checksum();ThermalReceiver saved[128];memcpy(saved,r,sizeof r);
 for(size_t i=0;i<sizeof offsets/sizeof *offsets;i++){
  memcpy(bytes,original,n);if(i<3)bytes[offsets[i]]^=0x80;else put(bytes+offsets[i],UINT64_C(0x7ff0000000000000));put(bytes+n-8,test_hash(bytes,n-8));output(bad,bytes,n);
  CHECK(!physics_sim_shear_load(bad,&v,engine,&next));CHECK(before==physics_sim_localized_checksum());CHECK(!memcmp(saved,r,sizeof r)&&gas.steps==0&&gas.input_j==0);for(int j=0;j<gas.grid.count;j++)assert(gas.energy[j]==0);
 }
 memcpy(bytes,original,n);bytes[n-1]^=1;output(bad,bytes,n);CHECK(!physics_sim_shear_load(bad,&v,engine,&next));output(bad,original,n-9);CHECK(!physics_sim_shear_load(bad,&v,engine,&next));
 memcpy(bytes,original,n);bytes[n-8]=0;put(bytes+n-7,test_hash(bytes,n-7));output(bad,bytes,n+1);CHECK(!physics_sim_shear_load(bad,&v,engine,&next));
 CHECK(physics_sim_shear_save(path,&v,engine,digest,5,0,3,&next)==SHEAR_CP_RELOAD);CHECK(v.pending_end==5&&v.operation);uint64_t op=v.operation;
 CHECK(physics_sim_shear_load(path,&v,engine,&digest)&&digest==next&&v.pending_end==5&&v.operation==op);
 CHECK(physics_sim_shear_save(path,&v,engine,digest,10,0,0,&next)==SHEAR_CP_REFUSED);CHECK(physics_sim_shear_save(path,&v,engine,digest,0,op,0,&next)==SHEAR_CP_REFUSED);
 free(bytes);free(original);physics_sim_localized_destroy();cfd_open_atmosphere3d_destroy(&gas);printf("%u checkpoint checks passed\n",checks);return 0;
}
