# Configuration JSON structure, field representations and save validation

Main Edit configuration parsing now uses actual object lookup instead of
substring/object-brace searches. It reuses the existing job-metadata strict JSON
byte parser through an additive app-owned `physics_sim_job_json_parse` entrypoint.
The job file reader delegates its already-admitted bytes to that same parser;
file admission policies remain separate. There is no duplicated grammar or
shared-core API/version change.

The reused parser accepts a complete UTF-8 JSON object under depth 64 and 100,000
value limits, rejects trailing content, duplicate decoded object keys, nonfinite
numbers and embedded/decoded NUL strings. Configuration file admission retains
its stricter 1 MiB byte bound. Full JSON parse happens before consuming fields.
Every present known section must be an object; every one of the 59 consumed known
fields has a representation check before overrides are applied. Missing fields
retain defaults, and unknown structurally admitted fields remain forward
compatible. Integer destinations require an integral finite value within C int
range. Float destinations require finite float-representable range; double
fields require finite values. Strings must fit their actual fixed destination
without silent truncation. Boolean fields accept JSON booleans and historical
finite numeric nonzero/zero values. Integral JSON doubles remain compatible.
Aliases remain supported, but conflicting pairs hold; matching aliases work.

Saved path strings now use JSON escaping and read back decoded quotes, slashes,
controls and Unicode correctly. All four serialized fixed path strings must
terminate in their declared arrays before staging. Completed config candidates
are flushed, bounded at 1 MiB, read through their retained descriptor and parsed
under the same structure/known-field contract before publication. Invalid UTF-8,
nonfinite emitted numbers or failed validation abort publication, close ownership
and retain the pending candidate. An additive app-local persistence abort API
supports that explicit failure outcome. Existing checked publication, ownership,
predecessor retention and unconfirmed post-rename semantics still apply.

This also fixes saved JSON true/false values being ignored by the prior numeric
substring parser. Existing valid serialization field set and numeric formatting
are retained. A process locale producing invalid numeric JSON now holds the
candidate rather than publishing it; locale-specific successful formatting is
not qualified. The save field set still does not persist every readable option;
that older asymmetry needs separate review.

## Verification

Twelve compiled semantic methods pass: optional defaults/unknowns/legacy aliases,
matching versus conflicting aliases, booleans, malformed/trailing/root types,
decoded duplicate/NUL handling, UTF-8/depth/value-count bounds, known types,
integer/fraction/float overflows, overlong paths, escaped Unicode/control paths,
nonfinite/invalid UTF-8 save predecessor retention, unterminated memory strings
before allocation, and section-name text isolation. Eight read-admission, six
save-lifecycle and fourteen persistence regressions also pass (forty configuration
and persistence methods total). Existing job JSON reader nine and atomic job
publication eleven methods pass after parser reuse. The actual configuration Make
roundtrip passes in `build/profiles/config-json-20261007`.

A source inventory matches all 59 consumed fields to representation declarations,
with no missing fields. This is coverage bookkeeping alongside behavior tests,
not a substitute for them. The new native reader also admits both current
Main Edit and canonical `config/app.json` bytes without changing either file
(1,134 bytes, SHA-256 f114aaa1cf38121c564a51a6e37487b965bb99a4f6bb2c45d002e5877853a96a).
That is input compatibility evidence, not canonical code adoption. Native probes
use current pkg-config json-c; the actual Make proof uses its selected JSON
flags/libs, recorded in the log. No dependency upgrade was performed.

Physical parameter limits, enum admissibility beyond existing selection logic,
cross-field timestep/grid relations, complete persistence of all options,
allocation-failure campaigns, hard I/O deadlines, hostile staging/path-swap
confinement and installed/package qualification remain open. Job parser reuse is
app-local; core_io/core_data/core_pack have different I/O/data/format ownership
and no shared extension is made here. No commit, canonical adoption, cleanup,
pruning, install, package or release occurred. This packet is local and outside
both frozen backup scopes; the broader lifecycle goal remains incomplete.

Evidence: `data/experiments/lifecycle-validation/20261007-config-json-contract`.
