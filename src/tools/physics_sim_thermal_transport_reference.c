/* Independent linear method-of-lines reference. Central conservative gas flux,
 * Crank-Nicolson time integration, direct tridiagonal + 2x2 Schur solve.
 * No PhysicsSim/Thermal headers or solver calls. Grid/time refinement required. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
static double mean(const double *v,int first,int end){double s=0;for(int i=first;i<end;i++)s+=v[i];return s/(end-first);}
static void solve(int n,const double *lower,const double *upper,const double *inverse,double *b){
 for(int i=1;i<n;i++)b[i]-=lower[i]*inverse[i-1]*b[i-1];
 b[n-1]*=inverse[n-1];for(int i=n-2;i>=0;i--)b[i]=(b[i]-upper[i]*b[i+1])*inverse[i];
}
int main(int argc,char **argv){
 if(argc!=4)return 2;int n=atoi(argv[1]),steps=atoi(argv[2]);double h=atof(argv[3]);
 if((n!=2048&&n!=4096&&n!=8192&&n!=16384&&n!=32768)||(steps!=10240&&steps!=20480&&steps!=40960)||(h!=0&&h!=10))return 2;
 double *storage=calloc((size_t)9*n,sizeof(double));if(!storage)return 3;
 double *t=storage,*b=t+n,*v=b+n,*low=v+n,*up=low+n,*inv=up+n,*diag=inv+n,*src=diag+n,*sink=src+n;
 double dz=.5/n,dt=3.2/steps,half=dt/2,diff=(.026/1206)/(dz*dz),adv=.05/(2*dz),g=h*.0625,gi=2,c=50;
 int first=11*n/16,end=12*n/16;double beta=h/(1206*.03125);
 for(int i=0;i<n;i++){
  sink[i]=(i>=first&&i<end)?beta:0;src[i]=(i>=n/4&&i<n/2)?1:0;
  diag[i]=(i==0||i==n-1?-diff-adv:-2*diff)-sink[i];
  low[i]=i?-half*(diff+adv):0;up[i]=i<n-1?-half*(diff-adv):0;
  double d=1-half*diag[i];if(i)d-=low[i]*inv[i-1]*up[i-1];inv[i]=1/d;
  v[i]=half*sink[i];
 }
 solve(n,low,up,inv,v);double mv=mean(v,first,end);
 double m00=1+half*(g+gi)/c-half*g/c*mv,m01=-half*gi/c,m10=m01,m11=1+half*gi/c,det=m00*m11-m01*m10;
 double s0=0,s1=0;
 for(int step=0;step<=steps;step++){
  if(!(step%(steps/128)))printf("{\"time_s\":%.17g,\"near_k\":%.17g,\"tail_k\":%.17g,\"solid0_k\":%.17g,\"solid1_k\":%.17g,\"q_j\":%.17g}\n",step*dt,mean(t,first,end),mean(t,14*n/16,15*n/16),s0,s1,c*(s0+s1));
  if(step==steps)break;
  double r0=s0+half*(g*(mean(t,first,end)-s0)-gi*(s0-s1))/c,r1=s1+half*gi*(s0-s1)/c;
  for(int i=0;i<n;i++)b[i]=(1+half*diag[i])*t[i]+(i?-low[i]*t[i-1]:0)+(i<n-1?-up[i]*t[i+1]:0)+half*sink[i]*s0+dt*src[i];
  solve(n,low,up,inv,b);r0+=half*g/c*mean(b,first,end);
  double next0=(r0*m11-m01*r1)/det,next1=(m00*r1-m10*r0)/det;
  for(int i=0;i<n;i++)t[i]=b[i]+v[i]*next0;
  s0=next0;s1=next1;
 }
 free(storage);return 0;
}
