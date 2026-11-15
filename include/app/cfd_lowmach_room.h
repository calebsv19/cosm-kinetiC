#ifndef PHYSICS_SIM_LOW_MACH_ROOM_H
#define PHYSICS_SIM_LOW_MACH_ROOM_H
#include <stdbool.h>
#include <stdint.h>

/* Opt-in CP2C engineering bridge: calorically perfect fixed air, unit room,
 * impermeable slip walls, one optional east reservoir. No reactions, diffusion,
 * wall heat transfer or acoustic waves. Source is volumetric sensible heat.
 * Thermodynamic p0 is uniform; pressure is the reduced dynamic correction.
 * Gravity subtracts the 300 K ambient hydrostat (small-height approximation).
 * Accepted states are atomic. No app/default worker integration. */
#define CFD_LM_R 287.0
#define CFD_LM_CP 1005.0
#define CFD_LM_T0 300.0
#define CFD_LM_P0 101325.0
typedef struct { int left, right, axis, xyz[3]; } CfdLmFace;
typedef struct {
    int n, cells, faces, fluid_cells;
    bool vent, obstacle;
    double gravity, p0, time;
    uint64_t steps;
    unsigned char *solid;
    int *cell_face, *lookup;
    CfdLmFace *face;
    double *rho, *enthalpy, *velocity, *pressure;
} CfdLmRoom;
typedef struct {
    double heat_j, outward_mass_kg, outward_enthalpy_j, pressure_work_j;
    double mass_residual_kg, enthalpy_residual_j, eos_relative_error;
    double divergence_error, advective_courant, max_mach;
    int pressure_iterations;
} CfdLmReceipt;
bool cfd_lm_create(CfdLmRoom *s, int n, bool vent, bool obstacle, double gravity);
void cfd_lm_destroy(CfdLmRoom *s);
bool cfd_lm_valid(const CfdLmRoom *s);
/* 0 < dt <= .01 s; accepted time <= .2 s, max 4000 steps. Reject, never clip.
 * Source length is cells; solid-cell sources must be exactly zero. */
bool cfd_lm_step(CfdLmRoom *s, double dt, const double *heat_w_m3, CfdLmReceipt *r);
/* Native deterministic helper for the prescribed-heater proof; not a burner. */
void cfd_lm_heater(const CfdLmRoom *s, double strength_w_m3, double *heat_w_m3);
#endif
