#define _POSIX_C_SOURCE 200809L
#include "import/shape_library_input.h"
#include "import/shape_asset_input.h"
#include <dirent.h>
#include <errno.h>
#include <json-c/json.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
static const char *mode;
static unsigned calls;
static char first_path[4096];
void *library_test_calloc(size_t a,size_t b){return !strcmp(mode,"allocfail")?NULL:calloc(a,b);}
struct dirent *library_test_readdir(DIR *dir){if(!strcmp(mode,"enumfail")){errno=EIO;return NULL;}return readdir(dir);}
bool library_test_load(const char *path,ShapeAsset *asset){
 bool ok=physics_sim_shape_asset_load(path,asset);++calls;
 if(calls==1)snprintf(first_path,sizeof(first_path),"%s",path);
 if(ok&&((!strcmp(mode,"mutate")&&calls==1)||(!strcmp(mode,"latemutate")&&calls==2))){FILE *f=fopen(first_path,"ab");if(f){fputc(' ',f);fclose(f);}}
 if(ok&&!strcmp(mode,"addentry")&&calls==1){char extra[4096];snprintf(extra,sizeof(extra),"%s.extra",path);FILE *f=fopen(extra,"wb");if(f)fclose(f);}
 if(ok&&!strcmp(mode,"replacefile")&&calls==1){char extra[4096];snprintf(extra,sizeof(extra),"%s.new",path);FILE *f=fopen(extra,"wb");if(f){fputs("replacement",f);fclose(f);rename(extra,path);}}
 if(ok&&!strcmp(mode,"swaproot")&&calls==1){char parent[4096],retained[4096];snprintf(parent,sizeof(parent),"%s",path);char *slash=strrchr(parent,'/');if(slash){*slash=0;snprintf(retained,sizeof(retained),"%s.old",parent);if(rename(parent,retained)==0)mkdir(parent,0700);}}
 return ok;
}
int main(int argc,char **argv){
 if(argc!=3)return 90;mode=argv[2];ShapeAssetLibrary lib={0};
 ShapeAsset *preserved=NULL;
 if(!strcmp(mode,"nonempty")){preserved=calloc(1,sizeof(*preserved));if(!preserved)return 91;preserved->name=strdup("existing");lib.assets=preserved;lib.count=1;}
#ifdef LEGACY_LIBRARY
 int ok=shape_library_load_dir(argv[1],&lib);
#else
 int ok=physics_sim_shape_library_load(argv[1],&lib);
#endif
 json_object *out=json_object_new_object(),*names=json_object_new_array();
 json_object_object_add(out,"ok",json_object_new_boolean(ok));json_object_object_add(out,"count",json_object_new_int64(lib.count));
 for(size_t i=0;i<lib.count;i++)json_object_array_add(names,json_object_new_string(lib.assets[i].name?lib.assets[i].name:""));
 json_object_object_add(out,"names",names);
 if(!strcmp(mode,"dump")){
  json_object *serialized=json_object_new_array();
  for(size_t i=0;i<lib.count;i++){char *text=shape_asset_to_json_text(&lib.assets[i]);if(!text)return 92;json_object_array_add(serialized,json_object_new_string(text));shape_asset_json_text_free(text);}
  json_object_object_add(out,"serialized",serialized);
 }
 puts(json_object_to_json_string_ext(out,JSON_C_TO_STRING_PLAIN));json_object_put(out);
 if(preserved&&lib.assets!=preserved){shape_asset_free(preserved);free(preserved);}
 for(size_t i=0;i<lib.count;i++)shape_asset_free(&lib.assets[i]);free(lib.assets);return 0;
}
