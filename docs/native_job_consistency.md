# Native job identity and counter consistency

Present status schema/program/tool/artifact-class tags must identify the native
operational job format. A present request_path must bind to the selected job's
canonical request slot. Overwrite policy is overwrite or fail_if_exists. Optional
absent identity tags remain compatible; this does not authenticate provenance or
implement a complete required-field schema.

A progress declaration cannot change the recorded requested frames, steps per
frame or output root. Present schema and artifact-class tags must match the
producer's actual progress format. Invalid relationships hold refresh/cancel
before publication: completed frames exceed requested frames; active completed
steps exceed their total; total steps exceed configured steps; a completed job
has unfinished requested frames; or its frame index is outside its work range.
Zero work/default records retain zero indices. The producer's canceled/failed
boundary representation permits index equal to requested frames only when all
frames are completed and active step totals are zero. Ordinary frame indices are
otherwise strictly below requested frames.

Merge is transactional in memory: a temporary record receives progress, and only
a fully admitted candidate replaces the caller's record. Invalid observations do
not partially modify that record or publish a cancel flag. Optional fields retain
current defaults/record values, so this is effective-record consistency rather
than proof of complete metadata. Monotonic progress across snapshots, summary
identity/consistency, request-content authentication and state transition history
remain open. Existing cancellation/failure final progress may reset counts; no
monotonic requirement is inferred from those producer semantics.

Four new behavior cases cover multiple counter/tag/path failures and valid frame,
completion and canceled-boundary representations. The focused suite passed 18
cases; 78 affected native-job checks and all six integration fixtures passed.
The synthetic stall fixture now points at its canonical request slot and uses a
matching progress output root, retaining its original stall assertions. Earlier
failed checks/logs remain retained; they did not justify weaker admission.

This is Main Edit source validation. No canonical adoption, commit, pruning,
archive transfer, installed-product or numerical acceptance occurred. This packet
is outside both the earlier independent backup and pending frozen 70-packet
snapshot. Worker identity/lifetime, log quotas, multi-file recovery and the full
writer/retirement audit remain incomplete.

Evidence: data/experiments/lifecycle-validation/20261007-native-job-consistency.
