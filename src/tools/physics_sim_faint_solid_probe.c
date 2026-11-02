/* Direct small-signal reproducer; shared solver is linked read-only. */
#include "core_thermal_material_rod.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
int main(void) {
    CoreThermalMaterialRod m={0};m.count=2;m.area_m2=.0625;
    for (int i=0;i<2;i++) {
        m.width_m[i]=.005;
        m.material[i]=(CoreThermalMaterial){300,1200,160000,160000,.16,.16};
    }
    double initial[2]={300,300},power[2]={1e-16,0};
    CoreThermalRodState state;
    assert(core_thermal_material_rod_init(&m,initial,&state)==CORE_THERMAL_OK);
    unsigned maximum_iterations=0;
    for (int i=0;i<5120;i++) {
        CoreThermalMaterialRodStep r;
        assert(core_thermal_material_rod_step(&m,&state,power,1./1600,&r)==CORE_THERMAL_OK);
        if (r.iterations>maximum_iterations) maximum_iterations=r.iterations;
        state=r.result.state;
    }
    /* Independent two-capacity energy solution, plus exact BE recurrence.
     * E0+E1=P*t; (E0-E1)'=P-(2G/C)*(E0-E1). */
    double c=50,g=2,dt=1./1600,total=power[0]*3.2;
    double difference=power[0]*c/(2*g)*(-expm1(-5120*log1p(2*g/c*dt)));
    double expected1=(total-difference)/2;
    double exact1=(total-power[0]*c/(2*g)*(-expm1(-2*g/c*3.2)))/2;
    printf("{\"steps\":5120,\"power_w\":%.17g,\"front_j\":%.17g,\"back_j\":%.17g,\"back_be_j\":%.17g,\"back_continuous_j\":%.17g,\"maximum_iterations\":%u,\"balance_j\":%.17g,\"back_relative_error\":%.17g}\n",
        power[0],state.energy_j[0],state.energy_j[1],expected1,exact1,maximum_iterations,
        state.energy_j[0]+state.energy_j[1]-total,fabs(state.energy_j[1]/expected1-1));
    return 0;
}
