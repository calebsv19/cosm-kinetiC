# Unforced 3D accuracy and independent cube refinement

Main Edit local source checkpoint, 2026-10-04. Physical accuracy takes priority
before runtime optimization. The persistent general CFD goal remains incomplete.

## Integrated unforced periodic Navier-Stokes

`cfd_periodic3d_init_unforced` accepts caller-owned lower-face initial velocities
on the existing uniform periodic Cartesian grid. It validates count, finite values
and divergence, copies the initial field and marks the solver unforced. Initial
pressure is zero; subsequent pressure is solved in Pa. The original BE/BDF2,
conservative shared-face advection, implicit viscous momentum, pressure solve,
complete momentum residual and failure publication controls are reused. Analytic
error fields are NaN in this mode; accuracy is assessed outside the solver. This
is a backend API, not an exposed scene/agent mode or arbitrary wall/open solver.

The independent fixture initializes ABC/Beltrami velocity at amplitude 0.2 m/s on
a 2 m periodic cube, density 1 kg/m3 and viscosity 0.05 Pa s, then solves without any
body force. For k=pi/m, a(t)=a0 exp(-nu k^2 t); pressure is the zero-mean
-rho|u|^2/2, energy 1.5 rho a^2 L^3 and dissipation 3 mu k^2 a^2 L^3. The
[viscous Beltrami decay identity](https://www.cambridge.org/core/journals/journal-of-fluid-mechanics/article/some-topological-aspects-of-fluid-dynamics/E139B6426152FE1971F49427533A9301)
supports this exact unforced Navier-Stokes benchmark. The test implements its
continuous fields independently and does not feed prescribed final fields into
the solver.

At t = 0.2 s, dt = 0.001 s:

| Grid | Velocity error | Pressure error | Energy error | Physical dissipation error |
|---|---:|---:|---:|---:|
| 8 cubed | 0.49827% | 13.79197% | 0.99903% | 18.13327% |
| 16 cubed | 0.12628% | 3.56297% | 0.25272% | 4.79588% |
| 32 cubed | 0.03169% | 0.89800% | 0.06339% | 1.21594% |

All four diagnostics converge about second order. The finer dissipation observer
still underestimates the exact value by 1.21594%; this is a measured remaining
error, not a sub-1% pass. Fixed-grid temporal velocity errors against independently
derived discrete Fourier decay decrease by 4.01345 and4.00661 when dt halves
0.02→0.01→0.005s. This separates spatial from temporal error and does not change
the continuous-field accuracy comparison.

At every step, original complete momentum residual 1e-11 and divergence 1e-8/s
pass; largest measured values 4.29643e-12 and 1.50991e-13/s across ordinary and sanitized
cases. Forcing and forcing power are exactly zero; energy decreases; global
momentum, shared-face transport sums and advective work pass 1e-10 absolute
checks. Owner memory peaks 39,735,312 bytes, with no new allocations after step 2
and all owned allocations released. Count, nonfinite input, nonzero divergence,
low-memory rejection and CFL failure publication checks pass. Sanitizers pass.

The original manufactured 16 cubed dt = 0.001 s case retains bitwise-identical complete
velocity/pressure output (131072 bytes, SHA256
`064694736eb4558b152a1d4d992852d6df1437cd519528658df4724b089a2561`)
and identical numerical/physical controls. Affected ordinary and sanitized 3D
session regressions pass. Protected worker/package state is unchanged.

Developer commands from this source checkout:

```bash
make test-cfd-periodic3d-unforced
make test-cfd-periodic3d-unforced-sanitize
make test-cfd-obstacle3d-material-scaling
make test-cfd-obstacle3d-material-scaling-sanitize
PYTHONDONTWRITEBYTECODE=1 OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 \
  build/cfd-reference-venv/bin/python scripts/run_cfd_periodic3d_unforced.py --name NEW_NAME
```

The named runner requires the existing NumPy-capable developer environment and
clang. It freezes sources, bounds compile 60 s/run 600 s/sampled RSS 512 MiB, retains
logs plus success/failure receipt, and never overwrites a run name. The passing
frozen packet is `build/c3d-unforced-periodic/runs/initial-accuracy/receipt.json`.

## Material, imposed-flow and density controls

Nine complete native steady-Stokes cube cases vary viscosity, imposed flow and
density independently and together. Velocity follows Q; pressure, original and
optional pressure loads and original viscous force follow mu*Q; physical power
and dissipation follow mu*Q^2. Density independence is expected because this
stationary Stokes model has no inertia. Whole fields and SI scalar/load controls
pass 1e-8 scaling limits, maximum relative error 2.36556e-12 ordinary/sanitized.
Original residual, flux, divergence and discrete balance limits remain. These
are physical law regression checks, not proof of absolute cube force accuracy.
Receipt: `build/c3d-material-scaling/receipt.json`.

## Actual cube reference results

Both new L4 fields retain the original guarded P4/DG-P3 Stokes equations,
pressure modes, complete residual, flux/divergence, energy and resource gates.

| Trial vs retained matched-edge047 | Raw surface/reaction mismatch | New/old mismatch | Decision |
|---|---:|---:|---|
| Retained paired L4 reference | 1.19511849% | 1 | Best existing paired reference |
| Surface-edge interval 0.03125 m | 1.41138701% | 1.18096 | Worse; rejected, no L8 |
| First Y/Z normal interval 0.0234375 m | 1.13430390% | 0.949114 | Modest improvement; misses prospective 10% improvement, no L8 |

All separate force and fixed-domain pressure/dissipation changes are below 1%.
Original raw 1% still fails, so native physical certification remains open. The
side-normal trial passes fullFE residual 1.00146e-11 (original target 1e-10),
flux 9.79685e-12, maximum divergence 1.04482e-9/s and energy imbalance 1.60437e-11;
616.023 s/4621 MiB under the declared 1800 s/8192 MiB reference allowance.

The side-normal geometry experiment prospectively permits additional anisotropy
(up to 2.1 parent shape/conditioning ratio and absolute shape 90/condition 400),
while retaining positive volume, exact body planes/areas, reflection/YZ symmetry,
matched actual inner cells and 120000 tetrahedron limits. It does not reinterpret prior
geometry rejections. Side023 passes; half-normal side016 fails geometry and has
no field solve. Speed is not a selection criterion.

Independent prescribed quartic-velocity/cubic-pressure traction calibration on
the actual paired edge031 and side023 meshes passes all six individual body
faces, two pressure gauges and two quadrature orders. This establishes smooth
observer consistency, not singular solved cube stresses. Side-normal physical
assessment: `build/c3d-side-normal-field/L4-physical-assessment.json`.

## Next physical development sequence

1. Extend unforced qualification to material/time scales, anisotropic grids and
   a non-Beltrami field that transfers energy between modes; keep conservation
   and temporal/spatial errors separate. Diagnose the physical strain observer.
2. Resolve cube edge/normal stress error using the recorded directional results,
   exact face-load controls and local mesh conditioning. Require an independently
   qualified reference before claiming native absolute force accuracy.
3. Generalize stationary grid-aligned bodies through measured aspect-ratio,
   displacement/reflection and matched-domain tests. Pressure diagnostic tests on
   a rectangular body do not already qualify flow around it.
4. Add transient inertia/advection to wall/open and obstacle equations through
   the full coupled boundary-aware momentum/continuity system. The exact periodic
   pressure commutation argument does not apply to these boundaries.
5. Qualify smooth/curved shapes and genuine wake behavior with independent
   references and domain, space and time convergence before broad wind-tunnel use.
6. Expose qualified capabilities and limits through scene/agent diagnostics,
   including separate force components, Reynolds/CFL, conservation and convergence.

This source slice reuses existing CFD operators, allocation and frozen supervision.
App-specific physical validation remains local; shared core/kit APIs and versions
are unchanged. No commit, package, install, default-force switch or general CFD
certification is performed.
