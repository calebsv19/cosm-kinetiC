#ifndef PHYSICS_SIM_THERMAL_STEP_H
#define PHYSICS_SIM_THERMAL_STEP_H
#include "receiver.h"
#include "physics_sim_heated_face.h"
#include "physics_sim_local_matrix.h"
enum { MAX_NORMAL=512, TPS=51200, END=5*TPS, OBS=512, MICRO=8, WINDOW=40 };
typedef struct { double v[MAX_NORMAL],correction[MAX_NORMAL],source; ThermalReceiver r[4]; int tick; } PhysicsSimThermalState;
typedef struct { double q,allocation,debit,solid; double amounts[5],energy[5][2]; int micro; } PhysicsSimThermalTrace;
double physics_sim_thermal_solid_energy(const PhysicsSimThermalState *s);
/* Numerical kernel only. Runtime owns profile, admission and accepted state.
 * gas/matrix are scratch. Failure leaves accepted and output values unchanged. */
typedef bool (*PhysicsSimReceiverStage)(void *context,int column,const ThermalReceiver *accepted,
    const ThermalReceiverExchange *packet,CoreSimTimePoint available,ThermalReceiver *candidate,ThermalReceiverReceipt *receipt);
bool physics_sim_thermal_step(const PhysicsSimThermalState *s,PhysicsSimThermalState *out,
    CfdOpenAtmosphere3d *gas,PhysicsSimLocalMatrix *matrix,int ticks,int scalar_factor,
    int mode,int normal_cells,int fail,PhysicsSimThermalTrace *trace);
bool physics_sim_thermal_step_with_receiver(const PhysicsSimThermalState *s,PhysicsSimThermalState *out,
    CfdOpenAtmosphere3d *gas,PhysicsSimLocalMatrix *matrix,int ticks,int scalar_factor,
    int mode,int normal_cells,int fail,PhysicsSimThermalTrace *trace,PhysicsSimReceiverStage stage,void *context);
#endif
