#ifndef PHYSICS_SIM_CFD_MIXTURE3D_H
#define PHYSICS_SIM_CFD_MIXTURE3D_H
#include "app/cfd_cartesian3d.h"
#include "app/cfd_mixture_thermo.h"
/* Conservative 3D periodic HLLC Euler + prescribed equal-D Fick/Fourier.
 * First order. No gravity/walls/reservoirs/chemistry/radiation. */
typedef struct {
    CfdCartesian3d grid;CfdMixtureThermo thermo;
    double diffusion_m2_s,conductivity_w_m_k;
    double *state,*candidate;
} CfdMixture3d;
bool cfd_mixture3d_init(CfdMixture3d *s,const int n[3],const double length[3],double diffusion,double conductivity);
void cfd_mixture3d_destroy(CfdMixture3d *s);
bool cfd_mixture_flux(const CfdMixtureThermo *s,const double l[10],const double r[10],
    int axis,double diffusion,double conductivity,double spacing,double output[10],double diffusive[10],double *speed);
/* Integrated per-step sources; refusal preserves accepted state. */
bool cfd_mixture3d_step(CfdMixture3d *s,const double *source,double dt);
#endif
