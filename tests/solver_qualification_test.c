#include "app/sim_runtime_3d_solver.h"
#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static SimRuntime3DDomainDesc domain(int n) {
    return (SimRuntime3DDomainDesc){.grid_w=n,.grid_h=n,.grid_d=n,
        .slice_cell_count=(size_t)n*n,.cell_count=(size_t)n*n*n,.voxel_size=1.0f/n};
}
// The Neumann cosine shear has zero divergence and zero self-advection.
// Exact continuum diffusion amplitude is exp(-nu*pi^2*t).
static double shear(int n, double dt, double nu) {
    SimRuntime3DDomainDesc d=domain(n);
    SimRuntime3DVolume v={0}; SimRuntime3DSolverScratch s={0};
    assert(sim_runtime_3d_volume_init(&v,&d)); assert(sim_runtime_3d_solver_scratch_init(&s,&d));
    for(int z=0;z<n;z++) for(int y=0;y<n;y++) for(int x=0;x<n;x++)
        v.velocity_x[sim_runtime_3d_volume_index(&d,x,y,z)]=(float)cos(3.141592653589793*(y+.5)/n);
    AppConfig cfg=app_config_default(); cfg.fluid_buoyancy_force=0;
    cfg.fluid_3d_si_viscosity=true; cfg.fluid_3d_kinematic_viscosity_m2_s=(float)nu;
    int count=(int)lround(.2/dt);
    for(int k=0;k<count;k++) {
        SimRuntime3DSolverStepMetrics m={0};
        assert(sim_runtime_3d_solver_step_first_pass(&v,&s,NULL,NULL,&cfg,dt,100,&m));
        assert(m.max_abs_divergence_after_project<1e-5);
    }
    double error=0, amplitude=exp(-nu*3.141592653589793*3.141592653589793*count*dt);
    for(int z=0;z<n;z++) for(int y=0;y<n;y++) for(int x=0;x<n;x++) {
        double expected=amplitude*cos(3.141592653589793*(y+.5)/n);
        double e=v.velocity_x[sim_runtime_3d_volume_index(&d,x,y,z)]-expected;error+=e*e;
    }
    error=sqrt(error/d.cell_count);
    printf("shear n=%d dt=%g nu=%g L2_error=%g\n",n,dt,nu,error);
    assert(error<.004);
    sim_runtime_3d_solver_scratch_destroy(&s);sim_runtime_3d_volume_destroy(&v);
    return error;
}
static void projection_and_rest(void) {
    SimRuntime3DDomainDesc d=domain(16); SimRuntime3DVolume v={0};SimRuntime3DSolverScratch s={0};
    assert(sim_runtime_3d_volume_init(&v,&d));assert(sim_runtime_3d_solver_scratch_init(&s,&d));
    AppConfig cfg=app_config_default();cfg.fluid_buoyancy_force=0;cfg.fluid_3d_si_viscosity=true;
    SimRuntime3DSolverStepMetrics m={0};
    assert(sim_runtime_3d_solver_step_first_pass(&v,&s,NULL,NULL,&cfg,.01,100,&m));
    assert(m.max_abs_divergence_after_project==0 && m.pressure_residual_linf==0);
    for(int z=0;z<16;z++) for(int y=0;y<16;y++) for(int x=0;x<16;x++)
        v.velocity_x[sim_runtime_3d_volume_index(&d,x,y,z)]=.1f*sinf(6.28318530718f*(x+.5f)/16);
    assert(sim_runtime_3d_solver_step_first_pass(&v,&s,NULL,NULL,&cfg,.001,100,&m));
    printf("projection divergence before=%g after=%g Poisson residual=%g\n",
        m.max_abs_divergence_before_project,m.max_abs_divergence_after_project,m.pressure_residual_linf);
    assert(m.max_abs_divergence_after_project<m.max_abs_divergence_before_project);
    assert(isfinite(m.pressure_residual_linf));
    assert(!sim_runtime_3d_diffuse_velocity_si(&v,&s,NULL,1e9,1));
    uint8_t *solid=calloc(d.cell_count,1);solid[100]=1;v.velocity_x[100]=1;
    assert(sim_runtime_3d_diffuse_velocity_si(&v,&s,solid,.1,.01));assert(v.velocity_x[100]==0);
    free(solid);sim_runtime_3d_solver_scratch_destroy(&s);sim_runtime_3d_volume_destroy(&v);
}
static void compatible_projection(void) {
    for (int walls = 0; walls < 2; ++walls) {
        SimRuntime3DDomainDesc d = domain(12);
        SimRuntime3DVolume v = {0};
        assert(sim_runtime_3d_volume_init(&v, &d));
        uint8_t *solid = calloc(d.cell_count, 1);
        for (int z = 0; z < 12; ++z)
        for (int y = 0; y < 12; ++y)
        for (int x = 0; x < 12; ++x) {
            size_t i = sim_runtime_3d_volume_index(&d,x,y,z);
            solid[i] = walls && x >= 5 && x <= 6 && y >= 4 && y <= 7 && z >= 4 && z <= 7;
            v.velocity_x[i] = .1f*sinf(.4f*x + .2f*y);
            v.velocity_y[i] = .1f*cosf(.3f*y + .1f*z);
            v.velocity_z[i] = .1f*sinf(.5f*z + .2f*x);
        }
        double momentum_before[3] = {0}, momentum_after[3] = {0};
        for (size_t i = 0; i < d.cell_count; ++i) {
            momentum_before[0] += v.velocity_x[i];
            momentum_before[1] += v.velocity_y[i];
            momentum_before[2] += v.velocity_z[i];
        }
        double energy_before = 0;
        for (size_t i = 0; i < d.cell_count; ++i)
            energy_before += v.velocity_x[i]*v.velocity_x[i] + v.velocity_y[i]*v.velocity_y[i] + v.velocity_z[i]*v.velocity_z[i];
        SimRuntime3DSolverStepMetrics m = {0};
        assert(sim_runtime_3d_project_compatible(&v, solid, 48, &m));
        printf("compatible walls=%d before=%g after=%g residual=%g\n", walls,
            m.max_abs_divergence_before_project, m.max_abs_divergence_after_project, m.pressure_residual_linf);
        assert(m.max_abs_divergence_after_project < m.max_abs_divergence_before_project*.001);
        assert(fabs(m.max_abs_divergence_after_project-m.pressure_residual_linf) < 2e-6);
        double energy_after = 0, independently_measured_divergence = 0;
        for (int z = 0; z < 12; ++z)
        for (int y = 0; y < 12; ++y)
        for (int x = 0; x < 12; ++x) {
            size_t i = sim_runtime_3d_volume_index(&d,x,y,z);
            float u[3] = {v.velocity_x[i],v.velocity_y[i],v.velocity_z[i]};
            int c[3] = {x,y,z};
            double cell_divergence = 0;
            const float *fields[3] = {v.velocity_x,v.velocity_y,v.velocity_z};
            for (int a = 0; a < 3; ++a) {
                c[a]++;
                size_t hi = sim_runtime_3d_volume_index_clamped(&d,c[0],c[1],c[2]);
                c[a]-=2;
                size_t lo = sim_runtime_3d_volume_index_clamped(&d,c[0],c[1],c[2]);
                c[a]++;
                if (solid[i] || solid[hi] || solid[lo]) assert(u[a] == 0);
                energy_after += u[a]*u[a];
                momentum_after[a] += u[a];
                cell_divergence += (fields[a][hi]-fields[a][lo])/(2*d.voxel_size);
            }
            if (!solid[i]) independently_measured_divergence = fmax(independently_measured_divergence, fabs(cell_divergence));
        }
        assert(energy_after <= energy_before);
        assert(fabs(independently_measured_divergence-m.max_abs_divergence_after_project) < 2e-6);
        if (!walls) for (int a = 0; a < 3; ++a)
            assert(fabs(momentum_before[a]-momentum_after[a]) < 1e-5);
        float saved_h = v.desc.voxel_size;
        v.desc.voxel_size = NAN;
        assert(!sim_runtime_3d_project_compatible(&v, solid, 48, &m));
        v.desc.voxel_size = saved_h;
        v.velocity_x[0] = NAN;
        assert(!sim_runtime_3d_project_compatible(&v, solid, 48, &m));
        free(solid); sim_runtime_3d_volume_destroy(&v);
    }
}
static void conservation_identity(void) {
    SimRuntime3DDomainDesc d = domain(8);
    SimRuntime3DVolume v = {0};
    assert(sim_runtime_3d_volume_init(&v, &d));
    uint8_t *solid = calloc(d.cell_count, 1);
    SimRuntime3DConservation c = {0};
    for (int z = 0; z < 8; ++z)
    for (int y = 0; y < 8; ++y)
    for (int x = 0; x < 8; ++x) {
        size_t i = sim_runtime_3d_volume_index(&d,x,y,z);
        v.velocity_x[i] = (x+.5f)/8;
    }
    assert(sim_runtime_3d_measure_conservation(&d,v.velocity_x,v.velocity_y,v.velocity_z,solid,&c));
    assert(fabs(c.net_outward_flux_m3_s-.875) < 1e-12);
    assert(fabs(c.integrated_divergence_m3_s-c.net_outward_flux_m3_s) < 1e-12);
    assert(fabs(c.max_abs_divergence_s_inv-1) < 1e-12);
    solid[sim_runtime_3d_volume_index(&d,4,4,4)] = 1;
    assert(sim_runtime_3d_measure_conservation(&d,v.velocity_x,v.velocity_y,v.velocity_z,solid,&c));
    assert(fabs(c.integrated_divergence_m3_s-c.net_outward_flux_m3_s) < 1e-12);
    v.velocity_x[0] = NAN;
    assert(!sim_runtime_3d_measure_conservation(&d,v.velocity_x,v.velocity_y,v.velocity_z,solid,&c));
    assert(!c.valid);
    free(solid); sim_runtime_3d_volume_destroy(&v);
    puts("final snapshot conservation passed: analytic flux, discrete divergence theorem, solid mask, nonfinite rejection");
}
static void prescribed_inlet(void) {
    for (int axis = 0; axis < 3; ++axis) for (int side = 0; side < 2; ++side) {
        SimRuntime3DDomainDesc d = domain(8);
        SimRuntime3DVolume v = {0};
        assert(sim_runtime_3d_volume_init(&v,&d));
        uint8_t *solid = calloc(d.cell_count,1);
        for (int z=0;z<8;++z) for (int y=0;y<8;++y) for (int x=0;x<8;++x) {
            int c[3]={x,y,z}; size_t i=sim_runtime_3d_volume_index(&d,x,y,z);
            for (int a=0;a<3;++a) if (a!=axis && (c[a]==0 || c[a]==7)) solid[i]=1;
        }
        SimRuntime3DProjectionBoundary boundary={.prescribed_inlet=true,
            .inlet_axis=axis,.inlet_at_max=side,.inflow_speed=2};
        SimRuntime3DSolverStepMetrics m={0};
        assert(sim_runtime_3d_project_boundary(&v,solid,1,&boundary,&m));
        assert(m.projection_iterations_used==1 && !m.projection_converged);
        assert(sim_runtime_3d_project_boundary(&v,solid,48,&boundary,&m));
        assert(m.projection_converged && m.projection_iterations_used<=48);
        float *velocity[3]={v.velocity_x,v.velocity_y,v.velocity_z};
        double max_error=0;
        for (int z=0;z<8;++z) for (int y=0;y<8;++y) for (int x=0;x<8;++x) {
            int c[3]={x,y,z}; size_t i=sim_runtime_3d_volume_index(&d,x,y,z);
            if (solid[i]) continue;
            if (c[axis]==(side ? 7:0)) assert(velocity[axis][i]==(side ? -2:2));
            max_error=fmax(max_error,fabs(velocity[axis][i]-(side ? -2:2)));
        }
        SimRuntime3DConservation balance={0};
        assert(sim_runtime_3d_measure_conservation(&d,v.velocity_x,v.velocity_y,v.velocity_z,solid,&balance));
        printf("prescribed inlet axis=%d side=%d velocity_error=%g divergence=%g flux_balance=%g\n",
            axis,side,max_error,balance.max_abs_divergence_s_inv,balance.net_outward_flux_m3_s);
        assert(max_error<1e-4);
        assert(balance.max_abs_divergence_s_inv<1e-4);
        assert(fabs(balance.net_outward_flux_m3_s)<1e-6);
        free(solid);sim_runtime_3d_volume_destroy(&v);
    }
}
/* Isolated transported shear: amplitude and phase compared with the continuum
 * solution; enough transverse padding to exclude clamped-edge influence.
 * First-order amplification is retained only as the previous-method baseline. */
