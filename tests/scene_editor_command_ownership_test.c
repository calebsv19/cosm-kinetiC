#define _DARWIN_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "app/editor/scene_editor_draft_commit.h"
#include "app/editor/scene_editor_input.h"
#include "app/editor/scene_editor_input_common.h"
#include "app/editor/scene_editor_input_import_helpers.h"
#include "app/editor/scene_editor_model.h"

static SceneEditorState *begin(FluidScenePreset *target, char *name, size_t capacity) {
    SceneEditorState *s=calloc(1,sizeof(*s));assert(s);
    s->cfg=app_config_default();s->target=target;s->working=*target;
    s->name_buffer=name;s->name_capacity=capacity;s->running=true;
    s->selected_object=s->selected_emitter=s->selected_row=-1;
    s->boundary_selected_edge=-1;
    physics_sim_editor_session_init(&s->session,&s->working,NULL);
    scene_editor_draft_begin_name(s);return s;
}
static void rename_draft(SceneEditorState *s,const char *name) {
    editor_begin_name_edit(s);
    text_input_begin(&s->name_input,name,s->name_edit_capacity-1);
    editor_key_down(s,SDLK_RETURN,0);
    assert(!s->renaming_name&&!strcmp(s->working.name,name));
}
int main(void) {
    SDL_setenv("SDL_VIDEODRIVER","dummy",1);assert(!SDL_Init(SDL_INIT_VIDEO));
    char name[128]="Original";
    FluidScenePreset target={0};target.name=name;target.domain_width=10;target.domain_height=8;
    FluidScenePreset before=target;
    SceneEditorState *s=begin(&target,name,sizeof(name));
    rename_draft(s,"Cancelled rename");assert(!strcmp(name,"Original"));
    s->working.domain_width=12;
    scene_editor_input_cancel_and_close(s);scene_editor_draft_publish(s);
    assert(!memcmp(&before,&target,sizeof(target))&&!strcmp(name,"Original"));free(s);
    s=begin(&target,name,sizeof(name));rename_draft(s,"Applied rename");
    s->working.domain_width=14;editor_key_down(s,SDLK_RETURN,0);
    assert(s->applied&&!s->running&&target.domain_width==10&&!strcmp(name,"Original"));
    scene_editor_draft_publish(s);assert(target.domain_width==14&&target.name==name&&!strcmp(name,"Applied rename"));
    free(s);assert(!strcmp(target.name,"Applied rename"));
    s=begin(&target,NULL,0);rename_draft(s,"Temporary title");
    editor_key_down(s,SDLK_RETURN,0);scene_editor_draft_publish(s);free(s);
    assert(target.name==name&&!strcmp(target.name,"Applied rename"));

    char dir[]="/private/tmp/kinetic-command-XXXXXX";assert(mkdtemp(dir));
    char path[256];snprintf(path,sizeof(path),"%s/item.asset.json",dir);
    FILE *f=fopen(path,"w");assert(f);assert(fputs("{\"schema\":1,\"name\":\"item\",\"paths\":[{\"closed\":false,\"points\":[{\"x\":0,\"y\":0},{\"x\":1,\"y\":1}]}]}",f)>=0);assert(!fclose(f));
    ShapeAssetLibrary libraries[2]={{0},{0}};SceneEditorState *pair[2];
    for(int i=0;i<2;i++) {
        pair[i]=begin(&target,name,sizeof(name));pair[i]->shape_library=&libraries[i];
        pair[i]->showing_import_picker=true;pair[i]->import_file_count=1;pair[i]->selected_import_row=0;
        snprintf(pair[i]->import_files[0],sizeof(pair[i]->import_files[0]),"%s",path);
    }
    editor_key_down(pair[0],SDLK_RETURN,0);
    assert(scene_editor_input_drop_import_from_picker(pair[1],0,.5f,.5f));
    assert(pair[0]->working.import_shape_count==1&&pair[1]->working.import_shape_count==1);
    assert(!memcmp(pair[0]->working.import_shapes,pair[1]->working.import_shapes,sizeof(pair[0]->working.import_shapes)));
    assert(pair[0]->dirty&&pair[1]->dirty&&!target.import_shape_count);
    FluidScenePreset admitted=pair[1]->working;pair[1]->dirty=false;
    assert(scene_editor_input_drop_import_from_picker(pair[1],0,.2f,.8f));
    assert(!pair[1]->dirty&&!memcmp(&admitted,&pair[1]->working,sizeof(admitted)));
    for(int i=0;i<2;i++) {scene_editor_input_cancel_and_close(pair[i]);scene_editor_draft_publish(pair[i]);
        assert(!target.import_shape_count);for(size_t j=0;j<libraries[i].count;j++)shape_asset_free(&libraries[i].assets[j]);free(libraries[i].assets);free(pair[i]);}
    assert(!unlink(path)&&!rmdir(dir));SDL_Quit();puts("Command draft ownership, native picker parity, cancellation and stable name lifetime passed");
    return 0;
}
