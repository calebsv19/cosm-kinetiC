
#include "app/sim_runtime_backend_2d_internal.h"
#include "app/shape_lookup.h"
#include "import/shape_asset_input.h"
#include "import/shape_import.h"
#include "physics/objects/physics_object_builder.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static const char *mode;
static int allocations, loads, releases, raster_calls;
void *import_mask_test_calloc(size_t n, size_t s) {
    ++allocations;
    return !strcmp(mode,"allocation") ? NULL : calloc(n,s);
}
bool import_mask_test_load(const char *path, ShapeDocument *doc) {
    ++loads;
    if(!strcmp(mode,"empty_document")) {
        memset(doc,0,sizeof(*doc));doc->shapes=calloc(1,sizeof(*doc->shapes));
        return doc->shapes!=NULL;
    }
    return shape_import_load(path,doc);
}
void import_mask_test_free(ShapeDocument *doc) {++releases;ShapeDocument_Free(doc);}
bool import_mask_test_raster(const Shape *shape,int w,int h,const ShapeRasterOptions *opts,uint8_t *out) {
    ++raster_calls;
    if(!strcmp(mode,"partial_raster")){out[0]=1;return false;}
    return shape_import_rasterize(shape,w,h,opts,out);
}
void ts_start_timer(const char *name){(void)name;}
void ts_stop_timer(const char *name){(void)name;}
SimRuntimeBackend *sim_runtime_backend_3d_scaffold_create(const AppConfig *cfg,
 const FluidScenePreset *preset,const SimModeRoute *route,const PhysicsSimRuntimeVisualBootstrap *visual) {
 (void)cfg;(void)preset;(void)route;(void)visual;return NULL;
}
int main(int argc,char **argv) {
    if(argc<2)return 2;mode=argv[1];
    AppConfig cfg={0};cfg.grid_w=cfg.grid_h=32;cfg.window_w=cfg.window_h=128;
    ShapeAssetPoint pts[3]={{-5,-5},{5,5},{-5,5}};
    ShapeAssetPath path={.points=pts,.point_count=3,.closed=true};
    ShapeAsset asset={.schema=1,.name="asset",.paths=&path,.path_count=1};
    ShapeAssetLibrary lib={.assets=&asset,.count=1};
    SceneState scene={0};scene.config=&cfg;scene.shape_library=&lib;
    ImportedShape imp={0};strcpy(imp.path,"asset");imp.scale=1;imp.position_x=imp.position_y=.5f;
    unsigned char out[2048],before[2048],expected[1024];memset(out,0xa5,sizeof(out));memcpy(before,out,sizeof(out));
    size_t count=1024;int valid=0, early=1;
    if(!strcmp(mode,"valid")) {valid=1;early=0;}
    else if(!strcmp(mode,"short"))count=1023;
    else if(!strcmp(mode,"long"))count=1025;
    else if(!strcmp(mode,"zero"))count=0;
    else if(!strcmp(mode,"size_max"))count=(size_t)-1;
    else if(!strcmp(mode,"grid_large")){cfg.grid_w=67108866;cfg.grid_h=2;}
    else if(!strcmp(mode,"grid_int")){cfg.grid_w=cfg.grid_h=2147483647;}
    else if(!strcmp(mode,"grid_zero"))cfg.grid_w=0;
    else if(!strcmp(mode,"grid_negative"))cfg.grid_w=-1;
    else if(!strcmp(mode,"grid_one"))cfg.grid_h=1;
    else if(!strcmp(mode,"path_unterminated"))memset(imp.path,'a',sizeof(imp.path));
    else if(!strcmp(mode,"path_empty"))imp.path[0]=0;
    else if(!strcmp(mode,"library_missing"))lib.assets=NULL;
    else if(!strcmp(mode,"library_count"))lib.count=1025;
    else if(!strncmp(mode,"nonfinite_",10)) {
      switch(atoi(mode+10)){case 0:imp.position_x=NAN;break;case 1:imp.position_y=INFINITY;break;
       case 2:imp.rotation_deg=NAN;break;case 3:imp.scale=INFINITY;break;default:return 2;}
    }
    else if(!strcmp(mode,"missing")){strcpy(imp.path,"/private/tmp/physics-import-mask-absent-20261007.json");early=0;}
    else if(!strcmp(mode,"asset_refused")){imp.scale=1e30f;early=0;}
    else if(!strcmp(mode,"empty_document")){lib.count=0;early=0;}
    else if(!strcmp(mode,"raw") || !strcmp(mode,"partial_raster") || !strcmp(mode,"allocation") || !strcmp(mode,"malformed")) {
        if(argc!=3)return 2;snprintf(imp.path,sizeof(imp.path),"%s",argv[2]);lib.count=0;early=0;
        valid=!strcmp(mode,"raw");
    }
    else return 2;
    int ok=backend_2d_rasterize_import_to_mask(&scene,&imp,out,count);
    int result=0;
    if(valid) {
        if(!ok)result=3;
        else if(!strcmp(mode,"valid")) {
            ShapeAssetRasterOptions opts={.margin_cells=1,.stroke=1,.scale=.8f,.position_x_norm=.5f,.position_y_norm=.5f};
            SceneObjectBase base={.position={.5f,.5f},.scale={.8f,.8f}};PhysicsObject obj={0};
            if(!physics_object_from_asset(&asset,&base,32,32,&opts,&obj))result=4;
            else {memcpy(expected,obj.mask,1024);physics_object_free(&obj);}
        } else {
            ShapeDocument doc={0};ShapeBounds bounds={0};
            if(!shape_import_load(imp.path,&doc) || !doc.shapeCount || !shape_import_bounds(&doc.shapes[0],&bounds))result=5;
            else {
                float d=fmaxf(bounds.max_x-bounds.min_x,bounds.max_y-bounds.min_y);if(d<=.0001f)d=1;
                ShapeRasterOptions opts={.margin_cells=1,.stroke=1,.max_error=.5f,.scale=(.25f/d)*32,.position_x_norm=.5f,.position_y_norm=.5f};
                if(!shape_import_rasterize(&doc.shapes[0],32,32,&opts,expected))result=6;
            }
            ShapeDocument_Free(&doc);
        }
        if(!result && memcmp(expected,out,1024))result=7;
        if(memcmp(before+1024,out+1024,1024))result=8;
    } else if(ok || memcmp(before,out,sizeof(out))) {
        fprintf(stderr,"refusal failed or changed caller bytes\n");result=9;
    }
#ifndef LEGACY_IMPORT_MASK
    if(early && (loads || allocations || raster_calls))result=10;
    if(!strcmp(mode,"empty_document") && (releases!=1 || allocations!=0))result=11;
    if(!strcmp(mode,"allocation") && (allocations!=1 || releases!=1 || raster_calls!=0))result=12;
    if(!strcmp(mode,"partial_raster") && (allocations!=1 || releases!=1 || raster_calls!=1))result=13;
#endif
    if(result)fprintf(stderr,"case=%s code=%d load=%d alloc=%d free=%d raster=%d\n",mode,result,loads,allocations,releases,raster_calls);
    return result;
}
