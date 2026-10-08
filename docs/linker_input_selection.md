# Final linker input selection counterproof

Two native fixture experiments using current Main Edit build identity and atomic
publication helpers demonstrate incomplete final executable reuse identity:

- Replacing the selected static archive at the same path, preserving its mtime,
  leaves Make's accepted executable unchanged. That executable returns 1; a fresh
  direct link against current inputs returns 2.
- Creating a previously absent static archive in the earlier directory of the
  unchanged -Lone -Ltwo search list also leaves the executable unchanged. A fresh
  link selects that new archive and returns 3, while the cached executable returns 1.

The fixture uses the actual ordinary object recipe and configuration-selection
helper, plus the existing atomic link wrapper. Flags/environment remain identical
between Make invocations. These are actual native behavior counterproofs, not
assertions inferred from recipe text. No existing user profile was rebuilt.

The installed Darwin linker accepts -dependency_info. Its trace header reports
ld-1230.1. Observed record opcodes are 0x00 (version), 0x10 (selected input),
0x11 (unsuccessful search) and 0x40 (output). The baseline has 442 records,
including duplicates, 42 unique selected paths and 360 unique absent paths.
Selected SDK .tbd stubs and object/archive inputs appear. Missed paths can be
relative to link cwd, including one/libfixture.a; selected paths can be absolute.
After adding the earlier archive, the trace selects it and drops its former miss.
Apple's ld64 source documents these opcode values and writes NUL-terminated
records; the local experiment separately verifies the installed linker format.
Sources: https://github.com/apple-oss-distributions/ld64/blob/main/src/ld/Options.h
and https://github.com/apple-oss-distributions/ld64/blob/main/src/ld/Options.cpp.

The required repair must bind actual selected input contents and failed search
paths, not only explicit .a arguments or the existing selected path. Proposed
acceptance: dependency trace bounds/grammar admission; safe cwd-relative path
normalization; before/after link input observations; retained candidate holds on
mutation; receipt-bound graph reuse admission; both stale cases force relink;
matching inputs remain no-ops; missing/contradictory metadata holds or explicit
legacy refresh. This requires actual recipe integration and native qualification.
Direct source-to-binary commands need source/header closure as well as link inputs.
SDK/toolchain resources, wrapper-selected tools, hostile races, platform variants
and power-loss/multi-file coherence remain separate requirements.

This packet records discovery, not an implemented linker freshness repair or a
passing regression gate. Fixture copies retain the final state; trace pathnames
refer to the original now-terminal temporary fixture. No production source change,
canonical adoption, commit, install, release, user cleanup or backup coverage
change occurred. The full TL01-TL13 lifecycle goal remains active and incomplete.
