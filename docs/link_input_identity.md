# Final linker input identity

Main Edit selected Darwin object-only links now publish an owned linker input
manifest next to the executable. Ordinary application, FisiCs final link, shape
sanity, headless, runner and session-worker recipes opt into the contract. The
native fixture and real headless lane are qualified here; wiring the other recipes
does not establish their complete runtime, FisiCs or installed acceptance. Direct
C source-to-binary recipes are deliberately not declared complete: they need a
source/header closure as well as final linker input identity.

A supervised discovery link writes a staged executable and Darwin dependency_info
trace. Bounded binary record admission normalizes cwd-relative paths and deduplicates
selected inputs and unsuccessful search candidates. Known opcodes and one first
version record are required. The exact staged output must be the trace's sole
output. Known caller-selected secondary link outputs are rejected before execution.
This is not a complete compiler-option allowlist or arbitrary-wrapper sandbox.

Before actual linking, the selected contents/resolutions and absent candidate paths
are observed under count/file/aggregate budgets. Inputs are hashed twice with
complete identity witnesses. After actual linking, its trace, bounded content
snapshot and full observed identities must match. Mutation or unreadable inputs
hold publication, retaining the candidate and link-input-hold.json. Accepted
predecessor binary, manifest and receipts remain unchanged. Fresh failure publishes
neither output nor manifest. Observations do not make the inputs immutable.

Successful publication records a linker-inputs ownership receipt for the manifest
and binds the executable receipt to the manifest SHA-256. Graph admission verifies
both ownership records, exact manifest/snapshot schema, count coherence, content
binding and complete metadata witnesses. Changed selected contents or resolution,
disappeared selected input, or a previously absent candidate appearing forces only
the affected executable to relink. Missing bound or contradictory metadata holds
reuse. Valid legacy compiler output without any prior manifest binding forces one
refresh, then matching inputs remain no-ops. Unknown outputs are never adopted.

Bounds: trace and manifest each 1 MiB; 10,000 total selected/absent paths; UTF-8 path
bytes at most 4096; regular selected files at most 64 MiB and selected bytes at most
256 MiB per snapshot. Existing complete-pass limits are shared by before/after
producer observations and separately by graph admission: 100,000 entries and
1 GiB charged hash bytes (both hash passes count). Graph metadata limits are 1,000
outputs and 64 MiB manifests, plus existing per-receipt admission. Trusted source
and SDK symlinks/hardlinks may resolve to regular inputs; disposable outputs retain
the separate unique ownership policy.

The earlier native counterproof demonstrated stale reuse after selected archive
content changed at preserved mtime and after an earlier absent archive appeared.
Current tests prove both relink, matching no-op, missing/binding/count holds, legacy
refresh, installed trace normalization/deduplication, malformed trace refusal,
pre-hash bounds, staged output binding and secondary-output refusal. Native mutation
probes cover same-byte library rewriting, a search candidate appearing during link,
and fresh publication refusal with retained candidate evidence.

Final gate: 73 distinct methods, comprising 13 link methods, 13 dependency/input,
7 compiler boundary, 12 atomic output, 4 compiler environment, 5 configuration
admission, 18 cleanup and 1 build identity. Eleven link methods run with the 60
existing regressions; two final additional mutation methods run separately. No
failure is concealed by a Make recipe. The fresh link-input-identity-20261007
headless profile builds. Final matching repeat leaves 232 objects, executable and
manifest unchanged (234 outputs); Water and scene-cache first proofs pass. Its
manifest records 277 selected paths and 375 absent paths. Read-only cleanup preview
admits 469 owned files, including the additional retained configuration generation
created by the final control-tool tightening. Cleanup was not applied.

Limits: this is Darwin selected object-only link provenance, not complete platform
or wrapper/toolchain hermeticity. Full compiler/linker resources and actual tool
selection contents, direct-source closure, hostile post-observation races, escaped
descendants, hard I/O deadlines, power-loss behavior and coherent binary/manifest/
receipt transaction recovery remain open. SDK linker stubs are observed inputs;
this does not bind all dynamically loaded runtime library contents. Status still
must not claim complete profile identity verification. No physical CFD accuracy,
installed package or public release acceptance is implied.

No canonical adoption, commit, installation, release, old user profile refresh,
user cleanup or independent backup coverage changed. Evidence packet:
data/experiments/lifecycle-validation/20261007-link-input-identity. It is outside
the frozen seventy-packet backup batch. The full TL01-TL13 goal remains incomplete.
