/* Private bounded LE field transport; physics and lifecycle SHA256 stay outside. */
#ifndef PHYSICS_SIM_FIELD_PACKET_IO_H
#define PHYSICS_SIM_FIELD_PACKET_IO_H
#include <stdint.h>
#include <stdlib.h>
#include <float.h>
#define FP_LIMIT (256u*1024u*1024u)
#define FP_META (1024u*1024u)
#define FP_LOGICAL (512u*1024u*1024u)
#define FP_TAG "$field_packet_array_v1"
typedef struct {struct json_object *source;unsigned char *bytes;size_t count,offset;bool integer;} FieldArray;
typedef struct {FieldArray arrays[1024];size_t used,total,logical;bool ok;} FieldPacket;
static uint64_t fp_u64(const unsigned char *p){uint64_t v=0;for(int i=7;i>=0;i--)v=(v<<8)|p[i];return v;}
static void fp_le(unsigned char *p,uint64_t v){for(int i=0;i<8;i++){p[i]=(unsigned char)v;v>>=8;}}
static uint32_t fp_crc(const unsigned char *p,size_t n){
    uint32_t table[256];for(unsigned i=0;i<256;i++){uint32_t c=i;for(int j=0;j<8;j++)c=(c>>1)^((c&1)?0xedb88320u:0);table[i]=c;}
    uint32_t c=0xffffffffu;for(size_t i=0;i<n;i++)c=table[(c^p[i])&255]^(c>>8);return c^0xffffffffu;
}
static struct json_object *fp_get(struct json_object *o,const char *k){struct json_object *v=NULL;return o && json_object_object_get_ex(o,k,&v)?v:NULL;}
static bool fp_int(struct json_object *o,uint64_t *v){if(!o || !json_object_is_type(o,json_type_int) || json_object_get_int64(o)<0)return false;*v=(uint64_t)json_object_get_int64(o);return true;}
static struct json_object *fp_restore(struct json_object *v,FieldPacket *p,unsigned depth){
    if(depth>32){p->ok=false;return NULL;}
    if(!v)return NULL;
    if(json_object_is_type(v,json_type_object)){
        struct json_object *ref=NULL;
        if(json_object_object_get_ex(v,FP_TAG,&ref)){uint64_t i;if(json_object_object_length(v)!=1 || !fp_int(ref,&i) || i>=p->used){p->ok=false;return NULL;}
            FieldArray *a=&p->arrays[i];a->source=v;if(p->logical>FP_LOGICAL-a->count*8){p->ok=false;return NULL;}p->logical+=a->count*8;
            struct json_object *out=json_object_new_array_ext((int)a->count);if(!out){p->ok=false;return NULL;}
            for(size_t q=0;q<a->count;q++){uint64_t bits=fp_u64(a->bytes+8*q);struct json_object *x;
                if(a->integer){int64_t z;memcpy(&z,&bits,8);x=json_object_new_int64(z);}else{double z;memcpy(&z,&bits,8);if(!isfinite(z)){p->ok=false;json_object_put(out);return NULL;}x=json_object_new_double(z);}
                json_object_array_add(out,x);
            }return out;
        }
        struct json_object *out=json_object_new_object();
        json_object_object_foreach(v,k,x){struct json_object *y=fp_restore(x,p,depth+1);if(!p->ok){json_object_put(out);return NULL;}json_object_object_add(out,k,y);}return out;
    }
    if(json_object_is_type(v,json_type_array)){
        size_t n=json_object_array_length(v);struct json_object *out=json_object_new_array_ext((int)n);
        for(size_t i=0;i<n;i++){struct json_object *x=fp_restore(json_object_array_get_idx(v,i),p,depth+1);if(!p->ok){json_object_put(out);return NULL;}json_object_array_add(out,x);}return out;
    }
    if(json_object_is_type(v,json_type_double) && !isfinite(json_object_get_double(v)))p->ok=false;
    return json_object_get(v);
}
static struct json_object *fp_read(const char *path){
    if(sizeof(double)!=8 || DBL_MANT_DIG!=53 || DBL_MAX_EXP!=1024)return NULL;
    FILE *f=fopen(path,"rb");if(!f)return NULL;unsigned char h[24];struct json_object *m=NULL,*out=NULL;unsigned char *bytes=NULL;char *meta=NULL;FieldPacket *p=NULL;
    if(fread(h,1,24,f)!=24 || memcmp(h,"PSFPK01\n",8))goto done;
    uint64_t ml=fp_u64(h+8),pl=fp_u64(h+16);if(!ml || ml>FP_META || pl>FP_LIMIT || ml+pl+24>FP_LIMIT)goto done;
    meta=malloc((size_t)ml+1);bytes=malloc(pl? (size_t)pl:1);p=calloc(1,sizeof(*p));if(!meta || !bytes || !p)goto done;
    if(fread(meta,1,(size_t)ml,f)!=ml || fread(bytes,1,(size_t)pl,f)!=pl || fgetc(f)!=EOF)goto done;meta[ml]=0;
    struct json_tokener *tok=json_tokener_new();m=json_tokener_parse_ex(tok,meta,(int)ml);bool parsed=json_tokener_get_error(tok)==json_tokener_success && json_tokener_get_parse_end(tok)==ml;json_tokener_free(tok);
    if(!parsed || !m || !json_object_is_type(m,json_type_object) || json_object_object_length(m)!=3)goto done;
    struct json_object *schema=fp_get(m,"schema"),*arrays=fp_get(m,"arrays");
    if(!schema || !json_object_is_type(schema,json_type_string) || strcmp(json_object_get_string(schema),"physics_sim_field_packet/v1") || !arrays || !json_object_is_type(arrays,json_type_array))goto done;
    size_t n=json_object_array_length(arrays),end=0;if(n>1024)goto done;p->used=n;p->ok=true;
    for(size_t i=0;i<n;i++){
        struct json_object *r=json_object_array_get_idx(arrays,i),*dtype=fp_get(r,"dtype");uint64_t count,offset,crc;
        if(!r || !json_object_is_type(r,json_type_object) || json_object_object_length(r)!=4 || !dtype || !json_object_is_type(dtype,json_type_string) ||
           !fp_int(fp_get(r,"count"),&count) || count<256 || count>8u*1024u*1024u || !fp_int(fp_get(r,"offset"),&offset) || offset!=end ||
           !fp_int(fp_get(r,"crc32"),&crc) || crc>UINT32_MAX || count*8>pl-end)goto done;
        const char *t=json_object_get_string(dtype);if(strcmp(t,"f64") && strcmp(t,"i64"))goto done;
        p->arrays[i]=(FieldArray){.bytes=bytes+end,.count=(size_t)count,.integer=!strcmp(t,"i64")};if(fp_crc(bytes+end,(size_t)count*8)!=crc)goto done;end+=(size_t)count*8;
    }
    if(end!=pl)goto done;out=fp_restore(fp_get(m,"value"),p,0);for(size_t i=0;i<n;i++)if(!p->arrays[i].source)p->ok=false;
    if(!p->ok){if(out)json_object_put(out);out=NULL;}
 done:
    if(m)json_object_put(m);free(p);free(meta);free(bytes);fclose(f);return out;
}
static struct json_object *fp_reduce(struct json_object *v,FieldPacket *p,unsigned depth){
    if(depth>32){p->ok=false;return NULL;}if(!v)return NULL;
    if(json_object_is_type(v,json_type_array)){
        size_t n=json_object_array_length(v);bool ints=true,doubles=true;
        for(size_t i=0;i<n;i++){struct json_object *x=json_object_array_get_idx(v,i);ints=ints && json_object_is_type(x,json_type_int);doubles=doubles && json_object_is_type(x,json_type_double);}
        if(n>=256 && (ints || doubles)){
            if(n>8u*1024u*1024u || p->logical>FP_LOGICAL-n*8){p->ok=false;return NULL;}p->logical+=n*8;
            size_t index=0;while(index<p->used && p->arrays[index].source!=v)index++;
            if(index==p->used){
                if(p->used==1024 || p->total>FP_LIMIT-n*8){p->ok=false;return NULL;}
                unsigned char *raw=malloc(n*8);if(!raw){p->ok=false;return NULL;}
                for(size_t i=0;i<n;i++){struct json_object *x=json_object_array_get_idx(v,i);uint64_t bits;if(ints){int64_t z=json_object_get_int64(x);memcpy(&bits,&z,8);}else{double z=json_object_get_double(x);if(!isfinite(z)){free(raw);p->ok=false;return NULL;}memcpy(&bits,&z,8);}fp_le(raw+8*i,bits);}
                p->arrays[p->used++]=(FieldArray){.source=v,.bytes=raw,.count=n,.offset=p->total,.integer=ints};p->total+=n*8;
            }
            struct json_object *ref=json_object_new_object();json_object_object_add(ref,FP_TAG,json_object_new_int64((int64_t)index));return ref;
        }
        struct json_object *out=json_object_new_array_ext((int)n);
        for(size_t i=0;i<n;i++){struct json_object *x=fp_reduce(json_object_array_get_idx(v,i),p,depth+1);if(!p->ok){json_object_put(out);return NULL;}json_object_array_add(out,x);}return out;
    }
    if(json_object_is_type(v,json_type_object)){
        if(fp_get(v,FP_TAG)){p->ok=false;return NULL;}struct json_object *out=json_object_new_object();
        json_object_object_foreach(v,k,x){struct json_object *y=fp_reduce(x,p,depth+1);if(!p->ok){json_object_put(out);return NULL;}json_object_object_add(out,k,y);}return out;
    }
    if(json_object_is_type(v,json_type_double) && !isfinite(json_object_get_double(v)))p->ok=false;
    return json_object_get(v);
}
static bool fp_write(struct json_object *value,size_t limit){
    FieldPacket *p=calloc(1,sizeof(*p));if(!p)return false;p->ok=true;bool ok=false;struct json_object *m=json_object_new_object(),*arrays=json_object_new_array(),*reduced=fp_reduce(value,p,0);
    json_object_object_add(m,"schema",json_object_new_string("physics_sim_field_packet/v1"));json_object_object_add(m,"value",reduced);json_object_object_add(m,"arrays",arrays);
    if(!p->ok)goto done;
    for(size_t i=0;i<p->used;i++){FieldArray *a=&p->arrays[i];struct json_object *r=json_object_new_object();json_object_object_add(r,"dtype",json_object_new_string(a->integer?"i64":"f64"));json_object_object_add(r,"count",json_object_new_int64((int64_t)a->count));json_object_object_add(r,"offset",json_object_new_int64((int64_t)a->offset));json_object_object_add(r,"crc32",json_object_new_int64(fp_crc(a->bytes,a->count*8)));json_object_array_add(arrays,r);}
    const char *meta=json_object_to_json_string_ext(m,JSON_C_TO_STRING_PLAIN);size_t ml=strlen(meta);if(ml>FP_META || ml+p->total+24>limit)goto done;
    unsigned char h[24];memcpy(h,"PSFPK01\n",8);fp_le(h+8,ml);fp_le(h+16,p->total);if(fwrite(h,1,24,stdout)!=24 || fwrite(meta,1,ml,stdout)!=ml)goto done;
    for(size_t i=0;i<p->used;i++)if(fwrite(p->arrays[i].bytes,8,p->arrays[i].count,stdout)!=p->arrays[i].count)goto done;
    ok=fflush(stdout)==0;
 done:
    json_object_put(m);for(size_t i=0;i<p->used;i++)free(p->arrays[i].bytes);free(p);return ok;
}
#endif
