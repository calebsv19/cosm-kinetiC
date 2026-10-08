# Ancestor package reservation cleanup admission

Main Edit cleanup preflights all ancestor `.package-reservations` directories
before reading any reservation. One set across the selected root's ancestors is
limited to 1,000 receipts and 64 MiB total bytes, with the existing 1 MiB per-file
limit and at most 64 ancestors. Only single-linked regular `.json` entries are
admitted; other names, directories, links and special files hold cleanup rather
than being ignored. Missing reservation directories are witnessed too.

Directory enumeration uses nofollow descriptors and named/descriptor witnesses.
After reading reservation metadata and classifying the selected tree, the entire
reservation set is rescanned. `clean_outputs.plan` checks the same original
witness set again after output hashing, so changes during that later phase also
hold the whole plan. Byte-identical rewrites, new entries and newly created
previously absent reservation directories are detected. Existing output-overlap
refusal remains intact. Reservation receipts are preserved, never pruned here.

Each reservation scan and the read/classification pass have sampled 120-second
checks. These are not hard deadlines for blocked filesystem calls. Whole-plan
atomic deletion and complete hostile ancestor-race confinement remain unproven;
cooperative owner locks and witnesses retain their scoped role. The semantic
reservation schema has not changed: broader schema/producer authentication is a
separate requirement. Limits intentionally hold unusually large retained sets;
no automatic eviction or permission to delete follows from a budget hold.

Before-code counterproof runs eleven methods with ten failures. The focused
repaired suite passes all eleven methods, including aggregate pre-read bounds,
non-JSON/non-directory holds, mutation during reads and mutation during output
hashing. The authoritative Make gate passes 130 distinct methods across these
checks, build inventory, cleanup, package transactions, doctor, JSON/read
admission, atomic outputs, build identity, evidence, retention, retirement and
restore. An earlier test recipe used an unset variable and skipped the new suite;
it was corrected to the repository's explicit python3 convention, and this
claim relies on the subsequent authoritative gate with all eleven tests run.
Read-only owned-profile preview still admits 721 files; no user cleanup applied.

No canonical adoption, commit, installation, release, deletion or independent
backup coverage changed. Evidence is sealed in 20261007-clean-reservations and is
outside the frozen seventy-packet backup batch. The broader lifecycle contract
remains incomplete, including provenance, retirement and coherent readers.
