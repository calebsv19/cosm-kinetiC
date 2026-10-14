#include "app/cfd_mixture3d.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
static bool values(double *u,int n) {
    for(int i=0;i<n;++i) if(scanf("%lf",u+i)!=1 || !isfinite(u[i])) return false;
    return true;
}
static void print_array(const double *u,int n) {
    putchar('[');for(int i=0;i<n;++i) printf("%s%.17g",i?",":"",u[i]);putchar(']');
}
static bool real_arg(const char *text,double *out) {
    char *end;errno=0;double value=strtod(text,&end);
    if(errno || end==text || *end || !isfinite(value)) return false;
    *out=value;return true;
}
static bool integer_arg(const char *text,int *out) {
    char *end;errno=0;long value=strtol(text,&end,10);
    if(errno || end==text || *end || value<INT_MIN || value>INT_MAX) return false;
    *out=(int)value;return true;
}
int main(int argc,char **argv) {
    CfdMixtureThermo thermo;if(!cfd_mixture_thermo_init(&thermo)) return 2;
    if(argc==2 && !strcmp(argv[1],"properties")) {
        double y[5],t,e,cp,r,h[5];
        if(!values(y,5) || !values(&t,1) || !cfd_mixture_properties(&thermo,y,t,&e,&cp,&r) ||
           !cfd_mixture_enthalpies(&thermo,t,h)) return 3;
        printf("{\"energy\":%.17g,\"cp\":%.17g,\"R\":%.17g,\"enthalpies\":",e,cp,r);
        print_array(h,5);puts("}");return 0;
    }
    if(argc==6 && !strcmp(argv[1],"flux")) {
        double l[10],r[10],f[10],d[10],speed,diffusion,k,h;int axis;
        if(!integer_arg(argv[2],&axis) || !real_arg(argv[3],&diffusion) ||
           !real_arg(argv[4],&k) || !real_arg(argv[5],&h) || !values(l,10) || !values(r,10) ||
           !cfd_mixture_flux(&thermo,l,r,axis,diffusion,k,h,f,d,&speed)) return 4;
        printf("{\"flux\":");print_array(f,10);printf(",\"diffusive\":");print_array(d,10);
        printf(",\"speed\":%.17g}\n",speed);return 0;
    }
    if(argc!=11) return 5;
    int n[3],steps;double length[3],dt,diffusion,k;
    for(int a=0;a<3;++a) if(!integer_arg(argv[a+1],n+a) || n[a]<4 || n[a]>256 ||
        !real_arg(argv[a+4],length+a) || length[a]<.001 || length[a]>1000) return 6;
    if(!real_arg(argv[7],&dt) || !integer_arg(argv[8],&steps) ||
       !real_arg(argv[9],&diffusion) || !real_arg(argv[10],&k) || dt<=0 || steps<1 || steps>100000) return 6;
    CfdMixture3d s;if(!cfd_mixture3d_init(&s,n,length,diffusion,k)) return 7;
    int count=10*s.grid.count;double *source=cfd_memory_calloc((size_t)count,sizeof(double));
    bool ok=source && values(s.state,count) && values(source,count);
    for(int step=0;ok && step<steps;++step) ok=cfd_mixture3d_step(&s,source,dt);
    if(ok) {printf("{\"conserved\":");print_array(s.state,count);puts("}");}
    cfd_memory_free(source);cfd_mixture3d_destroy(&s);return ok?0:8;
}
