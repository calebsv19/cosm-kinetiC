# Prepared native job metadata stages

The existing single-file job metadata writer now exposes an additive prepare /
publish / discard lifecycle. This is a bridge toward complete coordinated
status/report publication, not the completed transaction.

Prepare writes no destination. It checks stream errors, flushes and fsyncs the
pending descriptor, admits the bounded JSON object or text size, and binds the
named stage, retained descriptor and stream to a full inode/mode/link/size and
nanosecond modification/change-time witness. It retains the cooperative metadata
lock and predecessor/directory descriptors after success. Failure consumes the
stream, releases descriptors and retains the pending file. Discard likewise
retains the stage without publishing or claiming persistence success.

Publish first flushes any unexpected buffered writes, rechecks the prepared stage,
operation guard, metadata lock and predecessor, then uses the existing single-file
publisher. Changes to buffered content, same-byte stage rewrite or replacement,
and predecessor changes hold without replacing the destination. This is a
cooperative consistency witness, not cryptographic authentication. Successful
prepare owns the stream until exactly one publish or discard; failure consumes
it. The helper does not support reuse of consumed handles.

The existing finish API composes prepare and publish. Its consumers therefore
retain ordinary single-file behavior while receiving full staged-witness checks.
Neither status nor report has yet been converted into a coordinated pair. The
current pair still writes status then report; a later I/O/namespace failure can
leave mixed generations. No pair journal, durable hold, predecessor retention,
recovery or generation visibility is established by this bridge.

Verification: 17 native metadata methods pass, including interrupted prepared
stages, lock contention while prepared, explicit retained discard, initial and
replacement publication, buffered/same-byte stage change, inode replacement,
predecessor change and a two-file staged rejection. The latter prepares a valid
first file, rejects an invalid second file and discards the first, proving exact
preservation of both originals and subsequent lock availability. It does not
publish a pair or qualify interruption between two promotions. The 26 field,
10 path and 9 operation-guard methods also pass (62 distinct across these suites),
as do ordinary, policy and bundle integrations. The first complete run had 16
metadata methods; a final 17-method run adds the paired rejection test. No source
changed between those runs, only that final test addition.

Reuse: the app keeps its existing headless sidecar publisher and strict JSON
reader, with purpose-specific lock/stage ownership in PhysicsSimJobFile. There is
no new generic storage semantic contract, shared API/version or minimum dependency
change. The app source handle adds fields; no installed-binary ABI compatibility
is claimed. Canonical adoption, worker lifetime/log bounds, archive retirement,
full coordinated recovery and the broad lifecycle objective remain incomplete.

Evidence: data/experiments/lifecycle-validation/20261007-job-file-preparation.
This new packet is outside independently archived batches. No commit, numerical
algorithm, package, installation or release was changed.
