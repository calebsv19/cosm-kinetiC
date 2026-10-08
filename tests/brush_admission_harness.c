
#include "app/sim_runtime_backend_2d_internal.h"
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int allocations,frees;
void *grid_test_calloc(size_t n,size_t s){++allocations;return calloc(n,s);}
void *grid_test_malloc(size_t n){++allocations;return malloc(n);}
void grid_test_free(void *p){if(p)++frees;free(p);}
void ts_start_timer(const char *s){(void)s;}
void ts_stop_timer(const char *s){(void)s;}
SimRuntimeBackend *sim_runtime_backend_3d_scaffold_create(const AppConfig *c,const FluidScenePreset *p,const SimModeRoute *r,const PhysicsSimRuntimeVisualBootstrap *v){(void)c;(void)p;(void)r;(void)v;return NULL;}
static int coordinate(int p,int window,int grid){
 int x=p<0?0:p>=window?grid-1:(int)(((float)p/(float)window)*(float)grid);
 if(x<1)x=1;if(x>grid-2)x=grid-2;return x;
}
int main(int argc,char **argv){
 if(argc!=2)return 2;const char *mode=argv[1];
 AppConfig cfg={0};cfg.grid_w=32;cfg.grid_h=24;cfg.window_w=320;cfg.window_h=240;
 if(!strcmp(mode,"minimum"))cfg.grid_w=cfg.grid_h=2;
 if(!strcmp(mode,"extreme_int"))cfg.window_w=cfg.window_h=1;
 if(!strcmp(mode,"scale_overflow") || !strcmp(mode,"sum_overflow"))cfg.window_w=1;
 SimRuntimeBackend *b=sim_runtime_backend_create(&cfg,NULL,NULL,NULL);if(!b)return 3;
 SimRuntimeBackend2D *s=backend_2d_state(b);size_t count=(size_t)s->fluid->w*s->fluid->h;
 StrokeSample sample={.x=160,.y=120,.vx=8,.vy=-4,.mode=BRUSH_MODE_DENSITY};int valid=0;
 if(!strcmp(mode,"density"))valid=1;
 else if(!strcmp(mode,"velocity")){sample.mode=BRUSH_MODE_VELOCITY;valid=1;}
 else if(!strcmp(mode,"negative")){sample.x=-1000;sample.y=-2000;valid=1;}
 else if(!strcmp(mode,"edge")){sample.x=320;sample.y=240;valid=1;}
 else if(!strcmp(mode,"offscreen")){sample.x=4000;sample.y=5000;valid=1;}
 else if(!strcmp(mode,"extreme_int")){sample.x=INT_MAX;sample.y=INT_MIN;valid=1;}
 else if(!strcmp(mode,"minimum"))valid=1;
 else if(!strcmp(mode,"nan_vx"))sample.vx=NAN;
 else if(!strcmp(mode,"inf_vy"))sample.vy=INFINITY;
 else if(!strcmp(mode,"scale_overflow")){sample.mode=BRUSH_MODE_VELOCITY;sample.vx=FLT_MAX;}
 else if(!strcmp(mode,"sum_overflow")){sample.mode=BRUSH_MODE_VELOCITY;sample.vx=FLT_MAX/70;}
 else if(!strcmp(mode,"bad_mode"))sample.mode=(BrushMode)99;
 else if(!strcmp(mode,"negative_mode"))sample.mode=(BrushMode)-1;
 else if(!strcmp(mode,"window_zero"))cfg.window_w=0;
 else if(!strcmp(mode,"window_negative"))cfg.window_h=-1;
 else if(!strcmp(mode,"grid_mismatch"))cfg.grid_w=33;
 else if(strcmp(mode,"nan_density") && strcmp(mode,"inf_velx") && strcmp(mode,"nan_vely") && strcmp(mode,"null_sample"))return 2;
 int x=coordinate(sample.x,cfg.window_w>0?cfg.window_w:1,s->fluid->w);
 int y=coordinate(sample.y,cfg.window_h>0?cfg.window_h:1,s->fluid->h);size_t id=(size_t)y*s->fluid->w+x;
 if(!strcmp(mode,"nan_density"))s->fluid->density[id]=NAN;
 if(!strcmp(mode,"inf_velx"))s->fluid->velX[id]=INFINITY;
 if(!strcmp(mode,"nan_vely"))s->fluid->velY[id]=NAN;
 if(!strcmp(mode,"sum_overflow"))s->fluid->velX[id]=FLT_MAX;
 float before[3][768];memcpy(before[0],s->fluid->density,count*sizeof(float));memcpy(before[1],s->fluid->velX,count*sizeof(float));memcpy(before[2],s->fluid->velY,count*sizeof(float));
 allocations=frees=0;
 int ok=b->ops->apply_brush_sample(b,&cfg,!strcmp(mode,"null_sample")?NULL:&sample),result=0;
 if(ok!=valid)result=4;
 if(valid){
  Fluid2D *expected=fluid2d_create(s->fluid->w,s->fluid->h);if(!expected)return 5;
  memcpy(expected->density,before[0],count*sizeof(float));memcpy(expected->velX,before[1],count*sizeof(float));memcpy(expected->velY,before[2],count*sizeof(float));
  float vx=(sample.vx/(float)cfg.window_w)*35,vy=(sample.vy/(float)cfg.window_h)*35;
  if(sample.mode==BRUSH_MODE_VELOCITY){fluid2d_add_velocity(expected,x,y,vx,vy);fluid2d_add_density(expected,x,y,4);}
  else{fluid2d_add_density(expected,x,y,20);fluid2d_add_velocity(expected,x,y,vx*.25f,vy*.25f);}
  if(memcmp(expected->density,s->fluid->density,count*sizeof(float)) || memcmp(expected->velX,s->fluid->velX,count*sizeof(float)) || memcmp(expected->velY,s->fluid->velY,count*sizeof(float)))result=6;
  fluid2d_destroy(expected);
 }else if(memcmp(before[0],s->fluid->density,count*sizeof(float)) || memcmp(before[1],s->fluid->velX,count*sizeof(float)) || memcmp(before[2],s->fluid->velY,count*sizeof(float)))result=7;
 if(allocations || frees)result=8;
 if(result)fprintf(stderr,"mode=%s code=%d accepted=%d\n",mode,result,ok);
 sim_runtime_backend_destroy(b);return result;
}
