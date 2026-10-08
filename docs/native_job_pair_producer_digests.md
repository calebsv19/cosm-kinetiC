# Producer-written job-pair content digests

New native retained publications use physics_sim_job_pair_v2. The fixed plan
includes exact SHA-256 digests for all four retained old/new roles and explicit
nulls for absent predecessors. physics_sim_job_pair_pending_v2 binds the exact
plan bytes through plan_sha256. Both records retain their existing 1 KiB / 256-byte
bounds, typed schema admission and cooperative publication ownership.

After each retained copy is flushed and byte-checked against its source, the
publisher reads it through an admitted descriptor and full named/descriptor
witness. It uses the existing core_scene_compile_sha256 public byte utility, then
writes and flushes the plan and its binding hold before promotion. There is no
new hash implementation. The one-shot helper allocates at most 1 MiB for one
admitted retained file at a time and frees it before proceeding to another role.
This is a bound on the hash buffer, not all JSON/parser/program memory. All
present pair members receive the 1 MiB preflight before retained attempt
allocation; failures still retain already prepared sidecar stages.

Recovery requires the v2 journal to match its hold digest, exact retained-role
inventory, each observed content hash and explicit absences before offering a
read-only plan. This detects retained-file or plan changes predating the first
recovery read. It supplements inode/time witnesses and the exact reviewed recovery
plan binding; no unsigned local digest proves producer identity or defeats an
actor that can coherently forge the entire receipt set. No signing/key trust or
external cryptographic anchor is claimed.

Legacy v1 remains separately admitted by its original schema and reports
producer_inventory_verified=false. A v2 mismatch does not fall back to v1.
Explicit legacy handling does not retrofit historical integrity evidence; its
older snapshot-authentication limitation remains. New-format read plans report
producer_inventory_verified=true only after complete digest admission. Existing
rollback intent/recovery bindings remain unchanged and bind the selected journal.

Verification: 14 recovery methods, seven native pair methods, 17 metadata methods,
28 field methods, 10 path methods and nine operation-guard methods pass (85
 distinct), plus ordinary, policy and bundle integrations. Native tests compare
all declared hashes and the hold's journal hash against Python hashlib, including
absent roles. Exact 1 MiB metadata digests match the oracle; an oversized
predecessor is held before attempt allocation with original bytes preserved.
Recovery tests alter each retained role or only journal representation before
first planning and require rejection. Explicit legacy recovery remains covered,
along with interrupted publication/rollback, partial stages and namespace/drift
gates from the earlier qualification.

An initial standalone test command omitted the actual-runner environment needed
by two guard tests; the authoritative Make invocation supplies it. A maximum-size
probe exposed that the original oversize test expected an earlier rejection than
the implementation provided. The publisher now moves the cap check before
retained attempt allocation, and the final full run passed. All attempts/logs are
retained; none of these failed test invocations is presented as acceptance.

Reuse-adopted: core_scene_compile 0.8.0 is already imported/linked into the source
runner/headless graph; its existing public one-shot SHA utility supplies digest
meaning. core_io/core_data/core_pack public headers do not expose a suitable
SHA-256 function. Fixed receipt roles, byte/resource bounds and lifecycle policy
remain app-owned. No shared source/API/version, vendor pin or minimum dependency
changed. Isolated native harnesses link the exact existing digest implementation.
Shared headers/source/VERSION are included in the packet for provenance. The
large-cache streaming-hash requirement is not solved by this bounded one-shot
metadata integration.

Evidence: data/experiments/lifecycle-validation/20261007-job-pair-producer-digests
contains source/tests/logs and a retained v2 native interrupted-pair/read-plan /
rollback example. Its toy fields prove metadata control, not CFD numerical
accuracy. No real user job, solver, commit, canonical adoption, installed product,
package or release changed. This packet is outside independently archived batches.
Forward recovery, authenticated producer identity, atomic reader generations,
full durable-boundary/platform faults, worker lifetime/log bounds, hard workflow
budgets, archive-backed retirement and canonical adoption remain incomplete.
