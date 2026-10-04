# CFD agent laboratory

This source-checkout laboratory uses the existing local session worker and eleven
MCP tools documented in [agent_session.md](agent_session.md). It is 2D, laminar,
incompressible and exploratory. It is not a qualified 3D wind tunnel.

## Presets and units

`cfd_channel_2d` provides the existing periodic channel verification slice.
`cfd_obstacle_2d` adds one stationary rectangular solid in a periodic channel.
Default domain dimensions are [4, 2, .5] metres and the rectangle bounds are
[1.5, .75, 2.5, 1.25] metres, ordered xmin, ymin, xmax, ymax. Override with
`scene_create.channel.obstacle_bounds_m`. Bounds must align exactly with the
chosen grid and retain two fluid cells of padding; validation rejects silent
snapping. Obstacles initialize at rest and reject divergence perturbations.
Use grid [16,16,1] or [32,32,1] for the default geometry.

Specify `fluid.density_kg_m3` and `fluid.dynamic_viscosity_pa_s` on validation
and start. Default drive is .01 Pa/m, applied as a body force in periodic flow.
Obstacle pressure fields contain periodic correction pressure; they do not
pretend a fictitious affine pressure is a physical inlet/outlet pressure drop.

## Lifecycle and diagnostics

Create an immutable scene, retain its revision, validate, then start paused.
Poll out of `starting` before sampling or controlling. `run_sample` supports
solid-mask slices and probes. `run_control` supports step, continue, pause and
cancel; retain command IDs when collecting pending receipts.

`run_inspect` exposes `boundary_force_budget` and momentum/divergence health.
With `history: true`, bounded history includes force components. The nested
`control_volume_comparison` reports reconstructed pressure and wall shear,
outer pressure/viscous traction, advective flux and body drive. After a step,
it adds momentum rate, inferred obstacle force and the surface/CV mismatch.
These are diagnostic estimates, not physical accuracy certificates. The
normal-strain reconstruction artifact is separately reported and excluded from
straight-wall physical traction. Step averages and instantaneous estimates must
not be interchanged when interpreting transient force balances.

`run_result` supplies terminal artifact paths and SHA-256 provenance; exported
staggered fields include `solid_cells`. Inspect completion before consuming them.

## Refinement comparisons

Call `run_compare` with two or three completed run IDs, ordered coarse to fine,
and `kind: spatial` (default) or `kind: temporal`. The scene revision, fluid,
solver model, worker binary and physical endpoint time must match. Spatial
comparisons require nested increasing grids and equal dt; temporal comparisons
require equal grids and decreasing dt. Relative changes use the finer value as
the denominator, with explicit null when it is zero and the difference is not.
No convergence order, steady state or reference error is inferred from endpoints.

## Qualification and repeatable proof

`make test-agent-cfd-lab` checks real worker lifecycle, mask sampling, live force
and conservation diagnostics, hashes, spatial/temporal comparison and rejection
of incompatible comparisons and misaligned geometry. Existing agent session,
MAC and reduced-channel regression tests remain required.

`build/s3-agent-cfd-lab/` records a bounded T=.2 s demonstration at 16/32 cells
and dt=.01/.005 s. These short runs demonstrate operability, not steady forces.
The corrected corner stencil passes the fixed-case 2% force-consistency threshold
(1.65% established reconstruction at 64²). This does not qualify drag for arbitrary
runs: live status continues to distinguish case consistency from reference drag
validation. The higher-order reconstruction remains diagnostic only.
The periodic higher-order reconstruction remains diagnostic only; the open model below uses its separately calibrated reconstruction.

## Open inlet/outlet agent presets

`cfd_open_channel_2d` and `cfd_open_obstacle_2d` select
`incompressible_open2d_v1`. They use physical pressure in Pa with a zero outlet
datum, distinct inlet/outlet velocity faces, and MC-limited momentum transport.
Set `channel.inlet_mean_m_s` (defaults .05 for the channel, .002 for the obstacle),
and explicit SI density and dynamic viscosity. The obstacle default geometry is
the same rectangle described above. Open obstacle bounds require three fluid
cells of padding for the calibrated surface reconstruction and exact grid
alignment. Unsafe initial timesteps and height Reynolds numbers above 100 are
rejected; this limit is a scope guard, not an accuracy certificate.

Validation supplies effective grid and nominal voxel size; snapshots additionally
provide the full three-axis cell spacing. Startup flux and divergence are measured
from the actual initial fields, even while paused before the first projection.
The clipped obstacle initial condition can therefore truthfully report nonzero
divergence and `projection_status: not_started`.

Live `boundary_force_budget` reports separate pressure, viscous and total forces,
plus the surrounding control-volume estimate and its retained one-step momentum
change. Histories preserve those measurements. `run_compare` accepts compatible
open-model runs and reports force sensitivity. Terminal
`physics_sim_open2d_fields_v1` exports contain physical pressure, solid mask and
staggered velocities including distinct inlet and outlet faces.

Qualification is explicitly case-specific. One confined low-Re rectangle passes
a 2% total-drag independent reference screen. Individual pressure/viscous force
accuracy, masked energy, arbitrary scenes and 3D remain unqualified. Starting a
preset does not automatically qualify that run or establish steady state.

`make test-agent-open2d` verifies validation, paused readbacks, step/continue,
pressure and mask sampling, histories, hashed exports and spatial/temporal
comparison. The full five-target agent regression suite passes 19 tests.
`build/s3-open-agent-lab/` contains three completed T=.2 s demonstration runs,
samples, histories, result manifests and refinement comparisons. These short
runs prove agent operability, not settled drag.

## Automated run checks

Use `run_assess` for separate numerical readiness gates. See
[per-run acceptance](cfd_run_acceptance.md) and [component/energy evidence](cfd_component_energy.md).
