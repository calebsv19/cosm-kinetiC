# Chemistry experiment workspace (development source)

Launch `physics_sim --chemistry-workspace /absolute/experiment-root` with
`PHYSICS_SIM_CHEMISTRY_LIBRARY` pointing to the qualified RC2 transport library.
The root parent must exist. This source-only mode needs the checkout's Python3
client, json-c and SDL runtime; it is not an installed release feature.

Use Dimension, D/u/dt and rate controls to author the draft, then Apply/New.
Run computes bounded batches; Pause retains the accepted state and rejects pending
publication. Step advances one tick. Reset restores the loaded initial field.
Apply/New, Reset and Open can invalidate and queue behind active computation.
Species and Z controls select a slice; click it for actual concentration at a cell.
Totals, cumulative directed reaction extents and budget residuals show accepted data.

Save writes a new experiment under the selected root and displays its path. Click
the path field, type the full path and press Enter/Open path, or drop saved JSON on
the window. RC2 config JSON opens a new experiment; saved session JSON resumes its
accepted state. Existing files are preserved. The saved engine hash must match.

The model is synthetic, isothermal, uniform fully filled cells, periodic/sealed,
constant prescribed velocity and shared D. Unstable timesteps refuse. Open a custom
RC2 JSON for different supported mechanisms and initial inventories. Draft labels
are distinct from accepted field/time readouts. No solved flow or calibrated chemical
prediction is supplied by this UI. Build with the normal fresh-contained Clang route.

## RC4 selected solved flow

A flow-capable engine can also open `rc4/v1` experiment JSON. It uses periodic
unsteady Stokes with a bounded solutal Boussinesq force, <=1% density contrast and
Re<=0.1. The native readout shows solved-flow Re, contrast and divergence. Draft
Apply/New still creates the RC3 prescribed-flow preset; author the flow closure
through RC4 JSON and use Open. No automatic reinterpretation of historical saves.
`PHYSICS_SIM_CHEMISTRY_INITIAL=/absolute/rc4.json` opens that model at launch.
Saved flow sessions include velocity, reduced pressure and mechanical energy ledgers.
The existing Run/Pause/Step/Reset/Save/Open generation guards apply to both models.
The exact retained older RC3 engine cannot execute RC4; select the new engine.

## RC5 selected thermal chemistry

A thermal-capable engine opens `rc5/v1` spatial JSON. The selected
`synthetic-caloric/v1` model stores formation-inclusive internal energy; temperature
comes from composition and energy, with temperature-dependent irreversible rates.
Required property provenance, units and temperature range travel with the config.
This is a fixed-volume synthetic liquid model, not calibrated chemical data.

View temperature toggles between the selected species and a temperature field;
click the slice for kelvin, with an automatic range, property id and internal-energy
residual. Run/Pause/Step/Reset/Save/Open keep the same publication guards. Saved
sessions include energy, heat-exchange ledgers, velocity and pressure. Open an RC5
JSON to set properties, bath/power, conductivity and bounded thermal buoyancy;
Apply/New still creates the prescribed isothermal draft. The native inspector
opens spatial RC5; local thermal reactors use the separate CLI.

Build a fresh thermal engine with
`python3 -B scripts/build_chemistry_thermal.py --output /absolute/fresh-directory`.
Use `scripts/chemistry_thermal.py --library ... --config ... --steps N` for
`rc5-local/v1` or `rc5/v1`; optional `--checkpoint` writes a new file and `--resume`
requires the same config and engine. Stiff/depleted, invalid-temperature, high-Re,
excess-density-contrast or unstable transport candidates refuse without clipping.
Physical viscosity returns heat; numerical damping is reported separately. Thermal
coupling retains the periodic Stokes Re<=0.1 and <=1% contrast limits. Reverse thermal
reaction pairs await an equilibrium/entropy model. Phase change, compressible gas,
AMR, particles, optics and real organic chemistry remain separate development gates.

## RC6 conditional aqueous organic model

`rc6/v1` opens a versioned, isothermal aqueous D-glucose mutarotation experiment.
It represents effective alpha/beta glucopyranose conversion, with matching elemental
C/H/O inventories and an equilibrium ratio constrained by the curated data. Open
`data/chemistry/glucose_spatial_v1.json` with a transport-capable engine. The native
readout identifies the conditional aqueous model, reference temperature, equilibrium
ratio and unreported pH. Species slices/probes and reaction extents use accepted
amounts. Apply/New still creates the synthetic draft.

The reference preparation is water with no added catalyst listed, at274.65K and
approximately0.236mol/L total glucose. This model requires that exact reference
condition record and uniform total stock concentration. Alpha/beta fractions can
vary spatially, with the underlying1D/2D/3D prescribed transport; transport coefficients
are operator inputs, not empirically calibrated glucose diffusivities or solved flow.
Different temperature, specified pH, cosolvent, salts or catalysts refuse. Unknown
pH is not interpreted as neutral pH. All physical scope is supplied by the RC6
envelope; the internal RC1 numeric-kernel schema keeps its legacy phase tag for
backward compatibility. That internal tag is not a claim about the aqueous phase.

