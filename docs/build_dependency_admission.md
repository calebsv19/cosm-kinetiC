# Generated dependency admission before Make inclusion

Main Edit now admits every selected generated dependency file before Make reaches
`-include $(DEPS)`. The actual graph-selection command and configuration publication
both receive the selected dependency list. Existing files must live in the build
root, be single-linked regular `.d` files, have exact owned bytes and a dependency
producer receipt. Missing dependency metadata beside an existing object holds,
rather than silently abandoning header tracking. Fresh absent object/dep pairs
remain allowed. Count is bounded at 10,000 selections, 1 MiB per file and 64 MiB
aggregate before reading/hashing the admitted set.

The reader accepts one Clang main object rule and optional empty phony rules for
that rule's declared dependencies. The object target must match the `.d` path.
Continued lines and escaped space/hash/colon/backslash tokens are understood.
Variable expansion, recipes, assignments, include directives, pattern/order-only
rules, arbitrary additional targets, unsupported escapes and control characters
hold. This is deliberately a generated-dependency grammar, not general Make.
A byte ownership receipt alone never authorizes executing Make syntax.

Reads are bounded, descriptor-based and nofollow. Exact output ownership is
rechecked after reading and at the end of the pass. Every ownership receipt's
full stat witness is also checked after its read and after the complete pass.
This retains cooperative ownership assumptions; it does not eliminate a hostile
writer changing the file after admission and before Make inclusion. Immutable
admitted include snapshots or equivalent full confinement remain open.

Corrected before-code counterproof runs six methods with seven failures across
four methods. Both changed and freshly ownership-recorded dependencies execute
the injected Make expression; missing receipt and four unsupported-rule subcases
also bypass admission. An initial ordinary header test was sensitive to whole-
second Make timestamps; it now explicitly advances header mtime for that ordinary
compatibility probe. Final native tests preserve no-op and ordinary header rebuild
behavior, accept a real Clang escaped-space header, reject the unsupported-rule
cases and hold prior receipt rewrites during later file inspection.

The authoritative gate passes 29 distinct methods: seven dependency admission,
one existing build identity, four build environment, five configuration admission
and twelve atomic-output methods. Read-only admission also accepts all 330 actual
dependency files in the retained contract-fixture profile. That read rebuilt or
removed nothing. No physical, installed-package or repository-wide completion
is inferred.

Header-content identity despite preserved timestamps, transitive library/SDK
content, full workflow budgets and coherent build provenance remain incomplete.
No canonical adoption, commit, installation, release, deletion or independent
backup coverage changed. Evidence is sealed in 20261007-build-dependency-admission,
outside the frozen seventy-packet backup batch. The TL01–TL13 goal remains active.
