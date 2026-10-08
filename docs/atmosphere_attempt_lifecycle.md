# Retained native atmosphere attempts

The passive, evolving and open-atmosphere `run` adapters now allocate fresh
`<experiment-root>/atmosphere-attempts/<role>-<uuid>` capsules instead of automatic
temporary-directory removal. Existing worker ownership spans capture and
numerical acceptance. Existing numerical gates and returned results are unchanged.

Each attempt retains the original input, exact native request, raw stdout/stderr,
parsed-field input when available, worker payload and selected control sources.
Requests and fields use separate files. Receipts start running; only successful
worker capture, verified teardown, stable original/preserved inputs and completed
numerical acceptance allow completed status and `accepted.json`. Failure keeps
its diagnostic capsule and names it in ValueError messages. Unverified teardown
stays held. Reruns allocate new identities; no automatic retirement is performed.
Source payloads are retained for inspection and are not automatically executed.

Capture retains the existing 120-second wall limit, stdout limit selected by the
adapter (64 MiB default, 256 MiB maximum), 64 KiB stderr limit and owned process
group teardown. The helper bounds worker capture at 128 MiB, each of eight
selected source files at 4 MiB and retained JSON at 256 MiB. JSON encoding and
filesystem checks are cooperative; these are not hard RSS/disk or whole-workflow
limits. Source files and receipt writes are flushed, including source namespaces
before worker effects. Receipt replacement is atomic under the trusted-local
cooperative path model. Forced death can leave running receipts, which remain
held; recovery and retirement still require separate work.

Selected source hashes detect on-disk drift. They do not authenticate imported
bytecode, interpreter, libraries or all transitive inputs. Complete dependency
capture remains false. These controls are local operator protections, not public
upload sandboxing or descriptor-relative filesystem isolation. Broader coupled
and movie adapters require their own writer review.

Validation: 22 focused and worker lifecycle checks, plus 21 existing numerical
adapter regressions, passed. A fixture quoting error and an incomplete malformed
request fixture were corrected before final runs; their logs remain in evidence.
Existing explicitly selected worker binaries were used without rebuilding or
replacement. These regressions preserve existing numerical behavior; they do
not resolve the independent cube/transient accuracy qualification gates.

Evidence: `data/experiments/lifecycle-validation/20261007-atmosphere-retained-attempts`.
No canonical adoption, package, release, deletion or backup of this new packet
occurred. The prepared independent snapshot does not cover these later attempts.
