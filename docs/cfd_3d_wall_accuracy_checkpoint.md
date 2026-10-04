# Native cube-wall accuracy and pressure reconstruction checkpoint

2026-10-04, PhysicsSim Main Edit. Accuracy-first local source work; no native
operator/default, package, install or version change. The original pressure-driven
cube reference's raw surface/reaction 1% gate remains open.

## Independently forced wall case

The new case produces nonzero tangential shear on the cube front face while its
support stays away from the cube edges. In the same 4x2x2 m tunnel with the unit
cube at [1.5,2.5]x[0.5,1.5]x[0.5,1.5] m, use
`u=(psi_y,-psi_x,0)` and compact separable pressure. Streamfunction amplitude is
0.0001 m²/s, pressure amplitude 0.001 Pa and viscosity 0.1 Pa s. The X factor is
45.5625*t^4*(1-t)^2 on [0.25,1.5] m; Y/Z factors are 256*t^4*(1-t)^4 on
[0.625,1.375] m. Velocity vanishes at all physical boundaries; its front-face
one-sided tangential derivative is nonzero. All open-end traction is zero.

The actual native solve is forced by independently integrated `-mu*Delta(u)+grad(p)`
over velocity dual volumes. No native matrix action generates the forcing. Exact
face-area velocity and cell-volume pressure averages supply the volume-weighted
relative L2 errors. The physical pressure zero is fixed, with no fitted gauge.
Eighty-four C factor-integral samples, including one-sided endpoint derivatives,
are checked by independent unexpanded Leibniz derivatives and six-point Gauss
integration; maximum difference3.041e-9 is below the declared2e-8 threshold.
Analytic pressure force is zero; analytic viscous force is
[0,5.416751020408e-5,0] N. Force errors below use this nonzero load as their scale,
including the absolute error of the zero pressure load.

| Grid | Velocity error | Pressure error |
| --- | ---: | ---: |
| 16x8x8 | 71.1719% | 15.8106% |
| 32x16x16 | 16.8057% | 5.6134% |
| 64x32x32 | 3.97196% | 1.47406% |
| 128x64x64 | 0.990184% | 0.378642% |
| 160x80x80 | 0.633972% | 0.243373% |

At n64, velocity meets1% but pressure misses0.3%. The n80 field meets both original
known-answer field targets, preserves at least order1.8 across the non-dyadic
64-to80 refinement, and gives current total force error0.634810%. Momentum residual
2.301e-14 and maximum divergence1.440e-14 meet the unchanged numerical limits.
Its312.564 s,976142812 B owned allocation and1068728320 B sampled child RSS fit
600 s/1 GiB owned/1.5 GiB RSS supervision.

A separately attempted256x128x128 grid is rejected by the existing native1048576-cell
admission limit before a PDE solve. The failed receipt is retained. The larger test had its own declared4GiB/1800s allowance, but never passed grid
admission. No production grid or resource limit is raised. The supported
160x80x80 grid uses1024000 cells under1GiB/600s bounds. The first
four wall fields and two repeated n32/n64 pressure-comparison fields complete;
n80 is the fifth distinct physical resolution.

## Select and reject candidates using solved fields

A test-only quadratic interval-average wall derivative improves traction on exact
prescribed fields but worsens traction on solved fields. At n64 its viscous error
changes9.6312→0.7130% on prescribed fields, yet0.3393→13.5684% on solved fields.
It fails the prospective5% solved-force and improvement criteria and is rejected.
It remains frozen evidence, not an integrated native observer.

Local polynomial checks expose why operator and observer consistency need joint
validation. At the first tangential cube-wall row, quadratic velocity
`u_t=a*s+b*s²` has native momentum-density defect `(2/3)*mu*b` for physical interval
means, versus exact forcing density `-2*mu*b`. Linear wall rows and interior
quadratic rows are exact to roundoff; maximum prediction error is below1e-15 over
three grids and18 controls. Point samples also retain a `.5*mu*b` local defect.
This local truncation defect does not prove global nonconvergence: the full fields
converge at approximately second order. It also does not justify an observer-only
correction or a change to forcing/field semantics merely to fit the operator.

