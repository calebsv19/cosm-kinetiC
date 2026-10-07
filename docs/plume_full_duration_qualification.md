# Full-duration local plume qualification

The local native worker has an explicitly selected experiment mode:

```sh
build/open-atmosphere/physics_sim_open_atmosphere_worker request.json \
  --sparse-qualification forcing.bin --samples-root <fresh-output-directory>
```

The ordinary one-request JSON workflow is unchanged. This mode is bounded to
fresh state, eight physical seconds, 3200 native steps, three ordered sample
times including the final step, 64 MiB forcing/configuration/sample artifacts,
and an explicit adapter timeout up to 7200 seconds (3600-second default). The
finest 3200-step laptop candidate selects the larger budget explicitly. Its step loop calls the same
`cfd_open_atmosphere3d_step`; it does not change numerical integration, source
timing, transport, projection or the physical applicability gates. The native
worker observes peak temperature, velocity component, divergence and projection
residual after every accepted step, so saved-snapshot sampling does not hide a
transient peak. The existing default resource limits remain unchanged; the
explicit scalar-work ceiling is now one billion.

`scripts/plume_qualification.py` encodes previously validated sparse source
allocations in app-private little-endian binary64 `PSQUAL01` packets and decodes
lossless native `PSCPQ001` samples. These are experimental local transport
formats, not shared public APIs or new VF3D contracts. The encoding preserves
each transferred energy/mass amount exactly. Duplicate indices, nonfinite or
negative amounts, invalid grid/dt/clock, truncated/trailing bytes, sample order
and artifact bounds are rejected. Workers and forcing bytes are pinned before
and after execution. A successful process exit is insufficient: every decoded
sample goes through the ordinary independent `accept_fields` checks, including
finite arrays, checkpoint counters, SI source budgets, chunk/lifetime balances,
ground flux, divergence, pressure residual and temperature applicability.

The compact runner avoids repeatedly formatting a complete 64³ state as JSON
between small native request chunks. It saves full binary64 state at selected
physical times. The output directory retains worker stderr and native packets
on a rejected run, while the absence of an accepted final qualification is
explicit. The sparse mode does not expand transactional revision/journal limits
or introduce a shared scheduler, renderer or release build.

`tests/test_plume_qualification.py` requires exact native state and budget parity
against the ordinary dense JSON worker for nonuniform mixed heat/soot forcing
at both intermediate and final times. It independently rejects corrupt/truncated
packets, nonfinite and negative fields, changed source budgets, invalid forcing,
duplicate allocations and excessive duration/work controls. GrowthSim's full
eight-second 32³ and 64³ parity receipts compare every native state field at
2/4/8 seconds against the retained earlier native experiment, including exact
binary64 tagged data digests. The model remains an all-fluid grounded Cartesian
warm Boussinesq plume with periodic XY and an open top. These qualification
mechanisms do not establish spatial convergence or physical hot-fire validity.

Build and run the dedicated parity/rejection checks from the checkout:

```sh
make open-atmosphere-worker
python3 -B tests/test_plume_qualification.py
```

## Separately selected full-burn movie mode

`--sparse-movie` selects the same step loop with a closed 32-second,
6400-step, 160-sample envelope. Only that mode accepts an explicit scalar-work
ceiling up to two billion. The ordinary JSON worker and `--sparse-qualification`
retain their earlier ceilings; their defaults do not change. The Python packet
writer, runner and reader require `movie=True` for this selection. Movie fields
pass the same independent `accept_fields` budget, ground, SI, divergence,
projection and warm-temperature checks with the selected resource ceiling.
The native solver, boundary model and numerical equations are unchanged.

Packet/configuration/sample byte bounds remain 64 MiB each, numerical allocation
is explicitly 256 MiB for the current 64³ movie, and the harness timeout is
explicitly bounded to 14400 seconds in movie mode (qualification remains at
7200 seconds and the ordinary adapter at 120 seconds). Sampling each 40 steps at dt 0.005 s supplies native
five-frame-per-second states. The current GrowthSim fixture uses its unchanged
0.02-second Fire evolution and 0.2-second source exchange through complete fuel
burnout at 27.2 seconds, followed by a tail to 32 seconds. This is a duration
extension for local audit media, not a spatial-convergence or hot-fire claim.

Additional tests compare four movie sample states and budgets exactly against
ordinary dense JSON states, require explicit mode selection, reject excess
time/sample/step controls, and exercise a nine-second packet that the shorter
qualification mode rejects. Full-resolution legacy parity is checked separately
by GrowthSim's movie against the accepted candidate at 2/4/8 seconds.

The optional movie packet reader's `compact=True` path returns private
`physics_sim_movie_sample_fields/v1` fields after the same shared numeric gates.
It checks request/data bounds, counters, physical clock, initial authority,
field/data identity, SI conversion, temperature applicability, divergence,
projection, source totals, ground flux and lifetime/chunk balances. It omits
canonical JSON checkpoint/result serialization. Original binary packet SHA-256
and the pinned worker/configuration/forcing identify these snapshots; this
carrier identity is distinct from canonical tagged-state identity. It does not
claim ordinary checkpoint restart support or change any existing digest.
Canonical tagged-state digests remain computed at the required parity times.
Tests require exact compact/ordinary fields and budgets and reject changed
source amounts through this path. The unchanged full reader remains available.
