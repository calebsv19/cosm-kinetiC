# Component-force and obstacle-energy qualification

This continuation establishes automated per-run numerical checks and bounded
masked-energy acceptance. The 2% separate pressure/viscous force gate still
fails. The bounded 3D channel slice has not started; no 3D accuracy is implied.

## What changed

`cfd_open2d_energy` now evaluates fluid-cell kinetic energy and strain dissipation,
plus signed inlet/outlet pressure work, viscous work and kinetic-energy flux.
Solid cells are excluded. Cross derivatives adjacent to a stationary no-slip
wall use a quadratic ghost reconstruction rather than treating the solid cell's
zero velocity as a sample at a full-cell distance. Stationary body work is zero.
The older whole-domain momentum budget still rejects masks: this energy API does
not silently turn its incomplete body-momentum accounting into a valid budget.

The session worker retains one-step kinetic-energy change, publishes the energy
terms/residual, and includes them in bounded history. `run_assess` separates
energy, conservation, steady velocity, force consistency and refinement checks.
See [per-run acceptance](cfd_run_acceptance.md).

Pressure projection reuses the previous physical pressure, scaled by the new
step's dt/rho, as an initial guess. It still solves the new residual and checks
true residual and divergence. Warm/cold comparison, including a timestep change,
gives maximum velocity difference 1.12e-11 m/s and reduces accumulated iterations
from 72,967 to 49,159 in the tested case. Nonfinite guesses are rejected.

## Masked energy acceptance

An analytic two-slot parabolic field separated by a solid band recovers the
expected dissipation quadrature error exactly at 16/32/64. This is an observation
calibration, not a new runtime geometry preset. The actual confined-rectangle
case then gives:

| Grid | Steady relative energy residual | Transient integrated residual |
|---|---:|---:|
| 32² | 2.163% | 2.362% |
| 64² | .980% | 1.082% |
| 128², verification only | .473% | not run |

The transient window is **0.1 to 1.1 seconds**, excluding the initial clipped-field
projection impulse. Halving dt from .001 to .0005 gives 1.077% transient residual
and .980% steady residual. Both finest-grid checks pass the unchanged 2% gate;
32² does not. This is one stationary low-Re rectangle, not general energy
certification for arbitrary masks, startup discontinuities or moving walls.

## Why component accuracy is still open

The earlier uniformly refined reference converged total reaction much faster
than individual pressure and viscous contributions. The independent P2/P1 Stokes
reference now refines geometrically near all four re-entrant fluid corners.
Increasing local refinement from 8 to 11 levels changes pressure by .183% and
viscous force by .295%. Its finest values are:

- total reaction: .005980728624 N;
- pressure contribution: .003633153362 N;
- viscous contribution: .002347529941 N.

The same known outlet-formulation difference remains: FEM natural traction and
MAC pressure outlet are not claimed formally equivalent. Prior domain-extension
screening applies to total drag, not a universal component equivalence theorem.

| MAC grid | Pressure error | Viscous error | Total-force error |
|---|---:|---:|---:|
| 16² | 22.15% | 17.60% | 6.54% |
| 32² | 13.40% | 13.60% | 2.80% |
| 64² | 8.48% | 9.26% | 1.52% |
| 128², verification only | 5.58% | 6.07% | 1.01% |

Pressure is underestimated and viscous force overestimated, partly cancelling
in total drag. Halving dt at 64² changes either component by less than 1e-9
relative, while grid refinement changes them substantially. This identifies
spatial resolution/treatment, rather than timestep size, as the dominant measured
component error in this case. It does not prove every spatial contribution has
been isolated. The reference's sensitivity to corner refinement points to corner
stress resolution as the next specific target.

A quadratic body-wall diffusion probe was rejected: at 32² it worsened the
surface/control-volume comparison and energy residual despite improving some
individual contributions. The default boundary formula was preserved. The probe
source and results are retained only under the evidence build directory.

The runtime/agent grid cap remains 64 per axis. A compile-time verification-only
128 cap produced the extra refinement point; it does not expand supported agent
requests or the steady-monitor storage bound.

## Reproduction and next boundary

- `make test-cfd-run-acceptance`
- `make test-cfd-masked-energy test-cfd-masked-energy-transient`
- `make test-cfd-pressure-guess`
- `make test-cfd-open2d-reference`
- `build/cfd_open2d_reference_test 64 .002 4 .5` (half timestep)
- `build/cfd-reference-venv/bin/python scripts/cfd_fem_reference.py --grids 64 --corner-levels 11 --output build/s3-component-energy/stokes-corners11.json`
- `python3 scripts/assess_cfd_component_energy.py build/s3-component-energy`

The final report preserves `component_force_gate.status: failed` separately from
passing evidence-integrity, energy and total-force checks. Report-generation
success is not component qualification. Evidence: `build/s3-component-energy/`.

The next numerical boundary is a corner-resolved body treatment: measure local
surface contributions against the refined reference, then validate local spatial
refinement or a corner-aware traction reconstruction. Do not fit a force scale
factor or relax the 2% gate. A bounded 3D channel can follow the agreed 2D gates;
this continuation does not declare those gates all closed.
