# Declared scene-cache inventory and raw frame structure

Main Edit cache publication admits the exact declared artifact set before creating
an attempt, rechecks both copied staged directory pairs before any predecessor
rename, and uses the same contract for active status readback. Volume output must
have exactly one admitted preset directory; ambiguous sources and unexpected
root entries hold rather than selecting by filesystem enumeration order.

The required v2 VF3D manifest has positive XYZ dimensions and exactly the declared
frame count/selection sequence. Every entry names its canonical frame_NNNNNN.vf3d
file with the matching ordinal and vf3d contract. Both VF3D and physics directories
must contain semantically identical volume manifests. The physics scene bundle
must name physics_scene_bundle_v1/version 1 and link the local manifest.json with
kind=manifest and contract=vf3d. Missing/extra/noncanonical frame or optional pack
stems hold. Pack conversion is optional in the exporter; matching pack stems are
retained but pack format/content is not qualified by this check.

Each declared raw frame is opened nofollow and checked against the existing
exporter-owned native-layout v1 header: magic/version, ordinal, dimensions,
finite time/dt/origin/voxel/up fields, nonnegative dt, positive voxel size,
nonzero scene-up, zero reserved words, overflow-safe cell arithmetic and exact
header-plus-five-float-arrays-plus-byte-mask length. Byte limits are 8 GiB per
frame; existing tree preflight limits combined metadata sizes to 32 GiB and
10,000 entries/depth 16. The subsequent `cache_payload_validation.md` slice now streams all fields
for finite-value checks and verifies the producer mask hash. Filenames use bounded digit arithmetic, not overflowing scanf
conversion. Metadata admission is now at most 1 MiB, with the existing strict
parser depth/value bounds; the earlier 16 KiB limit was inadequate for per-frame
volume manifests.

The existing application header struct moved unchanged into
include/export/volume_frame_vf3d_contract.h for reuse by exporter and admission.
No binary format, solver, shared API/version, installed package or release changed.
core_pack offers conversion that loads arrays and writes a pack; it is not used
as a bounded read-only cache validator. This slice reuses the producer-owned raw
contract and existing strict parser/tree admission instead of introducing another
header definition or a conversion side effect.

Thirty-four inventory/status/native methods and twenty-five recovery/native
regressions pass (forty-three distinct methods; sixteen overlap). Eight inventory
methods exercise missing source artifacts, manifest/bundle linkage, bad header
fields, empty/truncated/trailing/oversized frames, extra/noncanonical/overflowing
names and ambiguous sources, silent staged copy corruption preserving all seven
predecessors, missing/corrupt active frame readback and copied-manifest agreement/
optional-pack behavior. Earlier path/ownership/death/rename/recovery proofs remain
covered. Controlled fixtures now carry structurally valid raw headers/payload
lengths and producer-shaped manifests/bundles rather than arbitrary frame text.
The real status Make contract and supported headless scene-project cache fixture
pass against actual exported VF3D files.

Retrospective proof compiles the sealed previous status reader under the same
valid fixture. Both readers report valid output ready; previous code also reports
ready with a missing or truncated declared frame, while current status refuses
both and clears readiness. Baseline SHA-256:
a10afa07cb659160fad8fd2273bfa8ff1d613c9dae8a8a9f521d3e7c9ebf2929.

## Remaining requirements

Structural completeness does not authenticate complete payload bytes or source
provenance, qualify physical field values, pack contents or prove
CFD physical accuracy. Candidate digests/source freeze and immutable-generation
readers are still required for forward recovery and full coherent readback.
Static/per-entry witness checks are not complete hostile path-swap confinement.
Hard filesystem deadlines, disk reservation/aggregate copied-byte quotas,
archive-backed retirement, canonical adoption and installed/downstream
qualification remain incomplete. Newer evidence is outside the frozen prepared
backup batch awaiting exact PC destination approval.

Evidence: data/experiments/lifecycle-validation/20261007-cache-inventory-admission.
