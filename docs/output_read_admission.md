# Cleanup ownership and metadata read binding

Disposable output recording and verification now require regular single-linked
files. Fingerprints compare full descriptor and named-path identity before and
after hashing, including stable link count. Existing six-field v1 receipt identity
and byte SHA-256 representations remain unchanged; valid old single-linked
receipts need no adoption or rewrite. Generic trusted toolchain fingerprints may
read stable multiply linked files, while disposable ownership always requires one
link. This distinction preserves legitimate system compiler/SDK files.

The reused cleanup JSON reader now requires bounded regular single-linked input,
matching descriptor/named witnesses, exact original-size bytes and unchanged
identity after read and decoding. Duplicate fields and nonfinite constants retain
their existing rejection. Changes or same-byte replacement during decode hold
before the decoded metadata can establish a cleanup plan. No unknown output is
adopted, and no cleanup filename exceptions were introduced.

Before-code behavior probes demonstrated four acceptances: hardlinked output
recording, hardlinked JSON, decode-time metadata mutation and same-byte receipt
replacement. The same-byte output hash replacement already held through ctime
checks and remains a regression case. Six focused methods cover these cases and
normal admission. An initial repair applied unique-link requirements too broadly
to tool fingerprints; the build-identity regression exposed a legitimate linked
system tool. The final implementation makes the ownership requirement explicit
only at disposable record/verification call sites. The failure log is retained.

Final validation passes 112 distinct methods: six admission, eighteen cleanup,
twelve atomic publication, one build identity, thirteen doctor, four evidence,
three 58-recipe fixture ownership, one earlier 48-recipe lifecycle, seven retention,
27 package transaction, eight retirement plan, five restore and seven final release
artifact methods. Controlled recipe tests verify lifecycle behavior rather than
solver results. Existing owned profile cleanup preview still admits its current
file inventory. No cleanup apply ran against user profiles.

Named-path witnesses are sampled checks, not complete hostile ancestor-race
confinement. Per-file/aggregate hash budgets, hard I/O deadlines, broader namespace
ownership, retirement and canonical adoption remain open. Receipt publication
transactions and whole-tree deletion races are separate requirements. No commit,
canonical adoption, installation, release, archive transfer or user-output deletion
occurred. The sealed packet is outside independently archived coverage. This
advances TL02/TL06 admission without completing the broader lifecycle contract.