Use `scripts/chemistry_aqueous.py --qualify` for the source-data comparisons and
uncertainty limitations without a native engine. To simulate the historical local
reference, use `--library ... --config data/chemistry/glucose_local_v1.json --steps N`.
Time zero is the first measurement at4min, with30s default ticks. `--checkpoint`
creates a new file; `--resume` requires exact config, native engine, curated data and
model/adapter code identity. The spatial model uses the same CLI and native workspace.

Digit-rounding bands are partial sensitivity bounds, not complete confidence
intervals. Two observations excluded from parameter fitting pass a declared0.20degS
engineering tolerance but lie outside those narrow bands. Measurement/systematic,
temperature/pH and preparation uncertainties remain incomplete. The split/tolerance
was fixed after exploratory source inspection; it is not blinded or preregistered
validation. Effective relative Gibbs diagnostics use an ideal-mixture concentration
closure at the fixed reference condition. Neither those diagnostics nor inferred
conditional deltaG supplies a reaction enthalpy; RC5 thermal reverse-pair guards
remain unchanged. See `data/chemistry/README.md` for the source and reduction.

RC6 reports explicitly flag whether elapsed time remains within0..14160s, equivalent
to the observed4..240min experiment window. Longer numerical integrations are
extrapolations. The later equilibrium reading supplies asymptotic calibration;
its timed approach and late temperature protocol are not qualified by this model.

## RC7 selected oxygen partition slice

A phase-capable engine opens `rc7/v1` finite gas/water oxygen experiments.
Use `scripts/build_chemistry_phase.py --output /absolute/fresh-engine` and select
its `phase.dylib`. Open `data/chemistry/oxygen_partition_spatial_v1.json` in the
native workspace, or use `scripts/chemistry_phase.py --library ... --config ...`.
The local example uses one cell; both use the same API and complete checkpoints.

Each cell has explicit occupied gas/liquid volumes, a fixed interface area and a
selected permeance. Oxygen transfers both ways toward Henry equilibrium. Gas and
liquid concentrations divide by their respective occupied volumes; bulk concentrations
are separately labeled in JSON. Native slice/probe, totals and directed extents show
the accepted state; readouts include Henry coefficient, occupied volumes and bath
heat/energy residual. Save/reopen and active cancellation keep existing guards.
The external bath holds298.15K. Oxygen inventory, phase energy and bath exchange
are accounted once. Gas/liquid D and imposed velocities are independent inputs.

Phase config identifies the property pack/hash/variant, energy convention, solvent,
salinity, occupied fraction, per-cell area, permeance and both transports. Legacy
top-level velocity/D must be zero; `reacting` enables phase exchange for RC7, not
chemical reaction. Apply/New retains the synthetic prescribed-flow draft. Unsupported
conditions, negative/depleted states, pressure/concentration screens or unstable
transport refuse whole joint ticks. Same-engine/config/model-code continuation is
exact; old chemistry saves retain their own schema and engine requirements.

This selected slice models nonreactive O2 dissolution/degassing at fixed geometry
and temperature. Transfer speed is not experimentally calibrated. Its inferred
phase energy is a declared approximation, and literature sensitivity scenarios are
not confidence intervals. Other RC7 branches—evaporation/condensation, droplets,
aerosols, moving interfaces and reactive surfaces—remain separate. See
`data/chemistry/oxygen_partition.md` for the physical/data limitations.

## RC7 finite water chamber

Build the phase engine with `scripts/build_chemistry_phase.py --water --output
/absolute/fresh-engine` and select its `phase.dylib`. Open any
`data/chemistry/water_*_v1.json` experiment in the workspace. The evaporation and
condensation examples are insulated. The dry-out example uses a bath. The JSON
sets chamber volume, water mass, initial vapor mass, temperature, lumped sensible
heat capacity, relaxation time and timestep. Use `scripts/chemistry_water.py` with
`--library`, `--config`, and `--steps` for a CLI report. It also accepts
`--checkpoint` and `--resume`.

The admitted range is 283.15–323.15 K. Saturation properties use IAPWS SR1-86(1992).
The chamber updates occupied volumes with the liquid mass and tracks heat into
the chamber as positive. Vapor pressure is a saturation-anchored linear-density
approximation away from equilibrium. Phase exchange uses a specified relaxation
time. See the source tests for NISTIR 5078 property and energy witnesses.

