# Native cube force convergence and optional pressure diagnostic

Main Edit source checkpoint, 2026-10-04. Physical accuracy remains the priority.
This is local source evidence for pressure-driven steady Stokes flow around a
stationary unit cube in a 2 m by 2 m no-slip tunnel. It does not qualify arbitrary
objects, inertia, transient flow, curved surfaces, moving boundaries or turbulence.

## Native results

All four complete native fields pass fresh readback against the unchanged SI
momentum equation, divergence, original strict flux limit and discrete conservation.
The exported field includes every active velocity face, every fluid pressure cell,
and the outlet velocity plane. Pressure and viscous forces are assessed separately.

| Case | Pressure force N | Viscous force N | Inlet pressure Pa | Physical energy imbalance |
|---|---:|---:|---:|---:|
| L4, 64x32x32 | 0.0227207683 | 0.0142324250 | 0.0215614549 | 0.65763% |
| L4, 128x64x64 | 0.0236556274 | 0.0147043674 | 0.0220316788 | 0.31174% |
| L8, 256x64x64 | 0.0236564805 | 0.0147043501 | 0.0277175610 | 0.23894% |
| L4, 160x80x80 | 0.0238307490 | 0.0147632773 | 0.0220983288 | 0.23846% |

The fixed-length n64-to-n80 changes are pressure 0.74030%, viscous 0.40063%,
current total 0.61009%, inlet pressure 0.30252%, dissipation 0.22891%. All are below
1%, while n32-to-n64 fails that convergence screen. This is a successive-grid
comparison, not an extrapolated error bound. Native n64 body forces change by
less than0.0037% when tunnel length doubles. Inlet pressure and dissipation are
length-dependent and are not required to stay flat across domains.

Relative to the retained graded FE fields, n64 cases and L4 n80 meet the native
5% separate-force and3% scalar comparison limits plus2% physical momentum and3%
physical energy limits. At n80, current pressure error 3.85089%, viscous 3.31681%,
total raw-surface 1.22970%, total reaction 2.61614%, inlet-pressure 0.88009% and
physical-dissipation 0.64293%. The reference still misses raw surface/reaction 1%
(1.40370% L4 and1.40664% L8), so these comparisons are screening evidence only.
No native physical certification or default cutover follows from them.

The complete SI momentum residual ranges1.8448e-14 to6.0851e-14, maximum flux
error5.4002e-12. Fresh readback requires momentum1e-11, divergence1e-8,
flux1e-9, discrete momentum/energy1e-9 and transverse force1e-7N. The field
runner's diagnostic flux gate was1e-8; the stricter original qualification gate
is rechecked by the readback and physical assessor. Existing native1048576-cell
admission,1GiB owned numerical memory,1.5GiB sampled RSS and600s field/630s
parent limits remain. Resource caps bound execution; they are not accuracy claims.

## Integrated optional pressure diagnostic

`include/app/cfd_obstacle3d_pressure_trace.h` and
`src/app/cfd_obstacle3d_pressure_trace.c` add an explicit optional backend API.
It integrates pressure reconstructed from actual interval means on all six body
faces. Four available intervals reproduce a cubic trace; three/two retain the
previous lower-order fallback. The API reports separate side loads and counts
of patches at each stencil depth, validates relevant finite pressure and geometry,
requires two fluid intervals, allocates nothing and leaves output untouched on
failure. Solver equations, published default force fields and reference equations
remain unchanged. Source-list integration includes the helper in the existing
headless/backend source group; no worker binary or desktop package was rebuilt.

The production API passes 660 analytic physical-face controls, 26 failure controls,
closed-body gauge checks and ordinary plus ASan/UBSan runs. Maximum face-load
error4.6186e-14N and gauge error 4.3077e-14 N. All four saved pressure-driven fields
pass production API parity with the previously calibrated observer. Every original
full-field readback control remains identical, including forces and energy.
The original obstacle solver contract also passes with the new helper linked. A displaced
rectangular body on 48x16x32 anisotropic cells adds 240 exact face controls and 7
failure controls, ordinary and sanitized, confirming actual face areas and units.
These tests prescribe pressure; they do not qualify solved flow around this box.

At n80, optional pressure force is 0.0238712802 N, comparison error 3.68736% versus
current 3.85089%. It also improves streamwise momentum closure 0.67917→0.63332%.
The candidate is retained as a diagnostic; its streamwise closure does not establish
complete candidate wall-load balance in all components. Earlier known nonzero-wall
traction and940 patch controls remain separate immutable evidence. A square-root
counterexample retains half-order convergence; cubic exactness is not a cure for
cube-edge singularities. The current viscous observer remains in use.

Reuse decision: reuse the existing native solver, allocator, field fixtures,
readback and bounded supervision. Keep this app-specific finite-volume trace
policy local; a shared core abstraction is premature. No shared library API,
version, adoption, package, registry or release state changed.

## Reference geometry and field trial

Two genuine two-pass refinement profiles fail the new regional geometry screen.
The first profile exceeds the mesh cap in both lengths; the second also fails
symmetry/paired-inner-cell and regional shape/conditioning checks. Actual boundary
planes and areas remain intact; no PDE or factorization is run on rejected meshes.
Global-only quality checks had hidden deterioration close to cube edges.

Three directional tensor redistributions preserve boundaries, areas, volume,
reflections/YZ symmetry, worst global/regional shape and conditioning, with the
actual inner cell correspondence checked to 1e-12 m. The selected profile changes
front-normal axis spacing to 0.03125 m and first body-edge spacing to 0.046875 m.
It retains 81792/100608 tetrahedra for L4/L8; it is a redistribution, not uniform
refinement or the original tetrahedron partition.

