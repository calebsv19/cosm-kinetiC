# Retained reference environment setup

The setup targets preserve existing environments. A matching environment is
reused without writes. A mismatched, failed or incomplete environment is held;
select a fresh tools root instead of upgrading or resetting it in place.

```sh
make reference-env-plan
# Base profile: scikit-fem, NumPy and SciPy.
make cfd-reference-env
# For a fresh full profile, invoke this target directly.
make REFERENCE_TOOLS_DIR=data/tools/reference-full-01 cfd-reference-amg-env
```

The full profile adds pyamg. Creating a base profile and then adding AMG to the
same prefix is no longer supported. An existing full matching environment can
serve both targets. `CFD_REFINED_REFERENCE_PYTHON` must identify exactly the
selected tools root's `cfd-reference-venv/bin/python`; setup refuses global or
unrelated interpreters. Doctor performs read-only inspection and never installs.

A creation request acquires cleanup exclusion and exclusive prefix ownership.
It freezes the selected requirements under
`REFERENCE_TOOLS_DIR/.reference-environment-attempts/<id>/`, creates the venv at
its final prefix, and retains commands, logs and terminal readback. Pip runs with
isolated configuration, without a cache, using exact tracked direct pins. This
explicit creation target may contact the package index. Pins do not establish
complete transitive dependency, wheel, ABI or numerical qualification.

A managed prefix contains `.reference-setup.json`. Doctor requires passed state,
the exact prefix and the requested pin profile before importing packages.
Failed attempts retain their prefix and logs. Forced termination can leave a
running marker; it is held and never automatically resumed or removed. Existing
healthy unmarked environments may be reused, without writing adoption metadata.
Ordinary clean must not prune environments or their attempt receipts.

Venvs are created at their final path because their scripts embed that prefix.
Do not relocate a successful prefix and assume it still works. Archive recovery,
restoration at the original prefix, and runtime requalification require separate
proof. No automatic pruning or recovery is implemented here.

Validation exercised controlled fresh success, failed installation retention,
requirements drift, protected roots, lock admission and actual Make routing.
A real venv was created at its final prefix with a deliberate offline installer
failure, proving prefix behavior and failure admission without network access.
Both targets reused the existing working full environment, with all 5,161 file
and symlink entries unchanged before and after. A fresh network installation and
native numerical qualification were not performed for this slice.
