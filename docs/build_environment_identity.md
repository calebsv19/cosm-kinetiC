# Implicit compiler environment build identity

Main Edit build configuration now binds 25 selected implicit compiler/linker
and toolchain environment settings alongside explicit Make flags and tool identity.
These include CPATH and language include paths, LIBRARY_PATH, compiler/toolchain
search prefixes, SDK/developer/toolchain selections, platform deployment targets,
Clang argument/config overrides, reproducibility settings and selected locale
settings. The exact allowlist is in `build_identity.py`.

Absent and present-empty values are distinct. Original whitespace and search-path
order are retained, rather than stripped or sorted. Each present value is limited
to 64 KiB UTF-8 bytes, with a 1 MiB aggregate bound; oversize selection holds before
stamp publication. No unrelated environment or credentials are recorded. The
configuration digest changes automatically, causing the existing Make stamp
prerequisites to rebuild owning outputs without asking operators to clean.
Configuration generation history and predecessor admission remain unchanged.

Before-code native probes demonstrate three stale-reuse cases across two methods:
CPATH and C_INCLUDE_PATH changes left the prior native object unchanged, while
LIBRARY_PATH changes left an executable linked to the prior static library. The
final probes use actual Clang compilation, ar-built libraries, the owning ordinary
object rule and the real build identity/atomic publication helpers. They prove
matching selections remain no-ops and changed paths rebuild/relink. The link fixture
was adjusted to declare its target/library variables before including identity
rules; corrected before-code counterproof still fails all three cases.

The first regression gate passes 34 distinct methods across native environment
rebuilds, existing build identity, configuration admission, output isolation,
atomic publication and doctor. Two additional policy methods verify exact absent/
empty/path-order preservation and inclusive Unicode/aggregate byte boundaries;
the final focused suite passes all four methods. Total distinct coverage is 36.
No user build-profile or cleanup operation was run; fixtures own temporary trees.

This binds selected environment values, not the contents of directories or files
they select. Complete transitive header/library/SDK/config-file content identity,
wrapper-selected downstream tools, arbitrary tool-specific environment settings
and build/consumer outcome provenance remain open. Status still does not claim
full profile identity verification. This is a trusted local build contract, not
an authenticated or hermetic toolchain.

No canonical adoption, commit, installation, release, deletion or independent
backup coverage changed. Evidence is sealed in 20261007-build-environment, outside
the frozen seventy-packet backup batch. The full TL01–TL13 goal remains incomplete.
