# Cleanup metadata JSON resource admission

The generic cleanup metadata reader now preflights at most 64 nested containers
and 100,000 structural events before JSON tree decoding. Events are opening
containers and commas/colons outside strings; escaped quotes/backslashes and
punctuation inside strings do not spend that budget. JSON itself still validates
syntax and complete input. Generic object, array and scalar roots remain supported.

Integer/float tokens are limited to 128 characters before numeric conversion.
Floating overflow is rejected alongside explicit nonfinite constants. Valid finite
values and integer values within the representation budget remain allowed; this is
not a domain-specific counter/range validator. The reader retains its caller-owned
byte cap, single-link regular file admission and before/after read/decode named /
descriptor witnesses. Depth/event policy follows the existing reference metadata
admission boundaries; the cleanup helper stays independent of object-only frozen
solver supervision and introduces no new module dependency.

Before-code probes demonstrated four acceptance subcases: depth 65, event 100,001
and positive/negative floating overflow. A subsequent focused counterprobe showed
both integer and finite float tokens of 129 characters were accepted. Six new
methods cover inclusive depth/event/token boundaries, escaped string punctuation,
finite values, preserved duplicate-field rejection and malformed complete syntax.
The old explicit nonfinite constant and ordinary JSON behavior remain covered.

Final validation passes 107 distinct methods: six JSON bounds, six read admission,
eighteen cleanup, twelve atomic publication, one build identity, thirteen doctor,
four evidence lifecycle, seven retention, 27 package transaction, eight retirement
plan and five restore methods. Read-only cleanup preview continues to admit the
existing owned profile. No cleanup apply ran against user storage. No physical,
installed, signing/platform or repository-wide completion is inferred.

These are bounded trusted-local metadata reads, not a public upload interface.
The byte buffer and decoded tree still allocate within the selected class bounds;
aggregate workflow memory/storage and hard I/O/decode deadlines are separate open
requirements. Complete hostile path-race confinement, remaining writer behavior,
archive-backed retirement, full build provenance and canonical adoption remain
incomplete. No commit, installation, release, deletion or archive coverage changed.
Evidence is sealed in 20261007-clean-json-bounds, outside the frozen backup batch.
