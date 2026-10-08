#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "import/shape_import.h"
int main(int argc,char **argv){
 if(argc!=2)return 2;
 ShapeSegment seg={.type=SHAPE_SEGMENT_LINE,.p0={-5,-5},.p1={5,5}};
 ShapeSegment polygon[3];
 ShapePath path={.closed=false,.segmentCount=1,.segments=&seg};
 Shape shape={.pathCount=1,.paths=&path};
 ShapeRasterOptions opts={.margin_cells=2,.stroke=1,.max_error=.5,.position_x_norm=.5,.position_y_norm=.5,.rotation_deg=0,.scale=1,.center_fit=false};
 unsigned char mask[1024];memset(mask,0xa5,sizeof(mask));int w=32,h=32;int valid=0;
 if(!strcmp(argv[1],"scale"))opts.scale=1e30f;
 else if(!strcmp(argv[1],"stroke"))opts.stroke=FLT_MAX;
 else if(!strcmp(argv[1],"rotation"))opts.rotation_deg=FLT_MAX;
 else if(!strcmp(argv[1],"nan"))opts.rotation_deg=NAN;
 else if(!strcmp(argv[1],"line_work")){seg.p0.x=-1e8f;seg.p1.x=1e8f;}
 else if(!strcmp(argv[1],"cubic")){seg.type=SHAPE_SEGMENT_CUBIC_BEZIER;seg.c1=(ShapeVec2){1e30f,1e30f};seg.c2=(ShapeVec2){-1e30f,1e30f};}
 else if(!strcmp(argv[1],"cubic_offset")){seg.type=SHAPE_SEGMENT_CUBIC_BEZIER;seg.p0=seg.p1=seg.c1=seg.c2=(ShapeVec2){1e17f,1e17f};}
 else if(!strcmp(argv[1],"source_nan"))seg.p0.x=NAN;
 else if(!strcmp(argv[1],"grid"))w=67108865,h=1;
 else if(!strcmp(argv[1],"fill_work")){polygon[0]=seg;polygon[1]=(ShapeSegment){.type=SHAPE_SEGMENT_LINE,.p0={5,5},.p1={-5,5}};path.segments=polygon;path.segmentCount=2;path.closed=true;w=8192;h=8192;}
 else if(!strcmp(argv[1],"aggregate")){seg.p0.x=-1.5e6f;seg.p1.x=1.5e6f;polygon[0]=seg;polygon[1]=seg;polygon[1].p1.x=-1.5e6f;polygon[2]=seg;path.segments=polygon;path.segmentCount=3;}
 else if(!strcmp(argv[1],"paths"))shape.pathCount=10001;
 else if(!strcmp(argv[1],"points"))path.segmentCount=1048577;
 else if(!strcmp(argv[1],"valid")){valid=1;opts.center_fit=true;}
 else if(!strcmp(argv[1],"fallback")){valid=1;opts.center_fit=true;opts.scale=0;opts.stroke=0;opts.max_error=0;opts.margin_cells=-1;opts.position_x_norm=-1;}
 else return 2;
 int ok=shape_import_rasterize(&shape,w,h,&opts,mask);
 if(valid){if(!ok)return 3;puts("valid");return 0;}
 if(ok){fprintf(stderr,"unexpected successful raster\n");return 4;}
 for(size_t i=0;i<sizeof(mask);i++)if(mask[i]!=0xa5){fprintf(stderr,"refusal mutated mask\n");return 5;}
 puts("refused without mutation");return 0;
}
