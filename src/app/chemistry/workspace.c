#define _DARWIN_C_SOURCE 1
#include "app/chemistry/workspace.h"
#include "app/session_workspace_ui.h"
#include <json-c/json.h>
#include <spawn.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
extern char **environ;
#ifndef PHYSICS_SIM_REPO_ROOT
#define PHYSICS_SIM_REPO_ROOT "."
#endif
typedef struct ChemistryWorkspace {
 SessionUi ui;struct json_object *session;
 pid_t child;unsigned token,submitted,serial,discarded;
 char root[1024],input[1200],output[1200],message[256],path[1024],saved[1200];
 bool play,quit,typing,temperature_view;int pending;int species,slice,dimensions,reaction,probe_x,probe_y;double diffusion,speed,dt,rate;
} ChemistryWorkspace;
static struct json_object *get(struct json_object *o,const char *key) {struct json_object *v=NULL;if(o) json_object_object_get_ex(o,key,&v);return v;}
static struct json_object *at(struct json_object *o,int i) {return o?json_object_array_get_idx(o,(size_t)i):NULL;}
static void put(struct json_object *o,const char *k,const char *v) {json_object_object_add(o,k,json_object_new_string(v));}
static int interface_edge_x(struct json_object *edges, int edge, int cells) {
 return 24+(int)lround(720*json_object_get_double(at(edges,edge)) /
                       json_object_get_double(at(edges,cells)));
}
static long tick(ChemistryWorkspace *w) {return json_object_get_int64(get(get(w->session,"report"),"tick"));}
static bool launch(ChemistryWorkspace *w,const char *action,struct json_object *args) {
 if(w->child) {json_object_put(args);return false;}
 struct json_object *req=json_object_new_object();json_object_object_add(req,"token",json_object_new_int64(w->token));put(req,"action",action);json_object_object_add(req,"args",args);
 snprintf(w->input,sizeof w->input,"%s/chem-%ld-%u.input.json",w->root,(long)getpid(),++w->serial);
 snprintf(w->output,sizeof w->output,"%s/chem-%ld-%u.output.json",w->root,(long)getpid(),w->serial);
 FILE *f=fopen(w->input,"wx");bool ok=f!=NULL;
 if(f) {ok=fputs(json_object_to_json_string_ext(req,JSON_C_TO_STRING_PLAIN),f)>=0;if(fclose(f)) ok=false;}
 json_object_put(req);
 if(!ok) {snprintf(w->message,sizeof w->message,"Could not create request");return false;}
 const char *lib=getenv("PHYSICS_SIM_CHEMISTRY_LIBRARY");
 char script[1200];snprintf(script,sizeof script,"%s/scripts/chemistry_workspace.py",PHYSICS_SIM_REPO_ROOT);
 char *argv[]={"python3","-B",script,"--library",(char *)(lib?lib:""),"--input",w->input,"--output",w->output,NULL};
 int code=posix_spawnp(&w->child,"python3",NULL,NULL,argv,environ);w->submitted=w->token;
 if(code) {w->child=0;unlink(w->input);snprintf(w->message,sizeof w->message,"Worker launch failed: %d",code);return false;}
 snprintf(w->message,sizeof w->message,"%s pending",action);return true;
}
static struct json_object *with_session(ChemistryWorkspace *w) {struct json_object *a=json_object_new_object();json_object_object_add(a,"session",json_object_get(w->session));return a;}
static void create(ChemistryWorkspace *w) {
 struct json_object *a=json_object_new_object();json_object_object_add(a,"dimensions",json_object_new_int(w->dimensions));
 json_object_object_add(a,"diffusion",json_object_new_double(w->diffusion));json_object_object_add(a,"speed",json_object_new_double(w->speed));json_object_object_add(a,"dt",json_object_new_double(w->dt));json_object_object_add(a,"rate",json_object_new_double(w->rate));
 ++w->token;w->play=false;launch(w,"create",a);
}
static void poll_job(ChemistryWorkspace *w) {
 if(!w->child) return;int status=0;pid_t result=waitpid(w->child,&status,WNOHANG);if(result==0) return;
 w->child=0;struct json_object *r=json_object_from_file(w->output);
 if(w->submitted!=w->token) {w->discarded++;snprintf(w->message,sizeof w->message,"Cancelled result discarded");}
 else if(result<0 || !WIFEXITED(status) || WEXITSTATUS(status)!=0 || !r || !get(r,"session") || json_object_get_int64(get(r,"token"))!=w->token) {
  w->play=false;snprintf(w->message,sizeof w->message,"Rejected: %.210s",r&&get(r,"error")?json_object_get_string(get(r,"error")):"worker failed");
 } else {
  struct json_object *s=json_object_get(get(r,"session"));json_object_put(w->session);w->session=s;
  snprintf(w->message,sizeof w->message,"Accepted tick %ld",tick(w));
 }
 json_object_put(r);unlink(w->input);unlink(w->output);
}
static void cancel(void *user) {(void)user;}
static uint32_t scope(void *user) {ChemistryWorkspace *w=user;return w->token+1;}
static void action(void *user,uint64_t key) {
 ChemistryWorkspace *w=user;
 if(key==3) {w->play=false;++w->token;snprintf(w->message,sizeof w->message,"Paused; pending publication invalidated");return;}
 if(key==20) {struct json_object *species=get(get(get(w->session,"config"),"chemistry"),"species");int ns=species?(int)json_object_array_length(species):0;w->species=(w->species+1)%(ns>0?ns:7);return;}
 if(key==23) {w->temperature_view=!w->temperature_view;return;}
 if(key==21) {w->slice++;return;}
 if(key==22) {w->reaction++;return;}
 if(w->child) {
  if(key==1 || key==5 || key==7) {w->play=false;++w->token;w->pending=(int)key;snprintf(w->message,sizeof w->message,"Replacement queued; old result invalidated");}
  return;
 }
 if(key==1) create(w);
 else if(key==2 && w->session) w->play=true;
 else if(key==4 && w->session) {struct json_object *a=with_session(w);json_object_object_add(a,"steps",json_object_new_int(1));launch(w,"advance",a);}
 else if(key==5 && w->session) {++w->token;w->play=false;launch(w,"reset",with_session(w));}
 else if(key==6 && w->session) {
  w->play=false;struct json_object *a=with_session(w);snprintf(w->saved,sizeof w->saved,"%s/experiment-%ld-%u.json",w->root,(long)getpid(),w->serial+1);put(a,"path",w->saved);launch(w,"save",a);
 } else if(key==7 && w->path[0]) {w->play=false;++w->token;struct json_object *a=json_object_new_object();put(a,"path",w->path);launch(w,"open",a);}
 else if(key==10) w->dimensions=w->dimensions%3+1;
 else if(key==11) w->diffusion=fmin(.1,w->diffusion+.001);
 else if(key==12) w->diffusion=fmax(0,w->diffusion-.001);
 else if(key==13) w->speed=fmin(.5,w->speed+.02);
 else if(key==14) w->speed=fmax(-.5,w->speed-.02);
 else if(key==15) w->dt=fmin(.02,w->dt*2);
 else if(key==16) w->dt=fmax(.00001,w->dt*.5);
 else if(key==17) w->rate=fmin(5,w->rate+.25);
 else if(key==18) w->rate=fmax(0,w->rate-.25);
}
static void label(SessionText *font,SDL_Renderer *r,int x,int y,const char *text) {session_text_draw(font,r,2,x,y,(SDL_Color){220,230,240,255},text);}
static void button(ChemistryWorkspace *w,SessionText *f,int key,int x,int y,int width,const char *text,bool enabled) {session_ui_button(&w->ui,f,(uint64_t)key,(SDL_Rect){x,y,width,32},text,enabled);}
static void click(int x,int y) {SDL_Event e={0};e.type=SDL_MOUSEMOTION;e.motion.x=x;e.motion.y=y;SDL_PushEvent(&e);e.type=SDL_MOUSEBUTTONDOWN;e.button.button=SDL_BUTTON_LEFT;e.button.x=x;e.button.y=y;SDL_PushEvent(&e);e.type=SDL_MOUSEBUTTONUP;SDL_PushEvent(&e);}
int physics_sim_chemistry_workspace_run(const char *root) {
 ChemistryWorkspace w={.dimensions=2,.diffusion=.01,.speed=.1,.dt=.001,.rate=1};
 if(!root || !getenv("PHYSICS_SIM_CHEMISTRY_LIBRARY")) {fprintf(stderr,"Chemistry requires root argument and PHYSICS_SIM_CHEMISTRY_LIBRARY\n");return 1;}
 if(strlen(root)>1000 || (mkdir(root,0700)!=0 && errno!=EEXIST)) return 1;
 snprintf(w.root,sizeof w.root,"%s",root);
 if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER)!=0 || TTF_Init()!=0) return 1;
 SDL_Window *window=SDL_CreateWindow("kinetiC | Chemistry experiment",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,1180,800,SDL_WINDOW_RESIZABLE|SDL_WINDOW_ALLOW_HIGHDPI);if(!window) return 1;
 SDL_SetWindowMinimumSize(window,1180,800);SDL_Renderer *r=SDL_CreateRenderer(window,-1,SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);if(!r) r=SDL_CreateRenderer(window,-1,SDL_RENDERER_SOFTWARE);
 SessionText font={0};if(!r || !session_text_refresh(&font,r,window)) return 1;
 session_ui_init(&w.ui,window,r,&w,cancel);w.ui.host.action=action;w.ui.host.scope=scope;
 SDL_StartTextInput();const char *initial=getenv("PHYSICS_SIM_CHEMISTRY_INITIAL");
 if(initial) {snprintf(w.path,sizeof w.path,"%s",initial);action(&w,7);} else create(&w);unsigned begin=SDL_GetTicks(),previous=begin,max_gap=0;long max_tick=0;int test_stage=getenv("PHYSICS_SIM_CHEMISTRY_INITIAL")?0:-2;bool test=getenv("PHYSICS_SIM_CHEMISTRY_TEST")!=NULL;int result=0;
 while(!w.quit) {
  unsigned now=SDL_GetTicks();if(now-previous>max_gap) max_gap=now-previous;previous=now;
  physics_sim_ui_host_refresh(&w.ui.host);w.ui.host.geometry_dirty=false;SDL_Event e;
  while(SDL_PollEvent(&e)) {
   /* Text entry is owned here; the shared non-editor host intentionally consumes it. */
   if(e.type==SDL_TEXTINPUT && w.typing) {if(strlen(w.path)+strlen(e.text.text)<sizeof w.path) strcat(w.path,e.text.text);continue;}
   if(e.type==SDL_KEYDOWN && w.typing) {
    if(e.key.keysym.sym==SDLK_BACKSPACE && w.path[0]) {size_t n=strlen(w.path)-1;while(n>0 && (((unsigned char)w.path[n]&0xc0)==0x80)) --n;w.path[n]=0;}
    if(e.key.keysym.sym==SDLK_RETURN) action(&w,7);
    continue;
   }
   if(e.type==SDL_MOUSEBUTTONDOWN) w.typing=e.button.y>=738;
   if(physics_sim_ui_host_filter(&w.ui.host,&e)) continue;
   if(e.type==SDL_QUIT || (e.type==SDL_WINDOWEVENT && e.window.event==SDL_WINDOWEVENT_CLOSE)) w.quit=true;
   if(e.type==SDL_MOUSEBUTTONDOWN) {
    w.typing=e.button.y>=738;
    struct json_object *n=get(get(w.session,"config"),"n");int nx=json_object_get_int(at(n,0)),ny=json_object_get_int(at(n,1));
    if(nx>0 && ny>0 && e.button.x>=24 && e.button.x<744 && e.button.y>=260 && e.button.y<620) {w.probe_x=(e.button.x-24)*nx/720;w.probe_y=(e.button.y-260)*ny/360;
     struct json_object *interface=get(get(w.session,"report"),"water_interface");
     struct json_object *hydraulic=get(get(w.session,"report"),"hydraulic");
     struct json_object *edges=hydraulic?get(at(get(hydraulic,"columns"),w.probe_y),"edges_m"):get(interface,"edges_m");
     if(edges) {
      w.probe_x=0;
      while(w.probe_x+1<nx && e.button.x>=interface_edge_x(edges,w.probe_x+1,nx))
       ++w.probe_x;
     }
    }
   }
   if(e.type==SDL_TEXTINPUT && w.typing && strlen(w.path)+strlen(e.text.text)<sizeof w.path) strcat(w.path,e.text.text);
   if(e.type==SDL_KEYDOWN && w.typing) {if(e.key.keysym.sym==SDLK_BACKSPACE && w.path[0]) w.path[strlen(w.path)-1]=0;if(e.key.keysym.sym==SDLK_RETURN) action(&w,7);}
   if(e.type==SDL_DROPFILE) {snprintf(w.path,sizeof w.path,"%s",e.drop.file);SDL_free(e.drop.file);action(&w,7);}
  }
  poll_job(&w);if(tick(&w)>max_tick) max_tick=tick(&w);
  if(!w.child && w.pending) {int pending=w.pending;w.pending=0;action(&w,(uint64_t)pending);}
  if(w.play && !w.child) {struct json_object *a=with_session(&w);json_object_object_add(a,"steps",json_object_new_int(100));launch(&w,"advance",a);}
  int width,height;SDL_GetWindowSize(window,&width,&height);SDL_RenderSetLogicalSize(r,width,height);
  if(w.ui.host.observed.presentable) session_text_refresh(&font,r,window);
  physics_sim_ui_host_collect(&w.ui.host);
  SDL_SetRenderDrawColor(r,18,25,35,255);SDL_RenderClear(r);
  struct json_object *water=get(get(w.session,"report"),"water");
  struct json_object *water_interface=get(get(w.session,"report"),"water_interface");
  struct json_object *channel_flow=get(get(w.session,"report"),"channel_flow");
  struct json_object *hydraulic=get(get(w.session,"report"),"hydraulic");
  bool pressure_surface=json_object_get_boolean(get(hydraulic,"pressure_surface"));
  struct json_object *capillary=get(get(w.session,"report"),"capillary");
  struct json_object *stefan=get(get(w.session,"report"),"stefan");
  struct json_object *droplet_carrier=get(get(w.session,"report"),"droplet_carrier");
  label(&font,r,24,16,droplet_carrier?"Chemistry experiment | droplets coupled to solved moist gas":stefan?"Chemistry experiment | compressible vapor and a phase-changing interface":capillary?"Chemistry experiment | two-fluid capillary-gravity interface":pressure_surface?"Chemistry experiment | dynamic gas pressure and deforming surface":hydraulic?"Chemistry experiment | moving surfaces with solved liquid momentum":channel_flow?"Chemistry experiment | moving water interface with solved channel flow":water_interface?"Chemistry experiment | moving planar water interface":water?"Chemistry experiment | finite water evaporation and condensation":get(get(w.session,"report"),"phase")?"Chemistry experiment | finite oxygen gas/water partition, isothermal":get(get(w.session,"report"),"aqueous")?"Chemistry experiment | conditional aqueous glucose, isothermal":"Chemistry experiment | synthetic reaction, transport and thermal models");
  button(&w,&font,1,24,50,140,"Apply / New",true);button(&w,&font,2,174,50,90,"Run",w.session&&!w.child);button(&w,&font,3,274,50,90,"Pause",true);button(&w,&font,4,374,50,90,"Step",w.session&&!w.child);button(&w,&font,5,474,50,90,"Reset",w.session!=NULL);button(&w,&font,6,574,50,100,"Save",w.session&&!w.child);button(&w,&font,7,684,50,100,"Open path",true);
  button(&w,&font,10,24,98,100,"Dimension",!w.child);button(&w,&font,11,134,98,70,"D +",!w.child);button(&w,&font,12,214,98,70,"D -",!w.child);button(&w,&font,13,294,98,70,"u +",!w.child);button(&w,&font,14,374,98,70,"u -",!w.child);button(&w,&font,15,454,98,80,"dt x2",!w.child);button(&w,&font,16,544,98,80,"dt /2",!w.child);button(&w,&font,17,634,98,80,"rate +",!w.child);button(&w,&font,18,724,98,80,"rate -",!w.child);
  char line[512];snprintf(line,sizeof line,"Draft: %dD | D %.4g m2/s | u %.4g m/s | dt %.5g s | reaction multiplier %.2g",w.dimensions,w.diffusion,w.speed,w.dt,w.rate);label(&font,r,24,142,line);
  label(&font,r,24,168,"Apply creates split A/C pools. Open a chemistry experiment JSON for model properties and initial conditions.");
  button(&w,&font,20,24,200,150,"Next species",true);button(&w,&font,21,184,200,130,"Next Z slice",true);button(&w,&font,22,324,200,150,"Next reaction",true);
  struct json_object *report=get(w.session,"report"),*config=get(w.session,"config"),*n=get(config,"n"),*thermal=get(report,"thermal");
  button(&w,&font,23,484,200,190,w.temperature_view?"View species":"View temperature",thermal!=NULL);
  int nx=json_object_get_int(at(n,0)),ny=json_object_get_int(at(n,1)),nz=json_object_get_int(at(n,2));
  if(nx>0 && ny>0 && nz>0) {
   int ns=(int)json_object_array_length(get(get(config,"chemistry"),"species"));
   if(w.species>=ns) w.species=0;
   struct json_object *flow=get(report,"flow"),*fm=get(flow,"metrics");
   if(flow) {snprintf(line,sizeof line,"Solved Stokes | Re %.3g",json_object_get_double(get(fm,"reynolds")));label(&font,r,780,202,line);snprintf(line,sizeof line,"Contrast %.3g | div %.2g /s",json_object_get_double(get(fm,"contrast")),json_object_get_double(get(fm,"divergence")));label(&font,r,780,225,line);}
   struct json_object *aq=get(report,"aqueous");
   if(aq) {snprintf(line,sizeof line,"Aqueous K beta/alpha %.6g",json_object_get_double(get(aq,"equilibrium_ratio_beta_over_alpha")));label(&font,r,780,202,line);snprintf(line,sizeof line,"T %.2f K | pH unreported",json_object_get_double(get(aq,"temperature_k")));label(&font,r,780,225,line);label(&font,r,780,248,"Conditional; errors incomplete");}
   struct json_object *phase=get(report,"phase");
   if(phase) {snprintf(line,sizeof line,"Henry %.5g mol/m3/Pa",json_object_get_double(get(phase,"henry_mol_m3_pa")));label(&font,r,780,202,line);snprintf(line,sizeof line,"Bath received %.5g J",json_object_get_double(get(phase,"bath_received_j")));label(&font,r,780,225,line);snprintf(line,sizeof line,"Energy residual %.2g J",json_object_get_double(get(phase,"energy_residual_j")));label(&font,r,780,248,line);}
   int z=w.slice%nz;double high=0,low=0;bool heat=w.temperature_view&&thermal;struct json_object *field=heat?get(thermal,"temperature_k"):get(report,"concentration_mol_m3");
   if(heat) {low=1e300;for(int q=0;q<nx*ny*nz;q++) {double T=json_object_get_double(at(field,q));low=fmin(low,T);high=fmax(high,T);}if(!hydraulic) {snprintf(line,sizeof line,"U residual %.2g J",json_object_get_double(get(thermal,"internal_ledger_residual_j")));label(&font,r,780,470,line);}snprintf(line,sizeof line,"%.30s",json_object_get_string(get(thermal,"property_id")));label(&font,r,780,492,line);}
   if(w.probe_x>=nx) w.probe_x=0;if(w.probe_y>=ny) w.probe_y=0;
   int probe=w.probe_x+nx*(w.probe_y+ny*z);snprintf(line,sizeof line,"Cell (%d,%d,%d): %.9g %s | click slice to probe",w.probe_x,w.probe_y,z,json_object_get_double(heat?at(field,probe):at(at(field,probe),w.species)),heat?"K":"mol/m3");label(&font,r,24,238,line);
   if(!heat) for(int y=0;y<ny;y++) for(int x=0;x<nx;x++) high=fmax(high,json_object_get_double(at(at(field,x+nx*(y+ny*z)),w.species)));
   struct json_object *edges=get(water_interface,"edges_m");
   for(int y=0;y<ny;y++) for(int x=0;x<nx;x++) {
    if(hydraulic) edges=get(at(get(hydraulic,"columns"),y),"edges_m");
    int index=x+nx*(y+ny*z);double value=json_object_get_double(heat?at(field,index):at(at(field,index),w.species)),q=high>low?(value-low)/(high-low):0;
    SDL_SetRenderDrawColor(r,(Uint8)(30+220*q),(Uint8)(40+100*q),(Uint8)(70+130*(1-q)),255);SDL_Rect cell={24+x*720/nx,260+y*360/ny,720/nx+1,360/ny+1};if(edges) {
     cell.x=interface_edge_x(edges,x,nx);
     cell.w=interface_edge_x(edges,x+1,nx)-cell.x;
    }
    SDL_RenderFillRect(r,&cell);
   }
   snprintf(line,sizeof line,"Accepted: %d x %d x %d | tick %ld | time %.4g s | species %s | Z %d",nx,ny,nz,tick(&w),json_object_get_double(get(report,"time_s")),json_object_get_string(get(at(get(get(config,"chemistry"),"species"),w.species),"id")),z);label(&font,r,24,632,line);
   snprintf(line,sizeof line,"%s range: %.8g .. %.8g %s (automatic)",heat?"Temperature":"Slice",low,high,heat?"K":"mol/m3");label(&font,r,24,658,line);
   for(int row=0;row<7 && row<ns;row++) {int s=(w.species+row)%ns;snprintf(line,sizeof line,"%.31s total %.9g mol",json_object_get_string(get(at(get(get(config,"chemistry"),"species"),s),"id")),json_object_get_double(at(get(report,"totals_mol"),s)));label(&font,r,780,275+30*row,line);}
   if(phase) {struct json_object *volumes=get(phase,"occupied_volume_m3");snprintf(line,sizeof line,"Gas V %.4g | liquid V %.4g m3",json_object_get_double(at(volumes,0)),json_object_get_double(at(volumes,1)));label(&font,r,780,350,line);label(&font,r,780,375,"Concentration per phase volume");snprintf(line,sizeof line,"Area/cell %.4g m2",json_object_get_double(get(phase,"interface_area_m2")));label(&font,r,780,400,line);}
   if(droplet_carrier) {
    struct json_object *parcels=get(droplet_carrier,"parcels");
    size_t count=json_object_array_length(parcels);
    snprintf(line,sizeof line,"Droplet parcels %zu",count);
    label(&font,r,780,202,line);
    if(count) {
     snprintf(line,sizeof line,"First radius %.6g um",1e6*json_object_get_double(get(at(parcels,0),"radius_m")));
     label(&font,r,780,225,line);
    }
    snprintf(line,sizeof line,"Momentum residual %.2g kg m/s",json_object_get_double(get(droplet_carrier,"momentum_residual_kg_m_s")));
    label(&font,r,780,248,line);
   }
   struct json_object *dynamic_wetting=get(report,"dynamic_wetting");
   if(dynamic_wetting) {
    snprintf(line,sizeof line,"Contact angle %.5g deg",json_object_get_double(get(dynamic_wetting,"contact_angle_deg")));
    label(&font,r,780,350,line);
    snprintf(line,sizeof line,"Contact radius %.5g um",1e6*json_object_get_double(get(dynamic_wetting,"contact_radius_m")));
    label(&font,r,780,375,line);
    snprintf(line,sizeof line,"Contact-line heat %.3g J",json_object_get_double(get(dynamic_wetting,"contact_line_heat_j")));
    label(&font,r,780,400,line);
   }
   struct json_object *film=get(report,"wall_film");
   if(film) {
    struct json_object *heights=get(film,"height_m");
    double peak=0.;
    for(size_t i=0;i<json_object_array_length(heights);i++) {
     peak=fmax(peak,json_object_get_double(at(heights,i)));
    }
    snprintf(line,sizeof line,"Peak film height %.5g um",1e6*peak);
    label(&font,r,780,350,line);
    snprintf(line,sizeof line,"Water residual %.3g kg",json_object_get_double(get(film,"water_residual_kg")));
    label(&font,r,780,375,line);
    snprintf(line,sizeof line,"Energy residual %.3g J",json_object_get_double(get(film,"energy_residual_j")));
    label(&font,r,780,400,line);
   }
   if(capillary) {
    snprintf(line,sizeof line,"Pressure jump residual %.3g Pa",json_object_get_double(get(capillary,"max_jump_residual_pa")));
    label(&font,r,780,202,line);
    snprintf(line,sizeof line,"Energy residual %.3g J",json_object_get_double(get(capillary,"total_energy_residual_j")));
    label(&font,r,780,225,line);
    snprintf(line,sizeof line,"Surface energy %.5g J",json_object_get_double(get(capillary,"surface_j")));
    label(&font,r,780,248,line);
   }
   if(hydraulic) {
    struct json_object *columns=get(hydraulic,"columns");
    if(pressure_surface) {
     double low_p=INFINITY,high_p=0.,low_h=INFINITY,high_h=0.;
     for(int i=0;i<ny;i++) {
      struct json_object *patch=at(columns,i);
      double pressure=json_object_get_double(get(patch,"pressure_pa"));
      double height=json_object_get_double(get(patch,"position_m"));
      low_p=fmin(low_p,pressure);high_p=fmax(high_p,pressure);
      low_h=fmin(low_h,height);high_h=fmax(high_h,height);
     }
     snprintf(line,sizeof line,"Gas pressure %.6g / %.6g Pa",low_p,high_p);
     label(&font,r,780,202,line);
     snprintf(line,sizeof line,"Surface span %.5g mm",1000.*(high_h-low_h));
     label(&font,r,780,225,line);
    } else {
    snprintf(line,sizeof line,"Levels %.6g / %.6g m",json_object_get_double(get(at(columns,0),"position_m")),
             json_object_get_double(get(at(columns,1),"position_m")));
    label(&font,r,780,202,line);
    snprintf(line,sizeof line,"Liquid flow %+.5g kg/s",json_object_get_double(get(hydraulic,"flow_kg_s")));
    label(&font,r,780,225,line);
    }
    snprintf(line,sizeof line,"Total energy residual %.2g J",json_object_get_double(get(hydraulic,"total_energy_residual_j")));
    label(&font,r,780,400,line);
   }
   if(water_interface) {
    if(stefan) {
     snprintf(line,sizeof line,"Vapor pressure %.7g Pa",json_object_get_double(get(stefan,"vapor_pressure_pa")));
     label(&font,r,780,422,line);
     snprintf(line,sizeof line,"Momentum residual %.2g kg m/s",json_object_get_double(get(stefan,"momentum_residual_kg_m_s")));
     label(&font,r,780,445,line);
    }
    if(channel_flow) {
     snprintf(line,sizeof line,"Channel Stokes | Re %.3g",json_object_get_double(get(channel_flow,"reynolds")));
     label(&font,r,780,445,line);
    }
    snprintf(line,sizeof line,"Liquid interface %.6g m",json_object_get_double(get(water_interface,"position_m")));
    label(&font,r,780,202,line);
    snprintf(line,sizeof line,"Displacement %+.4g um",1e6*json_object_get_double(get(water_interface,"displacement_m")));
    label(&font,r,780,225,line);
    snprintf(line,sizeof line,"Saturation %.5g Pa",json_object_get_double(get(water_interface,"saturation_pressure_pa")));
    label(&font,r,780,248,line);
    snprintf(line,sizeof line,"Liquid temperature %.5g K",json_object_get_double(get(water_interface,"liquid_temperature_k")));
    label(&font,r,780,350,line);
    snprintf(line,sizeof line,"Bath into column %.5g J",json_object_get_double(get(water_interface,"bath_heat_into_chamber_j")));
    label(&font,r,780,375,line);
    snprintf(line,sizeof line,"Energy residual %.2g J",json_object_get_double(get(water_interface,"energy_residual_j")));
    label(&font,r,780,400,line);
   }
   if(water) {
    snprintf(line,sizeof line,"Temperature %.4f K | %s",json_object_get_double(get(water,"temperature_k")),json_object_get_boolean(get(water,"insulated"))?"insulated":"bath");
    label(&font,r,780,202,line);
    snprintf(line,sizeof line,"Relative saturation %.4g",json_object_get_double(get(water,"relative_saturation")));
    label(&font,r,780,225,line);
    snprintf(line,sizeof line,"Bath into chamber %.5g J",json_object_get_double(get(water,"bath_heat_into_chamber_j")));
    label(&font,r,780,248,line);
    snprintf(line,sizeof line,"Saturation pressure %.5g Pa",json_object_get_double(get(water,"saturation_pressure_pa")));
    label(&font,r,780,350,line);
    snprintf(line,sizeof line,"Vapor pressure %.5g Pa",json_object_get_double(get(water,"vapor_pressure_pa")));
    label(&font,r,780,375,line);
    snprintf(line,sizeof line,"Energy residual %.2g J",json_object_get_double(get(water,"energy_residual_j")));
    label(&font,r,780,400,line);
   }
   double budget=0;for(int s=0;s<ns;s++) budget=fmax(budget,fabs(json_object_get_double(at(get(report,"species_budget_residual_mol"),s))));snprintf(line,sizeof line,"Budget residual %.3g mol",budget);label(&font,r,780,510,line);
   struct json_object *ext=get(report,"extent_mol");int nr=ext?(int)json_object_array_length(ext):0;
   for(int row=0;row<5 && row<nr;row++) {int index=(w.reaction+row)%nr;snprintf(line,sizeof line,"R%d extent %.7g mol",index+1,json_object_get_double(at(ext,index)));label(&font,r,780,580+22*row,line);}
   snprintf(line,sizeof line,"Native busy: %s | stale discarded %u",w.child?"yes":"no",w.discarded);label(&font,r,780,540,line);
  }
  label(&font,r,24,694,w.message);snprintf(line,sizeof line,"Save target: %.470s",w.saved);label(&font,r,24,716,line);
  SDL_Rect pathbox={24,742,1110,34};SDL_SetRenderDrawColor(r,35,47,63,255);SDL_RenderFillRect(r,&pathbox);snprintf(line,sizeof line,"%s%.490s",w.typing?"> ":"Open path (click/type, Enter or drop JSON): ",w.path);label(&font,r,30,747,line);
  physics_sim_ui_host_commit(&w.ui.host);
  if(test) {
   if(test_stage==-2 && w.session && !w.child) {click(70,113);test_stage=-1;}
   else if(test_stage==-1 && !w.child) {click(70,65);test_stage=0;}
   else if(test_stage==0 && w.session && !w.child) {click(70,215);test_stage=10;}
   else if(test_stage==10 && !w.child) {click(220,215);test_stage=11;}
   else if(test_stage==11 && !w.child) {if(thermal) click(540,215);test_stage=12;}
   else if(test_stage==12 && !w.child) {click(400,65);test_stage=1;}
   else if(test_stage==1 && !w.child && tick(&w)==1) {click(600,65);test_stage=2;}
   else if(test_stage==2 && !w.child && w.saved[0]) {click(200,65);test_stage=3;}
   else if(test_stage==3 && w.child) {click(300,65);test_stage=4;}
   else if(test_stage==4 && !w.child && w.discarded) {click(500,65);test_stage=5;}
   else if(test_stage==5 && !w.child && tick(&w)==0) {snprintf(w.path,sizeof w.path,"%s",w.saved);click(720,65);test_stage=6;}
   else if(test_stage==6 && !w.child && tick(&w)==1) {click(200,65);test_stage=7;}
   else if(test_stage==7 && tick(&w)>=101 && w.child) {click(300,65);test_stage=8;}
   else if(test_stage==8 && !w.child && w.discarded>=2) {click(200,65);test_stage=20;}
   else if(test_stage==20 && w.child) {click(500,65);test_stage=21;}
   else if(test_stage==21 && !w.child && tick(&w)==0 && w.discarded>=3) {snprintf(w.path,sizeof w.path,"%s",w.saved);click(720,65);test_stage=22;}
   else if(test_stage==22 && !w.child && tick(&w)==1) {click(200,65);test_stage=23;}
   else if(test_stage==23 && w.child) {click(720,65);test_stage=24;}
   else if(test_stage==24 && !w.child && tick(&w)==1 && w.discarded>=4) {
    const char *capture=getenv("PHYSICS_SIM_CHEMISTRY_CAPTURE");if(capture) {snprintf(w.ui.capture_path,sizeof w.ui.capture_path,"%s",capture);session_ui_capture(&w.ui);}
    char proof[1200];snprintf(proof,sizeof proof,"%s/native-proof.json",w.root);struct json_object *p=json_object_new_object();json_object_object_add(p,"session",json_object_get(w.session));json_object_object_add(p,"max_accepted_tick",json_object_new_int64(max_tick));json_object_object_add(p,"temperature_view",json_object_new_boolean(w.temperature_view));json_object_object_add(p,"selected_species",json_object_new_int(w.species));json_object_object_add(p,"selected_z",json_object_new_int(w.slice));json_object_object_add(p,"stale_discarded",json_object_new_int(w.discarded));json_object_object_add(p,"max_frame_gap_ms",json_object_new_int(max_gap));json_object_to_file_ext(proof,p,JSON_C_TO_STRING_PRETTY);json_object_put(p);w.quit=true;
   }
   if(now-begin>30000) {fprintf(stderr,"Chemistry UI test timed out stage %d: %s\n",test_stage,w.message);result=1;w.quit=true;}
  }
  SDL_RenderPresent(r);session_ui_capture(&w.ui);
  SDL_Delay(8);
 }
 if(w.child) {kill(w.child,SIGTERM);for(int i=0;i<20 && waitpid(w.child,NULL,WNOHANG)==0;i++) SDL_Delay(5);if(waitpid(w.child,NULL,WNOHANG)==0) {kill(w.child,SIGKILL);waitpid(w.child,NULL,0);}unlink(w.input);unlink(w.output);}
 json_object_put(w.session);session_text_destroy(&font);SDL_DestroyRenderer(r);SDL_DestroyWindow(window);TTF_Quit();SDL_Quit();return result;
}
