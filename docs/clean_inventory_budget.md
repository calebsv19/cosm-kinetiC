# Cleanup inventory resource budgets

Main Edit cleanup now preflights the selected build tree and explicitly named
root executables before reading ownership receipts or hashing output bytes.
Admission permits at most 100,000 entries, depth 64, 1 GiB per file, 8 GiB total
output bytes and 64 MiB of JSON metadata plus ownership receipts. Each receipt
also retains its 16 KiB limit. Fixed policy limits are inclusive; an oversized
late file holds the whole plan before any earlier output hash starts.

Enumeration uses nofollow directory descriptors and named/descriptor witnesses.
Only single-linked regular files and real directories are classified. Every
selected entry, root executable presence/absence and ownership receipt is
witnessed again after hashing. A new entry, changed previously hashed file or
same-byte rewrite of an already consumed receipt holds the whole inventory.
Hashing owned disposable files admits only their original size plus one byte to
check growth. Generic trusted toolchain fingerprints retain their existing
compatibility behavior; this policy does not impose disposable limits on tools.

A sampled 120-second budget applies to each scan and the ownership/hash pass.
It cannot interrupt blocked filesystem calls and is not a hard workflow deadline.
Separate classification and inventory passes do not share one overall deadline.
The build scan does not bound ancestor package-reservation enumeration. Complete
hostile ancestor-race confinement and atomic deletion of an entire validated tree
remain separate open requirements.

Before-code probes demonstrated seven failing methods across aggregate bytes,
late oversized files, entries, depth, metadata receipts, prior-file mutation and
hash growth. Twelve final focused methods include inclusive boundaries, combined
root/build budgets, new entries, receipt rewrites and sampled time refusal.
Across the first regression gate and final focused/consumer gate, 123 distinct
methods pass. Evidence, retention, package transactions, retirement plans and
restore consumers remain compatible. Existing read-only owned-profile cleanup
preview admits 721 files. No cleanup apply ran on user storage.

These protections are in Main Edit. Canonical adoption, full build provenance,
class retirement, coherent readers and whole-lifetime resource limits remain open.
No commit, package installation, release, deletion or independent backup coverage
changed. This packet is outside the frozen seventy-packet backup batch.
