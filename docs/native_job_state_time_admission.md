# Native job state and UTC timestamp admission

Persisted status labels, when provided, must be one of queued, starting, running,
stalled, completed, cancelled or failed. Unknown or empty labels hold loading
before refresh, cancellation or job metadata publication. Worker progress keeps
its existing distinct pending/running/finishing/passed/canceled/failed vocabulary.

Nonempty consumed UTC timestamps must exactly match YYYY-MM-DDTHH:MM:SSZ.
Digit parsing is bounded and avoids scanf integer overflow. The parser checks
component ranges and an exact UTC calendar roundtrip, rejecting normalized
impossible dates, trailing text, offsets, non-padded fields and leap-second 60.
Empty optional timestamps remain valid reservations. The four status timestamps
and progress updated_at_utc are checked before they influence job state.
A representable timestamp equal to time_t -1 is valid if its calendar roundtrip
matches; it is not confused with conversion failure.

Four new behavior cases, comprising multiple malformed field/date variants,
exercise the actual runner. Holds preserve status bytes and do not publish a
cancel flag. Exact leap dates and empty optional timestamps remain compatible.
The focused suite passed 14 cases; 74 affected native-job cases and all six
supported source-checkout integration fixtures passed on the rebuilt runner.
This is local Main Edit validation, not installed-product or numerical acceptance.

Required-field schemas, timestamp ordering, future-clock policy, counter
consistency, authenticated worker lifetime, log quotas and recovery across
multiple metadata files remain open. Unknown extension fields are unchanged.
No canonical adoption, commit, prune or archive transfer occurred. This packet
is outside the earlier independent backup and the pending frozen seventy-packet
snapshot; the latter must not be modified under its existing transfer question.

Evidence: data/experiments/lifecycle-validation/20261007-native-job-state-time-admission.
