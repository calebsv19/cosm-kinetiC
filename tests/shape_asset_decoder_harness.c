#include "geo/shape_asset.h"
#include <stdio.h>
#include <string.h>
int main(int argc,char **argv){
 if(argc!=3)return 90;ShapeAsset file={0},text={0};
 int a=shape_asset_load_file(argv[1],&file),b=shape_asset_from_json_text(argv[2],&text),same=a==b;
 if(a&&b){char *x=shape_asset_to_json_text(&file),*y=shape_asset_to_json_text(&text);same=x&&y&&!strcmp(x,y);shape_asset_json_text_free(x);shape_asset_json_text_free(y);}
 printf("%d %d\n",a,b);shape_asset_free(&file);shape_asset_free(&text);return same?0:1;
}
