# Retained semantic and numerical compiler attempts

The 18 `dump-sema*` Make goals allocate fresh capsules under
`$(EXPERIMENT_DIR)/semantic-proofs/semantic-<unique-id>/`. The helper prints the
capsule path before starting the compiler. Inspect `compiler.log`, the generated
object, `request.json`, `receipt.json`, and the sealed `bundle_manifest.json`.
Each attempt preserves success and failure diagnostics. There is no overwrite,
reset, automatic cleanup or latest-success promotion.

`SEMA_*_OBJ` and `SEMA_*_OUT` remain validated legacy selection hints under the
selected `BUILD_DIR`. They are never written by these goals. This intentionally
changes the old fixed-path output behavior; consumers must use the reported
capsule. Existing legacy files remain intact. Objects created for semantic proof
are evidence, not application link inputs.

Top-level Make ownership excludes cleanup and conflicting builds. Direct helper
invocations acquire the same ownership when it is not inherited; compiler
children retain the ownership descriptors. Catchable termination records a failed
attempt. SIGKILL may leave an unsealed capsule with a running request; that is
incomplete evidence and must not be interpreted as passed. No generic pruning
policy has been introduced.

Receipts bind the selected source hash, preserved source bytes, requested and
executed command, compiler binary hash, helper hash and relevant process setting.
Source or compiler drift fails acceptance. Complete transitive header/dependency
capture is explicitly false: the capsule is diagnostic provenance, not yet a
standalone replay package or a complete per-command build-input identity.

The six active native box, cube-pressure, manufactured accuracy, box-scene,
cube-readback and pressure-trace workflows use `cfd_run_support.compile_probe`.
They already allocate unique retained bundles and freeze their declared source
inputs. The helper writes a fresh `.compiler-attempts/<id>/` candidate, retains
command logs and an attempt receipt, and publishes only after bounded successful
compilation and file validation. Atomic no-replace hard-link publication refuses
an existing or competing binary. Candidates remain for readback, including on
failure. These compiled probes are retained evidence; they do not get disposable
build-output receipts. Historical wrappers are unchanged.

Ordinary clean holds both `semantic_proof` and `retained_compiler_output` artifact
classes. It does not prune capsules. Failed evidence is useful for diagnosis and
requires an explicit retention policy before future removal. The canonical
checkout has not adopted this source contract.
