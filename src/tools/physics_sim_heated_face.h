#ifndef PHYSICS_SIM_HEATED_FACE_H
#define PHYSICS_SIM_HEATED_FACE_H
#include "app/cfd_open_atmosphere3d.h"
/* Development-only non-burning, isothermal face. SI, constant authored h and C.
 * No radiation, ignition, material calibration or worker/checkpoint integration.
 * The host owns the exchange cadence and cumulative signed transfer ledger. */
typedef struct {
    int axis, side;
    double temperature_k, capacity_j_k, h_w_m2_k;
    double gas_to_surface_j;
    /* Low part of temperature relative to gas.reference_k. Retain on replay;
     * avoids losing sub-ulp weak-plume increments when adding to ~300 K. */
    double temperature_correction_k;
} PhysicsSimHeatedFace;
/* Exchanges with the single layer of gas cells directly adjacent to one box
 * face. Symmetric forward/reverse exact cell-pair substeps avoid overshoot.
 * Atomic on refusal. Does not advance flow time or rewrite source input_j.
 * Gas balance must include face.gas_to_surface_j as a separate signed debit.
 * Call only between accepted flow steps, with an explicitly owned face state. */
bool physics_sim_heated_face_exchange(CfdOpenAtmosphere3d *gas,
                                     PhysicsSimHeatedFace *face, double dt);
/* Fixed physical support: full face footprint extruded outwards by depth_m.
 * Depth must align to grid faces and remain inside the domain. Conductance is
 * distributed by volume fraction, with total h*face_area independent of grid.
 * This authored nonlocal Robin support is not a resolved wall-gradient law. */
bool physics_sim_heated_face_exchange_depth(CfdOpenAtmosphere3d *gas,
    PhysicsSimHeatedFace *face, double dt, double depth_m);
/* Same volume weights as transfer. Returns temperature RISE above reference_k
 * to preserve weak signals. Read-only; refuses invalid/unaligned support. */
bool physics_sim_heated_face_sample(const CfdOpenAtmosphere3d *gas,
    const PhysicsSimHeatedFace *face, double depth_m, double *temperature_rise_k);
/* Full-XY exchange slab for prescribed-flow diagnostics, no solid box/wall.
 * Grid-aligned z bounds; area is domain XY area. Same conservative pair law. */
bool physics_sim_heated_slab_exchange(CfdOpenAtmosphere3d *gas,
    PhysicsSimHeatedFace *face,double dt,double lo,double hi);
/* Impermeable ground: one local x-strip per face, all y, bottom gas layer.
 * Count must equal nx. Capacity belongs to that strip. Atomic across the batch. */
bool physics_sim_heated_columns_exchange(CfdOpenAtmosphere3d *gas,
    PhysicsSimHeatedFace *faces,int count,double dt);
#endif
