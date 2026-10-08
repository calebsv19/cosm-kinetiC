# Reference artifact hashing budget

All 194 reference wrappers now publish artifact hashes through one whole-pass
admission helper; cached receipts use the same helper. Existing artifact paths
must match the wrapper-owned inventory. Every present path is admitted regular,
no linked components, at most 8 GiB, before any artifact content is read. The sum
of logical sizes must be at most 16 GiB. Oversized passes and later special files
hold before reading an earlier otherwise-valid artifact.

Hashing uses each admitted size as the streaming read bound. Growth is held rather
than consuming the looser per-file bound. Descriptor/path witnesses from the
existing digest helper remain enforced. Before/after witnesses across the entire
pass detect changed previously-read files, and declared artifact presence is
rechecked after hashing. Normal nofollow byte/identity witnesses do not establish
an adversarial ancestor-swap sandbox or authenticated receipt provenance.

The cap counts actual declared paths, including multiple reads of hard-linked
aliases, rather than reducing by disk allocation. Over-limit failures retain
original output and existing receipts. A fresh over-limit pass holds before
receipt publication; it does not manufacture a complete receipt, prune the
artifacts or automatically reset the run. Interrupted-prefix recovery is separate.

Focused behavior checks cover exact-budget success, a too-small aggregate budget
before reads, sparse files over the default cap without reading their contents,
a late special file, growth during digest and drift of an already-read file.
Family tests retain synthetic numerical probes and real controlled factor build
libraries. Solver/compiler command assignments compared unchanged for all 194
wrappers; this does not qualify physical accuracy or real factor ABI.

These are per-pass read limits. The underlying regular-file read can still block
on filesystem I/O; this does not impose an I/O deadline, disk-space quota,
aggregate source-freezing budget or whole-workflow quota. Resource-policy
completion, namespace locks, coherent interruption recovery, retirement/pruning
and canonical adoption remain open.

Evidence packet: `data/experiments/lifecycle-validation/20261007-reference-hash-budget`.
