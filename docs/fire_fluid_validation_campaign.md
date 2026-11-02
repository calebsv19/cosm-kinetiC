# Fire–Fluid channel, cadence, ventilation and inert-object references

This is a source-development qualification lane. It adds an opt-in native3D
channel reference and a private in-memory gas/object coupling host, while
retaining existing source/worker defaults and historical journals.

## Native3D analytical channel

`cfd_open_wind_half_channel(gas)` selects the full-aperture profile
`u(z) = U (2 z/H - (z/H)^2)` after ordinary inlet and full channel-patch
configuration, before any evolution or pressure-cache creation. Ground is
no-slip; top and sides are impermeable with tangential slip. Partial apertures,
balanced inlets, mismatched height, repeated selection and evolved state refuse.
The native API/probe is opt-in; the closed Python worker JSON protocols do not
expose a new profile and ordinary application/menu behavior stays unchanged.

The independent steady reference is `G = 2 mu U/H²`, `p(x)=G(L-x)` and
`Q = (2/3) U W H`. For L=.25m, W=.25m, H=1m, rho=1.2kg/m³,
mu=.12Pa s and U=.1m/s, exact pressure drop is.006Pa and Q=.0166666667m³/s.
The ordinary3D projection solver evolves from rest; only inlet velocity is
prescribed. No analytic interior velocity or pressure is inserted.

Fresh8/16/32/64 vertical-layer runs complete. At64 layers, velocity L2 error
is.0017429% and solved pressure-drop error.0473464%, inside the declared1%/3%
development gates. This qualifies the stated mixed boundaries at the tested
viscosity; it does not establish no-slip square-duct, arbitrary room or
experimental/turbulent-wake accuracy. The separate obstacle comparison reuses
the retained80³ live field: maximum selected64→80wake difference4.8316% fits a
5% development-reference envelope. That is same-solver finer-grid evidence.

Build with a fresh task-owned BUILD_DIR inside the registered checkout:

```sh
make BUILD_DIR=build/<fresh-owned-root> channel-reference-probe
build/<fresh-owned-root>/flow-validation/channel-reference 32 .002 4000
```

## Controlled live recipes

`coupling.validation_campaign.configuration_for(workers,name)` prepares:

- `cadence-100ms`, `cadence-40ms`:64³ gas,17×17 Fire, gasdt=.0025s,
  Firedt=.02s,1.2s horizon and the same.2–.6s gust/insulated box.
- `vent-full`, `vent-elevated`, `vent-obstacle`:32³ warm-flow behavior cases,
  each prescribing.525m³ through1.2s; report actual source heat/smoke input
  separately from stored quantities. Only the last includes the insulated box.

Cadence changes both source packetization and accepted-prior wind hold, so its
observed difference is not an isolated return-channel error. The schedules use
12/30 exchanges within existing32-window limits, with ordinary native packet
restart/full-window acceptance and lossless scientific journals.

All five live cases completed with independent exact head reopen, 30 selected
f64/journal hashes, native feedback sampling and separate source/air/scalar/MAC
accounts verified. Final gas/returned-wind/wake cadence L2 differences are
0.00735% / 0.03421% / 0.00368%. Fire heat-proxy difference at the selected0.4s
checkpoint is4.5415%, falling to0.3112% at1.2s; the final field alone misses the
transient. Full/elevated/obstructed openings retain89.15% /90.16% /84.09% of
actual supplied heat. Four comparison figures were visually reviewed.

## Reciprocal inert object host

`physics_sim_inert_coupling_step` composes one ordinary native flow step and
`physics_sim_heated_face_exchange_depth` at the same dt. It snapshots physical
arrays, clocks and receipts; either failure restores gas and object together.
Disposable pressure caches retain their actual ownership when rollback occurs.
This is an in-memory development host; it adds no durable object codec,
Framework thermal participant, Desktop object behavior or release activation.

The selected law uses authored h=2W/m²K, C=10J/K and fixed.125m physical gas
support. Existing pair transfer is signed: gas gain is object loss, and vice
versa. Source input, gas stored heat, gas outflow and solid stored heat remain
separate SI terms. The fixed-depth Robin support is not a resolved wall-gradient
law or measured material-convection model. Fire heat proxy is not used as solid
Kelvin temperature.

The completed 15-case thermal matrix covers16/32³ two-capacity controls,32³ warm-gas
heating/hot-object cooling, exchange-disabled controls, matched heat-source
on/off and timestep comparisons. It declares1e−5K analytic and1e−7J closure
gates plus5% flow-transfer timestep sensitivity; all pass, together with four
ASan/UBSan cases and after-flow fault rollback. Maximum analytical error is
1.26e−11K and total-energy defect7.28e−12J. Heating/cooling transfer is
+0.739618J / −0.738920J; exchange-off transfer is zero. The otherwise-matched
0.2J source toggle changes object temperature only5.43e−11K over1.2s. This
fixture validates exchange arithmetic but does not establish source-driven
object-heating response; a better source/transport/support geometry is next.
No ignition, radiation, below-reference cold-gas model or combustion is added.

## Verification state

Fresh native quiescent/ventilation/heated-support and half-channel strict and
ASan/UBSan checks pass, as do39 Python checks,320-step bitwise legacy/native
parity and a fresh Clang application compile. macOS does not support LeakSanitizer
in the new local invocation; its refused attempt is retained, and supported
Address/Undefined sanitizers plus explicit allocation-zero checks pass.
Broad application test-fast passed after scientific workers released their
build-root ownership. Never rebuild an actively consumed worker root.

Evidence: `_private_workspace_artifacts/fire-fluid-four-step-20261029/`.
All four numerical stages and visual review are complete. All work stays uncommitted
Main Edit; no VERSION, package, release, installation, Desktop or remote effect.

The subsequent [controlled source-to-object diagnostic](fire_fluid_source_object.md)
resolves the weak source-toggle fixture with cold initial gas/object and an
upstream separated heater. It remains independent of native Fire and durable
thermal participant integration.
