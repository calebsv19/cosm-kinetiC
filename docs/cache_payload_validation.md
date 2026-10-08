# Streaming VF3D cache payload validation

Main Edit source preflight, copied-stage validation and active status now stream
all five native float fields and the solid mask after structural header/length
admission. Each density/velocity-X/Y/Z/pressure sample must be finite. Field
values are not assigned new physical ranges: finite negative or large values
remain format-admissible and require separate numerical qualification.

The full mask stream must match the historical solid_mask_crc32 header field.
That producer field is FNV-1a, not CRC32. The unchanged update algorithm now lives
in the existing exporter-owned raw contract header and is reused by exporter and
reader. Nonzero mask bytes denote solid cells; matching hashes for 1 and 255 are
accepted rather than introducing a binary-only mask rule. No on-disk format,
solver, shared module/API/version or release identity changed. Fixtures now carry
the producer's correct mask hash instead of an unchecked zero placeholder.

The validator uses fixed buffers (4,096 floats plus 4,096 mask bytes), exact
partial-read loops with EINTR retry, final EOF and named-entry witness checks.
Per frame admission remains 8 GiB; each inventory pass additionally admits at
most 32 GiB of complete raw frame bytes before streaming. A monotonic sampled
120-second bound is checked between reads and at completion. No array-sized heap
allocation, conversion output, source rewrite or predecessor deletion is used.

Forty-three payload/inventory/status/native methods and twenty-five recovery/
native methods pass (fifty-two distinct methods, sixteen overlap). Nine payload
methods cover NaN and both infinities in every field with all seven predecessors
unchanged before attempt allocation; valid signed finite extremes; changed mask/
header-hash rejection; matching nonzero mask semantics; an 8,193-cell frame across
all chunk boundaries and mask tail; a nonfinite value in the final pressure cell;
active payload readiness holds; controlled sampled timeout/read failure; and short
plus interrupted read completion without copied-byte corruption. The existing
native status contract and supported headless scene-project cache fixture pass
with real producer masks/field arrays.

Retrospective proof compiles the sealed prior inventory reader under the same
fixture. Both report valid output ready; previous code also reports ready for
nonfinite density or a damaged mask, while current code refuses both and clears
readiness. Baseline SHA-256:
5791c0f2ed8dbe20a7c892a5f63212e00d2f95465b57f5f5b8a0fb591dacb7f5.

## Remaining requirements

Finite-value and FNV-mask validation are not cryptographic authentication of
complete payload bytes or source provenance. A finite field bit change can still
pass; FNV is not a security hash. Digest-bound source/candidate inventories,
immutable-generation readers and forward recovery remain required. Pack contents
remain unqualified. Payload verification reads complete raw frames and its GUI
responsiveness/large-cache cost has not been qualified; it is not a cached digest
readback or a hard I/O deadline. A blocked filesystem call can exceed the sampled
bound. Aggregate workflow/disk/copied-byte quotas, full hostile path-swap
confinement, archive-backed retirement, canonical adoption, installed/downstream
and CFD physical-accuracy qualification remain incomplete.

Evidence: data/experiments/lifecycle-validation/20261007-cache-payload-validation.
