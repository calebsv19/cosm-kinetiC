#define _POSIX_C_SOURCE 200809L
#include "reference_host.h"
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Example-owned canonical text format. Fixed v1 recipe reconstructs every
 * active T4 metadata field from plan version, window and native checkpoints;
 * raw C structs/padding are never serialized. FNV detects accidental damage,
 * not hostile edits. A real host needs its own authenticated content store. */
static uint64_t checksum(const char *body) {
    uint64_t h=REF_HASH_INIT;
    while (*body) { h^=(unsigned char)*body++; h*=UINT64_C(1099511628211); }
    return h;
}
static int encode(const RefState *s, char *text, size_t capacity) {
    char body[768];
    const RefNative *n=&s->native,*p=&s->previous;
    int size=snprintf(body,sizeof(body),
        "T5REF1 %" PRIu64 " %" PRIu64 " %" PRIu64 " %" PRIu64 " %" PRIu64 "\n"
        "NATIVE %" PRIu64 " %" PRIu64 " %" PRIu64 " %" PRIu64 " %" PRIu64 "\n"
        "PREVIOUS %" PRIu64 " %" PRIu64 " %" PRIu64 " %" PRIu64 " %" PRIu64 "\n",
        s->mode,s->windows,s->trajectory,90+s->mode,UINT64_C(0x543500)+s->mode,
        n->emitted,n->received,n->source_steps,n->receiver_steps,n->signal,
        p->emitted,p->received,p->source_steps,p->receiver_steps,p->signal);
    if (size<0 || (size_t)size>=sizeof(body)) return 0;
    size=snprintf(text,capacity,"%sCHECKSUM %016" PRIx64 "\n",body,checksum(body));
    return size>=0 && (size_t)size<capacity;
}
int ref_load(const char *path, const RefHost *host, uint64_t mode, RefState *out) {
    RefState s={0};
    RefNative *n=&s.native,*p=&s.previous;
    char text[1024],canonical[1024];
    uint64_t plan_id,plan_ref,digest;
    size_t size;
    FILE *file=fopen(path,"rb");
    if (!file) return 0;
    size=fread(text,1,sizeof(text)-1,file);
    if (ferror(file) || !feof(file)) { fclose(file); return 0; }
    if (fclose(file)) return 0;
    text[size]=0;
    if (sscanf(text,
        "T5REF1 %" SCNu64 " %" SCNu64 " %" SCNu64 " %" SCNu64 " %" SCNu64
        " NATIVE %" SCNu64 " %" SCNu64 " %" SCNu64 " %" SCNu64 " %" SCNu64
        " PREVIOUS %" SCNu64 " %" SCNu64 " %" SCNu64 " %" SCNu64 " %" SCNu64
        " CHECKSUM %" SCNx64,
        &s.mode,&s.windows,&s.trajectory,&plan_id,&plan_ref,
        &n->emitted,&n->received,&n->source_steps,&n->receiver_steps,&n->signal,
        &p->emitted,&p->received,&p->source_steps,&p->receiver_steps,&p->signal,&digest)!=16) return 0;
    (void)digest; /* Canonical whole-file comparison checks the computed digest. */
    if (s.mode!=mode || plan_id!=host->schedule.plan_id || plan_ref!=host->plan.plan_ref ||
        !encode(&s,canonical,sizeof(canonical)) || size!=strlen(canonical) ||
        memcmp(text,canonical,size)!=0 || !ref_restore_metadata(host,&s)) return 0;
    *out=s;
    return 1;
}
int ref_save(const char *path, const RefState *s, int fresh) {
    char text[1024],temp[1100],parent[1024];
    char *slash;
    size_t written=0,length;
    int fd,dir,size,ok=1;
    if (!ref_native_valid(s) || !encode(s,text,sizeof(text)) || strlen(path)>=sizeof(parent)) return 0;
    size=snprintf(temp,sizeof(temp),"%s.tmp.XXXXXX",path);
    if (size<0 || (size_t)size>=sizeof(temp)) return 0;
    fd=mkstemp(temp);
    if (fd<0) return 0;
    length=strlen(text);
    while (written<length) {
        ssize_t count=write(fd,text+written,length-written);
        if (count<0 && errno==EINTR) continue;
        if (count<=0) { ok=0; break; }
        written+=(size_t)count;
    }
    if (ok && fsync(fd)) ok=0;
    if (close(fd)) ok=0;
    /* Initial publication refuses to replace an existing checkpoint. Subsequent
     * rename is atomic on the same filesystem; this demo requires one writer. */
    if (ok) {
        if (fresh) { if (link(temp,path)) ok=0; }
        else if (rename(temp,path)) ok=0;
    }
    (void)unlink(temp);
    if (!ok) return 0;
    strcpy(parent,path); slash=strrchr(parent,'/');
    if (!slash) strcpy(parent,".");
    else if (slash==parent) slash[1]=0;
    else *slash=0;
    dir=open(parent,O_RDONLY);
    if (dir<0) return 0;
    ok=fsync(dir)==0;
    if (close(dir)) ok=0;
    return ok;
}
