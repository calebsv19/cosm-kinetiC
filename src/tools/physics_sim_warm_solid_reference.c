/* Independent real CN normal FV + two-cell solid Schur solve. No production
 * wall kernel, normal matrix, receiver or packet integration calls. */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
static void solve(int n,double off,const double *inv,double *b) {
    for(int i=1;i<n;i++)b[i]-=off*inv[i-1]*b[i-1];
    b[n-1]*=inv[n-1];for(int i=n-2;i>=0;i--)b[i]=(b[i]-off*b[i+1])*inv[i];
}
int main(int argc,char **argv) {
    if(argc!=4)return 2;
    int mode=atoi(argv[1]),n=atoi(argv[2]),tps=atoi(argv[3]);
    if(mode<0||mode>2||(n!=1024&&n!=2048)||(tps!=51200&&tps!=102400))return 2;
    double *mem=calloc((size_t)n*5,sizeof(double));assert(mem);
    double *t=mem,*b=t+n,*inv=b+n,*diag=inv+n,*v=diag+n;
    const double dz=.0625/n,half=.5/tps,diff=(.026/1206)/(dz*dz),off=-half*diff;
    const double g=mode==1?0:.0625/(dz/(2*.026)+.1+.005/(2*.16)),beta=g/(1206*.0625*dz);
    double s0=mode==2?20:0,s1=s0,initial=mode==2?2000:94.21875;
    for(int i=0;i<n;i++) {
        t[i]=mode==2?0:20;diag[i]=(i==0||i==n-1?-diff:-2*diff)-(i==0?beta:0);
        inv[i]=1/(1-half*diag[i]-(i?off*inv[i-1]*off:0));
    }
    v[0]=half*beta;solve(n,off,inv,v);
    for(int step=0;step<=5*tps;step++) {
        if(!(step%(tps/100))) {
            double mean=0;for(int i=0;i<n;i++)mean+=t[i]/n;
            double q=50*(s0+s1)-(mode==2?2000:0),source=4.7109375*fmin(1,step/(double)tps);
            printf("{\"time_s\":%.17g,\"q_j\":%.17g,\"gas_mean_k\":%.17g,\"solid0_k\":%.17g,\"solid1_k\":%.17g,\"source_j\":%.17g,\"balance_j\":%.17g,\"profile_k\":[",step/(double)tps,q,mean,s0,s1,source,4.7109375*mean+50*(s0+s1)-initial-source);
            for(int k=0;k<256;k++){double value=0;for(int i=k*n/256;i<(k+1)*n/256;i++)value+=t[i]/(n/256);printf("%s%.17g",k?",":"",value);}puts("]}");
        }
        if(step==5*tps)break;
        double r0=s0+half*(g*(t[0]-s0)-2*(s0-s1))/50,r1=s1+half*2*(s0-s1)/50;
        for(int i=0;i<n;i++)b[i]=(1+half*diag[i])*t[i]+(i?-off*t[i-1]:0)+(i<n-1?-off*t[i+1]:0)+(step<tps?1./tps:0);
        b[0]+=half*beta*s0;solve(n,off,inv,b);
        double m00=1+half*(g+2)/50-half*g/50*v[0],m01=-half*2/50,m11=1+half*2/50,det=m00*m11-m01*m01;
        r0+=half*g/50*b[0];double next0=(r0*m11-m01*r1)/det,next1=(m00*r1-m01*r0)/det;
        for(int i=0;i<n;i++)t[i]=b[i]+v[i]*next0;
        s0=next0;s1=next1;
    }
    free(mem);return 0;
}
