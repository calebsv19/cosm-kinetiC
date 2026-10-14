# Variable-density hot-gas foundation

October 9, 2026 Main Edit development checkpoint. This is a separately built
native solver and private adapter; the existing coupled Fire/Fluid baseline is
unchanged. It is not yet an experimentally qualified pool-fire model.

`cfd_hotgas3d` evolves cell density, three momentum densities, and total energy
density with conservative, periodic XYZ finite-volume Euler fluxes. Pressure is
`(gamma - 1) * (total energy - kinetic energy)` and temperature is `p / (rho R)`.
The first-order Rusanov update admits positive finite states under a sound-speed
CFL bound of 0.4, including the source-heated candidate. Failed steps preserve
accepted state. Geometry and allocator reuse the existing application CFD modules;
Core Sim continues to own scheduling rather than the equation of state.

The current fixture uses constant `gamma=1.4` and `R=287 J/(kg K)`. Those are
calorically perfect air-surrogate properties, not measured methanol combustion
products. At equal pressure, the density ratio at 1290 K versus 300 K is 0.232558.
In a closed-volume heating test density stays constant and pressure rises instead.
Both behaviors are checked; temperature admission alone is not physical flame
validation. No gravity, viscosity, conduction, species, combustion, radiation,
solid burner or open reservoir boundary is implemented here. Acoustic Rusanov
diffusion also limits efficient low-Mach resolution.

Fresh source build, from the Physics Main Edit root:

```sh
python3 scripts/build_hotgas.py --output build/my-hotgas-proof/strict
HOTGAS_WORKER="$PWD/build/my-hotgas-proof/strict/physics_sim_hotgas_worker" \
  PYTHONDONTWRITEBYTECODE=1 python3 tests/test_hotgas.py -v
```

The build refuses an existing output directory, performs no cleanup or shared
rebuild, and records compiler/source/binary identity. Use `--sanitized` for
AddressSanitizer/UndefinedBehaviorSanitizer and `--harness` for the native
transactional-refusal harness. The private native protocol takes grid, dimensions,
gas properties, dt and step count as arguments; stdin is the five conserved
values per cell followed by five integrated source increments per cell.
`coupling/hotgas.py` validates and seals checkpoints, independently checks global
mass/momentum/energy budgets, and pins configuration, worker, adapter, source and
rational substep clock. Recovery must preserve the same substep sequence.

Matched periodic contact comparison: constant pressure, 100 m/s advection,
0.0005 s duration, analytic finite-volume sine reference, 4×4 transverse cells.

| Cells along transport | Density L1 error, kg/m³ | Refinement ratio |
| --- | ---: | ---: |
| 32 | 0.0462685 | — |
| 64 | 0.0240725 | 1.9220 |
| 128 | 0.0122469 | 1.9656 |

Pressure error stays below 1.2e-10 Pa. Additional native checks cover acoustic
transport along XYZ, 300/1290 K contact transport, uniform heating to 1290 K,
conserved source budgets, exact split-run recovery, identity drift and CFL/state
refusals. Seven tests pass strict and sanitized builds; the sanitized native
harness verifies accepted bytes remain unchanged on refusal.

The separate prescribed-power probe uses the pinned 2024 source measurement and
exact circular overlap support on an 8³ periodic 0.6 m cube. It injects
18,546.8 W for 0.01 s: 185.468 J. Energy residual is 5.82e-11 J; split-run
recovery is exact. Peak temperature is 301.872 K. This is heat-only transport,
not fuel/species admission, burner geometry, or a prediction of the measured
1290 K plume. The independent uniform heating/contact tests cover hot-state
admission separately.

Four new Growth surface tests compare the boundary against an analytic Newton
cooling solution, an independently integrated RK4 radiation-cooling solution,
manufactured radiation/conduction equilibrium, and signed reservoir heat input.
Refinement approaches the continuous solutions at first order. These validate the
implementation; they do not identify actual burner coefficients.

The original NIST 2019 paper describes approximately 15% gauge-versus-surface
flux difference from an extrapolation at the **pool center**. This is a local
probe-plane constraint, not a whole-pool attenuation factor or identified surface
power. `gauge_height_constraint()` preserves that scope. The physical comparison
is not evaluated until gas/radiation fields can sample both planes. Source:
[NIST author manuscript, page 14](https://tsapps.nist.gov/publication/get_pdf.cfm?pub_id=928848).

`scripts/fire_hotgas_qualification.py` runs the numerical gas and independent
surface tests and retains sealed source/config/checkpoints/receipts/assessment.
It needs explicit `--worker`, `--reference`, `--growth-root`, `--thermal-worker`
and a fresh `--output`. Latest workspace evidence:
`_private_workspace_artifacts/fire-fluid-hotgas-20261009/accepted/`.

The next separate [domain module](hotgas_domain.md) now passes conservative
solid/open reservoir, hydrostatic gravity and manufactured conductive-plane
checks. Physical buoyancy accuracy and experimental gauge transfer remain open.
Temperature-dependent properties, fuel/species, combustion and gas radiation require
qualification before reciprocal thermal exchange or authoring controls are
enabled. No version, canonical adoption, package or remote state changed.
