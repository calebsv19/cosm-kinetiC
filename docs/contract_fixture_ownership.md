# Native contract-fixture compiler ownership

All 58 direct compiler recipes in make/rules-tests-contracts.mk now use the
existing staged atomic_output.py publisher. Compiler flags, output names,
source lists and executable invocation bodies remain unchanged except for six
required link-source additions exposed by real native verification. The collider
fixture now links data_paths.c; two scene compiler lists include the existing
digest/dependency units; the editor pane host includes render command validation.
These are fixture build corrections, not shared API or solver behavior changes.

The publisher admits existing outputs against exact receipts before launching
and rechecks predecessors before publication. Success emits compiler ownership
receipts; unknown predecessors hold without compiler execution. Failed compiler
rebuilds preserve published bytes and ownership receipts. No filename allowlist
was added to cleanup, and existing unclassified bytes were not adopted.

Before-code probes against preserved actual recipe bodies demonstrated missing
receipts and successful overwrite of an unknown predecessor. Three new behavior
methods exercise every selected recipe's publication and owned temporary cleanup,
failed rebuild preservation, and unknown-predecessor refusal. Eighteen cleanup,
twelve atomic publication and one earlier 48-recipe writer method pass (34 methods
in total). The controlled compiler fixtures prove lifecycle behavior, not C ABI
or numerical semantics.

Real native qualification builds all 58 selected recipes and executes their
fixtures (including collider-tests) under the final corrected source lists in
build/profiles/contract-fixture-ownership-20261007. An initial collider link error
and a later five-target dependency failure remain recorded alongside the final
successful full-family gate. No failure was reclassified as passing.

The final read-only cleanup preview accepts 721 owned files in that new profile. Cleanup
apply was tested only inside owned temporary fixture roots. The original
cache-transaction profile remains held on its unknown status-contract executable;
its bytes and missing ownership disposition are preserved. Future invocations in
that existing profile must resolve the old artifact through an explicit reviewed
ownership/retirement action or use a fresh profile. A new writer cannot silently
adopt old output simply because its filename matches.

The new control-only test target is test-contract-fixture-ownership. Source,
before/after logs, exact transformation checks and cleanup plans are sealed in
20261007-contract-fixture-ownership. No commit, canonical adoption, installation,
release, user-output deletion or independent backup occurred. This packet is
outside the frozen archive batch. Broader supported-writer coverage, full build
provenance, resource limits, retirement and canonical adoption remain incomplete.