Both selected original-equation P4/DG-P3 reference fields complete and are retained
as a useful physical improvement. L4 raw surface/reaction mismatch falls
1.40370→1.19512%; L8 falls1.40664→1.19960%, about15% improvement. Separate
pressure/viscous/reaction changes from the previous reference stay below 0.425%;
fixed-length pressure/dissipation changes stay below 0.040%. Paired body-force
changes stay below 0.0065%. Original raw equilibrium1% still fails in both lengths.
No force correction, gauge adjustment, pressure-mode removal or weak-reaction
replacement is applied.

Full original FE residuals are 6.8021e-12/1.0198e-11 against 1e-10, with retained
target 1e-11. Flux 1.6144e-11/2.4703e-12, maximum divergence 2.8961e-10/1.5701e-9
and physical energy 1.1525e-11/3.4883e-11 all pass. Both retain constant pressure
in the ten-direction pressure complement, unchanged original matrix action and
completed owner retirement. Fields finish 807.79/1183.48s at 4594.31/6182.78MiB
within the established 8 GiB/1800 s reference contract. These are resource receipts,
not speed or memory-efficiency gates.

Independent actual-mesh traction calibration passes eight prescribed-field controls
on both selected geometries, six faces/two pressure offsets/two integration orders,
maximum physical face-load error 3.997e-15 N. The updated native comparison against
this pair still passes n64 and n80 screens and rejects n32. At n80, current pressure
error 3.84961%, viscous 2.88023%, total raw-surface 1.38191%, total reaction 2.56052%,
inlet pressure 0.84087%, dissipation 0.60357%. Optional pressure error 3.68608%.
They remain comparisons against a reference whose raw force balance is unqualified.
Both earlier graded-reference comparisons and this updated assessment are retained.

Reference receipts are in
`build/c3d-directional-field/runs/6bc775b09447527ff4a7ea01c8a31ea31de3b75f403f4b7fb36f48caf24db8da/`;
physical assessments are `build/c3d-directional-field/L4-physical-assessment.json`
and `L8-physical-assessment.json`. Updated native comparison:
`build/c3d-native-cube-pressure/physical-directional-reference-assessment.json`.

Next: the 0.03125 m edge interval passes all paired geometry/quality checks;
0.0234375 m fails shape/conditioning and is excluded. Neither follow-up geometry
has a field yet. Test the accepted interval next on L4, then admit a paired L8
physical trial from measured results under a prospective criterion. Geometry
readiness is not physical accuracy. Preserve all original reference physical gates.

## Evidence and reproduction

Native source packet:
`build/c3d-native-cube-pressure/runs/50fe53d74510716a789dd4ee4836ac66e999262d790a58ad62b0f94814cf22b8/`.
Each case keeps a terminal `receipt.json`, complete `field.bin`, `readback/` and
`pressure-trace-api/`. Assessment: `build/c3d-native-cube-pressure/physical-assessment.json`.
Checkpoint audit: `build/c3d-native-cube-pressure/checkpoint-audit.json`.
All artifacts are local generated evidence, separate from source/package adoption.

From this Main Edit checkout, give every field run a new unique name:

```sh
PYTHONDONTWRITEBYTECODE=1 OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 \
build/cfd-reference-venv/bin/python scripts/run_cfd_native_cube_pressure.py \
  --name my-cube-n64 --n 64 --length 4

# Supply the receipt path printed by the field command.
build/cfd-reference-venv/bin/python scripts/check_cfd_native_cube_readback.py <receipt-path>
build/cfd-reference-venv/bin/python scripts/check_cfd_native_pressure_trace_api.py <receipt-path>
make test-cfd-obstacle3d-pressure-trace-api
make test-cfd-obstacle3d-pressure-trace-api-sanitize
make test-cfd-obstacle3d-pressure-trace-anisotropic
make test-cfd-obstacle3d-pressure-trace-anisotropic-sanitize
```

The receipt assessors are read-only over input evidence and write unique assessment
outputs. Geometry surveys and checkpoint audit use unique output roots and reject
existing output rather than overwrite earlier evidence. These are CFD developer
proof commands, not the public first-start workflow.

## Next major physical steps

1. Establish the independent cube reference: raw surface/reaction 1%, separate
   refinement/domain force 1%, fixed-length pressure/dissipation 1%, original full
   equations and conservation. Follow the measured directional trial, not more
   indiscriminate refinement. Regional geometry checks accompany global checks.
2. Reassess native pressure and viscous forces against a qualified reference.
   Preserve separate blocks so cancellation cannot make total force appear accurate.
   Promote useful observers only after matched cases and known-answer tests support it.
3. Add stationary rectangular bodies and physical scene/agent diagnostics, with
   units, boundary assumptions, convergence and supported-physics status explicit.
4. Qualify transient and inertial behavior: conservative transport, timestep
   refinement, startup flow, outlet/domain sensitivity and known vortex cases.
5. Extend geometry and boundary physics through independent curved/moving-body,
   water/free-surface and thermal cases where the model supports those behaviors.
6. Improve performance and wider product usability after physical evidence is
   established. Efficiency is secondary; resource limits remain explicit safeguards.

HEAD~1..HEAD, live changes, affected source/receipts, future_intent and bounded
MemDB project context were inspected. Unrelated dirty work is preserved. Existing
solver source, protected worker/audit and historical checkpoints remain unchanged;
only two bounded build additions and rolling docs change among preexisting files.
This checkpoint does not complete the persistent goal.
