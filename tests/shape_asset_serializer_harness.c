#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "geo/shape_asset.h"
#include "external/cjson/cJSON.h"
static size_t calls,fail_at,outstanding;
static void *allocate(size_t n){++calls;if(fail_at&&calls==fail_at)return NULL;void *p=malloc(n);if(p)++outstanding;return p;}
static void release(void *p){if(p){--outstanding;free(p);}}
int main(int argc,char **argv){
 if(argc!=3)return 2;
 ShapeAssetPoint points[]={{-1.25f,2.5f},{3.75f,-4.5f}};
 ShapeAssetPath path={.closed=false,.point_count=2,.points=points};
 ShapeAsset asset={.schema=1,.name="quoted \"shape\" 🐕",.path_count=1,.paths=&path};
 if(!strcmp(argv[1],"faults")){
  cJSON_Hooks hooks={.malloc_fn=allocate,.free_fn=release};
  for(size_t i=1;i<512;++i){calls=0;fail_at=i;cJSON_InitHooks(&hooks);char *text=shape_asset_to_json_text(&asset);int reached=calls>=i;
   if(reached&&text){shape_asset_json_text_free(text);return 3;}if(text)shape_asset_json_text_free(text);if(outstanding)return 4;
   if(!reached){cJSON_InitHooks(NULL);printf("allocation boundaries=%zu\n",i-1);return 0;}
  }return 5;
 }
 if(!strcmp(argv[1],"file_failure"))return shape_asset_save_file(&asset,argv[2])?12:0;
 if(!strcmp(argv[1],"invalid")){points[0].x=NAN;char *text=shape_asset_to_json_text(&asset);if(text){shape_asset_json_text_free(text);return 6;}if(shape_asset_save_file(&asset,argv[2]))return 7;return 0;}
 char *text=shape_asset_to_json_text(&asset);if(!text)return 8;
 if(!shape_asset_save_file(&asset,argv[2]))return 9;
 FILE *f=fopen(argv[2],"rb");if(!f)return 10;char buffer[4096]={0};size_t count=fread(buffer,1,sizeof(buffer),f);fclose(f);
 int same=count==strlen(text)&&memcmp(text,buffer,count)==0;shape_asset_json_text_free(text);return same?0:11;
}
