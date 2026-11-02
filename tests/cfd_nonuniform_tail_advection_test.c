/* Independent analytic face fixtures plus bounded paired transport. */
#include "../src/tools/physics_sim_nonuniform_advection.h"
#include <assert.h>
static PhysicsSimNonuniformAdvection s;
static double width[2112],v[2112];
static const struct {double means[4],face;int i;} fixtures[]={
    {{0.017882213955252708,0.01689532202776076,0.015818116785930189,0.014759842554068195},0.0163629895510468,1024},
    {{1.3212199587130344e-194,2.3283070883395132e-195,1.5486260430272516e-197,1.6478898025959478e-222},8.6321348798240291e-196,1536},
    {{0.080667004972758319,0.078930082932236981,0.077075126225364943,0.075211830436410207},0.078013461234081982,1024},
    {{1.0991474034974765e-46,7.2796873282949789e-47,4.4287638068069953e-48,4.9053503003051935e-54},5.8808072768723284e-47,1536},
};
int main(void) {
    int n=0;for(int x=0;x<128;x++){int count=x<48||x>=112?32:1;for(int k=0;k<count;k++)width[n++]=.25/128/count;}
    assert(n==2112);assert(physics_sim_nonuniform_advection_init(&s,n,width,1536));
    double worst_face=0,worst_mass=0;int checks=0;
    for(unsigned k=0;k<sizeof(fixtures)/sizeof(*fixtures);k++){
        int i=fixtures[k].i;for(int j=0;j<n;j++)v[j]=0;
        for(int j=0;j<4;j++)v[i-2+j]=fixtures[k].means[j];
        double face=physics_sim_nonuniform_tail_face(&s,v,i,.025),error=fabs(face/fixtures[k].face-1);
        assert(face>=0&&error<.005);worst_face=fmax(worst_face,error);checks++;
        if(k==1)assert(physics_sim_nonuniform_face(&s,v,i)<0);
    }
    for(int profile=0;profile<3;profile++)for(int direction=-1;direction<=1;direction+=2){
        double x=0,mass=0;
        for(int j=0;j<n;j++){double at=x+width[j]/2;v[j]=profile==0?.1*exp(-pow((at-.05)/.002,2)):profile==1?1e-300*exp(-pow((at-.05)/.002,2)):((direction>0?j==1023:j==1024)?.1:0);mass+=width[j]*v[j];x+=width[j];}
        for(int step=0;step<32;step++){
            double hi=0;for(int j=0;j<n;j++)hi=fmax(hi,v[j]);
            assert(physics_sim_nonuniform_advection_step(&s,v,direction*.025,1./51200,3));
            double after=0;for(int j=0;j<n;j++){assert(isfinite(v[j])&&v[j]>=0&&v[j]<=hi+64*DBL_EPSILON*hi);after+=width[j]*v[j];}
            double error=fabs(after/mass-1);assert(error<1e-12);worst_mass=fmax(worst_mass,error);checks++;
        }
        if(profile==2)assert(v[direction>0?1024:1023]>0);
    }
    printf("%d positive-tail checks; max independent face error %.17g; max mass error %.17g; bounds and cold/subnormal transport pass\n",checks,worst_face,worst_mass);
}
