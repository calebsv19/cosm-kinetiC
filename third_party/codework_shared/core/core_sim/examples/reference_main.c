#define _POSIX_C_SOURCE 200809L
#include "reference_host.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int number(const char *text, uint64_t *out) {
    uint64_t n=0;
    if (!*text) return 0;
    while (*text) {
        unsigned digit=(unsigned char)*text++-'0';
        if (digit>9 || n>(UINT64_MAX-digit)/10) return 0;
        n=n*10+digit;
    }
    *out=n; return 1;
}
int main(int argc, char **argv) {
    RefHost host;
    RefState current={0},proposed;
    RefObservations observations={0,REF_HASH_INIT,0};
    const char *path=NULL;
    uint64_t mode=2,fps=25,stop=200,fail_window=UINT64_MAX,phase=0;
    int resume=0,i;
    struct timespec begin,end;
    uint64_t elapsed;
    if (clock_gettime(CLOCK_MONOTONIC,&begin)) return 1;
    for (i=1;i<argc;++i) {
        const char *arg=argv[i];
        if (!strcmp(arg,"--resume")) { resume=1; continue; }
        if (++i==argc) goto usage;
        if (!strcmp(arg,"--checkpoint")) path=argv[i];
        else if (!strcmp(arg,"--mode")) { if (!number(argv[i],&mode)) goto usage; }
        else if (!strcmp(arg,"--fps")) { if (!number(argv[i],&fps)) goto usage; }
        else if (!strcmp(arg,"--stop-after")) { if (!number(argv[i],&stop)) goto usage; }
        else if (!strcmp(arg,"--fail-window")) { if (!number(argv[i],&fail_window)) goto usage; }
        else if (!strcmp(arg,"--fail-phase")) { if (!number(argv[i],&phase)) goto usage; }
        else goto usage;
    }
    if (!path || mode>2 || (fps!=5 && fps!=25 && fps!=60) || stop>200 || phase>3 ||
        ((phase!=0)!=(fail_window!=UINT64_MAX)) || (phase && fail_window>=200)) goto usage;
    if (!ref_host_init(&host,mode)) return 1;
    if (core_sim_joint_plan_validate(&host.plan)!=CORE_SIM_JOINT_OK) return 1;
    if (resume) {
        if (!ref_load(path,&host,mode,&current)) { fprintf(stderr,"Checkpoint rejected\n"); return 1; }
    } else {
        current.mode=mode; current.trajectory=REF_HASH_INIT;
        if (!ref_restore_metadata(&host,&current) || !ref_save(path,&current,1)) {
            fprintf(stderr,"Initial publication failed (existing files are preserved)\n"); return 1;
        }
    }
    if (stop<current.windows) goto usage;
    if (!current.windows) {
        observations.count=1;
        observations.hash=ref_mix(observations.hash,0);
        observations.hash=ref_mix(ref_mix(observations.hash,0),1);
        observations.hash=ref_mix(ref_mix(observations.hash,0),1);
    }
    while (current.windows<stop) {
        unsigned fault=current.windows==fail_window ? (unsigned)phase : 0;
        int result=ref_window(&host,&current,fault<=2 ? fault : 0,fps,&proposed,&observations);
        if (result==75) { fprintf(stderr,"Injected native failure; staged window discarded\n"); return 75; }
        if (result!=1) { fprintf(stderr,"Window qualification failed\n"); return 1; }
        if (!ref_save(path,&proposed,0)) {
            fprintf(stderr,"Publication uncertain; reread the same checkpoint before retry\n"); return 1;
        }
        if (fault==3) { fprintf(stderr,"Injected exit after durable publish, before memory adoption\n"); return 75; }
        current=proposed; /* Visible only after persistence succeeded. */
    }
    if (clock_gettime(CLOCK_MONOTONIC,&end)) return 1;
    elapsed=(uint64_t)(end.tv_sec-begin.tv_sec)*UINT64_C(1000000000);
    if (end.tv_nsec>=begin.tv_nsec) elapsed+=(uint64_t)(end.tv_nsec-begin.tv_nsec);
    else elapsed-=(uint64_t)(begin.tv_nsec-end.tv_nsec);
    printf("{\"windows\":%" PRIu64 ",\"model_ticks\":%" PRIu64 ",\"ticks_per_second\":600,"
        "\"emitted\":%" PRIu64 ",\"received\":%" PRIu64 ",\"source_steps\":%" PRIu64
        ",\"receiver_steps\":%" PRIu64 ",\"signal\":%" PRIu64 ",\"trajectory\":%" PRIu64
        ",\"observations\":%" PRIu64 ",\"observation_hash\":%" PRIu64 ",\"fractional_brackets\":%" PRIu64
        ",\"elapsed_wall_ns\":%" PRIu64 "}\n",current.windows,current.windows*120,
        current.native.emitted,current.native.received,current.native.source_steps,current.native.receiver_steps,
        current.native.signal,current.trajectory,observations.count,observations.hash,observations.fractional_brackets,elapsed);
    return 0;
usage:
    fprintf(stderr,"Usage: reference --checkpoint PATH [--resume] [--mode 0|1|2] [--fps 5|25|60] "
        "[--stop-after 0..200] [--fail-window 0..199 --fail-phase 1|2|3]\n");
    return 2;
}
