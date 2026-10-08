# Cache compiler-profile qualification

The current VF3D cache boundary object was tested on Apple Clang 17.0.0
(clang-1700.6.4.2), arm64-apple-darwin24.6.0, with `-O2`,
`-O2 -ffinite-math-only` and `-O2 -ffast-math`. The preliminary actual-object
probe rejected malformed finite-value fixtures under all three profiles; no
production-code defect was demonstrated and no production code was changed.

`make test-cache-compiler-profiles` now compiles the actual reader separately
for each profile and runs three methods comprising 258 publication cases.
Every floating-point header member and each of the five raw payload fields
reject both signed infinities, signed quiet NaNs and signed signaling NaNs.
Invalid inputs retain the four predecessor output directories and allocate no
publication attempt. Valid baseline and payload signed-zero, subnormal and
maximum-finite encodings publish with byte-for-byte output equality.

Only the cache boundary object gets the profile flags. JSON helpers, JSON-C and
the link/runtime initialization use the ordinary local profile. This separates
boundary compilation from process-wide floating-point modes. The qualification
does not approve fast-math for solver computation, qualify other compilers or
hosts, or prove the job/config/snapshot boundaries under these flags. Default
Make flags do not select fast-math; CFLAGS can be overridden by callers.

Reuse scan: core_math exposes geometry-oriented math and uses isfinite internally;
core_data has no applicable binary floating-point admission API. No new generic
helper, shared module/API/version or on-disk format was introduced. The test
reuses the existing native admission harness and exporter-contract fixtures.

Evidence: data/experiments/lifecycle-validation/20261007-cache-compiler-profiles.
Canonical adoption, backup of this newer packet, cryptographic cache/source
authentication, forward recovery and the broader lifecycle goal remain open.
