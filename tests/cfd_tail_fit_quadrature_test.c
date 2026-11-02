/* Independent positive Simpson moments qualify steep cell integration. */
#include "../src/tools/physics_sim_tail_fit.h"
#include <assert.h>
#include <stdio.h>
static void reference(double b,double c,double lo,double hi,long double out[3]) {
    const int count=262144;
    long double peak=fmaxl(b*lo+c*lo*lo,b*hi+c*hi*hi);
    if(c<0){long double at=-b/(2.L*c);if(at>lo&&at<hi)peak=fmaxl(peak,b*at+c*at*at);}
    long double sum=0,first=0,second=0;
    for(int k=0;k<=count;k++) {
        long double at=lo+(hi-lo)*(long double)k/count;
        long double q=(k==0||k==count?1:k%2?4:2)*expl(b*at+c*at*at-peak);
        sum+=q;first+=q*at;second+=q*at*at;
    }
    out[0]=peak+logl(sum/(3*count));out[1]=first/sum;out[2]=second/sum;
}
int main(void) {
    const double slopes[]={-640,-160,-40,-3,0,3,40,160,640};
    const double curves[]={0,-.5,-5,-100};
    const double intervals[][2]={{-2,-1},{0,1},{-1.5,0},{0,.5}};
    double worst=0;int checks=0;
    for(unsigned b=0;b<sizeof(slopes)/sizeof(*slopes);b++)
    for(unsigned c=0;c<sizeof(curves)/sizeof(*curves);c++)
    for(unsigned w=0;w<sizeof(intervals)/sizeof(*intervals);w++) {
        double m[3];long double exact[3];
        physics_sim_tail_moments(slopes[b],curves[c],intervals[w][0],intervals[w][1],m);
        reference(slopes[b],curves[c],intervals[w][0],intervals[w][1],exact);
        for(int k=0;k<3;k++){double error=fabs(m[k]-(double)exact[k]);worst=fmax(worst,error);if(error>=2e-10)fprintf(stderr,"b=%g c=%g lo=%g hi=%g k=%d got=%.17g exact=%.17Lg err=%g\n",slopes[b],curves[c],intervals[w][0],intervals[w][1],k,m[k],exact[k],error);assert(error<2e-10);checks++;}
    }
    const double width[4]={1,1,1,1};
    for(int slope=20;slope<=160;slope*=2) {
        double value[4],g=0;
        for(int k=0;k<4;k++){double m,d;physics_sim_tail_exp_moment(slope,&m,&d);value[k]=exp(slope*(k-4)+m);}
        assert(physics_sim_tail_fit(value,width,&g));
        assert(fabs(g/(slope*exp(-2.*slope))-1)<1e-8);checks++;
    }
    double refused[3];physics_sim_tail_moments(5000,0,0,1,refused);assert(!isfinite(refused[0]));
    puts("steep composite moments, independent positive quadrature and exponential face fits pass");
    printf("%d checks; worst moment absolute error %.17g\n",checks,worst);
    return 0;
}