[IAPWS supplementary release](https://iapws.org/public/documents/6dGkr/Supp-sat.pdf)

### Moving planar water interface

Open `data/chemistry/water_interface_evaporation_v1.json` or
`data/chemistry/water_interface_condensation_v1.json` in the Chemistry
workspace using the phase property library. Each fixture represents a
sealed one-dimensional column. Step or run to inspect vapor, temperature,
liquid surface displacement and bath and energy diagnostics. The display
and probe use the physical cell edges. Save and reopen preserve the accepted
state.

For a headless run, use
`python3 scripts/chemistry_water_interface.py --library PATH --config PATH --steps 100`.
The model uses a specified transfer velocity and mesh-following gas carrier
motion. It supports a fixed liquid bath or an insulated column.

### Moving water interface with solved channel flow

Open the evaporation or condensation experiment under
`data/chemistry/water_interface_flow_*_v1.json` using the phase property
library. The displayed horizontal axis is across the gap, and the rows
are stations along the periodic channel. A quasi-steady Stokes solve
updates tangential gas velocity from the current gap and specified drive.
The readout includes gap Reynolds number. Saved reports expose the
velocity profile, drive power, viscous power and cumulative drive work
under `channel_flow`, with water and energy residuals under
`water_interface`.

### Moving water surfaces with solved liquid momentum

Open data/chemistry/water_momentum_v1.json with the phase property library.
The two rows show the connected liquid arms and their separate moving gas
meshes. Positive liquid flow is from the upper row to the lower row.
The hydraulic report includes both surface velocities, liquid flow,
accumulated drag heat, phase transfer, and water and total energy residuals.

### Dynamic gas pressure and a deforming water surface

Open data/chemistry/water_surface_pressure_v1.json with the phase property
library. Each row is one connected liquid surface patch and its own sealed
gas mesh. Run or step to inspect the surface span and pressure range.
The saved hydraulic report includes per-patch pressure and phase transfers,
face flows and integrated transfers, and the total water and energy balances.

### Two-fluid capillary–gravity interface

Open data/chemistry/capillary_interface_v1.json. The benchmark displays
the displaced water and gas regions and reports the capillary pressure
jump residual, surface energy and total energy residual. Saved reports
include interface height, velocity and pressures, plus sampled gas and
liquid mid-depth velocity and dynamic pressure. The benchmark uses
linear inviscid mechanics with no phase crossing.

### Resolved phase change in compressible vapor

Open either stefan_interface experiment under data/chemistry with the
water phase property library. The workspace displays the moving surface,
vapor pressure, temperature and momentum residual. Saved reports include
per-cell gas pressure and velocity, phase transfer, interface traction,
wall impulse, and the closed mass and energy ledgers.

### Spherical droplet phase change and capillarity

Open a spherical_droplet experiment under data/chemistry with the water
phase property library. The horizontal coordinate is radius. The workspace
shows the interface position and vapor pressure. The saved droplet report
includes radius, area, Laplace pressure, curved saturation pressure,
surface energy and total energy.

### Humid-air exchange and settling

Open a humid_droplet fixture to inspect radius, temperature and the
radial vapor profile. The exterior has prescribed humidity and
temperature, and the saved report tracks mass and energy exchanged with
it. Open settling_droplet_v1.json for the same exchange model with one
droplet falling in still air. Its saved report includes displacement,
velocity and the momentum and mechanical energy ledgers.

### Droplets coupled to solved moist gas

Open a droplet_carrier fixture with the water phase property library.
The field shows vapor, dry-air concentration and gas temperature. The
saved report includes each parcel's count, radius, temperature and
velocity, the solved gas pressure and velocity, and the combined
water, dry-air, momentum and energy residuals.

### Droplet exchange width and wall capture

Open droplet_deposition_wall_v1.json with the water phase property
library. The report separates the airborne parcel from the wall
inventory after capture. It includes capture time, deposited water,
impact heat and signed wall impulses.

### Wet wall evaporation and condensation

Open either wetted_wall fixture with the water phase property library.
The saved report includes the captured water inventory, post-impact
vapor flux, liquid temperature, and the spherical cap footprint and
surface energy. The contact angle is prescribed in the experiment.

### Dynamic wetting on a captured droplet

Open data/chemistry/dynamic_wetting_v1.json with the water phase property
library. After contact the native readouts show the instantaneous angle,
contact radius and accumulated heat released by contact-line motion.
The saved report also includes the contact-line speed, footprint and
vapor flux. The fixture configures equilibrium angle, mobility and the
optional pinning range.

The RC7 lateral-film fixture `data/chemistry/wall_film_v1.json` uses the water
interface library and a periodic prewetted strip. The workspace shows peak
film height and water and energy residuals. The liquid species slice reports
inventory per display cell volume. Saved `wall_film` reports include film
height, liquid temperature, evaporated and condensed water, and viscous heat.

Load `data/chemistry/film_capture_v1.json` with the same water interface library
to run airborne flight, wet-wall capture, spreading and re-evaporation together.
The saved `film_capture` report includes captured mass and energy, impact time,
wall impulse and combined residuals. The wall-film slice displays the film
inventory, while the total liquid readout also includes any airborne water.

The `data/chemistry/normal_film_capture_v1.json` fixture adds two-way normal
drag and a stationary-wall momentum ledger. The saved `normal_momentum`
report separates gas momentum, parcel momentum, impact and wall traction
impulses, and drag and phase contributions. The gas normal velocity is
depth-averaged on the existing along-wall mesh.
