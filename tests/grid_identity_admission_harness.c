
#include "app/sim_runtime_backend_2d_internal.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int allocations, frees;
void *grid_test_calloc(size_t n,size_t s){++allocations;return calloc(n,s);}
void *grid_test_malloc(size_t n){++allocations;return malloc(n);}
void grid_test_free(void *p){if(p)++frees;free(p);}
void ts_start_timer(const char *s){(void)s;}
void ts_stop_timer(const char *s){(void)s;}
SimRuntimeBackend *sim_runtime_backend_3d_scaffold_create(const AppConfig *c,const FluidScenePreset *p,const SimModeRoute *r,const PhysicsSimRuntimeVisualBootstrap *v){(void)c;(void)p;(void)r;(void)v;return NULL;}
static unsigned long long hash_bytes(unsigned long long h,const void *p,size_t n){const unsigned char *s=p;for(size_t i=0;i<n;++i){h^=s[i];h*=1099511628211ULL;}return h;}
static unsigned long long snapshot(SimRuntimeBackend2D *s){
 unsigned long long h=hash_bytes(1469598103934665603ULL,s,sizeof(*s));
 h=hash_bytes(h,s->fluid,sizeof(*s->fluid));
 float *fields[]={s->fluid->density,s->fluid->density_prev,s->fluid->velX,s->fluid->velY,s->fluid->velX_prev,s->fluid->velY_prev,s->fluid->pressure,s->obstacle_vel_x,s->obstacle_vel_y,s->obstacle_distance};
 for(size_t i=0;i<10;++i)h=hash_bytes(h,fields[i],768*sizeof(float));
 h=hash_bytes(h,s->static_mask,768);h=hash_bytes(h,s->obstacle_mask,768);
 return s->emitter_masks[0].mask ? hash_bytes(h,s->emitter_masks[0].mask,768) : h;
}
int main(int argc,char **argv){
 if(argc<3)return 2;const char *action=argv[1],*change=argv[2];
 AppConfig cfg={0};cfg.grid_w=32;cfg.grid_h=24;cfg.window_w=320;cfg.window_h=240;cfg.sim_mode=SIM_MODE_WIND_TUNNEL;
 FluidScenePreset preset={0};preset.emitter_count=1;
 preset.emitters[0]=(FluidEmitter){.type=EMITTER_VELOCITY_JET,.position_x=.5f,.position_y=.5f,.radius=.08f,.strength=6,.dir_x=1,.attached_object=-1,.attached_import=-1};
 SimRuntimeBackend *b=sim_runtime_backend_create(&cfg,&preset,NULL,NULL);if(!b)return 3;
 SimRuntimeBackend2D *s=backend_2d_state(b);
 SceneObject obj={0};obj.source_import=-1;obj.body.shape=RIGID2D_SHAPE_CIRCLE;obj.body.position.x=160;obj.body.position.y=120;obj.body.radius=18;obj.body.velocity.x=40;
 SceneState scene={0};scene.config=&cfg;scene.preset=&preset;scene.backend=b;scene.emitters_enabled=true;scene.objects.objects=&obj;scene.objects.count=1;scene.objects.capacity=1;
 memset(s->static_mask,1,768);memset(s->obstacle_mask,0,768);
 s->emitter_masks[0].mask=malloc(768);if(!s->emitter_masks[0].mask)return 4;memset(s->emitter_masks[0].mask,1,768);
 s->emitter_masks[0].max_x=31;s->emitter_masks[0].max_y=23;
 for(size_t i=0;i<768;++i){s->fluid->density[i]=.5f;s->fluid->velX[i]=.5f;s->fluid->velY[i]=.5f;s->obstacle_distance[i]=.5f;}
 AppConfig step_cfg=cfg;int normal=!strcmp(change,"normal"), result=0;
 if(!strcmp(change,"grow"))cfg.grid_w=33;
 else if(!strcmp(change,"shrink"))cfg.grid_h=23;
 else if(!strcmp(change,"swap")){cfg.grid_w=24;cfg.grid_h=32;}
 else if(!strcmp(change,"zero"))cfg.grid_w=0;
 else if(!strcmp(change,"negative"))cfg.grid_h=-1;
 else if(!strcmp(change,"int"))cfg.grid_w=cfg.grid_h=INT_MAX;
 else if(!strcmp(change,"step_only"))step_cfg.grid_w=33;
 else if(!strcmp(change,"fluid"))s->fluid->w=31;
#ifndef LEGACY_GRID_IDENTITY
 else if(!strcmp(change,"metadata"))s->allocation_w=0;
#endif
 else if(!normal)return 2;
 unsigned long long before=snapshot(s);allocations=frees=0;
 StrokeSample sample={.x=160,.y=120,.vx=10,.vy=10,.mode=BRUSH_MODE_DENSITY};
 if(!strcmp(action,"static"))b->ops->build_static_obstacles(b,&scene);
 else if(!strcmp(action,"obstacles"))b->ops->build_obstacles(b,&scene);
 else if(!strcmp(action,"dynamic"))b->ops->rasterize_dynamic_obstacles(b,&scene);
 else if(!strcmp(action,"distance"))backend_2d_compute_obstacle_distance(&scene,s);
 else if(!strcmp(action,"emitter_masks"))b->ops->build_emitter_masks(b,&scene);
 else if(!strcmp(action,"emitters"))b->ops->apply_emitters(b,&scene,.1);
 else if(!strcmp(action,"boundary"))b->ops->apply_boundary_flows(b,&scene,.1);
 else if(!strcmp(action,"enforce_boundary"))b->ops->enforce_boundary_flows(b,&scene);
 else if(!strcmp(action,"enforce_obstacles"))b->ops->enforce_obstacles(b,&scene);
 else if(!strcmp(action,"motion"))b->ops->inject_object_motion(b,&scene);
 else if(!strcmp(action,"brush")){if(b->ops->apply_brush_sample(b,&cfg,&sample)!=normal)result=5;}
 else if(!strcmp(action,"step"))b->ops->step(b,&scene,!strcmp(change,"step_only")?&step_cfg:&cfg,.01);
 else if(!strcmp(action,"clear"))b->ops->clear(b);
 else if(!strcmp(action,"seed"))b->ops->seed_uniform_velocity_2d(b,2,3);
 else if(!strcmp(action,"valid")){if(b->ops->valid(b))result=6;}
 else if(!strcmp(action,"fluid_view")){
  SceneFluidFieldView2D v;memset(&v,0xa5,sizeof(v));SceneFluidFieldView2D saved=v;
  if(b->ops->get_fluid_view_2d(b,&v) || memcmp(&v,&saved,sizeof(v)))result=7;
 }
 else if(!strcmp(action,"obstacle_view")){
  SceneObstacleFieldView2D v;memset(&v,0xa5,sizeof(v));SceneObstacleFieldView2D saved=v;
  if(b->ops->get_obstacle_view_2d(b,&v) || memcmp(&v,&saved,sizeof(v)))result=8;
 }
 else if(!strcmp(action,"report")){
  SimRuntimeBackendReport v;memset(&v,0xa5,sizeof(v));SimRuntimeBackendReport saved=v;
  if(b->ops->get_report(b,&v) || memcmp(&v,&saved,sizeof(v)))result=9;
 }
 else if(!strcmp(action,"compatibility")){
  bool fluid=true,obstacle=true;
  if(b->ops->get_compatibility_slice_activity(b,0,&fluid,&obstacle) || !fluid || !obstacle)result=10;
 }
 else if(!strcmp(action,"snapshot")){
  if(argc!=4 || b->ops->export_snapshot(b,1,argv[3]))result=11;
 }
 else if(!strcmp(action,"view_original")){
  SceneFluidFieldView2D v={0};
  if(!b->ops->get_fluid_view_2d(b,&v) || v.width!=32 || v.height!=24 || v.cell_count!=768 || !b->ops->valid(b))result=12;
 }
 else if(!strcmp(action,"restore")){
  b->ops->rasterize_dynamic_obstacles(b,&scene);
  if(snapshot(s)!=before || allocations || frees)result=13;
  cfg.grid_w=32;cfg.grid_h=24;b->ops->rasterize_dynamic_obstacles(b,&scene);
  if(snapshot(s)==before)result=14;
  normal=1;
 }
 else return 2;
 if(!normal && (snapshot(s)!=before || allocations || frees))result=15;
 if(result)fprintf(stderr,"action=%s change=%s code=%d alloc=%d free=%d\n",action,change,result,allocations,frees);
 s->fluid->w=32;s->fluid->h=24;
 sim_runtime_backend_destroy(b);
 return result;
}
