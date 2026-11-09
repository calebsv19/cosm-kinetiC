/* Test-only interposition: exact summary commit boundaries in the actual CLI. */
#include <errno.h>
#include <stdbool.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
static void barrier(void) {
 const char *root=getenv("PHYSICS_L3_BARRIER_ROOT");if(!root)return;
 char marker[2048],release[2048];snprintf(marker,sizeof marker,"%s/entered",root);snprintf(release,sizeof release,"%s/release",root);
 int fd=open(marker,O_CREAT|O_WRONLY|O_EXCL,0600);if(fd>=0){write(fd,"1",1);close(fd);}
 for(int i=0;i<1000;i++){if(access(release,F_OK)==0)return;struct timespec delay={0,10000000};nanosleep(&delay,NULL);}
 _exit(86);
}
static int summary_renameat(int oldfd,const char *old,int newfd,const char *name) {
 const char *mode=getenv("PHYSICS_L3_BARRIER");bool selected=mode&&strcmp(name,"result_summary.json")==0;
 if(selected&&!strcmp(mode,"before"))barrier();
 if(selected&&!strcmp(mode,"fail")){errno=EIO;return -1;}
 int result=renameat(oldfd,old,newfd,name);
 if(selected&&result==0&&!strcmp(mode,"after"))barrier();
 return result;
}
__attribute__((used)) static struct {const void *replacement;const void *original;} interposition
__attribute__((section("__DATA,__interpose")))={(const void *)summary_renameat,(const void *)renameat};
