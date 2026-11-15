# Bounded variable-density room bridge (CP2C)

This is an opt-in source/headless engineering bridge, not a default application
mode or a validated room-fire model. It advances density, sensible enthalpy and
staggered velocity in a unit room with a small east window, internal wall cells
and an optional fixed block. Accepted time is capped at 0.2 s. No existing
compressible, Boussinesq, chemistry, cloth or desktop path is changed.

## Model and numerical contract

The first bridge uses calorically perfect, fixed-composition ideal air:
R = 287 J/(kg K), cp = 1005 J/(kg K), ambient T = 300 K, ambient p = 101325 Pa.
This explicit constant-cp approximation is distinct from the temperature-dependent
caloric model used in CP2A/CP2B; it is not its replacement or an accuracy claim at
high temperature. There is no combustion, species evolution, molecular diffusion,
viscosity, turbulence closure, radiation or wall heat transfer. Walls are
impermeable with slip, not a qualified no-slip boundary layer.

With H = rho cp T, cv = cp − R and prescribed volumetric heating Q:

```
rho_t + div(rho u) = 0
H_t + div(H u) = Q + dp0/dt
rho R T = p0(t)
div(u) = [R/cp Q − cv/cp dp0/dt] / p0
```

Vented rooms hold p0 fixed. In a sealed connected room,
dp0/dt = (R/cv) integral(Q dV) / V. The dynamic pressure is a separate reduced
pressure that enforces this divergence. Gravity acts on density relative to the
ambient hydrostatic reference. Uniform thermodynamic pressure is a small-height
low-Mach approximation; cold rest is relative to that subtracted hydrostat, not
an exact compressible stratified-column test. See the [NIST equation
overview](https://raw.githubusercontent.com/firemodels/fds/master/Manuals/FDS_Technical_Reference_Guide/Equation_Chapter.tex)
for the pressure-decomposition/expansion formulation background. This implementation
does not import FDS code or establish FDS equivalence.

The numerical step uses first-order upwind MAC velocity advection and buoyancy,
then a variable-density pressure solve with matrix-free Jacobi-preconditioned CG.
Internal face inverse density is 2/(rho_left + rho_right); the reservoir uses a
half-cell pressure distance and zero reduced pressure. The sealed solve removes
only the pressure gauge. Acceptance recomputes physical divergence, including
the compatibility equation. A true-residual and divergence gate rejects a failed
pressure solve.

Density and volumetric enthalpy use shared donor-cell face transfers, including
upwind 300 K reservoir inflow. Constant H = cp/R p0 and the divergence constraint
are discretely compatible; temperature is recovered from H/(rho cp), never
clipped and never repaired by resetting density. Step receipts report outward
mass/enthalpy, prescribed heat and sealed pressure work. The enthalpy ledger is
thermodynamic, not a kinetic-plus-potential total-energy validation. Momentum
advection is first order in advective form; global momentum/kinetic-energy
accuracy has not been qualified.

The source API permits grids 16³ and 32³, finite temperatures 250–1200 K,
0 < dt <= 0.01 s, at most 4000 steps and 0.2 s accepted time. Actual outward
advective Courant must be <= 0.4 and face Mach number <= 0.05. There is no acoustic
CFL restriction. These are bounded engineering admissions, not proof of physical
accuracy throughout those ranges. Native memory is accounted through the existing
`cfd_memory` owner. State and receipt are committed together only after all gates.

## Geometry and ownership

The small-window surfaces match `cfd_single_window_room`/`coupling.room_geometry`:
unit cube, wall thickness 1/16 m, window y in [3/8,5/8), z in [3/8,7/8).
The optional block occupies x in [1/2,5/8), y in [1/4,3/8), z in [0,1/4).
The proof uses that block. Gas volume is 0.7216796875 m³ and vent area is 0.125 m²
on both grids. Closing the vent closes its outer plane while retaining the same
gas cells. No face is created across a solid. Arbitrary disconnected geometry,
multiple pressure zones and exterior-air coupling are not supported.

The separate `cfd_lowmach_room*` modules own numerical state, geometry and
projection. `physics_sim_lowmach_room` is a trusted-local proof CLI; its hex-float
checkpoint rebuilds topology from a bounded configuration, validates state and
refuses trailing or malformed data. It is not an untrusted upload endpoint.
The Python qualification reconstructs the geometry from SI coordinates and
audits saved fields independently. No shared library, dependency or app default
is added; generic shared math/runtime libraries do not supply this masked solve.

## Fresh contained build and checks

From the selected PhysicsSim checkout, use new output directories:

```sh
python3 -B scripts/build_lowmach_room.py --output /absolute/fresh/strict
python3 -B scripts/build_lowmach_room.py --output /absolute/fresh/sanitized --sanitized
python3 -B scripts/qualify_lowmach_room.py \
  --strict /absolute/fresh/strict/room_worker \
  --sanitized /absolute/fresh/sanitized/room_worker \
  --output /absolute/fresh/checks
```

Use the workspace task-output lifecycle around a new invocation. Scripts refuse
existing output directories, build from source directly without stale objects,
write compile manifests and never clean previous output. The assessment runs
native strict/sanitized tests and a bounded matrix in at most 600 seconds, with
per-command timeouts. Do not use Python `-O`, which disables assessment assertions.

Proof CLI usage:

```sh
room_worker plume 16 0.005 40 /fresh/final.state /fresh/history.csv
room_worker plume 16 0.005 20 /fresh/prefix.state /fresh/prefix.csv
room_worker plume 16 0.005 20 /fresh/resumed.state /fresh/resumed.csv /fresh/prefix.state
```

Available modes are `cold`, `sealed`, `vent`, `plume`, and `nogravity`.
Uniform heated controls use 100 kW/m³. The localized prescribed heater uses
1 MW/m³ over x,y in [3/8,5/8), z in [1/8,1/4): 7812.5 W total, 1562.5 J
over 0.2 s. It injects heat only, with no imposed momentum. Output paths are
exclusive-create; completed state files are not overwritten.

## First qualification and remaining boundary

Cold rest is exact. Sealed heating matches the ideal-gas closed-volume reference.
Uniform vented expansion matches the discrete reference; halving dt halves its
error against the analytic exponential-density solution. Discrete manufactured
variable-density pressure gradients pass in sealed/open geometry. Invalid inputs,
memory refusal, actual fresh-process restart and strict/sanitized parity pass.

At 0.2 s the 16³ heater case reaches 528.98 K, with thermal-weighted vertical
velocity 0.19459 m/s and thermal centroid z = 0.20171 m. Disabling gravity gives
0.02211 m/s and z = 0.19052 m; heating expansion still causes flow. The 32³ case
gives 0.22019 m/s. Timestep halving changes mean vertical velocity by 0.575%;
spatial refinement changes it by 13.152%. This is a working short room bridge,
not a converged plume or room-fire accuracy result. The 0.2 s opening flow is
outward everywhere; sustained bidirectional ventilation is not demonstrated.

Stop before long campaigns. A next bounded slice should address spatial plume
sensitivity and qualify a temperature-dependent caloric/enthalpy closure before
connecting hotter room operation to the retained caloric lane. No automatic
extension of duration, geometry, source strength or default adoption follows.
