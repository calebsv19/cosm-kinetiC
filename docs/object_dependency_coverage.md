# Declared Clang object dependency coverage

Main Edit derives generated dependency admission/inclusion from one deduplicated
`CLANG_DEPENDENCY_OBJECTS` set in `make/objects.mk`. It covers ordinary application/
shared objects, SDL compatibility objects, mapped shape support objects, standalone
shape tools, headless/runner/session CLI objects and headless worker/stub objects.
Only actual `.o` entries participate: the historical SHAPE_SHARED_OBJS variable
also contains direct C link inputs. The same declared set supplies configuration
stamp prerequisites, with OBJS/FISICS_OBJS retained for ordinary/minimal consumer
compatibility. FisiCs objects without dependency generation do not acquire bogus
`.d` requirements; their separate provenance remains unqualified.

The current real default declarations previously had 340 Clang objects but only
334 dependency paths. Missing were all three headless rendering stubs and the
three shape tool objects. Generated dependency files for those objects therefore
bypassed admission, content-based stale reuse detection and Make header inclusion.
The repaired declarations have 340 dependency paths, zero duplicates and no gaps
against that same object set. This is selected graph coverage, not a claim that
every direct compiler command or supported platform emits dependencies.

Corrected before-code tests show eight failures: six missing declared dependencies
and two actual stale native rebuild cases for a headless stub and shape tool with
preserved header timestamps. Initial fixture setup selected the CLI object as the
implicit default goal; this was corrected to explicit all before relying on the
counterproof. Three new tests use the real object mapping and owning compile rule,
check changed contents rebuild, confirm matching no-ops and assert deduplication.

The regression gate passes 33 distinct methods: three object coverage, thirteen
dependency/input admission, one build identity, four environment, seven compiler
boundary and five configuration admission. Final formatted-source coverage tests
also pass. A fresh compiler-object-coverage-20261007 headless profile builds; its
232 actual generated dependencies are all admitted by the 340-entry declaration.
The matching repeated build leaves all 232 objects and the headless executable
unchanged. Supported Water and scene-project cache-output proofs pass. Read-only
cleanup preview admits 467 files; complete small runtime fixture roots are retained
in the evidence packet. No older profile was rebuilt or user storage cleaned.

This advances ordinary Clang source-checkout coverage. Direct C link inputs,
non-dependency compiler commands, FisiCs behavior, conditional/platform variants,
full SDK/library/toolchain contents, hostile races and coherent multi-file
publication remain open. Build ownership and smoke proof do not establish CFD
physical accuracy, installed package freshness or public release acceptance.

No canonical adoption, commit, installation, release, user evidence deletion or
independent backup coverage changed. Evidence is sealed in
20261007-object-dependency-coverage, outside the frozen seventy-packet backup batch.
The full TL01–TL13 goal remains active and incomplete.
