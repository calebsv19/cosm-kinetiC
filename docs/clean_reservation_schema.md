# Package reservation ownership schema admission

Main Edit cleanup now admits the two current PhysicsSim reservation schemas
before using their output paths. Every record requires the exact owner schema,
local-package artifact class, canonical UUID4 attempt identity and known state.
Transaction records have exactly five fields. Fresh records additionally require
the exact reservation directory's parent root and literal false release authority.
Unknown/missing/extra fields and contradictory values hold the whole cleanup plan.

The output must be an absolute canonical path inside that reservation root;
transaction outputs must be strict descendants. Control characters, dot/parent
aliases, linked output components and cross-root paths hold. The receipt filename
must equal SHA-256 of the exact declared output string plus `.json`, matching both
current producers. Reservation roots must be supported strict build descendants
or the dist namespace. Output-overlap protection remains conservative for both
formats, including outputs that do not yet exist. No receipt is rewritten or
adopted by this reader; a contradictory record requires deliberate reconciliation.

This establishes structural identity consistent with the current producers. It
does not authenticate historical producer execution or authorize release, deletion,
retirement or installed-package adoption. Existing aggregate limits and complete
before/after reservation witnesses remain in force. Full hostile path-race
confinement and hard filesystem deadlines remain open requirements.

Corrected before-code counterproof has 23 accepted malformed subcases across
three failing methods; two compatibility methods pass. The initial probe had an
absent-file setup error in its actual fresh-producer test, corrected in both old
and new fixtures before relying on the result. Five new methods cover missing/
wrong/extra fields, output aliases/containment/name binding, exact fresh authority,
the actual fresh producer and transaction overlap behavior. Earlier bounded-scan
fixtures now emit valid current producer records and retain all eleven methods.

The authoritative Make gate passes 94 distinct methods: five schema, eleven
reservation-scan, eight package output, 27 package transaction, eighteen cleanup,
twelve build-inventory and thirteen doctor methods. The read-only owned-profile
preview still admits 721 files. No user storage cleanup was applied.

No canonical adoption, commit, installation, release, deletion or independent
backup coverage changed. Evidence is sealed in 20261007-clean-reservation-schema,
outside the frozen seventy-packet backup batch. The broader lifecycle contract
remains incomplete, including full build provenance, archive-backed retirement,
worker lifetime and coherent readers.
