/* Face-local log-concave fit to unequal finite-volume means. Extends the
 * existing uniform log-quadratic reconstruction; no reference field is used. */
#ifndef PHYSICS_SIM_TAIL_FIT_H
#define PHYSICS_SIM_TAIL_FIT_H
#include "physics_sim_scalar_logquadratic.h"
#include <stddef.h>
static inline void physics_sim_tail_moments(double b,double c,double lo,double hi,double m[3]){
 static const double x[8]={-.9602898564975363,-.7966664774136267,-.5255324099163290,-.1834346424956498,.1834346424956498,.5255324099163290,.7966664774136267,.9602898564975363};
 static const double w[8]={.1012285362903763,.2223810344533745,.3137066458778873,.3626837833783620,.3626837833783620,.3137066458778873,.2223810344533745,.1012285362903763};
 /* Resolve steep within-cell exponent variation with bounded composite
  * Gauss integration. A single panel is retained for the smooth regime.
  * Unsupported variation returns a failed fit, never a clipped temperature. */
 double variation=(hi-lo)*fmax(fabs(b+2*c*lo),fabs(b+2*c*hi));
 if(!isfinite(variation)||variation>4096){m[0]=m[1]=m[2]=NAN;return;}
 int panels=(int)ceil(variation/4);if(panels>1){
  double peak=fmax(b*lo+c*lo*lo,b*hi+c*hi*hi);
  if(c<0){double at=-b/(2*c);if(at>lo&&at<hi)peak=fmax(peak,b*at+c*at*at);}
  double sum=0,first=0,second=0,span=(hi-lo)/panels;
  for(int k=0;k<panels;k++)for(int j=0;j<8;j++){
   double at=lo+span*(k+.5+.5*x[j]);
   double value=.5*w[j]/panels*exp(b*at+c*at*at-peak);
   sum+=value;first+=value*at;second+=value*at*at;
  }
  m[0]=peak+log(sum);m[1]=first/sum;m[2]=second/sum;return;
 }
 double z[8],at[8],top=-INFINITY;for(int j=0;j<8;j++){at[j]=(lo+hi)*.5+(hi-lo)*.5*x[j];z[j]=b*at[j]+c*at[j]*at[j];top=fmax(top,z[j]);}
 double sum=0,first=0,second=0;for(int j=0;j<8;j++){double v=.5*w[j]*exp(z[j]-top);sum+=v;first+=v*at[j];second+=v*at[j]*at[j];}m[0]=top+log(sum);m[1]=first/sum;m[2]=second/sum;
}
static inline void physics_sim_tail_exp_moment(double z,double *value,double *derivative){
 if(fabs(z)<1e-4){*value=z/2+z*z/24-z*z*z*z/2880;*derivative=.5+z/12-z*z*z/720;}
 else if(z>40){*value=z-log(z);*derivative=1-1/z;}
 else if(z< -40){*value=-log(-z);*derivative=-1/z;}
 else {*value=log(expm1(z)/z);*derivative=exp(z)/expm1(z)-1/z;}
}
static inline bool physics_sim_tail_exp_fit(double l,double r,double wl,double wr,double *gradient){
 if(!(l>0&&r>0)||l==r)return false;double h=(wl+wr)*.5,target=log(r)-log(l),b=target;
 for(int iteration=0;iteration<16;iteration++){
  double ml,dl,mr,dr;physics_sim_tail_exp_moment(-b*wl/h,&ml,&dl);physics_sim_tail_exp_moment(b*wr/h,&mr,&dr);
  double error=mr-ml-target;if(fabs(error)<1e-10){double value=exp(log(l)-ml);*gradient=b/h*value;return isfinite(*gradient)&&value>0&&b*target>0;}
  b-=error/(wr/h*dr+wl/h*dl);if(!isfinite(b))return false;
 }return false;
}
static inline bool physics_sim_tail_profile_fit(const double value[4],const double width[4],double *face_value,double *gradient){
 for(int j=0;j<4;j++)if(!(value[j]>0))return false;
 if(!((value[0]<=value[1]&&value[1]<=value[2]&&value[2]<=value[3])||(value[0]>=value[1]&&value[1]>=value[2]&&value[2]>=value[3])))return false;
 double h=(width[1]+width[2])*.5,bounds[5]={-(width[0]+width[1])/h,-width[1]/h,0,width[2]/h,(width[2]+width[3])/h};
 double logs[4],x[4];for(int j=0;j<4;j++){logs[j]=log(value[j]);x[j]=(bounds[j]+bounds[j+1])*.5;}
 double target0=logs[2]-logs[1],target1=(logs[0]+logs[3]-logs[1]-logs[2])*.5;
 double c=fmin(0,((logs[3]-logs[2])/(x[3]-x[2])-(logs[1]-logs[0])/(x[1]-x[0]))/(x[3]+x[2]-x[1]-x[0]));
 double b=target0-c*(x[1]+x[2]),m[4][3];
 for(int iteration=0;iteration<20;iteration++){
  for(int j=0;j<4;j++)physics_sim_tail_moments(b,c,bounds[j],bounds[j+1],m[j]);
  double f=m[2][0]-m[1][0]-target0,g=(m[0][0]+m[3][0]-m[1][0]-m[2][0])*.5-target1;
  if(!isfinite(f)||!isfinite(g))return false;
  if(fmax(fabs(f),fabs(g))<1e-10){double face=exp((logs[1]+logs[2]-m[1][0]-m[2][0])*.5),derivative=b*face/h;if(!isfinite(derivative)||face<=0||b*target0<0)return false;if(face_value)*face_value=face;*gradient=derivative;return true;}
  double a=m[2][1]-m[1][1],bb=m[2][2]-m[1][2],cc=(m[0][1]+m[3][1]-m[1][1]-m[2][1])*.5,dd=(m[0][2]+m[3][2]-m[1][2]-m[2][2])*.5,det=a*dd-bb*cc;
  if(!isfinite(det)||fabs(det)<DBL_EPSILON)return false;
  double db=(f*dd-g*bb)/det,dc=(a*g-cc*f)/det,scale=1;
  while(c-scale*dc>0&&scale>1./1024)scale*=.5;
  b-=scale*db;c-=scale*dc;if(!isfinite(b)||!isfinite(c)||c>0)return false;
 }
 return false;
}
/* Existing gradient-only API keeps its original arithmetic and fallback. */
static inline bool physics_sim_tail_fit(const double value[4],const double width[4],double *gradient){
 return physics_sim_tail_profile_fit(value,width,NULL,gradient);
}
#endif
