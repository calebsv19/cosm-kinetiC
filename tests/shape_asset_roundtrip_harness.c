#include "import/shape_asset_output.h"
#include "import/shape_asset_input.h"
#include <stdio.h>
#include <math.h>
#include <float.h>
#include <stdlib.h>
#include <string.h>
int main(int argc,char **argv){
 if(argc!=3)return 90;
 size_t paths=(!strcmp(argv[1],"aggregate")||!strcmp(argv[1],"aggregateboundary"))?2:!strcmp(argv[1],"paths")?1025:!strcmp(argv[1],"pathboundary")?1024:1;
 size_t points=!strcmp(argv[1],"aggregate")?5001:!strcmp(argv[1],"aggregateboundary")?5000:!strcmp(argv[1],"points")?10001:!strcmp(argv[1],"boundary")?10000:2;
 ShapeAsset a={.schema=!strcmp(argv[1],"schema")?2:!strcmp(argv[1],"defaultschema")?0:1,.name="roundtrip",.path_count=paths};
 a.paths=calloc(paths,sizeof(*a.paths));if(!a.paths)return 91;
 a.paths[0].point_count=points;a.paths[0].points=calloc(points,sizeof(*a.paths[0].points));if(!a.paths[0].points)return 92;
 if(!strcmp(argv[1],"aggregate")||!strcmp(argv[1],"aggregateboundary")){a.paths[1].point_count=points;a.paths[1].points=a.paths[0].points;}
 if(!strcmp(argv[1],"maxfloat"))a.paths[0].points[0].x=FLT_MAX;
 ShapeAssetPath *owned_paths=a.paths;ShapeAssetPoint *owned_points=a.paths[0].points;char *owned_name=NULL;
 if(!strcmp(argv[1],"nullpaths"))a.paths=NULL;
 if(!strcmp(argv[1],"nullpoints"))a.paths[0].points=NULL;
 if(!strcmp(argv[1],"nonfinite"))a.paths[0].points[0].x=NAN;
 if(!strcmp(argv[1],"longname")||!strcmp(argv[1],"nameboundary")||!strcmp(argv[1],"escapedoversize")||!strcmp(argv[1],"plainboundary")){
  size_t n=!strcmp(argv[1],"longname")?1048577:!strcmp(argv[1],"plainboundary")?1048574:!strcmp(argv[1],"escapedoversize")?174763:174762;owned_name=malloc(n+1);if(!owned_name)return 93;memset(owned_name,!strcmp(argv[1],"plainboundary")?'a':1,n);owned_name[n]=0;a.name=owned_name;
 }
 int saved=physics_sim_shape_asset_publish(&a,"missing-local-source",argv[2]);
 ShapeAsset b={0};int loaded=saved?physics_sim_shape_asset_load(argv[2],&b):0;
 printf("%d %d %zu\n",saved,loaded,b.path_count?b.paths[0].point_count:0);
 shape_asset_free(&b);free(owned_points);free(owned_paths);free(owned_name);return 0;
}
