#define main wall_pressure_main
#include "cfd_obstacle3d_wall_pressure_probe.c"
#undef main
static double power_mean(double left, double right, int power) {
    return (pow(right, power+1)-pow(left, power+1))/((power+1)*(right-left));
}
int main(void) {
    double maximum = 0, gauge_error = 0, cusp_old[2], cusp_new[2];
    int samples = 0, fallback_samples[3] = {0};
    for (int scene = 0; scene < 4; scene++) {
        int n = scene == 1 ? 32 : 16;
        int dims[3] = {2*n, n, n}, lo[3] = {3*n/4, n/4, n/4}, hi[3] = {5*n/4, 3*n/4, 3*n/4};
        if (scene >= 2) { lo[1] = scene == 2 ? 3 : 2; hi[1] = n-lo[1]; }
        double lengths[3] = {4, 2, 2}, h = 2./n;
        CfdMemoryBudget budget = {.limit_bytes=512*1024*1024};
        CfdMemoryBudget *previous = cfd_memory_scope(&budget);
        CfdCartesian3d grid; assert(cfd_cartesian3d_init(&grid,dims,lengths));
        CfdObstacleMixed3d *s = cfd_obstacle_mixed3d_create(&grid,.1,lo,hi);
        assert(s && cfd_obstacle_mixed3d_verify(s));
        CfdObstacle3d view = {.grid=grid,.mixed=s,.mu=.1};
        memcpy(view.lo,lo,sizeof(lo)); memcpy(view.hi,hi,sizeof(hi));
        double *p = cfd_memory_calloc(s->cells,sizeof(double)); assert(p);
        for (int px=0; px<=3; px++)
            for (int py=0; py<=3-px; py++)
                for (int pz=0; pz<=3-px-py; pz++) {
                    int powers[3] = {px,py,pz}; double before[3][2];
                    for (int gauge=0; gauge<2; gauge++) {
                        double offset = gauge ? 3.2 : 0;
                        for (int k=0; k<n; k++)
                            for (int j=0; j<n; j++)
                                for (int i=0; i<2*n; i++) {
                                    int q=cfd_obstacle_mixed3d_cell(s,i,j,k);
                                    if(q>=0) p[q]=offset+power_mean(i*h,(i+1)*h,px)*power_mean(j*h,(j+1)*h,py)*power_mean(k*h,(k+1)*h,pz);
                                }
                        for (int a=0; a<3; a++)
                            for (int side=0; side<2; side++) {
                                int direction=side?1:-1,cell[3]={lo[0]+1,lo[1]+1,lo[2]+1};
                                cell[a]=side?hi[a]:lo[a]-1;
                                int available=side?dims[a]-hi[a]:lo[a],degree=available>=4?3:available>=3?2:1;
                                if(powers[a]>degree) continue;
                                double force[3]={0}; candidate_pressure_load(&view,p,a,direction,cell,force);
                                double plane=(side?hi[a]:lo[a])*h,exact=pow(plane,powers[a]);
                                for(int b=0;b<3;b++) if(b!=a) exact*=power_mean(cell[b]*h,(cell[b]+1)*h,powers[b]);
                                exact=-(exact+offset)*direction*grid.area[a];
                                maximum=fmax(maximum,fabs(force[a]-exact)); samples++; fallback_samples[degree-1]++;
                                if(!gauge) before[a][side]=force[a];
                                else gauge_error=fmax(gauge_error,fabs((force[a]-before[a][side])+offset*direction*grid.area[a]));
                            }
                    }
                }
        if(scene<2) {
            for (int k=0;k<n;k++) for(int j=0;j<n;j++) for(int i=0;i<2*n;i++) {
                int q=cfd_obstacle_mixed3d_cell(s,i,j,k);
                if(q<0) continue;
                double front=lo[0]*h,back=hi[0]*h;
                if(i<lo[0]) {
                    double left=front-(i+1)*h,right=front-i*h;
                    p[q]=.02-.002*(pow(right,1.5)-pow(left,1.5))/(1.5*h);
                } else if(i>=hi[0]) {
                    double left=i*h-back,right=(i+1)*h-back;
                    p[q]=.005+.002*(pow(right,1.5)-pow(left,1.5))/(1.5*h);
                } else p[q]=.02-.015*((i+.5)*h-front);
            }
            double old[3]={0},old_v[3]={0},candidate[3]={0};
            for(int side=0;side<2;side++) for(int j=lo[1];j<hi[1];j++) for(int k=lo[2];k<hi[2];k++) {
                int cell[3]={side?hi[0]:lo[0]-1,j,k},direction=side?1:-1;
                double *zero=cfd_memory_calloc(s->count,sizeof(double)); assert(zero);
                patch_force_legacy(&view,zero,p,0,direction,cell,old,old_v);
                cfd_memory_free(zero);
                candidate_pressure_load(&view,p,0,direction,cell,candidate);
            }
            cusp_old[scene]=fabs(old[0]-.015); cusp_new[scene]=fabs(candidate[0]-.015);
        }
        cfd_memory_free(p); cfd_obstacle_mixed3d_destroy(s);
        assert(budget.live_bytes==0); cfd_memory_scope(previous);
    }
    assert(maximum<1e-11 && gauge_error<1e-11);
    double old_ratio=cusp_old[1]/cusp_old[0],new_ratio=cusp_new[1]/cusp_new[0];
    assert(fabs(old_ratio-sqrt(.5))<1e-9 && fabs(new_ratio-sqrt(.5))<1e-9);
    printf("{\"samples\":%d,\"maximum_patch_error_n\":%.17g,\"gauge_shift_error_n\":%.17g,"
           "\"fallback_degree_samples\":[%d,%d,%d],\"cusp_original_errors_n\":[%.17g,%.17g],"
           "\"cusp_candidate_errors_n\":[%.17g,%.17g],\"cusp_original_ratio\":%.17g,\"cusp_candidate_ratio\":%.17g}\n",
           samples,maximum,gauge_error,fallback_samples[0],fallback_samples[1],fallback_samples[2],
           cusp_old[0],cusp_old[1],cusp_new[0],cusp_new[1],old_ratio,new_ratio);
    return 0;
}