static double transported_shear(int nx, double dt, double nu) {
    float h=8.0f/nx;
    SimRuntime3DDomainDesc d={.grid_w=nx,.grid_h=16,.grid_d=4,.voxel_size=h,
        .slice_cell_count=(size_t)nx*16,.cell_count=(size_t)nx*64};
    SimRuntime3DVolume v={0};SimRuntime3DSolverScratch scratch={0};
    assert(sim_runtime_3d_volume_init(&v,&d));
    assert(sim_runtime_3d_solver_scratch_init(&scratch,&d));
    const double k=6.283185307179586;
    for(int z=0;z<4;z++) for(int y=0;y<16;y++) for(int x=0;x<nx;x++) {
        size_t i=sim_runtime_3d_volume_index(&d,x,y,z);
        v.velocity_x[i]=2;
        v.velocity_y[i]=.1*cos(k*(x+.5)*h);
    }
    int steps=(int)lround(.2/dt);
    for(int t=0;t<steps;t++) {
        assert(sim_runtime_3d_solver_capture_previous_fields(&scratch,&v));
        sim_runtime_3d_advect_velocity_bounded(&v,&scratch,NULL,dt/h,NULL);
        assert(sim_runtime_3d_diffuse_velocity_si(&v,&scratch,NULL,nu,dt));
    }
    double a=0,b=0;
    for(int x=nx/4;x<3*nx/4;x++) {
        double value=v.velocity_y[sim_runtime_3d_volume_index(&d,x,8,2)];
        a+=value*cos(k*(x+.5)*h);b+=value*sin(k*(x+.5)*h);
    }
    double ratio=hypot(a,b)*2/(nx/2)/.1;
    double fraction=2*dt/h-floor(2*dt/h);
    double gain=hypot(1-fraction+fraction*cos(k*h),fraction*sin(k*h));
    double predicted=pow(gain,steps);
    double first_order_phase=steps*(floor(2*dt/h)*k*h + atan2(fraction*sin(k*h),1-fraction+fraction*cos(k*h)));
    printf("first_order_phase_error_rad=%g\n",fabs(remainder(first_order_phase-k*2*.2,2*3.141592653589793)));
    double exact=exp(-nu*k*k*.2);
    double phase_error=fabs(remainder(atan2(b,a)-k*2*.2,2*3.141592653589793));
    printf("transported shear h=%g dt=%g retained_amplitude=%g first_order_predicted=%g equivalent_nu=%g\n",
        h,dt,ratio,predicted,-log(ratio)/(.2*k*k));
    printf("transport acceptance nu=%g exact_amplitude=%g phase_error_rad=%g\n",nu,exact,phase_error);
    /* Independent budgets: 5 percentage points amplitude, quarter-cell phase. */
    assert(fabs(ratio-exact) < .05);
    assert(ratio <= 1.00001 && phase_error/(k*h) < .25);
    sim_runtime_3d_solver_scratch_destroy(&scratch);sim_runtime_3d_volume_destroy(&v);
    return ratio;
}

