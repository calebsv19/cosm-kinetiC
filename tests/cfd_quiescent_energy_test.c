/* Both inlet sides, obstacle/no obstacle, actual legacy parity and atomic KE. */
#define main legacy_wind_main
#include "cfd_driven_wind_test.c"
#undef main
#include "app/cfd_open_energy.h"
int main(void) {
    for(int side=0;side<2;side++)for(int body=0;body<2;body++) {
        CfdOpenAtmosphere3d s={0},old={0};setup(&s,side,body,0,1);setup(&old,side,body,0,1);
        s.wind.quiescent_backflow=true;double zero[512]={0};
        for(int step=0;step<80;step++) {
            assert(cfd_open_atmosphere3d_step(&s,zero,zero));
            if(step<30) {
                assert(cfd_open_atmosphere3d_step(&old,zero,zero));
                assert(!memcmp(s.velocity,old.velocity,(size_t)s.velocity_count*sizeof(double)));
            }
            double total=0,scale=1;for(int z=0;z<10;z++){total+=s.kinetic_ledger[z];scale+=fabs(s.kinetic_ledger[z]);}
            assert(fabs(total-cfd_open_kinetic(&s,s.velocity))<1e-12*scale);
            assert(s.kinetic_ledger[KE_EULER]>=0);
        }
        double saved[10];memcpy(saved,s.kinetic_ledger,sizeof(saved));double source[512]={0};source[0]=1e9;
        assert(!cfd_open_atmosphere3d_step(&s,source,zero));assert(!memcmp(saved,s.kinetic_ledger,sizeof(saved)));
        cfd_open_atmosphere3d_destroy(&s);cfd_open_atmosphere3d_destroy(&old);
    }
    puts("quiescent momentum, discrete KE closure, pre-shutoff bitwise parity and rollback passed");return 0;
}
