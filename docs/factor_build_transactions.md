# Reference factor compiler and record transactions

All 175 direct factor compiler calls in the 139 reference wrappers now use
retained staged compilation through the frozen CFD supervisor. Declared solver
and factor compiler command expressions were compared with exact pre-edit ASTs
and remain unchanged. Execution redirects only the output into the retained
candidate. For dylibs without an explicit install_name, the helper adds the final
output path as install identity so staging paths cannot leak into the library.
The caller's original cwd is preserved.

Each factor compile has a 60-second cooperative observation cap, direct-command
sampled RSS cap of one GiB and the retained helper's sampled log cap. Compiler and
SDK identity queries use bounded retained execution (15 seconds, sampled 64 KiB
combined logs and bounded final text read). Limits remain per operation, not
aggregate/hard kernel or complete-descendant memory quotas.

Successful compiler bytes publish through an atomic no-replace hard link; failed
attempts retain source/request, candidate when produced, receipt and stdout/stderr.
Existing or unknown libraries are not replaced. Factor metadata is retained and
fsynced in that compile attempt before atomic no-replace record publication.
Version-two records bind requested compiler argv, output digest and exact compile
receipt digest. Bounded no-follow FD reads reject duplicate/nonfinite metadata,
links, special files, over-limit inputs and observed identity drift. Proof reads
are capped at 64 KiB and library digest reads at 64 MiB. Reuse checks the recorded
compile request and current library/receipt bytes. Old unbound records stay held.
The updated helper source digest creates a new source packet namespace rather
than upgrading or resetting historical prefixes.

The final six-method family suite passed against the fixed helper source. It
includes all 139 factor wrappers using real Clang/SDK recipe execution with small
controlled C sources, 278 fresh synthetic numerical success/nonzero cases and
cached readbacks; it also includes four plain-family control methods. Six focused
transaction checks and five retained compiler checks passed separately: 17 test
methods total. They cover real dylib loading and final install identity, failed
compilation, predecessor/candidate preservation, changed library/proof refusal,
metadata duplicates/links/FIFOs/bounds and retained tool readback.

A separate sealed 16-file native control packet persists the real library,
source, compile/build records, candidate and identity logs at
`data/experiments/lifecycle-validation/20261007-factor-native-control`.
Its controlled function returned 7, final install identity matched and exact
record reuse did not mutate bytes. This does not qualify actual CFD factor ABI,
Accelerate numerical correctness, a physical accuracy gate or installed product.
The first family run preceded the final no-follow tightening; its log is retained,
and the final complete run verifies the fixed source. Transformation attempts
that stopped before or during wrapper editing were repaired before verification.
The source baseline/audit records preserve exact final scope.

Complete writer/root admission, transitive toolchain/config identity on cached
reuse, aggregate quotas, forced-death multi-file recovery and explicit owner
reconciliation of interrupted prefixes remain open. A published library with
missing/partial metadata is held, never automatically adopted or reset. Complete
descendant termination and retirement eligibility remain unverified. Canonical
adoption, commit, installation, release, backup transfer and pruning did not occur.
New packets are outside the frozen prepared backup cutoff.
