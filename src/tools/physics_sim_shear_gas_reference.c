/* Independent positive free-space linear-shear Green kernel. The reflected
 * wall correction for the heated-band tail is bounded separately; no native
 * scalar, source-patch, limiter, receiver or contour/Fourier calls. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
static void gauss(int n,double *x,double *w){
 for(int j=0;j<n;j++){
  double z=cos(acos(-1)*(j+.75)/(n+.5)),d=0;
  for(int it=0;it<20;it++){double a=1,b=z;for(int m=2;m<=n;m++){double c=((2.*m-1)*z*b-(m-1)*a)/m;a=b;b=c;}d=n*(z*b-a)/(z*z-1);double change=b/d;z-=change;if(fabs(change)<4e-16)break;}
  x[j]=(z+1)/2;w[j]=1/((1-z*z)*d*d);
 }
}
static double primitive(double x){return x*erfc(x)-exp(-x*x)/sqrt(acos(-1));}
static double horizontal(double age,double z0,double z,double lo,double hi){
 double alpha=.026/1206,gamma=.8,shift=gamma*age*(z+z0)/2;
 double scale=sqrt(4*alpha*age*(1+gamma*gamma*age*age/12)),sum=0;
 for(int image=-1;image<=1;image++){
  double a=1./32+shift+image*.25,b=1./16+shift+image*.25;
  double overlap=fmax(0,fmin(hi,b)-fmax(lo,a))/(hi-lo);
  sum+=overlap+.5*scale/(hi-lo)*(-primitive(fabs(hi-a)/scale)+primitive(fabs(lo-a)/scale)+primitive(fabs(hi-b)/scale)-primitive(fabs(lo-b)/scale));
 }return sum;
}
int main(int argc,char **argv){
 if(argc!=3&&argc!=4)return 2;int segment=argc==4?atoi(argv[3]):-1;if(segment< -1||segment>7)return 2;int n=atoi(argv[1]);double end=strtod(argv[2],NULL);if((n!=32&&n!=64&&n!=96&&n!=128)||!isfinite(end)||end<=0||end>.025)return 2;
 double x[128],w[128],alpha=.026/1206,tail=0,main=0;gauss(n,x,w);
 for(int q=0;q<n;q++){
  double age=end*x[q]*x[q],wt=2*end*x[q]*w[q],scale=sqrt(4*alpha*age);
  for(int j=0;j<n;j++)for(int k=0;k<n;k++){
   double z0=1./64+x[j]/64,z=segment<0?1./64+x[k]/64:1./64+(segment+x[k])/(64*8);
   double gz=exp(-pow((z-z0)/scale,2))/(scale*sqrt(acos(-1)));
   double weight=wt*w[j]*w[k]/64*gz;
   tail+=weight*horizontal(age,z0,z,3./32,1./8);
   main+=weight*horizontal(age,z0,z,1./16,3./32);
  }
 }
 printf("{\"order\":%d,\"time_s\":%.17g,\"tail_k\":%.17g,\"transport_k\":%.17g}\n",n,end,tail,main);
}
