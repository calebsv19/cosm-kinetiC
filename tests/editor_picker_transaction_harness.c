#define _POSIX_C_SOURCE 200809L
#include "app/editor/scene_editor_input_import_helpers.h"
#include "app/editor/scene_editor_internal.h"
#include "app/editor/scene_editor_model.h"
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
static int selections,refreshes;
static int fail_growth;
void *picker_test_realloc(void *p,size_t n){return fail_growth?NULL:realloc(p,n);}
void set_dirty(SceneEditorState *s){s->dirty=true;}
void scene_editor_select_import(SceneEditorState *s,int i){s->selected_row=i;selections++;}
void scene_editor_refresh_import_files(SceneEditorState *s){refreshes++;strcpy(s->import_files[0],"changed-on-refresh");}
void scene_editor_sync_selection_session(SceneEditorState *s){(void)s;}
int emitter_index_for_import(const SceneEditorState *s,int i){(void)s;(void)i;return -1;}
void remove_emitter_at(SceneEditorState *s,int i){(void)s;(void)i;}
int main(int argc,char **argv){
 if(argc!=5)return 90;
 SceneEditorState *s=calloc(1,sizeof(*s));if(!s)return 91;
 ShapeAssetLibrary lib={0};s->shape_library=&lib;s->showing_import_picker=true;s->selected_row=-1;s->import_file_count=1;
 if(strlen(argv[2])>=sizeof(s->import_files[0]))return 92;
 strcpy(s->import_files[0],argv[2]);snprintf(s->cfg.input_root,sizeof(s->cfg.input_root),"%s",argv[4]);
 if(!strcmp(argv[1],"cached")){
  lib.assets=calloc(1,sizeof(*lib.assets));if(!lib.assets||!shape_asset_load_file(argv[3],lib.assets))return 93;lib.count=1;
 }
 if(!strcmp(argv[1],"empty"))s->import_files[0][0]=0;
 if(!strcmp(argv[1],"unterminated"))memset(s->import_files[0],'x',sizeof(s->import_files[0]));
 if(!strcmp(argv[1],"badcount"))s->import_file_count=MAX_IMPORT_FILES+1;
 if(!strcmp(argv[1],"growthfail"))fail_growth=1;
 if(!strcmp(argv[1],"libraryfull")){lib.assets=calloc(1024,sizeof(*lib.assets));if(!lib.assets)return 94;lib.count=1024;}
 if(!strcmp(argv[1],"existing")||!strcmp(argv[1],"drop-existing")){lib.assets=calloc(1,sizeof(*lib.assets));if(!lib.assets||!shape_asset_load_file(argv[3],lib.assets))return 93;lib.count=1;s->working.import_shape_count=1;s->working.import_shapes[0].shape_id=0;strcpy(s->working.import_shapes[0].path,argv[2]);}
 FluidScenePreset *before=malloc(sizeof(*before));if(!before)return 95;*before=s->working;
 if(!strcmp(argv[1],"full"))s->working.import_shape_count=MAX_IMPORTED_SHAPES;
 if(!strcmp(argv[1],"nolibrary"))s->shape_library=NULL;
 *before=s->working;
 int ok;
 if(!strncmp(argv[1],"drop",4))ok=scene_editor_input_drop_import_from_picker(s,0,!strcmp(argv[1],"dropnan")?NAN:0.25f,0.75f);
 else ok=scene_editor_input_add_import_from_picker(s,!strcmp(argv[1],"badrow")?-1:0);
 float x=-999;if(lib.count&&lib.assets&&lib.assets[0].path_count&&lib.assets[0].paths[0].point_count)x=lib.assets[0].paths[0].points[0].x;
 printf("{\"position_x\":%.9g,\"position_y\":%.9g,\"working_unchanged\":%d,\"ok\":%d,\"count\":%zu,\"library_count\":%zu,\"dirty\":%d,\"picker\":%d,\"selected\":%d,\"refreshes\":%d,\"selections\":%d,\"shape_id\":%d,\"x\":%.9g}\n",s->working.import_shapes[0].position_x,s->working.import_shapes[0].position_y,memcmp(before,&s->working,sizeof(*before))==0,ok,s->working.import_shape_count,lib.count,s->dirty,s->showing_import_picker,s->selected_row,refreshes,selections,s->working.import_shapes[0].shape_id,x);
 for(size_t i=0;i<lib.count;i++)shape_asset_free(&lib.assets[i]);free(lib.assets);free(before);free(s);return 0;
}
