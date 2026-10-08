# Passed summary completion receipt admission

The actual prior runner accepted passed summaries with missing, running,
wrong-root or hardlinked output receipts (four fault cases). Headless also printed
PASS before its final ownership completion check, so later completion failure
could leave a false console result and a passed summary interpreted as success.

The existing strict marker reader is now exposed through a read-only app helper
for current completed output-root admission. It uses existing generated-storage
path admission, directory identity and the exact single-linked producer marker.
Passed-summary observation requires this receipt before accepting summary counters
or mutating job state. Missing/invalid completion evidence holds status/cancel and
preserves the original saved record; restoring the valid receipt permits normal
summary recovery. Existing identity/work/result-code JSON checks remain in force.
Terminal fixtures now carry producer-shaped output receipts so invalid-field
probes continue to test those fields rather than an absent completion marker.

Headless prints its final PASS/FAIL and artifact paths after final sidecar/output
ownership checks. This ordering is source-inspected and normal integration-tested;
it is not a separate fault-injected console test. Source summaries/progress are
still separate files published before final ownership completion. A failed final
check can leave passed sidecars behind; the new runner admission holds them rather
than silently rewriting them or claiming a coherent successful terminal state.
A poll during that publication interval may be held until completion is visible.

Verification: 23 actual-runner field methods, 10 path methods, 9 operation-guard
methods and 30 headless output/sidecar methods pass (72 distinct). Supported
ordinary runner smoke, bundle smoke and runner policy integrations pass. The new
method checks status and cancellation for all four receipt faults without record
mutation or a cancel flag, then verifies successful recovery with a valid receipt.
One verification invocation stopped after successful field/path targets because
of an incorrect operation target name; the corrected existing operation target
and both remaining integrations then passed. Both raw logs are retained.

The retained prior status source exactly matches current status source with only
this new include/admission removed. The current source caller builds adopt the
additive app helper; shared APIs/versions and on-disk formats are unchanged. No
canonical, installed, release, commit or archive-backed pruning change occurred.

This qualifies passed-summary observation, not all terminal records: legacy
completed records without summaries and failed/canceled outcomes need further
coherence audit. Receipts remain trusted local consistency evidence, not
cryptographic provenance or authenticated process lifetime/exit evidence. Summary,
progress, completion marker and process termination are not one atomic transaction.
Hostile check-to-use races, durable authenticated terminal inventories, complete
worker ownership/reaping, log/workflow budgets, immutable recovery and canonical
adoption remain open.

Evidence: data/experiments/lifecycle-validation/20261007-passed-summary-receipt.
The newer packet is outside independently archived batches.
