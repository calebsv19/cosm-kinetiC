# Conservative gas boundaries, gravity and conductive plane observations

October 9, 2026 Main Edit source development. Extends the separate hot-gas
foundation without replacing the existing periodic solver or coupled proxy.
Boundary/conservation and manufactured conductive sampling checks pass; physical
buoyant accuracy, reacting plume and experimental gauge response are unqualified.

## Native model

`cfd_hotgas_domain3d` supports paired periodic faces, impermeable inviscid slip
walls, and prescribed far-field reservoirs. Reservoir pressure follows the
specified isothermal hydrostatic profile from the coordinate origin. These are
Dirichlet reservoirs, not qualified nonreflecting outlets. Gravity on a periodic
axis is refused because its potential would be multivalued.

Local isothermal hydrostatic reconstruction maps equilibrium cell averages to
face averages. It preserves a resting atmosphere while applying `rho*g` to
momentum. Gravity work uses the same mass flux and face potential as transport,
so gas energy plus `rho*potential` has an independent conservation check. Every
physical boundary records outward mass, momentum, gas energy, potential energy
and conducted energy. Walls have zero mass and advective energy transfer; wall
pressure contributes to the momentum ledger.

Conduction uses explicit Fourier energy flux with a supplied constant
conductivity. Walls may be insulated or held at a prescribed temperature;
reservoirs have specified temperatures. Thermal reservoirs may heat or cool gas.
The combined acoustic/diffusive CFL includes source-heated states. Refusal leaves
accepted state and the output budget unchanged. No wall thermal storage,
viscosity/no-slip layer, species, combustion or gas radiation is implemented.
Gas `gamma` and `R` remain constant specified air-surrogate values.

## Plane quantities and meaning

`cfd_hotgas_sampling3d` reads native double-precision fields at arbitrary admitted
horizontal heights. The lower face must be a prescribed-temperature solid wall;
observations stop at the uppermost cell center. Three arrays are returned:

- Temperature in kelvin.
- Conducted heat flux in W/m², positive downward. At `z=0` this is the actual
  modeled wall transfer. Away from the wall it is conducted transport across
  that horizontal plane, not heat absorbed by a gauge inserted there.
- Bulk total enthalpy transport in W/m², positive upward. This includes
  advected background enthalpy and is not an absorbed surface heat flux.

Samples at `z=0` and `z=0.013 m` use the liquid-relative experimental heights.
The flat numerical wall does not include the real burner lip or cooled-gauge
geometry. Radiation is explicitly unavailable, and observations carry
`experimental_gauge_response_qualified=false` and
`reciprocal_thermal_exchange_admissible=false`. They cannot be used as measured
total incident flux or automatically fed into Growth's thermal boundary.

## Qualification and limitations

Ten domain tests pass strict and ASan/UBSan builds; the sanitized native harness
checks byte-preserving acoustic, diffusive, source and policy refusals. Two
additional plane tests pass both builds, separating nonzero bulk enthalpy from
zero surface heat and refusing unsupported observations. Seven periodic solver
regressions and three existing held wind-return regressions pass.

Known uniform throughflow reproduces independently calculated reservoir mass and
energy budgets. Closed/open hydrostatic cases, including gravity along three
axes, remain at rest. The initial interior acceleration of a manufactured 900 K
region in a 300 K atmosphere matches the expected 19.62 m/s² upward.

A 10 ms hot slab did not retain positive mean velocity in its initial Eulerian
region: -1.64299e-4 m/s. The failed case is preserved. A localized 3D region stays
upward over the sampled timeline, ending at 1.37736e-5 m/s, but is strongly
damped. These are sign, conservation and restart checks, not buoyant-flow
accuracy validation. The cause of the slab reversal is not independently
identified; first-order acoustic Rusanov diffusion and boundary/pressure
adjustment need assessment before physical plume qualification.

For the manufactured cell-average profile `T=300+1000*z+20000*z² K`, with
`k=0.026 W/(m K)`, the exact downward flux is 26 W/m² at the surface and
39.52 W/m² at 13 mm. The wall error halves with each refinement; the resolved
interior plane matches the analytic gradient to numerical precision.

A separate 8×8×32 open-side/top fixture with a flat 300 K wall injects the pinned
2024 reference's potential complete-combustion power as **prescribed heat only**:
18,546.8 W for 10 ms, or 185.468 J. Its gas-plus-potential residual is
2.61e-11 J and checkpoint recovery is exact. Peak temperature is 305.768 K.
Instantaneous conductive power over the circular support is 2.23051 W at the
wall and -0.183790 W at 13 mm. Opposite signs reflect distinct transport planes;
they are not an experimental gauge discrepancy or a calibrated fire prediction.
The modeled wall's integrated conducted energy is tracked separately from
these final instantaneous samples.

The NIST 2019 approximately 15% gauge/surface difference remains a local
pool-center extrapolation, not a whole-pool scaling factor. A physical comparison
requires gauge/surface absorption, radiation, geometry, resolved boundary layers
and matched time statistics. [Primary NIST study](https://www.nist.gov/publications/energy-balance-medium-scale-methanol-ethanol-and-acetone-pool-fires).

## Reproduce

From the Physics source checkout, into fresh task-owned roots:

```sh
python3 scripts/build_hotgas.py --domain --output build/my-domain/strict
python3 scripts/build_hotgas.py --domain --sanitized --output build/my-domain/sanitized
python3 scripts/build_hotgas.py --domain --sanitized --harness --output build/my-domain/harness
HOTGAS_DOMAIN_WORKER="$PWD/build/my-domain/strict/physics_sim_hotgas_domain_worker" \
  PYTHONDONTWRITEBYTECODE=1 python3 tests/test_hotgas_domain.py -v
```

The separate private native protocol extends the earlier argument list with
domain policy, conserved state, source and optional sampling heights on stdin.
`coupling/hotgas_domain.py` seals source/config/worker/adapter/rational-clock
identity, verifies boundary and gas-plus-potential ledgers, and checks controls
again after execution. Observation reads preserve accepted checkpoint bytes.

`scripts/fire_hotgas_domain_qualification.py` needs explicit `--worker`,
`--sanitized-worker`, `--harness`, `--periodic-worker`, `--reference`, and fresh
`--output`. It retains numerical tests, transient states, failed-case references,
prescribed-power budgets, plane observations and sealed assessments.
Workspace evidence: `_private_workspace_artifacts/fire-fluid-domain-20261009/`.

Next physical gates: low-Mach buoyancy/damping accuracy, independently specified
temperature-dependent mixture thermodynamics/transport, then fuel/species and
combustion energy, radiation and experimental surface/gauge comparisons.
Reciprocal thermal feedback and authoring controls remain disabled. No source
adoption, VERSION, package, remote or Desktop changes occurred.

The later [contact/pressure and bounded mixture checkpoint](hotgas_lowmach.md)
adds explicit HLLC mode, isolates stationary-contact numerical mixing and checks
acoustic/boundary response. It improves localized buoyancy but retains the failed
slab and material mesh sensitivity. The NIST mixture reference is not yet the
native EOS; reciprocal thermal return remains disabled.
