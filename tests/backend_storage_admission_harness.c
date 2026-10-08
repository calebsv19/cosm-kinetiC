
#include "app/sim_runtime_backend_2d_internal.h"
#include "app/atmospheric/atmospheric_field.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static struct {void *pointer;size_t bytes;} owned[64];
static int calls, failure, live, bad_free;
static size_t requested;
static AppConfig *change_during_allocation;
static void *allocate(size_t n,size_t size,int clear) {
    ++calls;
    if(calls==1 && change_during_allocation) {
        change_during_allocation->grid_w=INT_MAX;
        change_during_allocation->grid_h=INT_MAX;
    }
    if(n && size>(size_t)-1/n)return NULL;
    if(n*size>256u*1024u*1024u)return NULL;
    requested+=n*size;
    if(calls==failure)return NULL;
    void *p=clear?calloc(n,size):malloc(n*size);
    if(p){for(size_t i=0;i<64;++i)if(!owned[i].pointer){owned[i].pointer=p;owned[i].bytes=n*size;++live;return p;}abort();}
    return p;
}
void *backend_storage_test_calloc(size_t n,size_t s){return allocate(n,s,1);}
void *backend_storage_test_malloc(size_t n){return allocate(1,n,0);}
void backend_storage_test_free(void *p){
    if(!p)return;
    for(size_t i=0;i<64;++i)if(owned[i].pointer==p){owned[i].pointer=NULL;--live;free(p);return;}
    ++bad_free;
}
void ts_start_timer(const char *name){(void)name;}
void ts_stop_timer(const char *name){(void)name;}
SimRuntimeBackend *sim_runtime_backend_3d_scaffold_create(const AppConfig *cfg,
 const FluidScenePreset *preset,const SimModeRoute *route,const PhysicsSimRuntimeVisualBootstrap *visual){
 (void)cfg;(void)preset;(void)route;(void)visual;return NULL;
}
int main(int argc,char **argv){
 if(argc<2)return 2;
 AppConfig cfg={0};cfg.grid_w=32;cfg.grid_h=24;cfg.window_w=320;cfg.window_h=240;
 FluidScenePreset preset={0};const char *mode=argv[1];int valid=0,invalid=0;
 if(!strcmp(mode,"valid"))valid=1;
 else if(!strcmp(mode,"config_snapshot")){valid=1;change_during_allocation=&cfg;}
 else if(!strcmp(mode,"minimum")){cfg.grid_w=cfg.grid_h=2;valid=1;}
 else if(!strcmp(mode,"rectangular")){cfg.grid_w=7;cfg.grid_h=41;valid=1;}
 else if(!strcmp(mode,"seeded")){preset.atmosphere=atmospheric_preset_default_settings();preset.domain=SCENE_DOMAIN_ATMOSPHERIC;valid=1;}
 else if(!strcmp(mode,"zero")){cfg.grid_w=0;invalid=1;}
 else if(!strcmp(mode,"negative")){cfg.grid_h=-1;invalid=1;}
 else if(!strcmp(mode,"one")){cfg.grid_h=1;invalid=1;}
 else if(!strcmp(mode,"large")){cfg.grid_w=cfg.grid_h=10000;invalid=1;}
 else if(!strcmp(mode,"int")){cfg.grid_w=cfg.grid_h=INT_MAX;invalid=1;}
 else if(!strcmp(mode,"null"))invalid=1;
 else if(!strcmp(mode,"fault")){if(argc!=3)return 2;failure=atoi(argv[2]);}
#ifndef LEGACY_BACKEND_STORAGE
 else if(!strcmp(mode,"plan")) {
    const size_t fixed=sizeof(SimRuntimeBackend)+sizeof(SimRuntimeBackend2D)+sizeof(Fluid2D);
    const size_t cells=(256u*1024u*1024u-fixed)/(10u*sizeof(float)+2u*sizeof(uint8_t));
    size_t bytes=123;
    if(!backend_2d_initial_storage_bytes(2,(int)(cells/2),&bytes) || bytes>256u*1024u*1024u)return 3;
    size_t before=bytes;
    if(backend_2d_initial_storage_bytes(2,(int)(cells/2)+1,&bytes) || before!=bytes || calls)return 4;
    if(backend_2d_initial_storage_bytes(32,24,NULL))return 5;
    return 0;
 }
#endif
 else return 2;
 /* Invalid-grid tests stop any old-code allocation before a large request. */
 if(invalid)failure=1;
 SimRuntimeBackend *b=sim_runtime_backend_create(!strcmp(mode,"null")?NULL:&cfg,&preset,NULL,NULL);
 int result=0;
 if(!strcmp(mode,"config_snapshot")){cfg.grid_w=32;cfg.grid_h=24;}
 if(valid){
    SimRuntimeBackend2D *s=backend_2d_state(b);
    if(!b || !b->ops || !s || !s->fluid || s->fluid->w!=cfg.grid_w || s->fluid->h!=cfg.grid_h || !s->static_mask || !s->obstacle_mask ||
       !s->obstacle_vel_x || !s->obstacle_vel_y || !s->obstacle_distance)result=6;
#ifndef LEGACY_BACKEND_STORAGE
    size_t bytes=0;
    if(!backend_2d_initial_storage_bytes(cfg.grid_w,cfg.grid_h,&bytes) || bytes!=requested || calls!=15)result=7;
#endif
    if(!result){
      SceneFluidFieldView2D view={0};
      if(!b->ops->get_fluid_view_2d(b,&view) || view.cell_count!=(size_t)cfg.grid_w*cfg.grid_h)result=8;
      if(strcmp(mode,"seeded"))for(size_t i=0;i<view.cell_count;++i)
       if(view.density[i]!=0 || view.velocity_x[i]!=0 || view.velocity_y[i]!=0)result=9;
    }
 } else if(b)result=10;
 sim_runtime_backend_destroy(b);
 if(live || bad_free)result=11;
#ifndef LEGACY_BACKEND_STORAGE
 if(invalid && calls)result=12;
#endif
 if(result)fprintf(stderr,"mode=%s fault=%d code=%d calls=%d live=%d bad_free=%d requested=%zu\n",mode,failure,result,calls,live,bad_free,requested);
 return result;
}
