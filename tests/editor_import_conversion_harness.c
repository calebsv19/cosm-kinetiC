#include "app/editor/scene_editor_input_import_helpers.h"
#include <stdio.h>
#include <stdlib.h>
int main(int argc,char **argv) {
 if(argc!=4)return 90;
 char path[1024]="sentinel";
 size_t capacity=(size_t)strtoul(argv[3],NULL,10);
 if(capacity>sizeof(path))return 91;
 int ok=scene_editor_input_convert_import_to_asset(argv[1],argv[2],path,capacity);
 printf("%s\n",path);return ok?0:1;
}
