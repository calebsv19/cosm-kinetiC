/* Independent Fourier-in-x / finite-volume-in-z reference. Exact periodic
 * advection multiplier, centered physical diffusion, CN time, direct solves.
 * The two-cell solid is spatially uniform along the wall: only mode zero
 * couples to its temperature. Nonzero modes retain the same Robin sink.
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
 if((n!=128&&n!=256&&n!=512)||(modes!=32&&modes!=64&&modes!=128)||(steps!=5120&&steps!=10240&&steps!=20480)||(h!=0&&h!=10))return 2;
 size_t count=(size_t)n*modes;
 double complex *storage=calloc(5*count+n,sizeof(double complex));if(!storage)return 3;
 double complex *t=storage,*b=t+count,*inv=b+count,*diag=inv+count,*src=diag+count,*v=src+count;
 double dz=.0625/n,dt=3.2/steps,half=dt/2,alpha=.026/1206,diff=alpha/(dz*dz),off=-half*diff;
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
 v[0]=half*beta;solve(n,off,inv,v);
 double m00=1+half*(g+2)/50-half*g/50*creal(v[0]),m01=-half*2/50,m11=1+half*2/50,det=m00*m11-m01*m01;
 double s0=0,s1=0;
 for(int step=0;step<=steps;step++){
  if(!(step%(steps/128)))printf("{\"time_s\":%.17g,\"transport_k\":%.17g,\"near_k\":%.17g,\"tail_k\":%.17g,\"solid0_k\":%.17g,\"solid1_k\":%.17g,\"q_j\":%.17g}\n",step*dt,sample(t,n,modes,1./16,3./32,n/4,n/2),sample(t,n,modes,1./16,3./32,0,n/8),sample(t,n,modes,3./32,1./8,n/4,n/2),s0,s1,50*(s0+s1));
  if(step==steps)break;
  double r0=s0+half*(g*(creal(t[0])-s0)-2*(s0-s1))/50,r1=s1+half*2*(s0-s1)/50;
  for(int m=0;m<modes;m++){
   for(int i=0;i<n;i++){
    int q=m*n+i;
    b[q]=(1+half*diag[q])*t[q]+(i?-off*t[q-1]:0)+(i<n-1?-off*t[q+1]:0)+(step<steps/8?dt*src[q]:0);
   }
   if(!m)b[0]+=half*beta*s0;
   solve(n,off,inv+m*n,b+m*n);
  }
  r0+=half*g/50*creal(b[0]);double next0=(r0*m11-m01*r1)/det,next1=(m00*r1-m01*r0)/det;
  for(size_t q=0;q<count;q++)t[q]=b[q]+(q<(size_t)n?v[q]*next0:0);
  s0=next0;s1=next1;
 }
 free(storage);return 0;
}
