#include "geo/shape_asset.h"
#include <stdio.h>
#ifdef HOST_READER
#include "import/shape_asset_input.h"
#define LOAD physics_sim_shape_asset_load
#else
#define LOAD shape_asset_load_file
#endif
int main(int argc,char **argv){
 if(argc!=2)return 90;ShapeAsset asset={0};
 int ok=LOAD(argv[1],&asset);
 printf("%d %zu\n",ok,asset.path_count);shape_asset_free(&asset);return ok?0:1;
}
