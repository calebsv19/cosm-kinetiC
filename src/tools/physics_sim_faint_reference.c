/* Independent tilted Fourier quadrature / normal finite volumes / CN.
 * Evaluate free-x source images separately: a complex contour moves the
 * inverse transform away from catastrophic real-axis tail cancellation.
 * No native transport, exchange, receiver or reconstruction calls. */
#include <complex.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static double complex average(double complex k, double lo, double hi) {
    if (cabs(k) == 0) return 1;
    return (cexp(I*k*hi)-cexp(I*k*lo))/(I*k*(hi-lo));
}
static void solve(int n, double off, const double complex *inv, double complex *b) {
    for (int i=1;i<n;i++) b[i]-=off*inv[i-1]*b[i-1];
    b[n-1]*=inv[n-1];
    for (int i=n-2;i>=0;i--) b[i]=(b[i]-off*b[i+1])*inv[i];
}
int main(int argc, char **argv) {
    if (argc!=10) return 2;
    int n=atoi(argv[1]), modes=atoi(argv[2]), steps=atoi(argv[3]);
    double end=atof(argv[4]), tilt=atof(argv[5]), period=atof(argv[6]);
    int patch=atoi(argv[7]), image=atoi(argv[8]), stride=atoi(argv[9]);
    if (n<64||n>16384||n%64||modes<1||modes>2048||steps<1||
        end<=0||end>3.2||period<.25||patch< -3||patch>7||
        image< -1||image>1||stride<1||steps%stride) return 2;
    if (patch==-3&&(modes!=1||tilt!=0||period!=.25||image!=0)) return 2;
    size_t count=(size_t)n*modes;
    double complex *storage=calloc(6*count+3*modes,sizeof(*storage));
    if (!storage) return 3;
    double complex *t=storage,*b=t+count,*inv=b+count,*diag=inv+count;
    double complex *src=diag+count,*v=src+count,*s0=v+count,*s1=s0+modes,*weight=s1+modes;
    const double dz=.0625/n, dt=end/steps, half=dt/2;
    const double alpha=.026/1206, diff=alpha/(dz*dz), off=-half*diff;
    const double g=.0625/(dz/(2*.026)+.1+.005/(2*.16)), beta=g/(1206*.0625*dz);
    double xl=patch>=0?patch/32.:patch==-1?3./32:1./16;
    double xh=xl+1./32;
    /* Balance source and sample exponentials. The transform is unchanged
     * by this origin, but rare tails must not overflow before multiplication. */
    const double origin=tilt>0?(1./16+xl+image*.25)/2:
        tilt<0?(1./32+xh+image*.25)/2:3./64;
    for (int m=0;m<modes;m++) {
        double complex k=2*acos(-1)*m/period+I*tilt;
        double complex amplitude=(1./32/period)*average(-k,1./32-origin,1./16-origin);
        weight[m]=average(k,xl+image*.25-origin,xh+image*.25-origin);
        for (int i=0;i<n;i++) {
            int q=m*n+i;
            diag[q]=(i==0||i==n-1?-diff:-2*diff)-(i==0?beta:0)-alpha*k*k-I*k*.05*(i+.5)/n;
            src[q]=(i>=n/4&&i<n/2)?amplitude:0;
            double complex d=1-half*diag[q];
            if (i) d-=off*inv[q-1]*off;
            inv[q]=1/d;
        }
        v[m*n]=half*beta;solve(n,off,inv+m*n,v+m*n);
    }
    for (int step=0;step<=steps;step++) {
        if (!(step%stride)) {
            double gas=0, solid0=0, solid1=0, absolute_gas=0, absolute_solid=0;
            double cg=0,c0=0,c1=0; /* compensated accumulation, no clipping */
            for (int m=0;m<modes;m++) {
                double complex z=0;
                int lo=patch==-2?0:n/4, hi=patch==-2?n/8:n/2;
                for (int i=lo;i<hi;i++) z+=t[m*n+i]/(hi-lo);
                double w=m?2:1;
                double ag=w*creal(z*weight[m]), a0=w*creal(s0[m]*weight[m]), a1=w*creal(s1[m]*weight[m]);
                double y=ag-cg,tmp=gas+y;cg=(tmp-gas)-y;gas=tmp;
                y=a0-c0;tmp=solid0+y;c0=(tmp-solid0)-y;solid0=tmp;
                y=a1-c1;tmp=solid1+y;c1=(tmp-solid1)-y;solid1=tmp;
                absolute_gas+=w*cabs(z*weight[m]);
                absolute_solid+=w*cabs((s0[m]+s1[m])*weight[m]);
            }
            if (patch==-3) {
                solid0=creal(s0[0]);solid1=creal(s1[0]);
                gas=creal(t[0]);absolute_solid=cabs(s0[0]+s1[0]);
            }
            printf("{\"time_s\":%.17g,\"gas_k\":%.17g,\"solid0_k\":%.17g,\"solid1_k\":%.17g,\"q_j\":%.17g,\"absolute_gas_sum\":%.17g,\"absolute_solid_sum\":%.17g,\"roundoff_scale\":%.17g}\n",
                step*dt,gas,solid0,solid1,(patch==-3?50:50./8)*(solid0+solid1),absolute_gas,absolute_solid,DBL_EPSILON*(absolute_gas+absolute_solid));
        }
        if (step==steps) break;
        double powered=fmax(0,fmin((step+1)*dt,.4)-step*dt);
        for (int m=0;m<modes;m++) {
            int base=m*n;
            double complex r0=s0[m]+half*(g*(t[base]-s0[m])-2*(s0[m]-s1[m]))/50;
            double complex r1=s1[m]+half*2*(s0[m]-s1[m])/50;
            for (int i=0;i<n;i++) {
                int q=base+i;
                b[q]=(1+half*diag[q])*t[q]+(i?-off*t[q-1]:0)+(i<n-1?-off*t[q+1]:0)+powered*src[q];
            }
            b[base]+=half*beta*s0[m];solve(n,off,inv+base,b+base);
            double complex m00=1+half*(g+2)/50-half*g/50*v[base],m01=-half*2/50,m11=1+half*2/50;
            double complex det=m00*m11-m01*m01;
            r0+=half*g/50*b[base];
            double complex next0=(r0*m11-m01*r1)/det,next1=(m00*r1-m01*r0)/det;
            for (int i=0;i<n;i++) t[base+i]=b[base+i]+v[base+i]*next0;
            s0[m]=next0;s1[m]=next1;
        }
    }
    free(storage);return 0;
}
