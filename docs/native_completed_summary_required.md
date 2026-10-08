# Completed-state summary requirement

The prior runner returned a saved completed state without any summary. A terminal
progress-only probe attempted to publish a completed status record before a later
shared-report publication error, changing saved evidence despite returning an
error. An output completion receipt alone was not enough to admit that state.

Refresh now refuses a candidate completed state when no admitted summary is
available, before metadata publication. This covers saved legacy completed labels
and completed candidates derived from final progress. Existing passed-summary
admission also requires the current strict completed-output receipt. Neither the
saved label nor final progress alone establishes success. Missing/invalid evidence
holds status and cancellation without rewriting originals or adding a cancel flag.
No legacy record is migrated, assigned an invented outcome or pruned.

The calendar-admission fixture now has positive completed work counts, a matching
passed summary, a completed receipt and consistent terminal metadata, so its
successful timestamp checks continue exercising the completed-state path. Two
fixture-repair runs are retained in logs; the final suite is authoritative.

Verification: 24 actual-runner field methods, 10 path methods and 9 operation-guard
methods pass (43 distinct). Ordinary, policy and bundle job-runner integrations
pass. The new method covers both saved completed records and final progress
without a summary, checking status/cancel holds and exact original record bytes.
The counterproof preserves both distinct prior failures rather than calling the
progress case a clean successful command. The prior status source exactly matches
current source with this new guard removed.

This completes the missing-summary gate for completed states, not every terminal
coherence requirement. Failed/canceled states, complete schema/provenance/digest
binding, authenticated process lifetime/exit, hard quotas and race confinement
remain open. Status and shared-report writes still form separate single-file
publications; other errors may leave one updated before the other fails. Terminal
artifacts and process death are not one atomic transaction. Receipt/summary
observation may hold while final artifacts are being published. Explicit recovery,
archive-backed retirement and canonical adoption remain outstanding.

Reuse: this adds an app-state admission condition to the existing candidate-first
refresh path and existing summary/receipt validators. No shared API/version,
on-disk schema, solver, installed product, release or canonical adoption changed.
Evidence: data/experiments/lifecycle-validation/20261007-completed-summary-required.
This new packet is outside independently archived batches.
