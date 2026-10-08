
#include "physics/objects/physics_object_builder.h"
#include "import/shape_asset_input.h"
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int calls, fail_allocation;
void *object_test_calloc(size_t n, size_t s) {
    ++calls;
    return fail_allocation ? NULL : calloc(n,s);
}
int main(int argc, char **argv) {
    if (argc < 2) return 2;
    const char *mode = argv[1];
    ShapeAssetPoint points[3] = {{-5,-5},{5,5},{-5,5}};
    ShapeAssetPath paths[1025] = {{.point_count=2,.points=points}};
    ShapeAsset asset = {.schema=1,.path_count=1,.paths=paths};
    SceneObjectBase base = {.position={.5f,.5f},.scale={1,1}};
    ShapeAssetRasterOptions opts = {.margin_cells=1,.stroke=1,.position_x_norm=.5f,.position_y_norm=.5f,.scale=1};
    PhysicsObject out = {0}; out.density=1.75f; out.mask_w=17;
    int w=32,h=32, valid=0, expected_calls=0;
    ShapeAsset loaded={0}; unsigned char *prior=NULL;
    if (!strcmp(mode,"valid")) {valid=1;expected_calls=1;}
    else if (!strcmp(mode,"closed")) {valid=1;expected_calls=1;paths[0].closed=true;paths[0].point_count=3;}
    else if (!strcmp(mode,"fit")) {valid=1;expected_calls=1;opts.center_fit=true;opts.rotation_deg=37;}
    else if (!strcmp(mode,"fallback")) {valid=1;expected_calls=1;opts.margin_cells=-1;opts.stroke=0;opts.scale=0;opts.position_x_norm=-1;}
    else if (!strcmp(mode,"file")) {
        if(argc!=3 || !physics_sim_shape_asset_load(argv[2],&loaded)) return 8;
        asset=loaded;opts.center_fit=true;opts.rotation_deg=17;valid=1;expected_calls=1;
    }
    else if (!strcmp(mode,"scale")) opts.scale=1e30f;
    else if (!strcmp(mode,"stroke")) opts.stroke=FLT_MAX;
    else if (!strcmp(mode,"rotation")) opts.rotation_deg=FLT_MAX;
    else if (!strcmp(mode,"coordinate")) points[0].x=FLT_MAX;
    else if (!strcmp(mode,"source_nan")) points[0].x=NAN;
    else if (!strcmp(mode,"grid")) {w=67108865;h=1;}
    else if (!strcmp(mode,"grid_int")) {w=2147483647;h=2147483647;}
    else if (!strcmp(mode,"grid_zero")) w=0;
    else if (!strcmp(mode,"paths")) asset.path_count=1025;
    else if (!strcmp(mode,"points")) paths[0].point_count=10001;
    else if (!strcmp(mode,"missing_paths")) asset.paths=NULL;
    else if (!strcmp(mode,"missing_points")) paths[0].points=NULL;
    else if (!strcmp(mode,"schema")) asset.schema=2;
    else if (!strcmp(mode,"empty")) asset.path_count=0;
    else if (!strcmp(mode,"line_work")) {points[0].x=-1e8f;points[1].x=1e8f;}
    else if (!strcmp(mode,"aggregate")) {
        points[0].x=-1.5e6f;points[1].x=1.5e6f;
        asset.path_count=3;paths[1]=paths[0];paths[2]=paths[0];
    }
    else if (!strcmp(mode,"fill_work")) {paths[0].closed=true;paths[0].point_count=3;w=8192;h=8192;}
    else if (!strcmp(mode,"allocation")) {fail_allocation=1;expected_calls=1;}
    else if (!strcmp(mode,"exact_grid")) {w=67108864;h=1;fail_allocation=1;expected_calls=1;}
    else if (!strcmp(mode,"owned_output")) {prior=malloc(1);if(!prior)return 9;*prior=0xa5;out.mask=prior;}
    else if (!strncmp(mode,"opts_nan_",9)) {
        int f=atoi(mode+9);
        switch(f){case 0:opts.margin_cells=NAN;break;case 1:opts.stroke=NAN;break;
        case 2:opts.max_error=NAN;break;case 3:opts.position_x_norm=NAN;break;
        case 4:opts.position_y_norm=NAN;break;case 5:opts.rotation_deg=NAN;break;
        case 6:opts.scale=NAN;break;default:return 2;}
    }
    else if (!strncmp(mode,"base_nan_",9)) {
        int f=atoi(mode+9);
        switch(f){case 0:base.position.x=NAN;break;case 1:base.position.y=NAN;break;
        case 2:base.rotation=NAN;break;case 3:base.scale.x=NAN;break;
        case 4:base.scale.y=NAN;break;default:return 2;}
    }
    else return 2;
    PhysicsObject before; memcpy(&before,&out,sizeof(out));
    int ok=physics_object_from_asset(&asset,&base,w,h,&opts,&out);
    int result=0;
    if (valid) {
        unsigned char expected[1024];
        ShapeAssetRasterOptions resolved=opts;
        if(resolved.margin_cells<0)resolved.margin_cells=1;
        if(resolved.stroke<=0)resolved.stroke=1;
        if(resolved.scale<=0)resolved.scale=1;
        if(resolved.position_x_norm<0 || resolved.position_x_norm>1)resolved.position_x_norm=.5f;
        if(resolved.position_y_norm<0 || resolved.position_y_norm>1)resolved.position_y_norm=.5f;
        if(!ok || !out.mask || !shape_asset_rasterize(&asset,w,h,&resolved,expected) ||
           memcmp(expected,out.mask,sizeof(expected))) result=3;
    } else {
        if(ok) {fprintf(stderr,"unexpected accepted raster\n");result=4;}
        else if(memcmp(&before,&out,sizeof(out))) {fprintf(stderr,"failure changed caller output\n");result=5;}
    }
#ifndef LEGACY_OBJECT
    if(calls!=expected_calls){fprintf(stderr,"allocation calls=%d expected=%d\n",calls,expected_calls);result=6;}
#endif
    if(out.mask!=prior)physics_object_free(&out);
    if(prior){if(*prior!=0xa5)result=7;free(prior);}
    shape_asset_free(&loaded);
    if(!result)puts(valid?"accepted with legacy mask parity":"refused with output preserved");
    return result;
}
