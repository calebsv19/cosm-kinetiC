#include <stdio.h>
#include <stdlib.h>
#include "ShapeLib/shape_json.h"
int main(int argc,char **argv){
 if(argc!=4)return 2;
 FILE *f=fopen(argv[1],"rb");if(!f)return 3;
 if(fseek(f,0,SEEK_END)!=0)return 3;long n=ftell(f);if(n<0||n>65536)return 3;rewind(f);
 char *text=calloc((size_t)n+1,1);if(!text)return 3;
 if(fread(text,1,(size_t)n,f)!=(size_t)n||fclose(f)!=0)return 3;
 ShapeDocument file={0},memory={0};
 int a=ShapeDocument_LoadFromJsonFile(argv[1],&file),b=ShapeDocument_LoadFromJsonText(text,&memory);free(text);
 if(a!=b)return 4;
 if(!a){ShapeDocument_Free(&file);ShapeDocument_Free(&memory);puts("rejected");return 0;}
 if(!ShapeDocument_SaveToJsonFile(&file,argv[2])||!ShapeDocument_SaveToJsonFile(&memory,argv[3]))return 5;
 ShapeDocument_Free(&file);ShapeDocument_Free(&memory);puts("loaded");return 0;
}
