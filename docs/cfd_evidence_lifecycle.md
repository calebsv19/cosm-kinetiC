# Local CFD build, test and retained evidence

Developer source-checkout contract, 2026-10-06. This repair is being implemented
in the persistent Main Edit lane. It does not promote canonical source or qualify
an installed product. See [repair status](cfd_lifecycle_repair_status.md) for the
validated slice and remaining work.

## Separate roots

| Purpose | Make variable | Default | Lifetime |
| --- | --- | --- | --- |
| Registered objects, support libraries and compiler binaries | `BUILD_DIR` | `build` | Rebuildable; normal clean after guard admission |
| Temporary regression fixtures | `TEST_TMP_DIR` | `tmp/tests` | Disposable; tests also use unique OS temporary directories |
| Retained numerical runs, frozen inputs, exports, receipts | `EXPERIMENT_DIR` | `data/experiments` | Never normal-cleaned |
| Pinned reference Python environment | `REFERENCE_TOOLS_DIR` | `data/tools` | Recreated explicitly; never normal-cleaned |

Make exports the corresponding `PHYSICS_SIM_BUILD_ROOT`,
`PHYSICS_SIM_TEST_ROOT`, `PHYSICS_SIM_EXPERIMENT_ROOT` and
`PHYSICS_SIM_REFERENCE_TOOLS_ROOT` paths. Active box, native cube and native
manufactured-accuracy runners accept `--experiment-root`; direct invocation
uses `data/experiments`. They reject placement overlapping protected source, tool, build/test and
configuration roots; see retained path admission below. The scene runner accepts an explicit `--worker`; its default
respects the configured build root. A run name is caller-selected and immutable:
existing directories are refused, including previously failed attempts.

Only the active box native/material/session Make contracts and all 19 distinct current-reference support ABIs have migrated their build output.
Many specialized targets still contain literal build paths. `BUILD_DIR` alone
is not isolation for those targets. Reference Python commands now use
`CFD_REFINED_REFERENCE_PYTHON` rather than literal interpreter paths.

```sh
make reference-env-plan
make cfd-reference-amg-env
make BUILD_DIR=build/my-current-proof test-cfd-current
make BUILD_DIR=build/my-current-proof test-cfd-reference-current
make test-cfd-evidence-lifecycle
python3 -B scripts/run_cfd_native_box.py --name my-box-01 \
  --grid 32 16 16 --lower-m 1.25 .75 .75 --upper-m 2.75 1.25 1.25
python3 -B scripts/cfd_evidence.py data/experiments/c3d-box/runs/my-box-01
```

For a new full environment, invoke the AMG target directly. Existing matching
environments are reused without writes; mismatched or incomplete environments
are held. A base-only prefix cannot be upgraded in place: select a fresh
`REFERENCE_TOOLS_DIR`. See [reference setup lifecycle](reference_environment_lifecycle.md).

