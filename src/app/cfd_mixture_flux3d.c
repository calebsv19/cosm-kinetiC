#include "app/cfd_mixture3d.h"
#include <math.h>
#include <string.h>

bool cfd_mixture_flux(const CfdMixtureThermo *s,const double l[10],const double r[10],
    int axis,double diffusion,double conductivity,double spacing,double output[10],double diffusive[10],double *speed) {
    if(!s || !l || !r || !output || !diffusive || !speed || axis<0 || axis>2 ||
       !isfinite(diffusion) || diffusion<0 || !isfinite(conductivity) || conductivity<0 ||
       !isfinite(spacing) || spacing<=0) return false;
    double tl,tr,pl,pr,cl,cr,cv;
    if(!cfd_mixture_primitive(s,l,&tl,&pl,&cl,&cv) ||
       !cfd_mixture_primitive(s,r,&tr,&pr,&cr,&cv)) return false;
    double vl=l[axis+1]/l[0],vr=r[axis+1]/r[0];
    double sl=fmin(vl-cl,vr-cr),sr=fmax(vl+cl,vr+cr),f[10],d[10]={0};
    double physical[2][10];
    for(int side=0;side<2;++side) {
        const double *u=side?r:l;double v=side?vr:vl,p=side?pr:pl;
        for(int i=0;i<10;++i) physical[side][i]=u[i]*v;
        physical[side][axis+1]+=p;physical[side][4]=(u[4]+p)*v;
    }
    if(sl>=0 || sr<=0) memcpy(f,physical[sl<0],sizeof(f));
    else {
        double al=l[0]*(sl-vl),ar=r[0]*(sr-vr);
        double middle=(pr-pl+al*vl-ar*vr)/(al-ar);
        if(!isfinite(middle) || middle<=sl || middle>=sr) return false;
        double stars[2][10],star_pressure[2];
        for(int side=0;side<2;++side) {
            const double *u=side?r:l;double wave=side?sr:sl,v=side?vr:vl,p=side?pr:pl;
            double *star=stars[side],rho=u[0]*(wave-v)/(wave-middle);
            double ps=p+u[0]*(wave-v)*(middle-v);
            if(!isfinite(ps) || ps<=0) return false;
            star_pressure[side]=ps;
            star[0]=rho;
            for(int a=0;a<3;++a) star[a+1]=rho*(a==axis?middle:u[a+1]/u[0]);
            star[4]=rho*(u[4]/u[0]+(middle-v)*(middle+p/(u[0]*(wave-v))));
            for(int i=5;i<10;++i) star[i]=rho*u[i]/u[0];
            double t,ignored;
            if(!cfd_mixture_primitive(s,star,&t,&ignored,&ignored,&ignored)) return false;
        }
        int side=middle<0;const double *u=side?r:l,*star=stars[side];
        /* Equivalent HLLC star flux avoids subtraction of nearly equal
         * densities at stationary contacts. Its mass sign matches upwinding. */
        f[0]=star[0]*middle;
        for(int i=1;i<4;++i) f[i]=star[i]*middle+(i==axis+1?star_pressure[side]:0);
        f[4]=(star[4]+star_pressure[side])*middle;
        /* All species share the mass/contact flux; no independent scalar speed. */
        for(int i=5;i<10;++i) f[i]=f[0]*u[i]/u[0];
    }
    double y[5],sum=0,rho=.5*(l[0]+r[0]);int largest=0;
    for(int i=0;i<5;++i) {
        y[i]=.5*(l[5+i]/l[0]+r[5+i]/r[0]);
        if(y[i]>y[largest]) largest=i;
        d[5+i]=-rho*diffusion*(r[5+i]/r[0]-l[5+i]/l[0])/spacing;sum+=d[5+i];
    }
    /* Barycentric correction and one dependent flux enforce zero total species
     * diffusion. The largest present species absorbs only rounded closure. */
    double others=0;
    for(int i=0;i<5;++i) {d[5+i]-=y[i]*sum;if(i!=largest) others+=d[5+i];}
    d[5+largest]=-others;
    double h[5];if(!cfd_mixture_enthalpies(s,.5*(tl+tr),h)) return false;
    d[4]=-conductivity*(tr-tl)/spacing;
    for(int i=0;i<5;++i) d[4]+=h[i]*d[5+i];
    double combined[10];
    for(int i=0;i<10;++i) {combined[i]=f[i]+d[i];if(!isfinite(combined[i])) return false;}
    memcpy(output,combined,sizeof(combined));memcpy(diffusive,d,sizeof(d));
    *speed=fmax(fabs(sl),fabs(sr));return isfinite(*speed);
}