/* Direct operator contracts isolate transport from pressure correction. */
static void transport_bounds_and_walls(void) {
    SimRuntime3DDomainDesc d=domain(16);
    SimRuntime3DVolume v={0}; SimRuntime3DSolverScratch s={0};
    assert(sim_runtime_3d_volume_init(&v,&d));
    assert(sim_runtime_3d_solver_scratch_init(&s,&d));
    uint8_t *solid=calloc(d.cell_count,1);
    float *field[3]={v.velocity_x,v.velocity_y,v.velocity_z};
    for(int axis=0;axis<3;axis++) for(int sign=-1;sign<=1;sign+=2) {
        int transverse=(axis+1)%3;
        for(int wall=0;wall<2;wall++) {
            for(int z=0;z<16;z++) for(int y=0;y<16;y++) for(int x=0;x<16;x++) {
                int q[3]={x,y,z};size_t i=sim_runtime_3d_volume_index(&d,x,y,z);
                solid[i]=wall && q[axis]==8;
                for(int a=0;a<3;a++) field[a][i]=0;
                field[axis][i]=sign*2;
                field[transverse][i]=q[axis]<8 ? .25f : .75f;
            }
            /* A three-cell departure crosses the one-cell wall. */
            assert(sim_runtime_3d_solver_capture_previous_fields(&s,&v));
            SimRuntime3DSolverStepMetrics metrics={0};
            sim_runtime_3d_advect_velocity_bounded(&v,&s,solid,1.5f,&metrics);
            size_t fluid_cells=d.cell_count-(wall?256:0);
            assert(metrics.transport_corrected_components+metrics.transport_fallback_components==3*fluid_cells);
            assert(metrics.transport_limited_components<=metrics.transport_corrected_components);
            for(int z=0;z<16;z++) for(int y=0;y<16;y++) for(int x=0;x<16;x++) {
                int q[3]={x,y,z};size_t i=sim_runtime_3d_volume_index(&d,x,y,z);
                if(solid[i]) { for(int a=0;a<3;a++) assert(field[a][i]==0);continue; }
                assert(fabsf(field[axis][i]-sign*2)<1e-6);
                assert(field[transverse][i]>=.25f-1e-6 && field[transverse][i]<=.75f+1e-6);
                if(wall) assert(fabsf(field[transverse][i]-(q[axis]<8?.25f:.75f))<1e-6);
            }
        }
    }
    /* An exact diagonal corner crossing must not slip past either side voxel. */
    memset(solid,0,d.cell_count);
    for(size_t i=0;i<d.cell_count;i++) { field[0][i]=-1;field[1][i]=-1;field[2][i]=.75f; }
    size_t origin=sim_runtime_3d_volume_index(&d,6,6,8);
    field[2][origin]=.25f;
    solid[sim_runtime_3d_volume_index(&d,7,6,8)]=1;
    assert(sim_runtime_3d_solver_capture_previous_fields(&s,&v));
    sim_runtime_3d_advect_velocity_bounded(&v,&s,solid,2,NULL);
    assert(field[2][origin]==.25f);
    free(solid);sim_runtime_3d_solver_scratch_destroy(&s);sim_runtime_3d_volume_destroy(&v);
    puts("transport constant preservation, sharp-step bounds and six-direction wall separation passed");
}

int main(void) {
    double transport_coarse=transported_shear(128,.02,0);
    double transport_smaller_dt=transported_shear(128,.01,0);
    double transport_smallest_dt=transported_shear(128,.005,0);
    double transport_finer_grid=transported_shear(256,.01,0);
    assert(transport_coarse > .95 && transport_smaller_dt > .95 && transport_smallest_dt > .95);
    assert(transport_finer_grid>transport_smaller_dt);
    transported_shear(128,.01,.01);
    transport_bounds_and_walls();
    prescribed_inlet();
    conservation_identity();
    compatible_projection();
    projection_and_rest();
    double a=shear(8,.001,.1),b=shear(16,.001,.1),c=shear(32,.001,.1);
    assert(b<a && c<b);shear(16,.002,.1);shear(16,.001,.01);shear(16,.001,0);
    puts("solver qualification passed: rest, projection reduction, SI shear decay, grid refinement, viscosity response, bounded diffusion");
}
