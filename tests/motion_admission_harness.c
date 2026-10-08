
#include "app/sim_runtime_backend_2d_internal.h"
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int allocations,frees,fail_stage;
void *grid_test_calloc(size_t n,size_t s){return calloc(n,s);}
void *grid_test_malloc(size_t n){++allocations;return fail_stage?NULL:malloc(n);}
void grid_test_free(void *p){if(p)++frees;free(p);}
void ts_start_timer(const char *s){(void)s;}
void ts_stop_timer(const char *s){(void)s;}
SimRuntimeBackend *sim_runtime_backend_3d_scaffold_create(const AppConfig *c,const FluidScenePreset *p,const SimModeRoute *r,const PhysicsSimRuntimeVisualBootstrap *v){(void)c;(void)p;(void)r;(void)v;return NULL;}
static int axis(float p,int window,int grid){
 float s=p/(float)window;s=fminf(fmaxf(s,0),1);int x=(int)lroundf(s*(float)grid);
 if(x<1)x=1;if(x>grid-2)x=grid-2;return x;
}
int main(int argc,char **argv){
 if(argc!=2)return 2;const char *mode=argv[1];AppConfig cfg={0};cfg.grid_w=32;cfg.grid_h=24;cfg.window_w=320;cfg.window_h=240;
 if(!strcmp(mode,"minimum"))cfg.grid_w=cfg.grid_h=2;
 SimRuntimeBackend *b=sim_runtime_backend_create(&cfg,NULL,NULL,NULL);if(!b)return 3;
 SimRuntimeBackend2D *s=backend_2d_state(b);size_t cells=(size_t)s->fluid->w*s->fluid->h;
 SceneObject objects[1024]={0};objects[0].body.position.x=160;objects[0].body.position.y=120;objects[0].body.velocity.x=40;objects[0].body.velocity.y=-20;objects[1]=objects[0];objects[1].body.position.x=80;
 SceneState scene={0};scene.backend=b;scene.config=&cfg;scene.objects.objects=objects;scene.objects.count=2;scene.objects.capacity=1024;
 SceneObject *large=NULL;int valid=0,early=0;
 if(!strcmp(mode,"valid") || !strcmp(mode,"minimum"))valid=1;
 else if(!strcmp(mode,"order")){
  valid=1;scene.objects.count=4;objects[2]=objects[0];objects[3]=objects[0];
  objects[0].body.velocity.x=1e12f;objects[2].body.velocity.x=1;objects[3].body.velocity.x=-1e12f;
 }
 else if(!strcmp(mode,"extreme")){valid=1;objects[0].body.position.x=FLT_MAX;objects[0].body.position.y=-FLT_MAX;}
 else if(!strcmp(mode,"negative")){valid=1;objects[0].body.position.x=-1000;objects[0].body.position.y=-2000;}
 else if(!strcmp(mode,"offscreen")){valid=1;objects[0].body.position.x=4000;objects[0].body.position.y=5000;}
 else if(!strcmp(mode,"static_ignored")){valid=1;objects[1].body.is_static=true;objects[1].body.position.x=NAN;objects[1].body.velocity.y=INFINITY;}
 else if(!strcmp(mode,"locked_ignored")){valid=1;objects[1].body.locked=true;objects[1].body.position.x=NAN;}
 else if(!strcmp(mode,"empty")){valid=1;early=1;scene.objects.count=0;}
 else if(!strcmp(mode,"all_static")){valid=1;objects[0].body.is_static=objects[1].body.is_static=true;}
 else if(!strcmp(mode,"exact_count")){
  valid=1;large=calloc(65536,sizeof(*large));if(!large)return 4;
  for(size_t i=0;i<65536;++i)large[i].body.is_static=true;
  scene.objects.objects=large;scene.objects.count=scene.objects.capacity=65536;
 }
 else if(!strcmp(mode,"negative_count")){early=1;scene.objects.count=-1;}
 else if(!strcmp(mode,"excess_count")){early=1;scene.objects.count=scene.objects.capacity=65537;}
 else if(!strcmp(mode,"capacity")){early=1;scene.objects.capacity=1;}
 else if(!strcmp(mode,"negative_capacity")){early=1;scene.objects.capacity=-1;}
 else if(!strcmp(mode,"null_objects")){early=1;scene.objects.objects=NULL;}
 else if(!strcmp(mode,"window_zero")){early=1;cfg.window_w=0;}
 else if(!strcmp(mode,"window_negative")){early=1;cfg.window_h=-1;}
 else if(!strcmp(mode,"grid_mismatch")){early=1;cfg.grid_w=33;}
 else if(!strcmp(mode,"allocation"))fail_stage=1;
 else if(!strcmp(mode,"late_nan_position"))objects[1].body.position.x=NAN;
 else if(!strcmp(mode,"late_inf_position"))objects[1].body.position.y=INFINITY;
 else if(!strcmp(mode,"late_nan_velocity"))objects[1].body.velocity.x=NAN;
 else if(!strcmp(mode,"late_inf_velocity"))objects[1].body.velocity.y=INFINITY;
 else if(!strcmp(mode,"aggregate_overflow")){
  scene.objects.count=1024;for(size_t i=0;i<1024;++i){objects[i]=objects[0];objects[i].body.velocity.x=FLT_MAX;}
 }
 else if(!strcmp(mode,"intermediate_overflow")){
  objects[1]=objects[0];objects[0].body.velocity.x=FLT_MAX;objects[1].body.velocity.x=-FLT_MAX;
  size_t id=(size_t)axis(objects[0].body.position.y,240,s->fluid->h)*s->fluid->w+axis(objects[0].body.position.x,320,s->fluid->w);s->fluid->velX[id]=FLT_MAX;
 }
 else if(!strcmp(mode,"target_nan") || !strcmp(mode,"target_inf")){
  size_t id=(size_t)axis(objects[1].body.position.y,240,s->fluid->h)*s->fluid->w+axis(objects[1].body.position.x,320,s->fluid->w);
  if(!strcmp(mode,"target_nan"))s->fluid->velX[id]=NAN;else s->fluid->velY[id]=INFINITY;
 }
 else return 2;
 float before[3][768];memcpy(before[0],s->fluid->density,cells*sizeof(float));memcpy(before[1],s->fluid->velX,cells*sizeof(float));memcpy(before[2],s->fluid->velY,cells*sizeof(float));
 allocations=frees=0;b->ops->inject_object_motion(b,&scene);int result=0;
 if(valid){
  Fluid2D *expected=fluid2d_create(s->fluid->w,s->fluid->h);if(!expected)return 5;
  memcpy(expected->velX,before[1],cells*sizeof(float));memcpy(expected->velY,before[2],cells*sizeof(float));
  for(int i=0;i<scene.objects.count;++i){const SceneObject *o=&scene.objects.objects[i];if(o->body.is_static || o->body.locked)continue;
   fluid2d_add_velocity(expected,axis(o->body.position.x,cfg.window_w,expected->w),axis(o->body.position.y,cfg.window_h,expected->h),o->body.velocity.x*.01f,o->body.velocity.y*.01f);
  }
  if(memcmp(expected->velX,s->fluid->velX,cells*sizeof(float)) || memcmp(expected->velY,s->fluid->velY,cells*sizeof(float)))result=6;
  fluid2d_destroy(expected);
 }else if(memcmp(before[1],s->fluid->velX,cells*sizeof(float)) || memcmp(before[2],s->fluid->velY,cells*sizeof(float)))result=7;
 if(memcmp(before[0],s->fluid->density,cells*sizeof(float)))result=8;
#ifndef LEGACY_MOTION
 if(early && (allocations || frees))result=9;
 if(!early && (allocations!=1 || frees!=(fail_stage?0:1)))result=10;
#endif
 if(result)fprintf(stderr,"case=%s code=%d alloc=%d free=%d\n",mode,result,allocations,frees);
 fail_stage=0;free(large);sim_runtime_backend_destroy(b);return result;
}