The exact tracked pins are scikit-fem 12.0.2, NumPy 2.5.3 and SciPy 1.18.1.
The full current reference suite additionally requires pyamg 5.3.0, installed
explicitly with `make cfd-reference-amg-env` ([upstream package](https://pypi.org/project/pyamg/5.3.0/)); mesh construction and current
non-AMG algebra no longer import it incidentally. The repair does not change the
pins or pretend every historical campaign runs with only these three packages.

## Bundle and readback contract

Active runners freeze their selected sources and numerical contracts, retain
bounded compiler/solver stdout and stderr, and write a terminal receipt.
`bundle_manifest.json` binds every regular artifact's relative name, byte size
and SHA-256, including the receipt, input contract and frozen sources. The
mutable session lock is excluded. Verification refuses missing, extra, changed,
escaping or symlink artifacts. The manifest is integrity evidence, not a digital
signature, physical-accuracy certificate, or backup.

The scene summary's artifact references are relative to its bundle root.
Original session records and logged commands can retain execution-time absolute
paths as provenance; portable offline inspection follows the summary references,
not those original operational paths. Relocation does not replay a session or
execute logged commands. Retained sources and worker bytes remain in the bundle.

```sh
python3 -B scripts/cfd_evidence.py <relocated-bundle>
python3 -B scripts/inspect_cfd_box_scene.py \
  --receipt <relocated-bundle>/receipt.json --output <new-inspection-directory>
```

The offline inspector verifies the completed receipt and all retained fields,
produces SI samples/previews, and seals a separate output bundle. It does not
advance the solver. Cube readback likewise requires `--output <new-directory>`
and refuses a descendant of its input bundle; it never appends to a sealed run.
Failed receipts remain failed even if intermediate numerical cases passed.

## Test groups

- `test-cfd-current`: bounded native box/material/session correctness, declared
  common Cholesky library build, deterministic block algebra, small transition
  topology/reconstruction, and fresh graded geometry/resources. It is a selected
  current regression group, not the whole program suite.
- `test-cfd-reference-current`: full current reference discovery with all 19
  declared support ABIs and explicit AMG environment; no saved campaigns.
- `test-cfd-evidence-lifecycle`: a disposable checkout-shaped fixture builds and
  tests the actual native contract, retains and relocates its output, executes
  the actual Make clean recipe, rebuilds/retests and verifies retained digests.
  Additional controls check corruption, path escape and cleanup refusal.
- Explicit expensive qualification: named scene two-grid runs, manufactured
  accuracy campaigns and independent-reference convergence/force decisions.
  These retain new evidence and retain their numerical/resource gates.
- Historical archive audits: exact saved-mesh, transform and force-diagnostic
  assertions live in `tests/archive_cfd_reference3d_*.py`. They require explicit
  `PHYSICS_SIM_GRADED_ARCHIVE`, `PHYSICS_SIM_FORCE_DIAGNOSTIC`, and, for the
  transition audit, `PHYSICS_SIM_FORCE_SURVEY` as appropriate. Missing archive
  input fails; it is never silently skipped. Additional archive modules require
  `PHYSICS_SIM_CFD_ARCHIVE_ROOT` or `PHYSICS_SIM_SELECTIVE_DIAGNOSTIC` explicitly. Use the corresponding
  `audit-cfd-reference3d-*` target. The old force-local test target explains the
  migration and fails rather than implying coverage ran.

The exact historical cube-pressure checkpoint audit still means verification of
its original hashes, predecessor receipts and changed-file set. It has not been
recast as today's generic regression. The current/archive classification is recorded in
[dependency inventory](cfd_lifecycle_dependency_inventory.md). Broader specialized
campaign runner migration remains separate.

## Cleanup and preservation

`make clean` verifies exact disposable-output receipts and the complete selected
file inventory before deleting anything. Unknown or changed files and unregistered
directories cause a hold. It also checks the selected root before deletion. It refuses a
root outside this checkout, checkout/ancestor roots, protected data/tool/test or
source namespaces, and legacy digest receipts, saved NumPy fields/geometry, frozen source/run
directories, complete fields or retained manifests under the selected build root. A refusal preserves those bytes; it is
not permission to remove the guard. No cleanup was run in the survivor build
root during this repair. Test cleanup only in a disposable fixture until an
owner inventories and migrates that root.

Retained `data/experiments` and `data/tools` are ignored to keep large runs out
of Git; ignored storage is not preservation. Keep successful and failed run
identities, inputs, sources, worker identity, receipts and manifests together.
Never prune an evidence dependency merely because a newer run exists.

Before deleting or recycling any retained lane, copy required bundles to an
independent backup destination, verify the full destination manifest, verify a
second read after relocation, and record destination and manifest digest in the
owner's retention ledger. A same-disk copy proves relocation, not disaster
recovery. The approved prepared survivor/fresh-evidence snapshot has a verified independent
cold copy; see [backup completion receipt](../data/experiments/lifecycle-validation/20261007-archive-completed/receipt.json).
Later hardening packets are outside that snapshot, and remote retrieval/restore
rehearsal remains unperformed. Normal clean never deletes these retained roots. Archive deletion is
an explicit owner operation, outside this cleanup contract.

Historical evidence loss limits historical readback; it does not make old fields
a runtime dependency. UI work requires a fresh shared consumer build and relevant
owner compatibility/visual checks. It does not require reconstruction of every
old CFD campaign. Steady aligned creeping-Stokes box forces remain provisional;
no transient obstacle, wake, general CFD or production readiness is inferred.


## Retained path and manifest admission

Experiment-root selection refuses overlap with source, headers, scripts, tests,
docs, Make/configuration, vendored dependencies, Git/agent metadata, export/dist,
visual output, tools and configured build/test/reference-tool roots. Default
`data/experiments` and distinct explicitly selected retained roots remain supported.
External roots are trusted local operator selections; this is not an untrusted
upload sandbox. Symlink components are refused after normalizing the known macOS
`/tmp` and `/var` system aliases.

Sealing and verification inspect the full tree and refuse symlinks and special
files, including a FIFO named `service.lock` or `bundle_manifest.json`. Empty
bundles cannot publish a manifest; existing manifests cannot be resealed.
Digests use nonblocking no-follow regular-file reads and check file identity
before/after reading. Readback rechecks the inventory and each inspected file's
identity at completion. These detect observed drift under the trusted cooperative
workflow; they are not a filesystem snapshot or protection against arbitrary
concurrent hostile mutation.

The v1 manifest reader is bounded to 64 MiB, rejects duplicate fields and
non-finite JSON, and validates schema, boolean state flags, canonical relative
paths, SHA-256 strings and nonnegative integer byte counts. Readback CLI failures
produce a nonzero hold diagnostic without a traceback. A manifest proves only
its declared regular-file inventory and bytes. Empty-directory metadata is not
part of v1. Regular files named `service.lock` remain the historical mutable-lock
exception and are not hashed; they must never carry durable evidence. No schema
migration or modification of existing sealed packets occurred.

The active frozen source packs now include `check_clean_root.py`, which owns the
shared strict JSON parser used by `cfd_evidence.py`. Backup receipts remain
separate from local integrity; new packets do not inherit earlier archive coverage.
