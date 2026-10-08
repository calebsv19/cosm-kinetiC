# PhysicsSim local build and cleanup operations

The default compilation profile is `build/profiles/local-owned`. Make aliases
such as `physics_sim_headless` remain stable; their executable is now
`build/profiles/local-owned/bin/physics_sim_headless`. Existing root binaries and
older build outputs are preserved. They are not the supported new build output.

## Routine source proof

Run each operation separately from the repository root:

```sh
make help
make physics_sim_headless
make test-physics-sim-headless-water-mode
make test-physics-sim-headless-scene-project-cache-output
make clean-plan
make clean
make physics_sim_headless
```

`make help` prints the local command and retention summary without compiler or
package discovery. It does not initialize build or runtime output.

The fixtures above exercise and reset their named output directories under
`tmp/`. Use a disposable checkout when those directories already contain work
that needs retention. They do not classify pre-existing data as disposable.
For a separate compilation profile, pass the same `BUILD_DIR=build/profiles/NAME`
to every Make invocation. Make exports the selected headless path to fixtures.
Direct executable commands must use that profile's `bin/` path.

## Storage and deletion contract

- `build/profiles/local-owned/`: default compiler output. Successful writers
  record exact output ownership under `tmp/`; receipts survive clean.
- `data/experiments/`: retained runs, repair evidence and receipts. Normal clean
  does not delete these records.
- `data/tools/`: retained reference environments/tools, separate from compilation.
- `tmp/tests/`: separate test storage. Tests may own individual fixture outputs;
  normal compilation clean does not remove this root.
- Legacy build trees, root binaries, packages and user exports are not adopted
  for deletion based on their names.

`clean-plan` inspects without deleting and does not require a working compiler.
`clean` acquires cleanup ownership, checks the entire selected inventory and
revalidates before deletion. Unknown files/directories, changed outputs,
unsafe paths and protected reservations hold cleanup. Mixed build/clean goals
are refused. Builds and cleanup share cooperative locks; this is not a sandbox
against an unrelated process deliberately replacing files.

When inspection refuses a target, preserve the reported path and review its
ownership. Do not bypass the hold with `rm -rf` or fabricate ownership receipts.
Choose a fresh build profile to continue compilation while the old output is
being assessed. Incomplete compiler attempts and semantic/contract outputs may
require separate review; normal clean is not their retirement mechanism.

## Recovery and preservation

A failed compiler invocation does not publish its staged output as a successful
build. An interrupted attempt may leave evidence that makes cleanup refuse it;
retain it for inspection. This is not a claim of automatic rollback after power
loss. Packaged launcher configuration recovery is documented in
`launcher_configuration_recovery.md`; its repaired source is adopted in canonical.
Installer/package source adoption is documented in `packaging_lifecycle_operations.md`;
installed-app validation remains separate work.

The earlier CFD archive remains retained. The final focused-cleanup snapshot was
also imported to the independent PC cold archive and retrieved/verified. See
`focused_cleanup_closeout.md` for the exact payload, coverage, exclusions and
retrieval receipt. Later changes require new preservation; a local receipt,
source test or prepared bundle alone is not proof of independent backup.

No command in this guide authorizes package refresh, installation, signing,
release, publication, version changes or remote worker execution.
