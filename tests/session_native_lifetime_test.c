#include <SDL2/SDL.h>
#include <signal.h>
#include <SDL2/SDL_ttf.h>
static SDL_Window *lifetime_window(const char *,int,int,int,int,Uint32);
static SDL_Renderer *lifetime_renderer(SDL_Window *,int,Uint32);
static int lifetime_ttf(void);
#define SDL_CreateWindow lifetime_window
#define SDL_CreateRenderer lifetime_renderer
#define TTF_Init lifetime_ttf
static void lifetime_present(SDL_Renderer *renderer);
#define SDL_RenderPresent lifetime_present
#include "../src/app/session_workspace.c"
#undef SDL_RenderPresent
#undef SDL_CreateWindow
#undef SDL_CreateRenderer
#undef TTF_Init
#include <assert.h>
static unsigned pass, count;
static int refuse;
static SDL_Window *lifetime_window(const char *name,int x,int y,int w,int h,Uint32 flags){return refuse==1?NULL:SDL_CreateWindow(name,x,y,w,h,flags);}
static SDL_Renderer *lifetime_renderer(SDL_Window *window,int index,Uint32 flags){return refuse==2?NULL:SDL_CreateRenderer(window,index,flags);}
static int lifetime_ttf(void){return refuse==3?-1:TTF_Init();}
static pid_t client;
static Uint32 opened;
static const char *proof_root;
static void lifetime_present(SDL_Renderer *renderer) {
 SDL_RenderPresent(renderer);count++;if(count==1)opened=SDL_GetTicks();
 SDL_Window *window=SDL_RenderGetWindow(renderer);assert(window);
 if(count==4||count==5){
  SDL_Event event={0};event.type=count==4?SDL_MOUSEBUTTONDOWN:SDL_MOUSEBUTTONUP;
  event.button.windowID=SDL_GetWindowID(window);event.button.button=SDL_BUTTON_LEFT;event.button.x=50;event.button.y=90;SDL_PushEvent(&event);
 }
 char ready[1200];snprintf(ready,sizeof ready,"%s/client.pid",proof_root);
 FILE *f=fopen(ready,"r");if(f){long value=0;if(fscanf(f,"%ld",&value)==1)client=(pid_t)value;fclose(f);}
 if((!pass&&client&&count>10)||(pass&&SDL_GetTicks()-opened>1500)){
  if(!pass){int w,h;SDL_GetRendererOutputSize(renderer,&w,&h);SDL_Surface *surface=SDL_CreateRGBSurfaceWithFormat(0,w,h,32,SDL_PIXELFORMAT_ARGB8888);assert(surface);assert(!SDL_RenderReadPixels(renderer,NULL,surface->format->format,surface->pixels,surface->pitch));char capture[1200];snprintf(capture,sizeof capture,"%s/native-pending.bmp",proof_root);assert(!SDL_SaveBMP(surface,capture));SDL_FreeSurface(surface);}
  SDL_Event event={.type=SDL_QUIT};SDL_PushEvent(&event);
 }
 assert(count<400);
}
int main(int argc,char **argv) {
 assert(argc==2);proof_root=argv[1];assert(!getenv("SDL_VIDEODRIVER"));
 for(refuse=1;refuse<=3;refuse++){assert(physics_sim_session_workspace_run(proof_root)==1);assert(!SDL_WasInit(SDL_INIT_VIDEO)&&!TTF_WasInit());}
 refuse=0;assert(!physics_sim_session_workspace_run(proof_root));assert(client>0);assert(kill(client,0)==0);
 count=0;pass=1;assert(!physics_sim_session_workspace_run(proof_root));
 char calls_path[1200];snprintf(calls_path,sizeof calls_path,"%s/calls",proof_root);
 FILE *calls=fopen(calls_path,"r");assert(calls);char line[4096],paths[2][1200];pid_t children[2];unsigned entries=0;
 while(fgets(line,sizeof line,calls)){assert(entries<2);struct json_object *call=json_tokener_parse(line);assert(call);children[entries]=(pid_t)json_object_get_int64(get(call,"pid"));snprintf(paths[entries],sizeof paths[entries],"%s",text(call,"output"));entries++;json_object_put(call);}fclose(calls);assert(entries==2);
 for(unsigned child=0;child<entries;child++){
  for(int i=0;i<800&&kill(children[child],0)==0;i++)SDL_Delay(5);
  assert(waitpid(children[child],NULL,WNOHANG)<0&&errno==ECHILD);
 }
 assert(strcmp(paths[0],paths[1])!=0); /* Reopened workspaces cannot share a reply slot. */
 struct json_object *held=json_object_from_file(paths[0]);assert(held);assert(!strcmp(text(held,"status"),"late-result"));
 Workspace replacement={0};snprintf(replacement.response,sizeof replacement.response,"%s",paths[0]);poll_client(&replacement);
 assert(!replacement.child&&!replacement.run_id[0]&&!replacement.scene_id[0]);json_object_put(held);
 puts("PASS actual native pending-close/reopen, late reply ignored, detached client reaped");return 0;
}
