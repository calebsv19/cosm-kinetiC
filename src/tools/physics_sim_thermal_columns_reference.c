/* Independent Fourier-in-x / finite-volume-in-z reference. Exact periodic
 * advection multiplier, centered physical diffusion, CN time, direct solves.
 * Independent solid columns have no lateral conduction. Every Fourier mode
 * couples to both solid-depth temperatures through the local Robin law.
 * No native solver calls. Refine modes, normal cells, and time independently. */
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
static void solve(int n,double off,const double complex *inv,double complex *b){
 for(int i=1;i<n;i++)b[i]-=off*inv[i-1]*b[i-1];
 b[n-1]*=inv[n-1];for(int i=n-2;i>=0;i--)b[i]=(b[i]-off*b[i+1])*inv[i];
}
static double complex average(int m,double lo,double hi){
 if(!m)return 1;double k=2*acos(-1)*m/.25;
 return (cexp(I*k*hi)-cexp(I*k*lo))/(I*k*(hi-lo));
}
static double sample(const double complex *t,int n,int modes,double xl,double xh,int lo,int hi){
 double sum=0;for(int m=0;m<modes;m++){
  double complex v=0;for(int i=lo;i<hi;i++)v+=t[m*n+i];
  sum+=(m?2:1)*creal(v*average(m,xl,xh))/(hi-lo);
 }return sum;
}
int main(int argc,char **argv){
 if(argc!=5)return 2;int n=atoi(argv[1]),modes=atoi(argv[2]),steps=atoi(argv[3]);double h=atof(argv[4]);
 #ifndef PHYSICS_SIM_COLUMNS_SHORT_REFERENCE
 if((n!=128&&n!=256&&n!=512)||(modes!=32&&modes!=64&&modes!=128)||(steps!=5120&&steps!=10240&&steps!=20480)||(h!=0&&h!=10))return 2;
#else
 if((n!=512&&n!=1024&&n!=2048)||(modes!=64&&modes!=128)||(steps!=1600&&steps!=3200)||(h!=0&&h!=10))return 2;
#endif
 size_t count=(size_t)n*modes;
 double complex *storage=calloc(6*count+2*modes,sizeof(double complex));if(!storage)return 3;
 double complex *t=storage,*b=t+count,*inv=b+count,*diag=inv+count,*src=diag+count,*v=src+count,*s0=v+count,*s1=s0+modes;
#ifdef PHYSICS_SIM_COLUMNS_SHORT_REFERENCE
 const double end=.5;const int observations=20,powered_steps=4*steps/5;
#else
 const double end=3.2;const int observations=128,powered_steps=steps/8;
#endif
 double dz=.0625/n,dt=end/steps,half=dt/2,alpha=.026/1206,diff=alpha/(dz*dz),off=-half*diff;
 double g=h==0?0:.0625/(dz/(2*.026)+1/h+.005/(2*.16)),beta=g/(1206*.0625*dz);
 for(int m=0;m<modes;m++){
  double k=2*acos(-1)*m/.25;
  double complex amplitude=.125*conj(average(m,1./32,1./16));
  for(int i=0;i<n;i++){
   int q=m*n+i;
   diag[q]=(i==0||i==n-1?-diff:-2*diff)-(i==0?beta:0)-alpha*k*k-I*k*.05*(i+.5)/n;
   src[q]=(i>=n/4&&i<n/2)?amplitude:0;
   double complex d=1-half*diag[q];if(i)d-=off*inv[q-1]*off;inv[q]=1/d;
  }
 }
 for(int m=0;m<modes;m++){v[m*n]=half*beta;solve(n,off,inv+m*n,v+m*n);}
 for(int step=0;step<=steps;step++){
  if(!(step%(steps/observations))){
   printf("{\"time_s\":%.17g,\"transport_k\":%.17g,\"near_k\":%.17g,\"tail_k\":%.17g,\"q_j\":%.17g",step*dt,sample(t,n,modes,1./16,3./32,n/4,n/2),sample(t,n,modes,1./16,3./32,0,n/8),sample(t,n,modes,3./32,1./8,n/4,n/2),50*creal(s0[0]+s1[0]));
   for(int d=0;d<3;d++){
    printf(",\"%s\":[",d==0?"solid0_k":d==1?"solid1_k":"local_q_j");
    for(int bin=0;bin<8;bin++){
     double value=0;for(int m=0;m<modes;m++)value+=(m?2:1)*creal((d==0?s0[m]:d==1?s1[m]:50./8*(s0[m]+s1[m]))*average(m,bin/32.,(bin+1)/32.));
     printf("%s%.17g",bin?",":"",value);
    }printf("]");
   }printf("}\n");
  }
#ifdef PHYSICS_SIM_THERMAL_REFERENCE_SNAPSHOT
  PHYSICS_SIM_THERMAL_REFERENCE_SNAPSHOT(t,s0,n,modes,step,steps,h);
#endif
  if(step==steps)break;
  for(int m=0;m<modes;m++){
   int base=m*n;
   double complex r0=s0[m]+half*(g*(t[base]-s0[m])-2*(s0[m]-s1[m]))/50,r1=s1[m]+half*2*(s0[m]-s1[m])/50;
   for(int i=0;i<n;i++){
    int q=base+i;
    b[q]=(1+half*diag[q])*t[q]+(i?-off*t[q-1]:0)+(i<n-1?-off*t[q+1]:0)+(step<powered_steps?dt*src[q]:0);
   }
   b[base]+=half*beta*s0[m];solve(n,off,inv+base,b+base);
   double complex m00=1+half*(g+2)/50-half*g/50*v[base],m01=-half*2/50,m11=1+half*2/50,det=m00*m11-m01*m01;
   r0+=half*g/50*b[base];double complex next0=(r0*m11-m01*r1)/det,next1=(m00*r1-m01*r0)/det;
   for(int i=0;i<n;i++)t[base+i]=b[base+i]+v[base+i]*next0;
   s0[m]=next0;s1[m]=next1;
  }
 }
 free(storage);return 0;
}
