# Build configuration metadata lifecycle

Build graph selection and configuration publication require exact disposable
ownership for existing metadata. An unknown or changed active selection or
selected stamp holds the build. The active selection uses bounded strict JSON:
duplicate fields, invalid digests, non-integer generations and special files are
refused. Identity is rechecked after inspection and before publication.

Configuration output must be the exact digest-and-generation stamp under the
selected root's `.configuration` directory. Matching metadata is reused without
writes; other output paths are refused. Each newly published stamp and active
selection has an ownership receipt. The metadata is build identity, not evidence
of numerical correctness or installed acceptance.

Missing active selection beside existing configuration files is held. Restarting
the generation counter could make an old stamp appear current while objects
contain another profile. Interrupted publication therefore requires preservation
and a fresh build root, rather than automatic metadata reconstruction.

```sh
make test-configuration-admission
make test-build-identity
make BUILD_DIR=build/new-profile physics_sim_headless
```

Selecting a fresh root is the supported response to held metadata. Do not delete
or rewrite receipts to make a historical root acceptable. Stamp and active
selection publication are separate filesystem operations; a crash between them
can produce held state. Cooperative Make ownership provides exclusion, rather
than a malicious-writer sandbox or multi-file power-loss transaction.

Validation covers unknown state before compiler probes, malformed registered
state, special files, changed metadata, exact output paths, no-op timestamps and
missing selection recovery holds. Actual Make graph tests verify that an
existing stamp target cannot bypass admission, that flags and helper changes
rebuild, and that returning to a prior profile advances the generation. The
supported headless build, Water smoke and scene-project cache-output fixture
passed in an isolated root. Compiler and SDK identity probes now use the [bounded local runner](tool_identity_probes.md).
Full transitive dependency identity remains separate work.

## Registry snapshot layout

The release preparation owner can select `snapshot_sandbox_v1`. In that mode,
`PHYSICS_SIM_LAYOUT_BUILD` identifies the attempt's `build` slot, and the source
must be the matching frozen `source/physics_sim` directory. The program checks
the existing attempt ownership marker and descriptor before admitting external
output paths. Build products use `build`, locks and receipts use `runtime`,
fixtures use `tests`, and local package staging uses `package`. Native fixtures
honor the owner's `TMPDIR`; integration-tool builds stay in their fixture root.
The Registry sandbox remains responsible for enforcing read-only source inputs.

This opt-in does not grant release, cleanup, publication, or deployment authority.
Without it, ordinary checkout-local build and fixture paths remain unchanged.
Compilation cleanup still admits only its existing checkout-local namespace;
it must not be used to retire Registry-owned attempts. Signed artifact outputs
still require their exact immutable package contract. Ownership, symlink,
source-identity, output-overlap, and predecessor refusals remain active.

`python3 -B -m unittest discover -s tests -p test_output_layout.py` checks the
isolated-path contract and the unchanged checkout defaults.
