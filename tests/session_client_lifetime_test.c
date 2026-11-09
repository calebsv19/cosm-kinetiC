#define _POSIX_C_SOURCE 200809L
#include "app/session_client_lifetime.h"
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
static int fail_create;
static int fail_attr;
int l3_pthread_attr_init(pthread_attr_t *a) { return fail_attr == 1 ? ENOMEM : pthread_attr_init(a); }
int l3_pthread_attr_setdetachstate(pthread_attr_t *a, int state) { return fail_attr == 2 ? EINVAL : pthread_attr_setdetachstate(a, state); }
int l3_pthread_create(pthread_t *,const pthread_attr_t *,void *(*)(void *),void *);
int l3_pthread_create(pthread_t *t,const pthread_attr_t *a,void *(*f)(void *),void *p) {
    return fail_create ? EAGAIN : pthread_create(t,a,f,p);
}
static void pause_ms(long ms) {struct timespec t={ms/1000,(ms%1000)*1000000};nanosleep(&t,NULL);}
int main(int argc,char **argv) {
    assert(argc==2);fail_create=!strcmp(argv[1],"failure");
    fail_attr=!strcmp(argv[1],"attr-init") ? 1 : !strcmp(argv[1],"attr-state") ? 2 : 0;
    bool refusal=fail_create||fail_attr;
    pid_t independent=fork();assert(independent>=0);
    if(!independent){pause_ms(4000);_exit(0);}
    pid_t child=fork();assert(child>=0);
    if(!child){pause_ms(refusal?3000:100);_exit(0);}
    bool accepted=physics_sim_session_client_release(child);
    if(!refusal)pause_ms(300);
    int status=0;pid_t waited=waitpid(child,&status,WNOHANG);int error=errno;
    if(waited==0){kill(child,SIGKILL);waitpid(child,&status,0);}
    fprintf(stderr,"create_failure=%d accepted=%d wait_result=%ld errno=%d\n",fail_create,accepted,(long)waited,error);
    int independent_status=0;
    pid_t independent_wait=waitpid(independent,&independent_status,WNOHANG);
    if(!independent_wait){kill(independent,SIGKILL);waitpid(independent,&independent_status,0);}
    assert(independent_wait==0);
    assert(accepted&&waited==-1&&error==ECHILD);
    assert(!physics_sim_session_client_release(0));
    assert(!physics_sim_session_client_release(-1));
    fail_create=1;assert(!physics_sim_session_client_release(getpid()));
    puts("PASS session client release, thread failure fallback and nonchild refusal");return 0;
}
