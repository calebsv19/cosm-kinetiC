#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <json-c/json.h>
#include "physics_sim_field_packet_io.h"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"packet test line %d\n",__LINE__);return 1;}}while(0)
int main(int argc,char **argv){
    CHECK(argc==2);const char *path=argv[1];CHECK(freopen(path,"wb",stdout));
    struct json_object *v=json_object_new_object(),*a=json_object_new_array(),*b=json_object_new_array();
    for(int i=0;i<300;i++){json_object_array_add(a,json_object_new_double(i%3==0?-0.:i%3==1?1e-300:1./7));json_object_array_add(b,json_object_new_int64(i%2?INT64_C(-9007199254740993):INT64_C(9007199254740993)));}
    json_object_object_add(v,"float",a);json_object_object_add(v,"same",json_object_get(a));json_object_object_add(v,"integer",b);json_object_object_add(v,"boolean",json_object_new_boolean(1));CHECK(fp_write(v,FP_LIMIT));
    struct json_object *r=fp_read(path);CHECK(r && json_object_equal(v,r));
    for(int i=0;i<300;i++){double x=json_object_get_double(json_object_array_get_idx(a,i)),y=json_object_get_double(json_object_array_get_idx(fp_get(r,"float"),i));CHECK(!memcmp(&x,&y,8));CHECK(json_object_is_type(json_object_array_get_idx(fp_get(r,"integer"),i),json_type_int));}
    json_object_put(r);CHECK(fflush(stdout)==0);FILE *f=fopen(path,"rb");CHECK(f);CHECK(fseek(f,0,SEEK_END)==0);long n=ftell(f);CHECK(n>24);rewind(f);unsigned char *raw=malloc((size_t)n);CHECK(raw && fread(raw,1,(size_t)n,f)==(size_t)n);fclose(f);
    for(int scenario=0;scenario<4;scenario++){
        CHECK(freopen(path,"wb",stdout));
        if(scenario==0)raw[n-1]^=1;
        if(scenario==1)raw[n-1]^=1;
        if(scenario==2)raw[0]^=1;
        CHECK(fwrite(raw,1,scenario==1?23:(size_t)n,stdout)==(scenario==1?23:(size_t)n));
        if(scenario==3)CHECK(fputc('x',stdout)!=EOF);
        CHECK(fflush(stdout)==0);r=fp_read(path);CHECK(!r);
        if(scenario==2)raw[0]^=1;
    }
    uint64_t ml=fp_u64(raw+8),pl=fp_u64(raw+16);
    for(int scenario=0;scenario<10;scenario++){
        struct json_object *m=json_tokener_parse((const char *)"{}");
        json_object_put(m);struct json_tokener *tok=json_tokener_new();m=json_tokener_parse_ex(tok,(const char *)raw+24,(int)ml);json_tokener_free(tok);CHECK(m);
        struct json_object *records=fp_get(m,"arrays"),*record=json_object_array_get_idx(records,0),*value=fp_get(m,"value");
        if(scenario==0)json_object_object_add(record,"dtype",json_object_new_string("f32"));
        if(scenario==1)json_object_object_add(record,"count",json_object_new_int64(8388609));
        if(scenario==2)json_object_object_add(record,"offset",json_object_new_int64(1));
        if(scenario==3)json_object_object_add(fp_get(value,"float"),FP_TAG,json_object_new_int64(99));
        if(scenario==4)json_object_object_add(fp_get(value,"float"),FP_TAG,json_object_new_boolean(1));
        if(scenario==5)json_object_object_add(fp_get(value,"float"),FP_TAG,NULL);
        if(scenario==6)json_object_object_add(fp_get(value,"float"),"extra",json_object_new_int64(0));
        if(scenario==7){json_object_object_del(value,"float");json_object_object_del(value,"same");}
        if(scenario==8)json_object_array_put_idx(records,0,json_object_new_int64(0));
        if(scenario==9)json_object_object_add(m,"schema",json_object_new_string("invalid"));
        const char *meta=json_object_to_json_string_ext(m,JSON_C_TO_STRING_PLAIN);size_t len=strlen(meta);unsigned char h[24];memcpy(h,raw,24);fp_le(h+8,len);
        CHECK(freopen(path,"wb",stdout));CHECK(fwrite(h,1,24,stdout)==24 && fwrite(meta,1,len,stdout)==len && fwrite(raw+24+ml,1,(size_t)pl,stdout)==pl && fflush(stdout)==0);CHECK(!fp_read(path));json_object_put(m);
    }
    free(raw);json_object_put(v);fprintf(stderr,"field packet native roundtrip, float bits, int64 types and14 framing/CRC/descriptor refusals passed\n");return 0;
}
