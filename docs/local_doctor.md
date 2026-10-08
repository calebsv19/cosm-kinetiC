# Read-only local doctor

`make doctor` reports configured-root safety, a portable-checkout baseline,
compiler/Make/pkg-config availability, the selected macOS dependency discovery
and SDK, reference environment pins/imports, output ownership, cleanup holds and
backup coverage. It installs nothing, creates no build/fixture roots, performs no
compilation and requests no network operation.

Use an isolated selected root while historical build contents are retained:

```sh
make BUILD_DIR=build/my-current-proof doctor
make BUILD_DIR=build/my-current-proof DOCTOR_PROFILE=cfd-reference doctor
```

The reference profile adds all four tracked reference-distribution pins and
successful imports with matching runtime versions to the common prerequisite checks.
The report includes the actual imported module paths, not only installed metadata. The default headless
profile reports the optional reference environment without requiring it. The
venv interpreter leaf may be its intentional system-runtime symlink; symlinked
parent directories are refused. Imports run with isolated Python and bytecode
writing disabled. Installed local tools and environments are trusted operator
selections, not an untrusted-execution sandbox.

Control-only Make routing avoids compiler/link/package fragments. macOS prefix
selection uses the existing `desktop_release_target_contract.sh`; dependency
probes use its target pkg-config directories. A json-c 0.19 selection must have
the existing declared 0.18 compatibility archive. Custom `CLANG`, `PKG_CONFIG`,
`TARGET_ARCH`, `PHYSICS_SIM_JSON_COMPAT_PREFIX`, and configured output roots are
passed through. `DOCTOR_REFERENCE_PYTHON` defaults to the existing selected
reference Python when specified, then the tools-root venv. `DOCTOR_PROFILE` is
`headless` or `cfd-reference`.

Exit 0 means `ready_for_build_attempt`: declared checks passed and the selected
cleanup plan is not held. Exit 2 means `attention_required`, with failed,
missing or unverified checks and next actions. A historical retained root
correctly causes attention; select a fresh root rather than weakening its hold.
Missing pkg-config metadata may still have a Make fallback, but doctor has not
established that selection and will not call it ready. It does not install or
repair prerequisites automatically.

Availability is distinct from build qualification. The source check covers only
its listed portable-checkout baseline, not the full dependency graph. It does not
prove write access, effective custom compile flags, dependency ABI, linkability,
UI/installed product acceptance or physical accuracy. A verified disposable-output
receipt proves exact recorded bytes/identity, not complete compiler-input or
profile provenance. Build, test, installed/public and physical qualification
flags remain false. Run the supported build, Water and scene-project fixtures
separately; archive, recovery and release acceptance are separate contracts.

The default status/doctor cleanup plan uses selected build/profile executables.
An unselected legacy checkout-root binary does not hold a fresh root. Explicit
executable overrides still receive full cleanup admission; Make forwards its
actual selected inventory. `make status` remains a lighter snapshot.

Probes run without a shell, with a 15-second wall limit and 64-KiB output limit.
Failed/oversized/hung probes are reaped, including descendants holding pipes
open after their parent exits. Version/metadata commands and reference imports
are trusted read-only conventions; doctor is not a security boundary for a
malicious operator-supplied tool.

If reference setup is missing, inspect the existing explicit
`cfd-reference-env`/`cfd-reference-amg-env` contracts in [reference setup lifecycle](reference_environment_lifecycle.md). Those mutation commands are
not run by doctor. Source versions, installed versions and Registry publication
remain distinct. Backup coverage stays exact: an older prepared archive does
not cover later proof packets, and a restore rehearsal is still a separate check.

Doctor shares the [bounded local tool probe runner](tool_identity_probes.md) with
build identity. Probe completion reaps descendants even when a successful parent
closed its captured pipes. Persistent services must not start through probes.

The prepared snapshot now has a separately sealed [retrieval/restore rehearsal](archive_restore_rehearsal.md).
Status binds that record to its exact copy receipt and payload, without rewriting
historical receipts or extending backup coverage to later evidence.
