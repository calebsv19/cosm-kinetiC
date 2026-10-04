# Graded reference pair improves physical force balance

2026-10-04, PhysicsSim Main Edit. Accuracy takes priority over speed. The declared
profile halves first Y/Z fluid-layer spacing0.0625→0.03125m and bisects remaining
axis intervals longer than0.4m, retaining actual cube triangles, boundaries, fluid
volumes, reflected cells/YZ exchange and matched translated inner cells. Neither
uniform refinement nor original tetrahedron nesting is claimed.

Global worst intrinsic shape/Jacobian condition remain21.450/107.872(L4) and
42.442/181.230(L8). Volume-weighted shape improves5.145→3.287 and8.600→4.831.
Local quality changes remain reported diagnostics; these do not certify force error.
Both meshes are archived and rebuilt bitwise before numerical use.

A separate predeclared120000tet/8192MiB/1800s/3000iteration contract on the verified
16GiB host admits the full original P4/DG-P3 equations and complete Float Cholesky PC.
Only resource/count guards and module routing change. The coefficient-count guard
increases50million→120million; measured L4 encoding needs59,050,872coefficients.
All original Float64 bits/actions, pressure modes, quadrature, pressure10/complement10,
restart6/bothVZ reservations, live factor/scratch/work+32MiB accounting, owner retirement,
strict full1e-10/retained1e-11 and atomic publication remain. No speed/savings gate.

| Accepted field | L4 | L8 |
|---|---:|---:|
| Tetrahedra |81792|100608|
| Iterations |80|120|
| Whole seconds |665.482|1039.367|
| Owned / sampled MiB |4648.766 /4646.891|6240.922 /6220.703|
| Independent full FE residual |9.046e-12|8.995e-12|
| Maximum divergence /s |7.633e-10|1.497e-9|
| Relative physical energy imbalance |4.004e-11|3.663e-11|
| Pressure-force change versus finer parent |0.0560%|0.2085%|
| Raw viscous-force change |0.6642%|0.6188%|
| Reaction-force change |0.1169%|0.2323%|
| Inlet-pressure / dissipation change |0.0813% /0.0805%|0.1340% /0.1341%|
| Raw surface/reaction mismatch |1.4037%|1.4066%|

Raw force mismatch improves from1.7215/1.7265%, about18.5% reduction. All separate
force and fixed-length scalar changes stay below1%. Graded-pair domain changes are
pressure0.00563%, viscous0.000801%, reaction0.00626%. These support selection of this
pair for further reference investigation, while original raw/reaction1% remains
failed. No physical qualification, native default, canonical or package adoption.
Length-dependent pressure/dissipation responses remain reported under the corrected
original domain interpretation; per-run energy checks remain strict.

Six support controls prove unchanged scalar/triangle algebra, nonzero eliminated
load reconstruction/full FE action, strict physical/resource boundaries, atomic
postserialization rejection, saved matched geometries, explicit coefficient limits
and literal source transforms. Frozen receipts/snapshots, geometry and comparisons
are under `build/c3d-accuracy-graded/`; its once-only audit seals both fields.

## Backend repair and next physical work

The larger accuracy adapter added resource caps to admission metadata. The original
rejection constructor expanded that metadata alongside explicit cap keywords,
causing a duplicate-key exception on oversized-factor rejection. A separate guarded
entrypoint corrects only record construction. Three controls reproduce the original
exception, prove a structured resource stop with unchanged complete estimate, and
prove identical successful-admission progress. Numerical sources that generated
these fields remain frozen and unchanged. Future entrypoint:

```sh
PYTHONDONTWRITEBYTECODE=1 OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 \
build/cfd-reference-venv/bin/python scripts/run_cfd_reference3d_accuracy_graded_guarded.py \
  --name NEXT-UNIQUE-CASE -- \
  --geometry build/c3d-accuracy-graded/paired-graded-geometry.npz --length 4
```

This is an optional local reference CLI, not native/desktop activation or remote
submission. Guard proof is under `build/c3d-accuracy-admission-guard/`.

The [calibrated cubic pressure projection](cfd_3d_cubic_projection_checkpoint.md)
provides the next independent native diagnostic. Continue reference near-edge
stress convergence and known-answer native pressure/operator/reconstruction controls.
The native investigation can proceed through independent tests while final physical
adoption still requires its original force/energy/domain criteria. Reusable objects
and physical transient/inertial transport remain later explicit model work. Steady
Stokes results do not qualify wakes, moving bodies, turbulence or free surfaces.
