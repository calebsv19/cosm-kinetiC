# Useful four-interval pressure reconstruction diagnostic

2026-10-04, PhysicsSim Main Edit. The candidate pressure trace uses exact cubic
volume-average weights(25,-23,13,-3)/12 on four adjacent fluid intervals. Five
support controls pass: all20 cubic monomials against independent analytic body-face
integrals in all three directions and three spacings, closed-body pressure gauge,
a square-root counterexample retaining half-order bias, literal source transforms,
actual native binary layout/readback and malformed-length rejection. The controls
are grouped into three polynomial/transform and two native-readback tests.

The calibrated DG-P3 clipping/integration is unchanged. Two immutable graded L4
observations finish within1024MiB/180s: n16 4.978s/441.516MiB, n32
3.923s/449.594MiB. Reference full residual, flux/divergence, energy, mesh/DOF identity,
original source and input hashes remain checked. No PDE solve or field replacement.

| Native grid | Previous reconstruction discrepancy | Candidate discrepancy | Relative reduction |
|---|---:|---:|---:|
|32x16x16|5.7226%|4.9302%|13.85%|
|64x32x32|3.5620%|3.0756%|13.66%|

Both pass the predeclared10% relative reduction criterion for diagnostic selection.
This is not a new physical acceptance threshold. The raw reference pressure force
is0.02478519977034N and the reference surface/reaction mismatch still exceeds1%.

Applying the candidate to matched archived native pressure, with actual complete
binary layout including outlet velocity tail, changes total pressure-force
difference from-17.3408% to-16.8288%(coarser) and-8.3293% to-8.0283%(finer).
This smaller improvement shows that reconstruction alone does not repair the native
pressure-force discrepancy. Signed reconstruction and field-functional contributions
sum exactly to the total; none is a complete pressure-field error norm.

The saved-field experimental CLI is now usable for this diagnostic:

```sh
PYTHONDONTWRITEBYTECODE=1 OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 \
build/cfd-reference-venv/bin/python scripts/run_cfd_reference3d_four_interval.py \
  --name NEXT-UNIQUE-DIAGNOSTIC -- --input-receipt PATH-TO-ACCEPTED-GRADED-RECEIPT \
  --n 32
```

Use a fresh name and an immutable accepted graded cube receipt. This is a local
reference diagnostic, not a native option or public untrusted endpoint.
`build/c3d-pressure-four-interval/assessment.json` seals observations and
`readback-support.json` seals complete-layout checks. Native physical equations,
defaults and protected worker remain unchanged. Pressure/operator known-answer
work and reference stress convergence continue before native physical adoption.