The calibrated four-interval pressure trace `(25,-23,13,-3)/12` is more useful.
The actual C candidate includes the original three/two-interval fallback when
four intervals are unavailable. At n64, solved pressure-force error changes
1.051884→0.520303% of the analytic wall-load scale, and prescribed error changes
0.608068→0.076109%. It meets the prospective next-diagnostic selection rule.
At n80, using this pressure candidate with the current viscous observer reduces
total force error0.634810→0.374703%. The rejected viscosity candidate is not used
in this total-force comparison.

The actual C pressure candidate passes940 individual patch controls: cubic,
quadratic and linear normal dependence with four, three and two available
intervals, respectively, plus a pressure offset. Maximum patch/gauge-shift errors
are5.56e-17/6.94e-17 N. A square-root pressure counterexample retains half-order
convergence, error ratio0.70710678 under halving for both original and candidate;
no false high-order claim is made at singular edges. These results and the prior
archived-reference projection support retaining the pressure candidate for further
physical diagnostics. Native defaults remain unchanged until broader force gates
are satisfied.

## Reproducible developer operation

These optional commands use the source checkout with clang and NumPy, separate
from public first-start headless proofs. Each run needs a new name and freezes
source inputs and artifacts. Investigation receipts distinguish measured errors
and candidate decisions from merely completing a solve.

```sh
PYTHONDONTWRITEBYTECODE=1 OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 \
  build/cfd-reference-venv/bin/python scripts/run_cfd_native_wall_shear.py \
  --name my-new-wall-series

PYTHONDONTWRITEBYTECODE=1 OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 \
  build/cfd-reference-venv/bin/python scripts/run_cfd_native_wall_pressure.py \
  --name my-new-pressure-comparison

PYTHONDONTWRITEBYTECODE=1 OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 \
  build/cfd-reference-venv/bin/python scripts/run_cfd_native_wall_shear_supported.py \
  --name my-new-supported-wall-check
```

The separately supported n80 runner compares against the sealed n64 pressure case.
The first two runners are investigations: exit0 denotes completion, with physical
flags in the receipt. For explicit physical assessment, run
`build/cfd-reference-venv/bin/python scripts/assess_cfd_native_wall_accuracy.py <receipt>`.
The assessor verifies frozen inputs/artifact hashes, numerical/resource limits and
field/total-force accuracy. Exit0 means this known-answer case passes; exit2 reports
failed physical targets or invalid evidence. Fresh checks confirm n80 passes and
n64 correctly reports its pressure/total-force misses. This is a read-only command.

Local stencil and pressure-patch controls are available in
`scripts/check_cfd_native_wall_consistency.py` and
`scripts/check_cfd_native_wall_pressure_calibration.py`; their evidence roots are
created once per source checkpoint. Shared reuse is the existing native fixtures,
observer and Python supervisor. This app-specific numerical verification adds no
cross-app runtime API or shared module/version change.

## Next physical development

1. Preserve the passing nonzero-wall-shear case as a regression alongside the
   interior known answer. Any wall-operator/traction change must improve solved
   fields and forces together while retaining exact forcing and field semantics.
2. Test the retained pressure candidate on finer pressure-driven native cube fields
   with the same tunnel/body/flow, while investigating reference edge convergence.
   Measure separate pressure, viscous and total forces, full residuals, conservation
   and physical energy; keep reference uncertainty visible. No final native cube
   qualification before the independent reference's original physical gates pass.
3. Continue directional normal/tangential sharp-edge refinement on the retained
   graded reference. The local-refinement failure and native wall truncation defect
   are different observations; do not attribute the reference's FE gap to the
   native stencil. Neither a reaction force substitution nor a relaxed residual
   or1% raw-equilibrium target is an acceptable fix.
4. Then qualify reusable stationary box scenes and physical transient/inertial
   transport before curved/moving objects, wakes, turbulence or free-surface work.
   These steady, independently forced fields do not qualify those behaviors.

Authoritative receipt roots: `build/c3d-wall-shear`, `build/c3d-wall-pressure`,
`build/c3d-wall-consistency`, `build/c3d-wall-pressure-calibration` and
`build/c3d-wall-shear-supported`. Their linked batch seal is
`build/c3d-wall-shear/checkpoint-audit.json`. The failed admission is retained in
`build/c3d-wall-shear-fine`. See the preceding
[local cube checkpoint](cfd_3d_local_accuracy_checkpoint.md) for the unchanged
1.4037% best reference mismatch and rejected1.8312% local candidate.
