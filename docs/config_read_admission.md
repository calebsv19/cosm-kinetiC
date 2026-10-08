# Configuration file read admission

Main Edit configuration reads now walk path components using nofollow directory
and leaf descriptors. Source-checkout config files remain supported: read
admission is distinct from generated-output admission. Parent traversal and
private .git/.ssh/.aws components are refused before opening any component;
paths are bounded at 4095 bytes and 128 components. macOS /tmp and /var aliases
map explicitly to /private paths; other linked components hold. The final file
must be regular, single-link, nonempty and at most 1 MiB.

Size/type/link count are checked again before bounded allocation. Reads consume
exact admitted bytes with EINTR handling, require EOF, reject embedded NUL and
compare descriptor identity, mode, size, link count and nanosecond modification/
change timestamps before and after. A fresh nofollow reopen must match the same
witness before acceptance. Growing, shrinking, replaced or concurrently modified
inputs hold. Both read descriptors must close successfully. The reader creates
no roots/files/records, does not execute source, and does not mutate input bytes.

allow_missing now applies only to actual ENOENT after full lexical admission.
Linked/dangling, special, empty, oversized, changed or invalid paths fail even
when that option is true. `config_loader_load` still seeds defaults at entry as
its existing API requires; false indicates that the supplied configuration was
not admitted. Startup currently logs false and explicitly continues with those
defaults. That application policy is distinct from a successful missing fallback.

Eight actual compiled-native methods pass: ordinary/relative/source config reads,
missing-only fallback with no allocation, linked leaf/parent/dangling holds,
FIFO/directory/hardlink holds without blocking, oversized/empty/NUL refusal,
controlled replacement/in-place modification, preallocation size change and
full path syntax/private-component refusal. Existing six configuration-save and
fourteen persistence methods also pass; the actual isolated configuration Make
roundtrip contract passes in `build/profiles/config-read-20261007`.

The first test exposed a missing-before-traversal fallback bug. Full lexical
preflight repaired the implementation; the failed and passing logs are retained.
The fault probes change their own disposable input to exercise races. No user's
configuration or source input was changed by those probes.

Limits remain material: this is metadata/cooperative race detection, not a
cryptographic content receipt or hostile filesystem confinement. Regular file
I/O has no hard interruption deadline. UTF-8, full JSON depth/structure/duplicate
keys, numeric conversion ranges and schema admission are not established by this
slice. The old substring parser still accepts some malformed content, and save
path escaping/nonfinite numeric handling remain open. Ordinary roundtrip output
is not arbitrary-input acceptance. Full configuration semantics needs reader and
writer compatibility proof; this file admission repair does not replace it.

Reuse is app-local configuration policy. core_io whole-file read supplies no
nofollow component/type/witness admission or this fixed resource/fallback policy;
core_data/core_pack concern data/format semantics. No shared API/version or
adoption state changed. No canonical adoption, commit, cleanup, pruning, install,
package or release occurred. This local packet is outside both frozen backup
scopes. The overall lifecycle goal remains incomplete.

Evidence: `data/experiments/lifecycle-validation/20261007-config-read-admission`.
